#include "chord/chord_node.h"
#include "chord/finger_table.h"
#include "chord/finger_table_node.h"

FingerTableNode::FingerTableNode(Transport* transport)
    : ChordNode(transport), finger_table_(transport->local_info().id) {}

NodeInfo FingerTableNode::find_next_hop(uint64_t key) {
    NodeInfo hop = finger_table_.closest_preceding_finger(key);
    if (hop.id == self_.id) return successor_;
    return hop;
}
void FingerTableNode::on_create() { finger_table_.init_all(self_); }
void FingerTableNode::on_join(const NodeInfo& succ) { finger_table_.init_all(succ); finger_table_.set_finger(0, succ); }
void FingerTableNode::on_stabilize() { finger_table_.set_finger(0, successor_); }
void FingerTableNode::do_maintenance() {
    int i;
    uint64_t target;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        i = finger_table_.next_fix_index();
        target = finger_table_.finger_start(i);
    }

    NodeInfo result = find_successor_for(target, true);

    if (result.id != 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        finger_table_.set_finger(i, result);
    }
}
std::string FingerTableNode::routing_mode_name() const { return "finger_table"; }