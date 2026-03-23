#include <iostream>
#include <cassert>
#include <atomic>
#include <thread>
#include <chrono>
#include <unistd.h>
#include "transport/ipc_transport.h"
#include "transport/message.h"

void cleanup_socket(const std::string& path) {
    unlink(path.c_str());
}

int main() {
    std::cout << "Running IPCTransport tests..." << std::endl;

    std::cout << "[1] Basic PING/PONG..." << std::endl;
    {
        NodeInfo node_a(1, "ipc", 1);
        NodeInfo node_b(2, "ipc", 2);
        std::string path_a = IPCTransport::path_for_node(node_a);
        std::string path_b = IPCTransport::path_for_node(node_b);
        cleanup_socket(path_a);
        cleanup_socket(path_b);

        IPCTransport transport_a(node_a, path_a);
        IPCTransport transport_b(node_b, path_b);

        std::atomic<bool> b_got_ping(false);
        std::atomic<bool> a_got_pong(false);

        transport_b.register_handler(MessageType::PING,
            [&](const NodeInfo& from, const Message& msg) {
                b_got_ping.store(true);
                transport_b.send(from, Message(MessageType::PONG, {}));
            });

        transport_a.register_handler(MessageType::PONG,
            [&](const NodeInfo& from, const Message& msg) {
                a_got_pong.store(true);
            });

        transport_a.start();
        transport_b.start();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        bool sent = transport_a.send(node_b, Message(MessageType::PING, {}));
        assert(sent);

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        assert(b_got_ping.load());
        assert(a_got_pong.load());

        transport_a.stop();
        transport_b.stop();

        std::cout << "  PASS" << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "[2] Three-node PING..." << std::endl;
    {


        NodeInfo node_a(1, "ipc", 1);
        NodeInfo node_b(2, "ipc", 2);
        NodeInfo node_c(3, "ipc", 3);
        std::string path_a = IPCTransport::path_for_node(node_a);
        std::string path_b = IPCTransport::path_for_node(node_b);
        std::string path_c = IPCTransport::path_for_node(node_c);
        cleanup_socket(path_a);
        cleanup_socket(path_b);
        cleanup_socket(path_c);

        IPCTransport ta(node_a, path_a);
        IPCTransport tb(node_b, path_b);
        IPCTransport tc(node_c, path_c);

        std::atomic<int> pong_count(0);

        tb.register_handler(MessageType::PING,
            [&](const NodeInfo& from, const Message& msg) {
                tb.send(from, Message(MessageType::PONG, {}));
            });

        tc.register_handler(MessageType::PING,
            [&](const NodeInfo& from, const Message& msg) {
                tc.send(from, Message(MessageType::PONG, {}));
            });

        ta.register_handler(MessageType::PONG,
            [&](const NodeInfo& from, const Message& msg) {
                pong_count.fetch_add(1);
            });

        ta.start();
        tb.start();
        tc.start();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        ta.send(node_b, Message(MessageType::PING, {}));
        ta.send(node_c, Message(MessageType::PING, {}));

        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        assert(pong_count.load() == 2);

        ta.stop();
        tb.stop();
        tc.stop();

        std::cout << "  PASS" << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // ── Test 3: Payload survives IPC delivery ────────────────
    std::cout << "[3] Payload integrity over IPC..." << std::endl;
    {


        NodeInfo node_a(1, "ipc", 1);
        NodeInfo node_b(2, "ipc", 2);
        std::string path_a = IPCTransport::path_for_node(node_a);
        std::string path_b = IPCTransport::path_for_node(node_b);
        cleanup_socket(path_a);
        cleanup_socket(path_b);

        IPCTransport ta(node_a, path_a);
        IPCTransport tb(node_b, path_b);

        NodeInfo payload_node(42, "10.0.0.1", 9000);
        std::vector<uint8_t> expected_payload = pack_node_info(payload_node);
        std::atomic<bool> payload_ok(false);

        tb.register_handler(MessageType::PING,
            [&](const NodeInfo& from, const Message& msg) {
                if (msg.payload == expected_payload) {
                    payload_ok.store(true);
                }
            });

        ta.start();
        tb.start();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        ta.send(node_b, Message(MessageType::PING, expected_payload));

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        assert(payload_ok.load());

        ta.stop();
        tb.stop();

        std::cout << "  PASS" << std::endl;
    }

    std::cout << "All IPCTransport tests passed." << std::endl;
    return 0;
}