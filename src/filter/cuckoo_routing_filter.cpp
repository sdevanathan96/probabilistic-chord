#include <unordered_map>
#include "filter/routing_filter.h"
#include "filter/cuckoo_filter.h"
#include "filter/cuckoo_routing_filter.h"

CuckooRoutingFilter::CuckooRoutingFilter(size_t num_buckets, int fingerprint_bits)
    : filter_(num_buckets, fingerprint_bits) {}

bool CuckooRoutingFilter::insert(uint64_t key_range_id, const NodeInfo& node) {
    if (filter_.insert(key_range_id)) {
        side_table_[key_range_id] = node;
        return true;
    }
    return false;
}

bool CuckooRoutingFilter::remove(uint64_t key_range_id) {
    if (filter_.remove(key_range_id)) {
        side_table_.erase(key_range_id);
        return true;
    }
    return false;
}

bool CuckooRoutingFilter::lookup(uint64_t key, NodeInfo& result) {
    if (side_table_.empty()) return false;
    NodeInfo best;
    bool found = false;
    uint64_t smallest_distance = UINT64_MAX;

    for (const auto& entry : side_table_) {
        uint64_t node_id = entry.first;
        uint64_t dist = key - node_id;
        if (dist > 0 && dist < smallest_distance) {
            smallest_distance = dist;
            best = entry.second;
            found = true;
        }
    }

    if (found) {
        result = best;
        return true;
    }

    result = side_table_.begin()->second;
    return true;
}

size_t CuckooRoutingFilter::count() const {
    return filter_.count();
}

size_t CuckooRoutingFilter::memory_usage() const {
    return filter_.memory_usage() + side_table_.size() * (sizeof(uint64_t) + sizeof(NodeInfo));
}

std::string CuckooRoutingFilter::filter_name() const {
    return "cuckoo_filter";
}