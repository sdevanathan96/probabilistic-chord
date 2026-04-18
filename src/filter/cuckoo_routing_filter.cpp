#include <unordered_map>
#include "filter/routing_filter.h"
#include "filter/cuckoo_filter.h"
#include "filter/cuckoo_routing_filter.h"

CuckooRoutingFilter::CuckooRoutingFilter(size_t num_buckets, int fingerprint_bits, int num_bins_bits)
    : RoutingFilter(num_bins_bits),
      c_filter_(num_buckets, fingerprint_bits) {
    filter_ = &c_filter_;
}

std::string CuckooRoutingFilter::filter_name() const {
    return "cuckoo_filter";
}

void CuckooRoutingFilter::filter_rebuild(const std::unordered_map<uint64_t, NodeInfo>& bin_table) {}