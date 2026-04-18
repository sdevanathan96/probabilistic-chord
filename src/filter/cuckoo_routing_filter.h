#ifndef CHORD_CUCKOO_ROUTING_FILTER_H
#define CHORD_CUCKOO_ROUTING_FILTER_H

#include <unordered_map>
#include "filter/routing_filter.h"
#include "filter/cuckoo_filter.h"

class CuckooRoutingFilter : public RoutingFilter {
public:
    CuckooRoutingFilter(size_t num_buckets, int fingerprint_bits,
                        int num_bins_bits = 10);
    std::string filter_name() const override;

protected:
    void filter_rebuild(const std::unordered_map<uint64_t, NodeInfo>& bin_table) override;

private:
    CuckooFilter c_filter_;
    size_t num_buckets_;
    int fingerprint_bits_;
};

#endif // CHORD_CUCKOO_ROUTING_FILTER_H