#ifndef CHORD_FINGER_TABLE_NODE_H
#define CHORD_FINGER_TABLE_NODE_H

#include "chord/chord_node.h"
#include "chord/finger_table.h"
#include "transport/transport.h"

class FingerTableNode : public ChordNode {
public:
    explicit FingerTableNode(Transport* transport);
    void do_maintenance();
    void print_finger_table() const;

protected:
    NodeInfo find_next_hop(uint64_t key);
    void on_create();
    void on_join(const NodeInfo& successor);
    void on_stabilize();
    std::string routing_mode_name() const;

private:
    FingerTable finger_table_;
};

#endif // CHORD_FINGER_TABLE_NODE_H