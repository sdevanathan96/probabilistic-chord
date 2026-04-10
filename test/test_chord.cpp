#include <iostream>
#include <cassert>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>
#include "transport/sim_transport.h"
#include "chord/ring_utils.h"
#include "chord/successor_walk_node.h"

void test_fnv1a_hash() {
    std::cout << "[1] FNV-1a hash..." << std::endl;

    assert(fnv1a_hash("hello") == fnv1a_hash("hello"));

    assert(fnv1a_hash("hello") != fnv1a_hash("world"));
    assert(fnv1a_hash("node:8001") != fnv1a_hash("node:8002"));

    uint64_t h = fnv1a_hash("");
    (void)h;

    std::cout << "  PASS" << std::endl;
}

void test_in_range() {
    std::cout << "[2] in_range..." << std::endl;

    assert(in_range(15, 10, 20) == true);
    assert(in_range(20, 10, 20) == true);
    assert(in_range(10, 10, 20) == false);
    assert(in_range(5,  10, 20) == false);
    assert(in_range(25, 10, 20) == false);

    uint64_t max = UINT64_MAX;
    assert(in_range(max, max - 5, 5) == true);
    assert(in_range(0, max - 5, 5) == true);
    assert(in_range(5, max - 5, 5) == true);
    assert(in_range(max - 5, max - 5, 5) == false);
    assert(in_range(max - 10, max - 5, 5) == false);


    assert(in_range(42, 10, 10) == true);
    assert(in_range(10, 10, 10) == true);

    std::cout << "  PASS" << std::endl;
}

void test_ring_distance() {
    std::cout << "[3] ring_distance..." << std::endl;

    assert(ring_distance(0, 0) == 0);
    assert(ring_distance(0, 10) == 10);
    assert(ring_distance(10, 0) == UINT64_MAX - 10 + 1);


    assert(ring_distance(5, 10) == 5);

    std::cout << "  PASS" << std::endl;
}


void test_single_node_ring() {
    std::cout << "[4] Single node ring..." << std::endl;

    NodeInfo n1_info(100, "sim", 1);
    SimTransport t1(n1_info);
    t1.start();

    ChordNode node1(&t1);
    node1.create();

    assert(node1.get_successor() == n1_info);
    assert(node1.get_predecessor() == n1_info);


    NodeInfo owner = node1.lookup(12345);
    assert(owner == n1_info);

    t1.stop();
    std::cout << "  PASS" << std::endl;
}

void test_two_node_ring() {
    std::cout << "[5] Two node ring..." << std::endl;

    NodeInfo n1_info(100, "sim", 1);
    NodeInfo n2_info(200, "sim", 2);

    SimTransport t1(n1_info);
    SimTransport t2(n2_info);
    t1.start();
    t2.start();

    ChordNode node1(&t1);
    ChordNode node2(&t2);


    node1.create();

    node2.join(n1_info);


    for (int i = 0; i < 5; ++i) {
        node1.stabilize();
        node2.stabilize();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }


    assert(node1.get_successor() == n2_info);
    assert(node2.get_successor() == n1_info);

    t1.stop();
    t2.stop();
    std::cout << "  PASS" << std::endl;
}

