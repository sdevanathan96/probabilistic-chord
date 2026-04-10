#include <iostream>
#include <cassert>
#include <vector>
#include <thread>
#include <chrono>
#include <fstream>
#include "transport/sim_transport.h"
#include "chord/ring_utils.h"
#include "chord/chord_node.h"
#include "chord/finger_table_node.h"
#include "bench/metrics.h"

void test_lookup_metrics() {
    std::cout << "[1] Lookup metrics recording..." << std::endl;

    const int N = 8;
    std::vector<NodeInfo> infos;
    std::vector<SimTransport*> transports;
    std::vector<ChordNode*> nodes;
    Metrics metrics;

    for (int i = 0; i < N; ++i) {
        NodeId id = (UINT64_MAX / N) * i + 1;
        infos.push_back(NodeInfo(id, "sim", 500 + i));
        transports.push_back(new SimTransport(infos[i]));
        transports[i]->start();
        nodes.push_back(new FingerTableNode(transports[i]));
    }

    nodes[0]->set_metrics(&metrics);

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
        uint64_t key = fnv1a_hash("metrics_test_" + std::to_string(i));
        NodeInfo owner = nodes[0]->lookup(key);
        assert(owner.id != 0);
    }

    assert(metrics.total_lookups() == 20);
    assert(metrics.avg_hop_count() > 0);
    assert(metrics.avg_latency_us() > 0);
    assert(metrics.total_messages() > 0);

    std::cout << "  Lookups: " << metrics.total_lookups() << std::endl;
    std::cout << "  Avg hops: " << metrics.avg_hop_count() << std::endl;
    std::cout << "  Avg latency: " << metrics.avg_latency_us() << " us" << std::endl;
    std::cout << "  Total messages: " << metrics.total_messages() << std::endl;
    std::cout << "  PASS" << std::endl;

    for (int i = 0; i < N; ++i) {
        transports[i]->stop();
        delete nodes[i];
        delete transports[i];
    }
}

void test_maintenance_metrics() {
    std::cout << "[2] Maintenance metrics recording..." << std::endl;

    const int N = 4;
    std::vector<NodeInfo> infos;
    std::vector<SimTransport*> transports;
    std::vector<ChordNode*> nodes;
    Metrics metrics;

    for (int i = 0; i < N; ++i) {
        NodeId id = (UINT64_MAX / N) * i + 1;
        infos.push_back(NodeInfo(id, "sim", 600 + i));
        transports.push_back(new SimTransport(infos[i]));
        transports[i]->start();
        nodes.push_back(new FingerTableNode(transports[i]));
        nodes[i]->set_metrics(&metrics);
    }

    nodes[0]->create();
    for (int i = 1; i < N; ++i) {
        nodes[i]->join(infos[0]);
    }

    metrics.reset();

    for (int round = 0; round < 10; ++round) {
        for (int j = 0; j < N; ++j) {
            nodes[j]->stabilize();
            nodes[j]->do_maintenance();
        }
    }

    assert(metrics.total_maintenance_messages() > 0);
    assert(metrics.total_messages() > 0);
    assert(metrics.total_maintenance_messages() <= metrics.total_messages());

    std::cout << "  Maintenance messages: " << metrics.total_maintenance_messages() << std::endl;
    std::cout << "  Total messages: " << metrics.total_messages() << std::endl;
    std::cout << "  PASS" << std::endl;

    for (int i = 0; i < N; ++i) {
        transports[i]->stop();
        delete nodes[i];
        delete transports[i];
    }
}

void test_membership_metrics() {
    std::cout << "[3] Membership metrics (join + leave)..." << std::endl;

    Metrics metrics;

    NodeInfo n1_info(1000, "sim", 700);
    NodeInfo n2_info(2000, "sim", 701);

    SimTransport t1(n1_info);
    SimTransport t2(n2_info);
    t1.start();
    t2.start();

    FingerTableNode node1(&t1);
    FingerTableNode node2(&t2);
    node2.set_metrics(&metrics);

    node1.create();
    node2.join(n1_info);

    for (int i = 0; i < 5; ++i) {
        node1.stabilize();
        node2.stabilize();
    }

    assert(metrics.total_lookups() == 0);

    node2.leave();

    metrics.dump_membership_csv("/tmp/test_membership_integration.csv");

    std::ifstream check("/tmp/test_membership_integration.csv");
    assert(check.good());
    std::remove("/tmp/test_membership_integration.csv");

    std::cout << "  PASS" << std::endl;

    t1.stop();
    t2.stop();
}

void test_csv_dump() {
    std::cout << "[4] CSV dump with real data..." << std::endl;

    const int N = 8;
    std::vector<NodeInfo> infos;
    std::vector<SimTransport*> transports;
    std::vector<ChordNode*> nodes;
    Metrics metrics;

    for (int i = 0; i < N; ++i) {
        NodeId id = (UINT64_MAX / N) * i + 1;
        infos.push_back(NodeInfo(id, "sim", 800 + i));
        transports.push_back(new SimTransport(infos[i]));
        transports[i]->start();
        nodes.push_back(new FingerTableNode(transports[i]));
    }

    nodes[0]->set_metrics(&metrics);
    nodes[0]->create();

    for (int i = 1; i < N; ++i) {
        nodes[i]->join(infos[0]);
        for (int round = 0; round < 5; ++round) {
            for (int j = 0; j <= i; ++j) {
                nodes[j]->stabilize();
                nodes[j]->do_maintenance();
            }
        }
    }

    for (int round = 0; round < 30; ++round) {
        for (int j = 0; j < N; ++j) {
            nodes[j]->stabilize();
            nodes[j]->do_maintenance();
        }
    }

    for (int i = 0; i < 50; ++i) {
        uint64_t key = fnv1a_hash("csv_test_" + std::to_string(i));
        nodes[0]->lookup(key);
    }

    metrics.dump_lookups_csv("/tmp/test_lookups_integration.csv");
    metrics.dump_summary_csv("/tmp/test_summary_integration.csv", "finger_table", N);

    std::ifstream lookups_file("/tmp/test_lookups_integration.csv");
    assert(lookups_file.good());
    std::string header;
    std::getline(lookups_file, header);
    assert(header.find("key") != std::string::npos);

    std::ifstream summary_file("/tmp/test_summary_integration.csv");
    assert(summary_file.good());

    std::cout << "  Summary: " << N << " nodes, "
              << metrics.total_lookups() << " lookups, "
              << "avg " << metrics.avg_hop_count() << " hops, "
              << "avg " << metrics.avg_latency_us() << " us" << std::endl;

    std::remove("/tmp/test_lookups_integration.csv");
    std::remove("/tmp/test_summary_integration.csv");

    for (int i = 0; i < N; ++i) {
        transports[i]->stop();
        delete nodes[i];
        delete transports[i];
    }

    std::cout << "  PASS" << std::endl;
}

int main() {
    std::cout << "Running Metrics Integration tests..." << std::endl;

    test_lookup_metrics();
    test_maintenance_metrics();
    test_membership_metrics();
    test_csv_dump();

    std::cout << "All Metrics Integration tests passed." << std::endl;
    return 0;
}