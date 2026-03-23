#include <iostream>
#include <cassert>
#include "transport/message.h"


#define TEST(name) void name(); \
    struct name##_reg { name##_reg() { tests.push_back({#name, name}); } } name##_inst; \
    void name()

struct TestEntry { const char* name; void (*fn)(); };
static std::vector<TestEntry> tests;



TEST(test_pack_unpack_uint64) {
    uint64_t original = 0xDEADBEEFCAFE1234ULL;
    std::vector<uint8_t> packed = pack_uint64(original);
    assert(packed.size() == 8);

    size_t offset = 0;
    uint64_t unpacked = 0;
    bool ok = unpack_uint64(packed, offset, unpacked);

    assert(ok);
    assert(offset == 8);
    assert(unpacked == original);
    std::cout << "  PASS: uint64 round-trip" << std::endl;
}

TEST(test_pack_unpack_uint64_zero) {
    uint64_t original = 0;
    std::vector<uint8_t> packed = pack_uint64(original);

    size_t offset = 0;
    uint64_t unpacked = 999;
    bool ok = unpack_uint64(packed, offset, unpacked);

    assert(ok);
    assert(unpacked == 0);
    std::cout << "  PASS: uint64 zero" << std::endl;
}

TEST(test_pack_unpack_node_info) {
    NodeInfo original(12345, "127.0.0.1", 8080);
    std::vector<uint8_t> packed = pack_node_info(original);

    size_t offset = 0;
    NodeInfo unpacked;
    bool ok = unpack_node_info(packed, offset, unpacked);

    assert(ok);
    assert(unpacked.id == original.id);
    assert(unpacked.ip == original.ip);
    assert(unpacked.port == original.port);
    std::cout << "  PASS: NodeInfo round-trip" << std::endl;
}

TEST(test_pack_unpack_node_info_empty_ip) {
    NodeInfo original(0, "", 0);
    std::vector<uint8_t> packed = pack_node_info(original);

    size_t offset = 0;
    NodeInfo unpacked;
    bool ok = unpack_node_info(packed, offset, unpacked);

    assert(ok);
    assert(unpacked.id == 0);
    assert(unpacked.ip == "");
    assert(unpacked.port == 0);
    std::cout << "  PASS: NodeInfo empty" << std::endl;
}

TEST(test_serialize_deserialize_ping) {

    NodeInfo sender(42, "10.0.0.1", 9000);
    Message original(MessageType::PING, pack_node_info(sender));

    std::vector<uint8_t> wire = serialize(original);

    Message recovered;
    bool ok = deserialize(wire, recovered);

    assert(ok);
    assert(recovered.type == MessageType::PING);
    assert(recovered.payload == original.payload);

    size_t offset = 0;
    NodeInfo recovered_sender;
    ok = unpack_node_info(recovered.payload, offset, recovered_sender);
    assert(ok);
    assert(recovered_sender.id == 42);
    assert(recovered_sender.ip == "10.0.0.1");
    assert(recovered_sender.port == 9000);
    std::cout << "  PASS: serialize/deserialize PING" << std::endl;
}

TEST(test_serialize_deserialize_empty_payload) {
    Message original(MessageType::PONG, {});
    std::vector<uint8_t> wire = serialize(original);

    Message recovered;
    bool ok = deserialize(wire, recovered);

    assert(ok);
    assert(recovered.type == MessageType::PONG);
    assert(recovered.payload.empty());
    std::cout << "  PASS: serialize/deserialize empty payload" << std::endl;
}

TEST(test_deserialize_truncated) {

    std::vector<uint8_t> bad_data = {0x00, 0x01};

    Message msg;
    bool ok = deserialize(bad_data, msg);
    assert(!ok);
    std::cout << "  PASS: reject truncated buffer" << std::endl;
}

int main() {
    std::cout << "Running message tests..." << std::endl;
    int passed = 0;
    for (size_t i = 0; i < tests.size(); ++i) {
        std::cout << "[" << (i + 1) << "/" << tests.size() << "] "
                  << tests[i].name << std::endl;
        tests[i].fn();
        ++passed;
    }
    std::cout << passed << "/" << tests.size() << " tests passed." << std::endl;
    return 0;
}