#include "chord/chord_node.h"
#include "filter/routing_filter.h"
#include "chord/filter_node.h"

FilterNode::FilterNode(Transport* transport, RoutingFilter* filter)
    : ChordNode(transport), routing_filter_(filter), next_maintenance_index_(0),
    maintenance_cycles_(0) {
    transport_->register_handler(MessageType::FILTER_UPDATE,
        std::bind(&FilterNode::handle_filter_update, this,
        std::placeholders::_1, std::placeholders::_2));
}

NodeInfo FilterNode::find_next_hop(uint64_t key) {
    NodeInfo result;
    if (routing_filter_->lookup(key, result) && result.id != self_.id) return result;
    return successor_;
}
void FilterNode::on_join(const NodeInfo& succ) { routing_filter_->insert(succ.id, succ); }
void FilterNode::on_stabilize() { routing_filter_->insert(successor_.id, successor_); }
void FilterNode::on_leave() {
    std::vector<uint8_t> payload;
    payload.push_back(1);
    std::vector<uint8_t> id_bytes = pack_uint64(self_.id);
    payload.insert(payload.end(), id_bytes.begin(), id_bytes.end());
    Message filter_msg(MessageType::FILTER_UPDATE, payload);

    transport_->send(successor_, filter_msg);
    transport_->send(predecessor_, filter_msg);
 }
std::string FilterNode::routing_mode_name() const {
    return routing_filter_->filter_name();
}

void FilterNode::handle_filter_update(const NodeInfo& from, const Message& msg) {
    if (!routing_filter_) return;

    size_t offset = 0;
    if (offset >= msg.payload.size()) return;
    uint8_t action = msg.payload[offset++];

    uint64_t key_range_id;
    if (!unpack_uint64(msg.payload, offset, key_range_id)) return;

    NodeInfo node;
    if (action == 0) {
        if (!unpack_node_info(msg.payload, offset, node)) return;
        routing_filter_->insert(key_range_id, node);
    } else {
        routing_filter_->remove(key_range_id);
    }
}

void FilterNode::do_maintenance() {
    uint64_t target;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        target = self_.id + (1ULL << next_maintenance_index_);
        next_maintenance_index_ = (next_maintenance_index_ + 1) % 64;
    }

    NodeInfo result = find_successor_for(target, true);

    if (result.id != 0) {
        routing_filter_->insert(result.id, result);
    }

    maintenance_cycles_++;
    if (maintenance_cycles_ % 64 == 0) {
        routing_filter_->rebuild();
    }
}