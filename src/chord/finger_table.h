#ifndef CHORD_FINGER_TABLE_H
#define CHORD_FINGER_TABLE_H

#include <vector>
#include <cstdint>
#include "transport/types.h"


class FingerTable {
public:
    static const int M = 64;

    explicit FingerTable(NodeId owner_id);

    uint64_t finger_start(int i) const;

    NodeInfo get_finger(int i) const;

    void set_finger(int i, const NodeInfo& node);

    NodeInfo closest_preceding_finger(NodeId key) const;

    void init_all(const NodeInfo& node);

    int next_fix_index();

private:
    NodeId owner_id_;
    std::vector<NodeInfo> fingers_;
    int next_fix_;
    void verify_index(int i) const;
};

#endif // CHORD_FINGER_TABLE_H