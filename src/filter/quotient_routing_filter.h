#ifndef CHORD_QUOTIENT_ROUTING_FILTER_H
#define CHORD_QUOTIENT_ROUTING_FILTER_H

#include <unordered_map>
#include "filter/routing_filter.h"
#include "filter/quotient_filter.h"

class QuotientRoutingFilter : public RoutingFilter {
public:
    QuotientRoutingFilter(int q_bits, int r_bits,
                          int num_bins_bits = 10);
    std::string filter_name() const override;

protected:
    void filter_rebuild(const std::unordered_map<uint64_t, NodeInfo>& bin_table) override;

private:
    int q_bits_;
    int r_bits_;
    QuotientFilter q_filter_;
};

#endif // CHORD_QUOTIENT_ROUTING_FILTER_H