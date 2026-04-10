#ifndef CHORD_SUCCESSOR_WALK_NODE_H
#define CHORD_SUCCESSOR_WALK_NODE_H

#include "chord/chord_node.h"


class SuccessorWalkNode : public ChordNode {
public:
    explicit SuccessorWalkNode(Transport* transport);

protected:
    NodeInfo find_next_hop(uint64_t key);
    std::string routing_mode_name() const;
};

#endif // CHORD_SUCCESSOR_WALK_NODE_H