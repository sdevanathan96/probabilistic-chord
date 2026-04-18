#ifndef CHORD_SUCCESSOR_WALK_NODE_H
#define CHORD_SUCCESSOR_WALK_NODE_H

#include "chord/chord_node.h"


class SuccessorWalkNode : public ChordNode {
public:
    explicit SuccessorWalkNode(Transport* transport);
    virtual NodeInfo find_next_hop(uint64_t key);
protected:
    std::string routing_mode_name() const;
};

#endif // CHORD_SUCCESSOR_WALK_NODE_H