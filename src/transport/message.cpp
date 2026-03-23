#include <cstdint>
#include <string>
#include <vector>
#include <stdio.h>
#include "transport/types.h"
#include "transport/message.h"


std::vector<uint8_t> pack_uint64(uint64_t val) {
    std::vector<uint8_t> byteVector(8);
    for (int i = 0; i < 8; ++i) {
        byteVector[i] = (val >> (56 - i * 8)) & 0xFF;
    }
    return byteVector;
}

bool unpack_uint64(const std::vector<uint8_t>& data, 
    size_t& offset, uint64_t& val) {
        if (offset + 8 > data.size()) return false;
        val = 0;
        for (int i = 0; i < 8; ++i) {
            val |= (static_cast<uint64_t>(data[offset + i]) << (56 - i * 8));
        }
        offset+=8;
        return true;
    }

std::vector<uint8_t> pack_node_info(const NodeInfo& node) {
    // We know the exact size: 8 (id) + 2 (port) + 4 (ip length) + ip.size()
    std::vector<uint8_t> info;
    info.reserve(14 + node.ip.size());

    // Pack id (8 bytes)
    std::vector<uint8_t> packed_id = pack_uint64(node.id);
    info.insert(info.end(), packed_id.begin(), packed_id.end());

    // Pack port (2 bytes, big-endian)
    info.push_back((node.port >> 8) & 0xFF);
    info.push_back(node.port & 0xFF);

    // Pack ip string length (4 bytes, big-endian)
    uint32_t len = node.ip.size();
    info.push_back((len >> 24) & 0xFF);
    info.push_back((len >> 16) & 0xFF);
    info.push_back((len >> 8) & 0xFF);
    info.push_back(len & 0xFF);

    // Pack ip string (raw bytes)
    info.insert(info.end(), node.ip.begin(), node.ip.end());

    return info;
}

bool unpack_node_info(const std::vector<uint8_t>& data, size_t& offset, NodeInfo& node) {
    NodeId id;
    if (!unpack_uint64(data, offset, id)) return false;
    node.id = id;
    if (offset + 2 > data.size()) return false;
    uint16_t port = 0;
    for (int i = 0; i < 2; ++i) {
        port |= (static_cast<uint16_t>(data[offset + i]) << (8 - i * 8));
    }
    offset += 2;
    node.port = port;
    if (offset + 4 > data.size()) return false;
    uint32_t ipLen = 0;
    for (int i=0;i<4;i++){
        ipLen |= (static_cast<uint32_t>(data[offset + i]) << (24 - i * 8));
    }
    offset += 4;
    if (offset + ipLen > data.size()) return false;
    node.ip.assign(data.begin() + offset, data.begin() + offset + ipLen);
    offset += ipLen;
    return true;
}

std::vector<uint8_t> serialize(const Message& msg) {
    std::vector<uint8_t> data;
    uint32_t payloadLen = msg.payload.size();
    data.reserve(5 + payloadLen);
    data.push_back((payloadLen >> 24) & 0xFF);
    data.push_back((payloadLen >> 16) & 0xFF);
    data.push_back((payloadLen >> 8) & 0xFF);
    data.push_back(payloadLen & 0xFF);
    data.push_back(static_cast<uint8_t>(msg.type));
    data.insert(data.end(), msg.payload.begin(), msg.payload.end());
    return data;
}

bool deserialize(const std::vector<uint8_t>& data, Message& msg) {
    if (data.size() < 5) return false;
    uint32_t payloadLen = 0;
    for (int i = 0; i < 4; ++i) {
        payloadLen |= (static_cast<uint32_t>(data[i]) << (24 - i * 8));
    }
    if (data.size() < 5 + payloadLen) return false;
    msg.type = static_cast<MessageType>(data[4]);
    msg.payload.assign(data.begin() + 5, data.begin() + 5 + payloadLen);
    return true;
}
