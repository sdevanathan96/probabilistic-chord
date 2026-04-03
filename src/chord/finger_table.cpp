#include <vector>
#include <cstdint>
#include "chord/finger_table.h"
#include "chord/ring_utils.h"


FingerTable::FingerTable(NodeId owner_id) {
    owner_id_ = owner_id;
    fingers_.assign(M, NodeInfo(owner_id, "", 0));
    next_fix_ = 0;
}

uint64_t FingerTable::finger_start(int i) const {
    return (owner_id_ + (1ULL << i));
}

void FingerTable::verify_index(int i) const {
    if (i < 0 || i >= M) throw std::out_of_range("Finger table index out of range!");
}

NodeInfo FingerTable::get_finger(int i) const {
    verify_index(i);
    return fingers_[i];
}

void FingerTable::set_finger(int i, const NodeInfo& node) {
    verify_index(i);
    fingers_[i] = node;
}

void FingerTable::init_all(const NodeInfo& node) {
    for (auto &finger : fingers_) finger = node;
}

int FingerTable::next_fix_index() {
    int idx = next_fix_;
    next_fix_ = (next_fix_ + 1) % M;
    return idx;
}

NodeInfo FingerTable::closest_preceding_finger(NodeId key) const {
    for (int i = M - 1; i >= 0; --i) {
        if (fingers_[i].id != 0 &&
            in_range(fingers_[i].id, owner_id_, key) &&
            fingers_[i].id != key) {
            return fingers_[i];
        }
    }
    return NodeInfo(owner_id_, "", 0);
}

