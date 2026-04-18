#ifndef CHORD_ROUTING_FILTER_H
#define CHORD_ROUTING_FILTER_H

#include <cstdint>
#include <unordered_map>
#include "transport/types.h"
#include "filter/filter.h"

class RoutingFilter {
public:
    RoutingFilter(int num_bins_bits);
    virtual ~RoutingFilter() {}

    bool insert(uint64_t key_range_id, const NodeInfo& node);
    bool remove(uint64_t key_range_id);
    bool lookup(uint64_t key, NodeInfo& result);
    size_t count() const;
    size_t memory_usage() const;
    void rebuild();

    virtual std::string filter_name() const = 0;

protected:
    static const size_t MAX_PROBE_DISTANCE = 128;
    static uint64_t to_bin(uint64_t position, int num_bins_bits);
    virtual void filter_rebuild(const std::unordered_map<uint64_t, NodeInfo>& bin_table) = 0;

    Filter* filter_;
    int num_bins_bits_;
    size_t num_bins_;
    std::unordered_map<uint64_t, NodeInfo> bin_table_;
    std::unordered_map<uint64_t, uint64_t> bin_to_key_;
};

#endif // CHORD_ROUTING_FILTER_H