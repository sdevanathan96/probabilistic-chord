#ifndef CHORD_RING_UTILS_H
#define CHORD_RING_UTILS_H

#include <cstdint>
#include <string>
#include "transport/types.h"

uint64_t fnv1a_hash(const std::string& data);

NodeId hash_node_address(const std::string& ip, uint16_t port);

uint64_t hash_key(const std::string& key);

bool in_range(NodeId id, NodeId start, NodeId end);

uint64_t ring_distance(NodeId a, NodeId b);

#endif // CHORD_RING_UTILS_H