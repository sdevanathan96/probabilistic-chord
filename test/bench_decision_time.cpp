#include <iostream>
#include <vector>
#include <chrono>
#include <fstream>
#include <algorithm>
#include "transport/sim_transport.h"
#include "transport/ipc_transport.h"
#include "chord/ring_utils.h"
#include "chord/finger_table_node.h"
#include "chord/filter_node.h"
#include "filter/cuckoo_routing_filter.h"
#include "filter/quotient_routing_filter.h"


struct DecisionBenchResult {
    std::string mode;
    int num_nodes;
    double avg_ns;
    double p50_ns;
    double p99_ns;
};

struct BenchRing {
    std::vector<Transport*> transports;
    std::vector<ChordNode*> nodes;
    std::vector<RoutingFilter*> filters;

    void teardown() {
        for (size_t i = 0; i < nodes.size(); ++i) {
            transports[i]->stop();
            delete nodes[i];
            if (i < filters.size() && filters[i]) delete filters[i];
            delete transports[i];
        }
    }
};

BenchRing build_ring(int n, const std::string& mode, bool use_ipc = false) {
    BenchRing ring;
    std::string path;
    for (int i = 0; i < n; ++i) {
        NodeId id = (UINT64_MAX / n) * i + 1;
        NodeInfo info(id, "sim", 5000 + i);
        if (!use_ipc){
            ring.transports.push_back(new SimTransport(info));
        } else {
            path = IPCTransport::path_for_node(info);
            ring.transports.push_back(new IPCTransport(info, path));
        }
        ring.transports[i]->start();

        ChordNode* node = NULL;
        RoutingFilter* filter = NULL;

        if (mode == "finger_table") {
            node = new FingerTableNode(ring.transports[i]);
        } else if (mode == "cuckoo") {
            filter = new CuckooRoutingFilter(256, 12);
            node = new FilterNode(ring.transports[i], filter);
        } else if (mode == "quotient") {
            filter = new QuotientRoutingFilter(10, 12);
            node = new FilterNode(ring.transports[i], filter);
        }

        ring.nodes.push_back(node);
        ring.filters.push_back(filter);
    }

    ring.nodes[0]->create();
    for (int i = 1; i < n; ++i) {
        ring.nodes[i]->join(ring.nodes[0]->get_info());
        for (int round = 0; round < 5; ++round) {
            for (int j = 0; j <= i; ++j) {
                ring.nodes[j]->stabilize();
                ring.nodes[j]->do_maintenance();
            }
        }
    }

    for (int round = 0; round < 40; ++round) {
        for (int j = 0; j < n; ++j) {
            ring.nodes[j]->stabilize();
            ring.nodes[j]->do_maintenance();
        }
    }

    return ring;
}

DecisionBenchResult bench_decision_time(const std::string& mode, int n,
                                         int num_calls, bool use_ipc = false) {
    BenchRing ring = build_ring(n, mode, use_ipc);

    std::vector<uint64_t> keys;
    for (int i = 0; i < num_calls; ++i) {
        keys.push_back(fnv1a_hash("decision_bench_" + std::to_string(i)));
    }

    std::vector<double> times_ns;
    for (int i = 0; i < num_calls; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        ring.nodes[0]->find_next_hop(keys[i]);
        auto end = std::chrono::high_resolution_clock::now();
        double ns = std::chrono::duration<double, std::nano>(end - start).count();
        times_ns.push_back(ns);
    }

    std::vector<double> sorted = times_ns;
    std::sort(sorted.begin(), sorted.end());

    double sum = 0;
    for (double t : times_ns) sum += t;

    DecisionBenchResult result;
    result.mode = mode;
    result.num_nodes = n;
    result.avg_ns = sum / num_calls;
    result.p50_ns = sorted[sorted.size() / 2];
    result.p99_ns = sorted[static_cast<size_t>(sorted.size() * 0.99)];

    ring.teardown();
    return result;
}

int main(int argc, char* argv[]) {
    if (argc > 2 || (argc == 2 && std::string(argv[1]) != "--ipc")) {
        std::cerr << "Usage: " << argv[0] << " [--ipc]" << std::endl;
        return 1;
    }
    bool use_ipc = (argc == 2 && std::string(argv[1]) == "--ipc");
    system("mkdir -p results");

    std::cout << "=== Per-Hop Decision Time Benchmark ===" << std::endl;
    std::cout << std::endl;

    std::ofstream csv("results/exp_decision_time.csv");
    csv << "routing_mode,num_nodes,avg_ns,p50_ns,p99_ns" << std::endl;

    std::vector<std::string> modes = {"finger_table", "cuckoo", "quotient"};
    std::vector<int> sizes = {8, 16, 32, 64, 128, 256};
    int num_calls = 10000;

    std::cout << "Mode              | Nodes | Avg (ns) | p50 (ns) | p99 (ns)" << std::endl;
    std::cout << "------------------|-------|----------|----------|--------" << std::endl;

    for (int n : sizes) {
        for (const auto& mode : modes) {
            DecisionBenchResult r = bench_decision_time(mode, n, num_calls, use_ipc);

            std::cout << r.mode;
            for (size_t s = r.mode.size(); s < 18; ++s) std::cout << " ";
            std::cout << "| " << r.num_nodes;
            for (size_t s = std::to_string(r.num_nodes).size(); s < 5; ++s) std::cout << " ";
            std::cout << " | " << r.avg_ns
                      << " | " << r.p50_ns
                      << " | " << r.p99_ns << std::endl;

            csv << r.mode << "," << r.num_nodes << ","
                << r.avg_ns << "," << r.p50_ns << "," << r.p99_ns << std::endl;
        }
        std::cout << "------------------|-------|----------|----------|--------" << std::endl;
    }

    csv.close();
    std::cout << std::endl;
    std::cout << "Results written to results/exp_decision_time.csv" << std::endl;
    return 0;
}