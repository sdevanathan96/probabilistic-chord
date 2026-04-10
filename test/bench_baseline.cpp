#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include "transport/sim_transport.h"
#include "chord/ring_utils.h"
#include "chord/chord_node.h"
#include "chord/finger_table_node.h"
#include "chord/successor_walk_node.h"
#include "bench/metrics.h"

struct RingSetup {
    std::vector<NodeInfo> infos;
    std::vector<SimTransport*> transports;
    std::vector<ChordNode*> nodes;
};

RingSetup build_ring(int n, std::string mode, Metrics* metrics) {
    RingSetup ring;

    for (int i = 0; i < n; ++i) {
        NodeId id = (UINT64_MAX / n) * i + 1;
        ring.infos.push_back(NodeInfo(id, "sim", 1000 + i));
        ring.transports.push_back(new SimTransport(ring.infos[i]));
        ring.transports[i]->start();
        if (mode == "finger_table") {
            ring.nodes.push_back(new FingerTableNode(ring.transports[i]));
        } else if (mode == "successor_walk") {
            ring.nodes.push_back(new SuccessorWalkNode(ring.transports[i]));
        }
    }

    if (metrics) ring.nodes[0]->set_metrics(metrics);

    ring.nodes[0]->create();
    for (int i = 1; i < n; ++i) {
        ring.nodes[i]->join(ring.infos[0]);
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

void teardown_ring(RingSetup& ring) {
    for (size_t i = 0; i < ring.nodes.size(); ++i) {
        ring.transports[i]->stop();
        delete ring.nodes[i];
        delete ring.transports[i];
    }
}

void run_lookups(ChordNode* node, int count) {
    for (int i = 0; i < count; ++i) {
        uint64_t key = fnv1a_hash("bench_key_" + std::to_string(i));
        node->lookup(key);
    }
}

int main() {
    std::vector<int> ring_sizes = {8, 16, 32, 64, 128, 256};
    int num_lookups = 100;
    std::string summary_path = "results/summary.csv";

    system("mkdir -p results");

    std::cout << "=== Baseline Benchmark ===" << std::endl;
    std::cout << "Lookups per ring: " << num_lookups << std::endl;
    std::cout << std::endl;

    std::cout << "Mode              | Nodes | Avg Hops | Avg Latency (us) | p50 (us) | p99 (us)" << std::endl;
    std::cout << "------------------|-------|----------|------------------|----------|--------" << std::endl;

    for (int n : ring_sizes) {
        {
            Metrics metrics;
            RingSetup ring = build_ring(n, "finger_table", &metrics);
            run_lookups(ring.nodes[0], num_lookups);

            std::cout << "Finger Table      | "
                      << n << "    | "
                      << metrics.avg_hop_count() << "      | "
                      << metrics.avg_latency_us() << "          | "
                      << metrics.p50_latency_us() << "    | "
                      << metrics.p99_latency_us() << std::endl;

            std::string lookup_path = "results/lookups_finger_" + std::to_string(n) + ".csv";
            metrics.dump_lookups_csv(lookup_path);
            metrics.dump_summary_csv(summary_path, "finger_table", n);

            teardown_ring(ring);
        }

        {
            Metrics metrics;
            RingSetup ring = build_ring(n, "successor_walk", &metrics);
            run_lookups(ring.nodes[0], num_lookups);

            std::cout << "Successor Walk    | "
                      << n << "    | "
                      << metrics.avg_hop_count() << "      | "
                      << metrics.avg_latency_us() << "          | "
                      << metrics.p50_latency_us() << "    | "
                      << metrics.p99_latency_us() << std::endl;

            std::string lookup_path = "results/lookups_successor_" + std::to_string(n) + ".csv";
            metrics.dump_lookups_csv(lookup_path);
            metrics.dump_summary_csv(summary_path, "successor_walk", n);

            teardown_ring(ring);
        }

        std::cout << "------------------|-------|----------|------------------|----------|--------" << std::endl;
    }

    std::cout << std::endl;
    std::cout << "Results written to results/" << std::endl;
    return 0;
}