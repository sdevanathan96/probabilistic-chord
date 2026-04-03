#include <cstdint>
#include <string>
#include "transport/types.h"
#include "chord/ring_utils.h"
#include "chord/node.h"


ChordNode::ChordNode(Transport* transport) {
    transport_ = transport;
    self_ = transport_->local_info();
    successor_ = self_;
    predecessor_ = self_;
    response_ready_ = false;
    transport_->register_handler(MessageType::FIND_SUCCESSOR_REQ,
        std::bind(&ChordNode::handle_find_successor, this, std::placeholders::_1, std::placeholders::_2));
    transport_->register_handler(MessageType::STABILIZE_REQ,
        std::bind(&ChordNode::handle_stabilize_req, this, std::placeholders::_1, std::placeholders::_2));
    transport_->register_handler(MessageType::NOTIFY,
        std::bind(&ChordNode::handle_notify, this, std::placeholders::_1, std::placeholders::_2));
    transport_->register_handler(MessageType::FIND_SUCCESSOR_RESP, 
        [this](const NodeInfo& from, const Message& msg) {
            std::lock_guard<std::mutex> lock(rpc_mutex_);
            pending_response_ = msg;
            response_ready_ = true;
            rpc_cv_.notify_one();
        });
    transport_->register_handler(MessageType::STABILIZE_RESP, 
        [this](const NodeInfo& from, const Message& msg) {
            std::lock_guard<std::mutex> lock(rpc_mutex_);
            pending_response_ = msg;
            response_ready_ = true;
            rpc_cv_.notify_one();
        });
    transport_->register_handler(MessageType::LEAVE_NOTIFY,
        std::bind(&ChordNode::handle_leave_notify, this,
        std::placeholders::_1, std::placeholders::_2));
}

void ChordNode::create() {
    std::lock_guard<std::mutex> lock(mutex_);
    successor_ = self_;
    predecessor_ = self_;
}

void ChordNode::join(const NodeInfo& bootstrap) {
    Message req = make_find_successor_req(self_.id);
    Message response;
    if (!send_rpc(bootstrap, req, MessageType::FIND_SUCCESSOR_RESP, response)) {
        throw std::runtime_error("Failed to join: no response from bootstrap");
    }
    NodeInfo successor;
    size_t offset = 0;
    if (!unpack_node_info(response.payload, offset, successor)) {
        throw std::runtime_error("Failed to join: invalid response from bootstrap");
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        successor_ = successor;
        predecessor_ = NodeInfo();
    }
}

NodeInfo ChordNode::lookup(uint64_t key) {
    if (in_range(key, self_.id, successor_.id)) return successor_;
    if (successor_ == self_){
        return self_;
    }
    Message req = make_find_successor_req(key);
    Message response;
    if (!send_rpc(successor_, req, MessageType::FIND_SUCCESSOR_RESP, response)) {
            throw std::runtime_error("Lookup failed: no response from successor");
        }   
    NodeInfo successor;
    size_t offset = 0;
    if (!unpack_node_info(response.payload, offset, successor)) {
        throw std::runtime_error("Lookup failed: invalid response from successor"); 
    }
    return successor;
}

void ChordNode::stabilize() {
    NodeInfo succ;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        succ = successor_;
    }
    Message req(MessageType::STABILIZE_REQ, {});
    Message response;
    if (!send_rpc(succ, req, MessageType::STABILIZE_REQ, response)) return;
    NodeInfo pred;
    size_t offset = 0;
    if (!unpack_node_info(response.payload, offset, pred)) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pred.id != 0 && in_range(pred.id, self_.id, successor_.id)){
            successor_ = pred;
        }
    }
    Message notify_msg;
    notify_msg.type = MessageType::NOTIFY;
    notify_msg.payload = pack_node_info(self_);
    transport_->send(successor_, notify_msg);
}

void ChordNode::notify(const NodeInfo& candidate) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (predecessor_.id == 0 || 
        in_range(candidate.id, predecessor_.id, self_.id)
    ){
        predecessor_ = candidate;
    }
}
NodeInfo ChordNode::get_info() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return self_;
}
NodeInfo ChordNode::get_successor() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return successor_;
}
NodeInfo ChordNode::get_predecessor() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return predecessor_;
}

void ChordNode::handle_find_successor(const NodeInfo& from, const Message& msg) {
    size_t offset = 0;
    uint64_t key;
    if (!unpack_uint64(msg.payload, offset, key)) return;
    bool is_in_range;
    NodeInfo successor;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        is_in_range = in_range(key, self_.id, successor_.id);
        if (is_in_range) {
            successor = successor_;
        }
    }
    if (!is_in_range) {
        successor = lookup(key);
    }
    Message response(MessageType::FIND_SUCCESSOR_RESP, pack_node_info(successor));
    transport_->send(from, response);
}

void ChordNode::handle_stabilize_req(const NodeInfo& from, const Message& msg) {
    NodeInfo pred;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pred = predecessor_;
    }
    Message response(MessageType::STABILIZE_RESP, pack_node_info(pred));
    transport_->send(from, response);
}

void ChordNode::handle_notify(const NodeInfo& from, const Message& msg) {
    size_t offset = 0;
    NodeInfo candidate;
    if (!unpack_node_info(msg.payload, offset, candidate)) return;
    notify(candidate);
}

Message ChordNode::make_find_successor_req(uint64_t key) {
    return Message(MessageType::FIND_SUCCESSOR_REQ, pack_uint64(key));
}

bool ChordNode::send_rpc(
    const NodeInfo& dest, const Message& request,
    MessageType response_type, Message& response,
    int timeout_ms
) {
    {
        std::lock_guard<std::mutex> lock(rpc_mutex_);
        response_ready_ = false;
    }
    if (!transport_->send(dest, request)) return false;
    std::unique_lock<std::mutex> lock(rpc_mutex_);
    rpc_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&]{return response_ready_;});
    if (response_ready_) {
        response = pending_response_;
        return true;
    }
    return false;
}

void ChordNode::leave() {
    NodeInfo pred;
    NodeInfo succ;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pred = predecessor_;
        succ = successor_;
    }
    Message to_succ(MessageType::LEAVE_NOTIFY, pack_node_info(pred));
    Message to_pred(MessageType::LEAVE_NOTIFY, pack_node_info(succ));
    transport_->send(successor_, to_succ);
    transport_->send(predecessor_, to_pred);
}

void ChordNode::handle_leave_notify(const NodeInfo& from, const Message& msg) {
    size_t offset = 0;
    NodeInfo candidate;
    if (!unpack_node_info(msg.payload, offset, candidate)) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (from.id == successor_.id) {
            successor_ = candidate;
        } else if (from.id == predecessor_.id) {
            predecessor_ = candidate;
        }
    }
}

ChordNode::~ChordNode() {}