void test_multi_node_ring() {
    std::cout << "[6] Multi-node ring (8 nodes)..." << std::endl;

    const int N = 8;
    std::vector<NodeInfo> infos;
    std::vector<SimTransport*> transports;
    std::vector<ChordNode*> nodes;


    for (int i = 0; i < N; ++i) {
        NodeId id = (UINT64_MAX / N) * i;
        infos.push_back(NodeInfo(id, "sim", 100 + i));
        transports.push_back(new SimTransport(infos[i]));
        transports[i]->start();
        nodes.push_back(new SuccessorWalkNode(transports[i]));
    }

    nodes[0]->create();

    for (int i = 1; i < N; ++i) {
        nodes[i]->join(infos[0]);
        for (int round = 0; round < 5; ++round) {
            for (int j = 0; j <= i; ++j) {
                nodes[j]->stabilize();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    for (int round = 0; round < 10; ++round) {
        for (int j = 0; j < N; ++j) {
            nodes[j]->stabilize();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::vector<NodeInfo> sorted_infos = infos;
    std::sort(sorted_infos.begin(), sorted_infos.end(),
        [](const NodeInfo& a, const NodeInfo& b) { return a.id < b.id; });

    for (int i = 0; i < N; ++i) {
        NodeId expected_succ = sorted_infos[(i + 1) % N].id;

        for (int j = 0; j < N; ++j) {
            if (nodes[j]->get_info().id == sorted_infos[i].id) {
                assert(nodes[j]->get_successor().id == expected_succ);
                break;
            }
        }
    }

    for (int i = 0; i < 20; ++i) {
        uint64_t key = fnv1a_hash("test_key_" + std::to_string(i));
        NodeInfo owner = nodes[0]->lookup(key);

        NodeInfo expected_owner = sorted_infos[0];
        for (int j = 0; j < N; ++j) {
            if (sorted_infos[j].id >= key) {
                expected_owner = sorted_infos[j];
                break;
            }
        }
        assert(owner == expected_owner);
    }

    for (int i = 0; i < N; ++i) {
        transports[i]->stop();
        delete nodes[i];
        delete transports[i];
    }

    std::cout << "  PASS" << std::endl;
}

void test_node_leave() {
    std::cout << "[7] Node leave..." << std::endl;

    const int N = 4;
    std::vector<NodeInfo> infos;
    std::vector<SimTransport*> transports;
    std::vector<ChordNode*> nodes;

    for (int i = 0; i < N; ++i) {
        NodeId id = (UINT64_MAX / N) * i + 1;
        infos.push_back(NodeInfo(id, "sim", 200 + i));
        transports.push_back(new SimTransport(infos[i]));
        transports[i]->start();
        nodes.push_back(new SuccessorWalkNode(transports[i]));
    }

    nodes[0]->create();
    for (int i = 1; i < N; ++i) {
        nodes[i]->join(infos[0]);
        for (int round = 0; round < 5; ++round) {
            for (int j = 0; j <= i; ++j) {
                nodes[j]->stabilize();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    for (int round = 0; round < 10; ++round) {
        for (int j = 0; j < N; ++j) {
            nodes[j]->stabilize();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    for (int i = 0; i < N; ++i) {
        assert(nodes[i]->get_successor().id != 0);
        assert(nodes[i]->get_predecessor().id != 0);
    }

    std::cout << "  Node " << infos[2].id << " leaving..." << std::endl;
    nodes[2]->leave();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    for (int round = 0; round < 10; ++round) {
        for (int j = 0; j < N; ++j) {
            if (j == 2) continue; // skip the departed node
            nodes[j]->stabilize();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::vector<NodeInfo> remaining;
    for (int i = 0; i < N; ++i) {
        if (i == 2) continue;
        remaining.push_back(infos[i]);
    }
    std::sort(remaining.begin(), remaining.end(),
        [](const NodeInfo& a, const NodeInfo& b) { return a.id < b.id; });

    for (size_t i = 0; i < remaining.size(); ++i) {
        NodeId expected_succ = remaining[(i + 1) % remaining.size()].id;
        for (int j = 0; j < N; ++j) {
            if (j == 2) continue;
            if (nodes[j]->get_info().id == remaining[i].id) {
                assert(nodes[j]->get_successor().id == expected_succ);
                break;
            }
        }
    }

    uint64_t key = hash_key("after_leave_test");
    NodeInfo owner = nodes[0]->lookup(key);
    assert(owner.id != 0);
    assert(owner.id != infos[2].id);

    std::cout << "  PASS" << std::endl;

    for (int i = 0; i < N; ++i) {
        transports[i]->stop();
        delete nodes[i];
        delete transports[i];
    }
}

int main() {
    std::cout << "Running Chord tests..." << std::endl;

    test_fnv1a_hash();
    test_in_range();
    test_ring_distance();
    test_single_node_ring();
    test_two_node_ring();
    test_multi_node_ring();
    test_node_leave();

    std::cout << "All Chord tests passed." << std::endl;
    return 0;
}