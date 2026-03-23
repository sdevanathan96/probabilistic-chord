#include <iostream>
#include <cassert>
#include <atomic>
#include <thread>
#include <chrono>
#include "transport/sim_transport.h"
#include "transport/message.h"

int main() {
    std::cout << "Running SimTransport tests..." << std::endl;


    std::cout << "[1] Basic PING/PONG..." << std::endl;
    {
        NodeInfo node_a(1, "sim", 1);
        NodeInfo node_b(2, "sim", 2);

        SimTransport transport_a(node_a);
        SimTransport transport_b(node_b);

        std::atomic<bool> b_got_ping(false);
        std::atomic<bool> a_got_pong(false);

        transport_b.register_handler(MessageType::PING,
            [&](const NodeInfo& from, const Message& msg) {
                b_got_ping.store(true);

                Message pong(MessageType::PONG, {});
                transport_b.send(from, pong);
            });


        transport_a.register_handler(MessageType::PONG,
            [&](const NodeInfo& from, const Message& msg) {
                a_got_pong.store(true);
            });

        transport_a.start();
        transport_b.start();


        Message ping(MessageType::PING, {});
        bool sent = transport_a.send(node_b, ping);
        assert(sent);


        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        assert(b_got_ping.load());
        assert(a_got_pong.load());

        transport_a.stop();
        transport_b.stop();

        std::cout << "  PASS" << std::endl;
    }


    std::cout << "[2] Five-node PING mesh..." << std::endl;
    {
        const int N = 5;
        std::vector<NodeInfo> infos;
        std::vector<SimTransport*> transports;
        std::vector<std::atomic<int>*> ping_counts;

        for (int i = 0; i < N; ++i) {
            infos.push_back(NodeInfo(100 + i, "sim", 100 + i));
            transports.push_back(new SimTransport(infos[i]));
            ping_counts.push_back(new std::atomic<int>(0));
        }

        for (int i = 0; i < N; ++i) {
            std::atomic<int>* counter = ping_counts[i];
            transports[i]->register_handler(MessageType::PING,
                [counter](const NodeInfo& from, const Message& msg) {
                    counter->fetch_add(1);
                });
            transports[i]->start();
        }

        Message ping(MessageType::PING, {});
        for (int i = 0; i < N; ++i) {
            for (int j = 0; j < N; ++j) {
                if (i != j) {
                    transports[i]->send(infos[j], ping);
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        for (int i = 0; i < N; ++i) {
            int count = ping_counts[i]->load();
            assert(count == N - 1);
        }

        for (int i = 0; i < N; ++i) {
            transports[i]->stop();
            delete transports[i];
            delete ping_counts[i];
        }

        std::cout << "  PASS" << std::endl;
    }

    std::cout << "[3] Send to unknown node..." << std::endl;
    {
        NodeInfo node_a(200, "sim", 200);
        NodeInfo node_ghost(999, "sim", 999);  // never registered

        SimTransport transport_a(node_a);
        transport_a.start();

        Message ping(MessageType::PING, {});
        bool sent = transport_a.send(node_ghost, ping);
        assert(!sent);

        transport_a.stop();
        std::cout << "  PASS" << std::endl;
    }

    std::cout << "[4] Payload integrity..." << std::endl;
    {
        NodeInfo node_a(300, "sim", 300);
        NodeInfo node_b(301, "sim", 301);

        SimTransport transport_a(node_a);
        SimTransport transport_b(node_b);

        NodeInfo payload_node(42, "10.0.0.1", 9000);
        std::vector<uint8_t> expected_payload = pack_node_info(payload_node);
        std::atomic<bool> payload_ok(false);

        transport_b.register_handler(MessageType::PING,
            [&](const NodeInfo& from, const Message& msg) {
                if (msg.payload == expected_payload) {
                    payload_ok.store(true);
                }
            });

        transport_a.start();
        transport_b.start();

        Message ping(MessageType::PING, expected_payload);
        transport_a.send(node_b, ping);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        assert(payload_ok.load());

        transport_a.stop();
        transport_b.stop();
        std::cout << "  PASS" << std::endl;
    }

    std::cout << "All SimTransport tests passed." << std::endl;
    return 0;
}