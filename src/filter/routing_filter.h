#ifndef CHORD_ROUTING_FILTER_H
#define CHORD_ROUTING_FILTER_H

#include <cstdint>
#include "transport/types.h"

class RoutingFilter {
public:
    virtual ~RoutingFilter() {}

    virtual bool insert(uint64_t key_range_id, const NodeInfo& node) = 0;

    virtual bool remove(uint64_t key_range_id) = 0;

    virtual bool lookup(uint64_t key, NodeInfo& result) = 0;

    virtual size_t count() const = 0;

    virtual size_t memory_usage() const = 0;

    virtual std::string filter_name() const = 0;
};

#endif // CHORD_ROUTING_FILTER_H