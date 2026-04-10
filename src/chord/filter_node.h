#ifndef CHORD_FILTER_NODE_H
#define CHORD_FILTER_NODE_H

#include "chord/chord_node.h"
#include "filter/routing_filter.h"

class FilterNode : public ChordNode {
public:
    FilterNode(Transport* transport, RoutingFilter* filter);

protected:
    NodeInfo find_next_hop(uint64_t key);
    void on_join(const NodeInfo& successor);
    void on_stabilize();
    void on_leave();
    std::string routing_mode_name() const;

private:
    void handle_filter_update(const NodeInfo& from, const Message& msg);

    RoutingFilter* routing_filter_;
};

#endif // CHORD_FILTER_NODE_H