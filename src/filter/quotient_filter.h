#ifndef CHORD_QUOTIENT_FILTER_H
#define CHORD_QUOTIENT_FILTER_H

#include <cstdint>
#include <vector>
#include <cstddef>
#include "filter/filter.h"


class QuotientFilter : public Filter {
public:
    QuotientFilter(int q_bits, int r_bits);

    bool insert(uint64_t element) override;

    bool lookup(uint64_t element) const override;

    bool remove(uint64_t element) override;

    size_t count() const override;

    size_t capacity() const override;

    double load_factor() const override;

    size_t memory_usage() const override;

private:

    uint64_t get_remainder(size_t slot) const;
    void set_remainder(size_t slot, uint64_t remainder);

    bool is_occupied(size_t slot) const;
    bool is_continuation(size_t slot) const;
    bool is_shifted(size_t slot) const;
    bool is_deleted(size_t slot) const;

    void set_occupied(size_t slot, bool val);
    void set_continuation(size_t slot, bool val);
    void set_shifted(size_t slot, bool val);
    void set_deleted(size_t slot, bool val);

    bool is_empty(size_t slot) const;

    uint64_t hash_element(uint64_t element) const;

    size_t get_quotient(uint64_t fingerprint) const;

    uint64_t get_remainder_from_fp(uint64_t fingerprint) const;

    size_t find_run_start(size_t quotient) const;

    size_t find_cluster_start(size_t slot) const;

    size_t next_slot(size_t slot) const;

    size_t prev_slot(size_t slot) const;

    bool shift_right(size_t slot);

    bool shift_left(size_t slot);

    struct Slot {
        uint64_t remainder;
        bool is_occupied;
        bool is_continuation;
        bool is_shifted;
        bool is_deleted;

        Slot() : remainder(0), is_occupied(false),
                 is_continuation(false), is_shifted(false), is_deleted(false) {}
    };

    std::vector<Slot> slots_;
    int q_bits_;
    int r_bits_;
    size_t num_slots_;
    uint64_t remainder_mask_;
    size_t count_;
};

#endif // CHORD_QUOTIENT_FILTER_H