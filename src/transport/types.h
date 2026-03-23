#ifndef CHORD_TYPES_H
#define CHORD_TYPES_H

#include <cstdint>
#include <string>

typedef uint64_t NodeId;

struct NodeInfo {
    NodeId id;
    std::string ip;
    uint16_t port;

    NodeInfo() : id(0), ip(""), port(0) {}
    NodeInfo(NodeId id, const std::string& ip, uint16_t port)
        : id(id), ip(ip), port(port) {}

    bool operator==(const NodeInfo& other) const {
        return id == other.id;
    }

    bool operator!=(const NodeInfo& other) const {
        return !(*this == other);
    }

    std::string address() const {
        return ip + ":" + std::to_string(port);
    }
};

#endif // CHORD_TYPES_H