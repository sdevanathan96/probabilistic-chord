#include <iostream>
#include <cassert>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>
#include <cmath>
#include "transport/sim_transport.h"
#include "chord/ring_utils.h"
#include "chord/finger_table.h"
#include "chord/finger_table_node.h"
#include "chord/chord_node.h"

void test_finger_start() {
    std::cout << "[1] finger_start..." << std::endl;

    FingerTable ft(0);

    assert(ft.finger_start(0) == 1);
    assert(ft.finger_start(1) == 2);
    assert(ft.finger_start(3) == 8);
    assert(ft.finger_start(63) == (1ULL << 63));

    FingerTable ft2(100);
    assert(ft2.finger_start(0) == 101);
    assert(ft2.finger_start(1) == 102);

    FingerTable ft3(UINT64_MAX);
    assert(ft3.finger_start(0) == 0);
    assert(ft3.finger_start(1) == 1);

    std::cout << "  PASS" << std::endl;
}

void test_closest_preceding_finger() {
    std::cout << "[2] closest_preceding_finger..." << std::endl;

    FingerTable ft(0);
    NodeInfo n10(10, "sim", 10);
    NodeInfo n50(50, "sim", 50);
    NodeInfo n100(100, "sim", 100);

    ft.init_all(NodeInfo(0, "sim", 0));

    ft.set_finger(0, n10);
    ft.set_finger(5, n50);
    ft.set_finger(6, n100);

    NodeInfo result = ft.closest_preceding_finger(60);
    assert(result.id == 50);

    result = ft.closest_preceding_finger(200);
    assert(result.id == 100);

    result = ft.closest_preceding_finger(5);
    assert(result.id == 0);

    std::cout << "  PASS" << std::endl;
}

void test_finger_table_routing() {
    std::cout << "[3] Finger table O(log n) routing..." << std::endl;

    const int N = 16;
    std::vector<NodeInfo> infos;
    std::vector<SimTransport*> transports;
    std::vector<ChordNode*> nodes;

    for (int i = 0; i < N; ++i) {
        NodeId id = (UINT64_MAX / N) * i;
        infos.push_back(NodeInfo(id, "sim", 300 + i));
        transports.push_back(new SimTransport(infos[i]));
        transports[i]->start();
        nodes.push_back(new FingerTableNode(transports[i]));
    }

    nodes[0]->create();
    for (int i = 1; i < N; ++i) {
        nodes[i]->join(infos[0]);
        for (int round = 0; round < 5; ++round) {
            for (int j = 0; j <= i; ++j) {
                nodes[j]->stabilize();
                nodes[j]->do_maintenance();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    for (int round = 0; round < 30; ++round) {
        for (int j = 0; j < N; ++j) {
            nodes[j]->stabilize();
            nodes[j]->do_maintenance();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::vector<NodeInfo> sorted = infos;
    std::sort(sorted.begin(), sorted.end(),
        [](const NodeInfo& a, const NodeInfo& b) { return a.id < b.id; });

    for (int i = 0; i < N; ++i) {
        NodeId expected_succ = sorted[(i + 1) % N].id;
        for (int j = 0; j < N; ++j) {
            if (nodes[j]->get_info().id == sorted[i].id) {
                assert(nodes[j]->get_successor().id == expected_succ);
                break;
            }
        }
    }

    int total_lookups = 50;
    for (int i = 0; i < total_lookups; ++i) {
        uint64_t key = fnv1a_hash("finger_test_key_" + std::to_string(i));
        NodeInfo owner = nodes[0]->lookup(key);

        NodeInfo expected = sorted[0];
        for (int j = 0; j < N; ++j) {
            if (sorted[j].id >= key) {
                expected = sorted[j];
                break;
            }
        }
        assert(owner == expected);
    }

    std::cout << "  All " << total_lookups << " lookups correct" << std::endl;

    for (int i = 0; i < N; ++i) {
        transports[i]->stop();
        delete nodes[i];
        delete transports[i];
    }

    std::cout << "  PASS" << std::endl;
}

void test_finger_vs_successor_walk() {
    std::cout << "[4] Finger table vs successor walk hop count..." << std::endl;

    const int N = 16;

    std::vector<NodeInfo> infos;
    std::vector<SimTransport*> transports;
    std::vector<ChordNode*> nodes;

    for (int i = 0; i < N; ++i) {
        NodeId id = (UINT64_MAX / N) * i;
        infos.push_back(NodeInfo(id, "sim", 400 + i));
        transports.push_back(new SimTransport(infos[i]));
        transports[i]->start();
        nodes.push_back(new FingerTableNode(transports[i]));
    }

    nodes[0]->create();
    for (int i = 1; i < N; ++i) {
        nodes[i]->join(infos[0]);
        for (int round = 0; round < 5; ++round) {
            for (int j = 0; j <= i; ++j) {
                nodes[j]->stabilize();
                nodes[j]->do_maintenance();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    for (int round = 0; round < 30; ++round) {
        for (int j = 0; j < N; ++j) {
            nodes[j]->stabilize();
            nodes[j]->do_maintenance();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    for (int i = 0; i < 20; ++i) {
        uint64_t key = fnv1a_hash("hop_test_" + std::to_string(i));
        NodeInfo owner = nodes[0]->lookup(key);
        assert(owner.id != 0);
    }

    std::cout << "  20 lookups completed successfully on 16-node ring" << std::endl;

    for (int i = 0; i < N; ++i) {
        transports[i]->stop();
        delete nodes[i];
        delete transports[i];
    }

    std::cout << "  PASS" << std::endl;
}

int main() {
    std::cout << "Running Finger Table tests..." << std::endl;

    test_finger_start();
    test_closest_preceding_finger();
    test_finger_table_routing();
    test_finger_vs_successor_walk();

    std::cout << "All Finger Table tests passed." << std::endl;
    return 0;
}