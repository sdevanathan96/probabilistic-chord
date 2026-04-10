#ifndef CHORD_CUCKOO_ROUTING_FILTER_H
#define CHORD_CUCKOO_ROUTING_FILTER_H

#include <unordered_map>
#include "filter/routing_filter.h"
#include "filter/cuckoo_filter.h"

class CuckooRoutingFilter : public RoutingFilter {
public:
    CuckooRoutingFilter(size_t num_buckets, int fingerprint_bits);

    bool insert(uint64_t key_range_id, const NodeInfo& node);
    bool remove(uint64_t key_range_id);
    bool lookup(uint64_t key, NodeInfo& result);
    size_t count() const;
    size_t memory_usage() const;
    std::string filter_name() const;

private:
    CuckooFilter filter_;
    std::unordered_map<uint64_t, NodeInfo> side_table_;
};

#endif // CHORD_CUCKOO_ROUTING_FILTER_H