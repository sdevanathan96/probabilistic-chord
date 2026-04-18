#include <cstdint>
#include <iostream>
#include <unordered_map>
#include "transport/types.h"
#include "filter/routing_filter.h"
#include "filter/filter.h"

RoutingFilter::RoutingFilter(int num_bins_bits)
    : filter_(nullptr),
      num_bins_bits_(num_bins_bits),
      num_bins_(1ULL << num_bins_bits) {}


uint64_t RoutingFilter::to_bin(uint64_t position, int num_bins_bits) {
    return position >> (64 - num_bins_bits);
}

bool RoutingFilter::insert(uint64_t key_range_id, const NodeInfo& node) {
    uint64_t bin = to_bin(key_range_id, num_bins_bits_);
    auto it = bin_table_.find(bin);
    if (it != bin_table_.end()) {
        if (node.id < it->second.id) {
            bin_table_[bin] = node;
            bin_to_key_[bin] = key_range_id;
        }
        return true;
    }
    if (!filter_->insert(bin)) return false;
    bin_table_[bin] = node;
    bin_to_key_[bin] = key_range_id;
    return true;
}

bool RoutingFilter::remove(uint64_t key_range_id) {
    uint64_t bin = to_bin(key_range_id, num_bins_bits_);
    auto it = bin_to_key_.find(bin);
    if (it == bin_to_key_.end()) return false;
    if (it->second != key_range_id) return false;
    filter_->remove(bin);
    bin_table_.erase(bin);
    bin_to_key_.erase(bin);
    return true;
}

bool RoutingFilter::lookup(uint64_t key, NodeInfo& result) {
    if (bin_table_.empty()) return false;

    uint64_t key_bin = to_bin(key, num_bins_bits_);
    for (size_t i = 1; i <= MAX_PROBE_DISTANCE; ++i) {
        uint64_t probe = (key_bin + num_bins_ - i) % num_bins_;

        if (filter_->lookup(probe)) {
            auto it = bin_table_.find(probe);
            if (it != bin_table_.end()) {
                result = it->second;
                return true;
            }
        }
    }
    if (!bin_table_.empty()) {
        result = bin_table_.begin()->second;
        return true;
    }
    return false;
}

size_t RoutingFilter::count() const {
    return filter_->count();
}

size_t RoutingFilter::memory_usage() const {
    return filter_->memory_usage() + bin_table_.size() * (sizeof(uint64_t) + sizeof(NodeInfo)) + bin_to_key_.size() * (sizeof(uint64_t)*2);
}

void RoutingFilter::rebuild() {
    filter_rebuild(bin_table_);
}