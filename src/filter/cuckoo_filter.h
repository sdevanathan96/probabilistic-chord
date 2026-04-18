#ifndef CHORD_CUCKOO_FILTER_H
#define CHORD_CUCKOO_FILTER_H

#include <cstdint>
#include <vector>
#include <cstddef>
#include "filter/filter.h"


class CuckooFilter: public Filter {
public:
    static const int DEFAULT_BUCKET_SIZE = 4;
    static const int DEFAULT_MAX_KICKS = 500;

    CuckooFilter(size_t num_buckets, int fingerprint_bits,
                 int bucket_size = DEFAULT_BUCKET_SIZE,
                 int max_kicks = DEFAULT_MAX_KICKS);

    bool insert(uint64_t element) override;

    bool lookup(uint64_t element) const override;

    bool remove(uint64_t element) override;

    size_t count() const override;

    size_t capacity() const override;

    double load_factor() const override;

    size_t memory_usage() const override;

private:
    uint32_t compute_fingerprint(uint64_t element) const;

    size_t bucket_index1(uint64_t element) const;
    size_t bucket_index2(size_t b1, uint32_t fingerprint) const;

    uint64_t hash_element(uint64_t element) const;
    uint64_t hash_fingerprint(uint32_t fingerprint) const;

    std::vector<std::vector<uint32_t>> buckets_;

    size_t num_buckets_;
    int fingerprint_bits_;
    int bucket_size_;
    int max_kicks_;
    uint32_t fingerprint_mask_;
    size_t count_;
};

#endif // CHORD_CUCKOO_FILTER_H