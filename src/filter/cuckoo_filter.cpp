#include <cstdint>
#include <vector>
#include <cstddef>
#include "cuckoo_filter.h"

CuckooFilter::CuckooFilter(size_t num_buckets, int fingerprint_bits,
                           int bucket_size, int max_kicks)
    : num_buckets_(num_buckets),
      fingerprint_bits_(fingerprint_bits),
      bucket_size_(bucket_size),
      max_kicks_(max_kicks),
      buckets_(num_buckets, std::vector<uint32_t>(bucket_size, 0)),
      count_(0) {
        fingerprint_mask_ = (1 << fingerprint_bits) - 1;
      }

uint64_t CuckooFilter::hash_element(uint64_t element) const {
    element ^= element >> 33;
    element *= 0xff51afd7ed558ccdULL;
    element ^= element >> 33;
    element *= 0xc4ceb9fe1a85ec53ULL;
    element ^= element >> 33;
    return element;
}

uint64_t CuckooFilter::hash_fingerprint(uint32_t fingerprint) const {
    uint64_t h = fingerprint;
    h ^= h >> 16;
    h *= 0x45d9f3b;
    h ^= h >> 16;
    return h;
}

uint32_t CuckooFilter::compute_fingerprint(uint64_t element) const {
    uint64_t h = hash_element(element);
    uint32_t fp = static_cast<uint32_t>(h >> 32) & fingerprint_mask_;
    if (fp == 0) fp = 1;
    return fp;
}

size_t CuckooFilter::bucket_index1(uint64_t element) const {
    return hash_element(element) % num_buckets_;
}

size_t CuckooFilter::bucket_index2(size_t b1, uint32_t fingerprint) const {
    return (b1 ^ hash_fingerprint(fingerprint)) % num_buckets_;
}

bool CuckooFilter::insert(uint64_t element) {
    uint32_t fingerprint = compute_fingerprint(element);
    size_t b1 = bucket_index1(element);
    size_t b2 = bucket_index2(b1, fingerprint);

    for (int i = 0; i < bucket_size_; i++) {
        if (buckets_[b1][i] == 0) {
            buckets_[b1][i] = fingerprint;
            count_++;
            return true;
        }
    }

    for (int i = 0; i < bucket_size_; i++) {
        if (buckets_[b2][i] == 0) {
            buckets_[b2][i] = fingerprint;
            count_++;
            return true;
        }
    }

    size_t current_bucket = (rand() % 2 == 0) ? b1 : b2;
    uint32_t current_fp = fingerprint;

    for (int kick_count = 0; kick_count < max_kicks_; kick_count++) {
        int slot = rand() % bucket_size_;
        std::swap(current_fp, buckets_[current_bucket][slot]);

        current_bucket = bucket_index2(current_bucket, current_fp);

        for (int i = 0; i < bucket_size_; i++) {
            if (buckets_[current_bucket][i] == 0) {
                buckets_[current_bucket][i] = current_fp;
                count_++;
                return true;
            }
        }
    }

    return false;
}

bool CuckooFilter::lookup(uint64_t element) const {
    uint32_t fingerprint = compute_fingerprint(element);
    size_t b1 = bucket_index1(element);
    size_t b2 = bucket_index2(b1, fingerprint);

    for (int i = 0; i < bucket_size_; i++) {
        if (buckets_[b1][i] == fingerprint || buckets_[b2][i] == fingerprint) {
            return true;
        }
    }
    return false;
}

bool CuckooFilter::remove(uint64_t element) {
    uint32_t fingerprint = compute_fingerprint(element);
    size_t b1 = bucket_index1(element);
    size_t b2 = bucket_index2(b1, fingerprint);

    for (int i = 0; i < bucket_size_; i++) {
        if (buckets_[b1][i] == fingerprint) {
            buckets_[b1][i] = 0;
            count_--;
            return true;
        }
        if (buckets_[b2][i] == fingerprint) {
            buckets_[b2][i] = 0;
            count_--;
            return true;
        }
    }
    return false;
}

size_t CuckooFilter::count() const {
    return count_;
}

size_t CuckooFilter::capacity() const {
    return num_buckets_ * bucket_size_;
}

double CuckooFilter::load_factor() const {
    return static_cast<double>(count_) / capacity();
}

size_t CuckooFilter::memory_usage() const {
    size_t total_bits = num_buckets_ * bucket_size_ * fingerprint_bits_;
    return (total_bits + 7) / 8;
}