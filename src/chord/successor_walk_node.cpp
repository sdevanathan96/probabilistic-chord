#include "chord/successor_walk_node.h"

SuccessorWalkNode::SuccessorWalkNode(Transport* t) : ChordNode(t) {}
NodeInfo SuccessorWalkNode::find_next_hop(uint64_t key) { return successor_; }
std::string SuccessorWalkNode::routing_mode_name() const { return "successor_walk"; }