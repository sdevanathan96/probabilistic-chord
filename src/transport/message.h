#ifndef CHORD_MESSAGE_H
#define CHORD_MESSAGE_H

#include <cstdint>
#include <string>
#include <vector>
#include "types.h"

// ── Message Types ────────────────────────────────────────────
// Add more as needed in later days (JOIN, STABILIZE, etc.)

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

// ── Message ──────────────────────────────────────────────────
// Wire format: [4-byte payload length][1-byte type][payload]
//
// The payload is a raw byte buffer. Each message type defines
// its own payload layout. For now, PING/PONG carry the sender's
// NodeInfo. You'll extend the payload format per message type
// as you build out the protocol.

struct Message {
    MessageType type;
    std::vector<uint8_t> payload;

    Message() : type(MessageType::PING) {}
    Message(MessageType type, const std::vector<uint8_t>& payload)
        : type(type), payload(payload) {}
};

// ── Serialization ────────────────────────────────────────────
// These convert a Message to/from a byte buffer suitable for
// sending over the wire or passing through SimTransport.
//
// Wire format:
//   bytes [0..3]: payload length as uint32_t (network byte order)
//   byte  [4]:    message type
//   bytes [5..]:  payload
//
// Implement these in message.cpp.

// Serialize a Message into a byte buffer (wire format).
std::vector<uint8_t> serialize(const Message& msg);

// Deserialize a byte buffer (wire format) back into a Message.
// Returns true on success, false if the buffer is malformed.
bool deserialize(const std::vector<uint8_t>& data, Message& msg);

// ── Payload Helpers ──────────────────────────────────────────
// Convenience functions to pack/unpack common types into the
// payload buffer. These make it easy to build payloads for
// different message types without hand-rolling byte offsets
// everywhere.
//
// Implement these in message.cpp.

// Pack a NodeInfo into a byte vector.
// Layout: [8 bytes id][2 bytes port][N bytes ip string]
std::vector<uint8_t> pack_node_info(const NodeInfo& node);

// Unpack a NodeInfo from a byte vector starting at offset.
// Returns true on success, advances offset past the read bytes.
bool unpack_node_info(const std::vector<uint8_t>& data,
                      size_t& offset, NodeInfo& node);

// Pack a uint64_t into 8 bytes (big-endian).
std::vector<uint8_t> pack_uint64(uint64_t val);

// Unpack a uint64_t from data at offset (big-endian).
// Returns true on success, advances offset by 8.
bool unpack_uint64(const std::vector<uint8_t>& data,
                   size_t& offset, uint64_t& val);

#endif // CHORD_MESSAGE_H