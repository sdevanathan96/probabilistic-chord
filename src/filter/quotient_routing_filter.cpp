#include <unordered_map>
#include "filter/routing_filter.h"
#include "filter/quotient_filter.h"
#include "filter/quotient_routing_filter.h"

QuotientRoutingFilter::QuotientRoutingFilter(int q_bits, int r_bits, int num_bins_bits)
    : RoutingFilter(num_bins_bits),
      q_filter_(q_bits, r_bits),
      q_bits_(q_bits),
      r_bits_(r_bits) {
    filter_ = &q_filter_;
}

std::string QuotientRoutingFilter::filter_name() const {
    return "quotient_filter";
}

void QuotientRoutingFilter::filter_rebuild(const std::unordered_map<uint64_t, NodeInfo>& bin_table) {
    QuotientFilter new_filter(q_bits_, r_bits_);
    for (const auto& entry : bin_table) {
        new_filter.insert(entry.first);
    }
    std::swap(q_filter_, new_filter);
    filter_ = &q_filter_;
}
