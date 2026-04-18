#include <cstdint>
#include <string>
#include <iostream>
#include <set>
#include "transport/types.h"
#include "chord/ring_utils.h"
#include "chord/chord_node.h"


ChordNode::ChordNode(Transport* transport) : transport_(transport){
    self_ = transport_->local_info();
    successor_ = self_;
    predecessor_ = self_;
    response_ready_ = false;
    metrics_ = nullptr;
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
    on_create();
}

void ChordNode::join(const NodeInfo& bootstrap) {
    double latency_us = 0.0;
    {
        ScopedTimer timer(latency_us);
        NodeInfo current = bootstrap;
        NodeInfo successor;
        std::set<NodeId> visited;

        while (true) {
            if (visited.count(current.id)) {
                throw std::runtime_error("Failed to join: routing loop");
            }
            visited.insert(current.id);

            Message req = make_find_successor_req(self_.id, 0);
            Message response;
            if (!send_rpc(current, req, MessageType::FIND_SUCCESSOR_RESP, response)) {
                throw std::runtime_error("Failed to join: no response");
            }

            size_t offset = 0;
            if (!unpack_node_info(response.payload, offset, successor)) {
                throw std::runtime_error("Failed to join: invalid response");
            }

            bool is_final = false;
            if (offset < response.payload.size()) {
                is_final = (response.payload[offset] == 1);
            }

            if (is_final) break;
            current = successor;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            successor_ = successor;
            predecessor_ = NodeInfo();
        }
        on_join(successor_);
    }
    if (metrics_) {
        MembershipRecord record;
        record.node_id = self_.id;
        record.event_type = "join";
        record.messages_sent = 1;
        record.latency_us = latency_us;
        metrics_->record_membership(record);
    }
}

NodeInfo ChordNode::lookup(uint64_t key) {
    double latency_us = 0.0;
    NodeInfo result;
    int hop_count = 0;

    {
        ScopedTimer timer(latency_us);
        result = find_successor_for(key, hop_count);
    }

    if (metrics_ && result.id != 0) {
        LookupRecord record;
        record.key = key;
        record.owner_id = result.id;
        record.hop_count = hop_count;
        record.latency_us = latency_us;
        record.used_fallback = false;
        record.routing_mode = routing_mode_name();
        metrics_->record_lookup(record);
    }

    return result;
}

void ChordNode::stabilize() {
    NodeInfo succ;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        succ = successor_;
    }
    Message req(MessageType::STABILIZE_REQ, {});
    Message response;
    if (!send_rpc(succ, req, MessageType::STABILIZE_RESP, response)) return;
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
    if (metrics_) {
        metrics_->count_maintenance_message();
        metrics_->count_message();
    }
    transport_->send(successor_, notify_msg);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        on_stabilize();
    }
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

    NodeInfo result;
    bool is_final = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (in_range(key, self_.id, successor_.id)) {
            result = successor_;
            is_final = true;
        } else {
            result = find_next_hop(key);
            if (result.id == self_.id || result.id == 0) {
                result = successor_;
            }
            is_final = false;
        }
    }

    std::vector<uint8_t> resp_payload = pack_node_info(result);
    resp_payload.push_back(is_final ? 1 : 0);
    Message response(MessageType::FIND_SUCCESSOR_RESP, resp_payload);
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
    return make_find_successor_req(key, 0);
}

Message ChordNode::make_find_successor_req(uint64_t key, int hop_count) {
    std::vector<uint8_t> payload = pack_uint64(key);
    payload.push_back(static_cast<uint8_t>((hop_count >> 8) & 0xFF));
    payload.push_back(static_cast<uint8_t>(hop_count & 0xFF));
    std::vector<uint8_t> origin = pack_uint64(self_.id);
    payload.insert(payload.end(), origin.begin(), origin.end());
    return Message(MessageType::FIND_SUCCESSOR_REQ, payload);
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
    if (metrics_) metrics_->count_message();
    std::unique_lock<std::mutex> lock(rpc_mutex_);
    rpc_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&]{return response_ready_;});
    if (response_ready_) {
        response = pending_response_;
        return true;
    }
    return false;
}

void ChordNode::leave() {
    on_leave();
    double latency_us = 0.0;
     {
        ScopedTimer timer(latency_us);
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
    if (metrics_) {
        MembershipRecord record;
        record.node_id = self_.id;
        record.event_type = "leave";
        record.messages_sent = 2;
        record.latency_us = latency_us;
        metrics_->record_membership(record);
    }
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

NodeInfo ChordNode::find_successor_for(uint64_t key) {
    int hop_count = 0;
    return find_successor_for(key, hop_count);
}

NodeInfo ChordNode::find_successor_for(uint64_t key, int& hop_count) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (in_range(key, self_.id, successor_.id)) {
            hop_count = 1;
            return successor_;
        }
        if (successor_ == self_) {
            hop_count = 0;
            return self_;
        }
    }

    NodeInfo current;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current = successor_;
    }

    NodeInfo next_hop = find_next_hop(key);
    if (next_hop.id != 0) {
        current = next_hop;
    }

    std::set<NodeId> visited;

    for (hop_count = 1; ; ++hop_count) {
        if (visited.count(current.id)) {
            std::lock_guard<std::mutex> lock(mutex_);
            return successor_;
        }
        visited.insert(current.id);
        Message req = make_find_successor_req(key, hop_count);
        Message response;
        if (!send_rpc(current, req, MessageType::FIND_SUCCESSOR_RESP, response)) {
            return NodeInfo();
        }

        NodeInfo result;
        size_t offset = 0;
        if (!unpack_node_info(response.payload, offset, result)) {
            return NodeInfo();
        }
        bool is_final = false;
        if (offset < response.payload.size()) {
            is_final = (response.payload[offset] == 1);
        }

        if (is_final) {
            return result;
        }
        current = result;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    return successor_;
}

NodeInfo ChordNode::find_next_hop(uint64_t key) {
    std::lock_guard<std::mutex> lock(mutex_);
    return successor_;
}

NodeInfo ChordNode::find_successor_for(uint64_t key, bool is_maintenance) {
    NodeInfo result = find_successor_for(key);
    if (is_maintenance && metrics_) {
        metrics_->count_maintenance_message();
    }
    return result;
}

void ChordNode::on_create() {}
void ChordNode::on_join(const NodeInfo& successor) {}
void ChordNode::on_stabilize() {}
void ChordNode::on_leave() {}
void ChordNode::do_maintenance() {}

std::string ChordNode::routing_mode_name() const {
    return "successor_walk";
}

void ChordNode::set_metrics(Metrics* metrics) {
    metrics_ = metrics;
}

ChordNode::~ChordNode() {}