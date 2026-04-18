#ifndef CHORD_FILTER_H
#define CHORD_FILTER_H

#include <cstdint>
#include <vector>
#include <cstddef>


class Filter {
public:
    virtual bool insert(uint64_t element) = 0;

    virtual bool lookup(uint64_t element) const = 0;

    virtual bool remove(uint64_t element) = 0;

    virtual size_t count() const = 0;

    virtual size_t capacity() const = 0;

    virtual double load_factor() const = 0;

    virtual size_t memory_usage() const = 0;
};

#endif // CHORD_FILTER_H