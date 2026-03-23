#ifndef CHORD_MESSAGE_H
#define CHORD_MESSAGE_H

#include <cstdint>
#include <string>
#include <vector>
#include "types.h"


enum class MessageType : uint8_t {
    PING            = 0x01,
    PONG            = 0x02,
    FIND_SUCCESSOR_REQ  = 0x10,
    FIND_SUCCESSOR_RESP = 0x11,
    JOIN_REQ        = 0x20,
    JOIN_RESP       = 0x21,
    NOTIFY          = 0x22,
    STABILIZE_REQ   = 0x30,
    STABILIZE_RESP  = 0x31,
    LOOKUP_REQ      = 0x40,
    LOOKUP_RESP     = 0x41,
    FILTER_UPDATE   = 0x50,
};


// Wire format: [4-byte payload length][1-byte type][payload]

struct Message {
    MessageType type;
    std::vector<uint8_t> payload;

    Message() : type(MessageType::PING) {}
    Message(MessageType type, const std::vector<uint8_t>& payload)
        : type(type), payload(payload) {}
};

// Wire format:
//   bytes [0..3]: payload length as uint32_t (network byte order)
//   byte  [4]:    message type
//   bytes [5..]:  payload


std::vector<uint8_t> serialize(const Message& msg);

bool deserialize(const std::vector<uint8_t>& data, Message& msg);

// Layout: [8 bytes id][2 bytes port][N bytes ip string]
std::vector<uint8_t> pack_node_info(const NodeInfo& node);

bool unpack_node_info(const std::vector<uint8_t>& data,
                      size_t& offset, NodeInfo& node);

std::vector<uint8_t> pack_uint64(uint64_t val);

bool unpack_uint64(const std::vector<uint8_t>& data,
                   size_t& offset, uint64_t& val);

#endif // CHORD_MESSAGE_H