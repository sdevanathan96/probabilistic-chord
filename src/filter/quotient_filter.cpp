#include <cstdint>
#include <vector>
#include <cstddef>
#include <iostream>
#include "filter/quotient_filter.h"

QuotientFilter::QuotientFilter(int q_bits, int r_bits)
    : q_bits_(q_bits), r_bits_(r_bits), count_(0) {
    num_slots_ = 1ULL << q_bits;
    remainder_mask_ = (1ULL << r_bits) - 1;
    slots_.resize(num_slots_);
    count_ = 0;
}

uint64_t QuotientFilter::hash_element(uint64_t element) const{
    element ^= element >> 33;
    element *= 0xff51afd7ed558ccdULL;
    element ^= element >> 33;
    element *= 0xc4ceb9fe1a85ec53ULL;
    element ^= element >> 33;
    return element & ((1ULL << (q_bits_ + r_bits_)) - 1);
}

size_t QuotientFilter::get_quotient(uint64_t fingerprint) const {
    return (fingerprint >> r_bits_) & ((1ULL << q_bits_) - 1);
}

uint64_t QuotientFilter::get_remainder_from_fp(uint64_t fingerprint) const {
    return fingerprint & remainder_mask_;
}

size_t QuotientFilter::next_slot(size_t slot) const {
    return (slot + 1) % num_slots_;
}

size_t QuotientFilter::prev_slot(size_t slot) const {
    return (slot + num_slots_ - 1) % num_slots_;
}

bool QuotientFilter::is_occupied(size_t slot) const {
    return slots_[slot].is_occupied;
}

bool QuotientFilter::is_continuation(size_t slot) const {
    return slots_[slot].is_continuation;
}

bool QuotientFilter::is_shifted(size_t slot) const {
    return slots_[slot].is_shifted;
}

bool QuotientFilter::is_deleted(size_t slot) const {
    return slots_[slot].is_deleted;
}

void QuotientFilter::set_occupied(size_t slot, bool val) {
    slots_[slot].is_occupied = val;
}

void QuotientFilter::set_continuation(size_t slot, bool val) {
    slots_[slot].is_continuation = val;
}

void QuotientFilter::set_shifted(size_t slot, bool val) {
    slots_[slot].is_shifted = val;
}

void QuotientFilter::set_deleted(size_t slot, bool val) {
    slots_[slot].is_deleted = val;
}

bool QuotientFilter::is_empty(size_t slot) const {
    return !is_occupied(slot) && !is_shifted(slot) && !is_continuation(slot);
}


size_t QuotientFilter::find_cluster_start(size_t slot) const {
    size_t num_steps = 0;
    while (is_shifted(slot) && num_steps < num_slots_) {
        slot = prev_slot(slot);
        num_steps++;
    }
    return slot;
}

size_t QuotientFilter::find_run_start(size_t quotient) const {
    size_t cluster_start = find_cluster_start(quotient);
    size_t slot = cluster_start;
    size_t num_occupied = 0;
    while (slot != quotient){
        if (is_occupied(slot)) num_occupied++;
        slot = next_slot(slot);
    }
    slot = cluster_start;
    while (num_occupied > 0) {
        slot = next_slot(slot);
        while (is_continuation(slot)) {
            slot = next_slot(slot);
        }
        num_occupied--;
    }
    return slot;
}

bool QuotientFilter::shift_right(size_t slot) {
    size_t empty = slot;
    size_t prev;
    size_t num_steps = 0;
    while (!is_empty(empty) && !is_deleted(empty) && num_steps < num_slots_) {
        empty = next_slot(empty);
        num_steps++;
    }
    if (num_steps >= num_slots_) return false;
    while (empty != slot){
        prev = prev_slot(empty);
        slots_[empty].remainder = slots_[prev].remainder;
        slots_[empty].is_continuation = slots_[prev].is_continuation;
        slots_[empty].is_shifted = true;
        empty = prev;
    }
    slots_[slot].remainder = 0;
    slots_[slot].is_continuation = false;
    slots_[slot].is_shifted = false;
    return true;
}

bool QuotientFilter::insert(uint64_t element) {
    if (count_ >= num_slots_) return false;
    uint64_t fingerprint = hash_element(element);
    size_t quotient = get_quotient(fingerprint);
    uint64_t remainder = get_remainder_from_fp(fingerprint);

    if (is_empty(quotient)) {
        set_remainder(quotient, remainder);
        set_occupied(quotient, true);
        count_++;
        return true;
    }

    bool was_occupied = is_occupied(quotient);
    set_occupied(quotient, true);

    size_t run_start = find_run_start(quotient);
    size_t slot = run_start;

    if (was_occupied) {
        while (get_remainder(slot) < remainder) {
            slot = next_slot(slot);
            if (!is_continuation(slot)) break;
        }
    }
    if (!shift_right(slot)) return false;
    set_remainder(slot, remainder);
    if (slot != run_start) {
        set_continuation(slot, true);
    }
    if (slot != quotient) {
        set_shifted(slot, true);
    }
    if (slot == run_start && was_occupied) {
        size_t shifted_slot = next_slot(slot);
        if (!is_empty(shifted_slot)) {
            set_continuation(shifted_slot, true);
        }
    }
    count_++;
    return true;
}

bool QuotientFilter::lookup(uint64_t element) const {
    uint64_t fingerprint = hash_element(element);
    size_t quotient = get_quotient(fingerprint);
    uint64_t remainder = get_remainder_from_fp(fingerprint);
    if (!is_occupied(quotient)) return false;
    size_t run_start = find_run_start(quotient);
    size_t slot = run_start;
    while (true) {
        if (!is_deleted(slot) && get_remainder(slot) == remainder) return true;
        if (!is_deleted(slot) && get_remainder(slot) > remainder) return false;
        slot = next_slot(slot);
        if (!is_continuation(slot)) return false;
    }
}

uint64_t QuotientFilter::get_remainder(size_t slot) const {
    return slots_[slot].remainder;
}

void QuotientFilter::set_remainder(size_t slot, uint64_t remainder) {
    slots_[slot].remainder = remainder;
}

size_t QuotientFilter::count() const{
    return count_;
}

size_t QuotientFilter::capacity() const{
    return num_slots_;
}

double QuotientFilter::load_factor() const {
    return static_cast<double>(count_) / num_slots_;
}

size_t QuotientFilter::memory_usage() const {
    size_t total_bits = num_slots_ * (r_bits_ + 4);
    return (total_bits + 7) / 8;
}

bool QuotientFilter::remove(uint64_t element) {
    uint64_t fingerprint = hash_element(element);
    size_t quotient = get_quotient(fingerprint);
    uint64_t remainder = get_remainder_from_fp(fingerprint);
    if (!is_occupied(quotient)) return false;
    size_t next;
    size_t run_start = find_run_start(quotient);
    size_t slot = run_start;
    bool found = false;
    while (true){
        if (get_remainder(slot) == remainder){
            found = true;
            break;
        }
        if (get_remainder(slot) > remainder) break;
        next = next_slot(slot);
        if (!is_continuation(next)) break;
        slot = next;
    }
    if (!found) return false;
    set_deleted(slot, true);
    count_--;
    return true;
}

bool QuotientFilter::shift_left(size_t slot) { return true;}
