#include <iostream>
#include <cassert>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>
#include "transport/sim_transport.h"
#include "chord/ring_utils.h"
#include "chord/filter_node.h"
#include "filter/cuckoo_routing_filter.h"
#include "filter/cuckoo_routing_filter.h"
#include "bench/metrics.h"

void test_cuckoo_routing_basic() {
    std::cout << "[1] Cuckoo filter routing basic..." << std::endl;

    const int N = 8;
    std::vector<NodeInfo> infos;
    std::vector<SimTransport*> transports;
    std::vector<ChordNode*> nodes;
    std::vector<CuckooRoutingFilter*> filters;

    for (int i = 0; i < N; ++i) {
        NodeId id = (UINT64_MAX / N) * i + 1;
        infos.push_back(NodeInfo(id, "sim", 900 + i));
        transports.push_back(new SimTransport(infos[i]));
        transports[i]->start();

        CuckooRoutingFilter* filter = new CuckooRoutingFilter(256, 12);
        filters.push_back(filter);

        ChordNode* node = new FilterNode(transports[i], filter);
        nodes.push_back(node);
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

    for (int round = 0; round < 20; ++round) {
        for (int j = 0; j < N; ++j) {
            nodes[j]->stabilize();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    int success = 0;
    for (int i = 0; i < 20; ++i) {
        uint64_t key = fnv1a_hash("cuckoo_routing_test_" + std::to_string(i));
        NodeInfo owner = nodes[0]->lookup(key);
        if (owner.id != 0) success++;
    }

    std::cout << "  " << success << "/20 lookups succeeded" << std::endl;
    assert(success == 20);

    std::vector<NodeInfo> sorted = infos;
    std::sort(sorted.begin(), sorted.end(),
        [](const NodeInfo& a, const NodeInfo& b) { return a.id < b.id; });

    int correct = 0;
    for (int i = 0; i < 20; ++i) {
        uint64_t key = fnv1a_hash("cuckoo_routing_test_" + std::to_string(i));
        NodeInfo owner = nodes[0]->lookup(key);

        NodeInfo expected = sorted[0];
        for (int j = 0; j < N; ++j) {
            if (sorted[j].id >= key) {
                expected = sorted[j];
                break;
            }
        }
        if (owner == expected) correct++;
    }

    std::cout << "  " << correct << "/20 lookups correct" << std::endl;
    assert(correct == 20);

    std::cout << "  PASS" << std::endl;

    for (int i = 0; i < N; ++i) {
        transports[i]->stop();
        delete nodes[i];
        delete filters[i];
        delete transports[i];
    }
}

void test_cuckoo_routing_with_metrics() {
    std::cout << "[2] Cuckoo filter routing with metrics..." << std::endl;

    const int N = 16;
    std::vector<NodeInfo> infos;
    std::vector<SimTransport*> transports;
    std::vector<ChordNode*> nodes;
    std::vector<CuckooRoutingFilter*> filters;
    Metrics metrics;

    for (int i = 0; i < N; ++i) {
        NodeId id = (UINT64_MAX / N) * i + 1;
        infos.push_back(NodeInfo(id, "sim", 1100 + i));
        transports.push_back(new SimTransport(infos[i]));
        transports[i]->start();

        CuckooRoutingFilter* filter = new CuckooRoutingFilter(256, 12);
        filters.push_back(filter);

        ChordNode* node = new FilterNode(transports[i], filter);
        nodes.push_back(node);
    }

    nodes[0]->set_metrics(&metrics);

    nodes[0]->create();
    for (int i = 1; i < N; ++i) {
        nodes[i]->join(infos[0]);
        for (int round = 0; round < 5; ++round) {
            for (int j = 0; j <= i; ++j) {
                nodes[j]->stabilize();
            }
        }
    }

    for (int round = 0; round < 30; ++round) {
        for (int j = 0; j < N; ++j) {
            nodes[j]->stabilize();
        }
    }

    for (int i = 0; i < 50; ++i) {
        uint64_t key = fnv1a_hash("metrics_cuckoo_" + std::to_string(i));
        nodes[0]->lookup(key);
    }

    std::cout << "  Lookups: " << metrics.total_lookups() << std::endl;
    std::cout << "  Avg hops: " << metrics.avg_hop_count() << std::endl;
    std::cout << "  Avg latency: " << metrics.avg_latency_us() << " us" << std::endl;

    assert(metrics.total_lookups() == 50);
    assert(metrics.avg_hop_count() > 0);

    std::cout << "  PASS" << std::endl;

    for (int i = 0; i < N; ++i) {
        transports[i]->stop();
        delete nodes[i];
        delete filters[i];
        delete transports[i];
    }
}

void test_cuckoo_node_leave() {
    std::cout << "[3] Cuckoo filter routing after node leave..." << std::endl;

    const int N = 4;
    std::vector<NodeInfo> infos;
    std::vector<SimTransport*> transports;
    std::vector<ChordNode*> nodes;
    std::vector<CuckooRoutingFilter*> filters;

    for (int i = 0; i < N; ++i) {
        NodeId id = (UINT64_MAX / N) * i + 1;
        infos.push_back(NodeInfo(id, "sim", 1200 + i));
        transports.push_back(new SimTransport(infos[i]));
        transports[i]->start();

        CuckooRoutingFilter* filter = new CuckooRoutingFilter(256, 12);
        filters.push_back(filter);

        ChordNode* node = new FilterNode(transports[i], filter);
        nodes.push_back(node);
    }

    nodes[0]->create();
    for (int i = 1; i < N; ++i) {
        nodes[i]->join(infos[0]);
        for (int round = 0; round < 5; ++round) {
            for (int j = 0; j <= i; ++j) {
                nodes[j]->stabilize();
            }
        }
    }

    for (int round = 0; round < 20; ++round) {
        for (int j = 0; j < N; ++j) {
            nodes[j]->stabilize();
        }
    }

    nodes[2]->leave();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    for (int round = 0; round < 10; ++round) {
        for (int j = 0; j < N; ++j) {
            if (j == 2) continue;
            nodes[j]->stabilize();
        }
    }

    int success = 0;
    for (int i = 0; i < 10; ++i) {
        uint64_t key = fnv1a_hash("leave_cuckoo_" + std::to_string(i));
        NodeInfo owner = nodes[0]->lookup(key);
        if (owner.id != 0 && owner.id != infos[2].id) success++;
    }

    std::cout << "  " << success << "/10 lookups succeeded after leave" << std::endl;
    assert(success == 10);

    std::cout << "  PASS" << std::endl;

    for (int i = 0; i < N; ++i) {
        transports[i]->stop();
        delete nodes[i];
        delete filters[i];
        delete transports[i];
    }
}

int main() {
    std::cout << "Running Cuckoo Routing tests..." << std::endl;

    test_cuckoo_routing_basic();
    test_cuckoo_routing_with_metrics();
    test_cuckoo_node_leave();

    std::cout << "All Cuckoo Routing tests passed." << std::endl;
    return 0;
}