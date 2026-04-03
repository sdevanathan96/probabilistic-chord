#include <cstdint>
#include <string>
#include "transport/types.h"
#include "chord/ring_utils.h"

uint64_t fnv1a_hash(const std::string &data) {
    const uint64_t hash = 0xcbf29ce484222325;
    const uint64_t prime = 0x100000001b3;
    uint64_t result = hash;
    for (char c : data) {
        result ^= static_cast<uint64_t>(c);
        result *= prime;
    }
    return result;
}

bool in_range(NodeId id, NodeId start, NodeId end) {
    if (start < end){
        return id > start && id <= end;
    } else if (start >= end){
        if(start == end) return true;
        return id > start || id <= end;
    }
    return false;
}

uint64_t ring_distance(NodeId a, NodeId b) {
    if (b>=a) return b-a;
    return (UINT64_MAX - a) + b + 1;
}

uint64_t hash_node_address(const std::string& ip, uint16_t port) {
    return fnv1a_hash(ip + ":" + std::to_string(port));
}

uint64_t hash_key(const std::string& key) {
    return fnv1a_hash(key);
}