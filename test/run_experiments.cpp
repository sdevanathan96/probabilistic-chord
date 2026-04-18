#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <fstream>
#include "transport/sim_transport.h"
#include "transport/ipc_transport.h"
#include "chord/ring_utils.h"
#include "chord/finger_table_node.h"
#include "chord/successor_walk_node.h"
#include "chord/filter_node.h"
#include "filter/cuckoo_routing_filter.h"
#include "filter/quotient_routing_filter.h"
#include "bench/metrics.h"

struct ExperimentRing {
    std::vector<NodeInfo> infos;
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

ExperimentRing build_ring(int n, const std::string& mode,
                           int fp_bits, Metrics* metrics, bool use_ipc = false) {
    ExperimentRing ring;
    std::string path;
    for (int i = 0; i < n; ++i) {
        NodeId id = (UINT64_MAX / n) * i + 1;
        ring.infos.push_back(NodeInfo(id, "sim", 2000 + i));

        if (!use_ipc){
            ring.transports.push_back(new SimTransport(ring.infos[i]));
        } else {
            path = IPCTransport::path_for_node(ring.infos[i]);
            ring.transports.push_back(new IPCTransport(ring.infos[i], path));
        }
        ring.transports[i]->start();

        ChordNode* node = NULL;
        RoutingFilter* filter = NULL;

        if (mode == "finger_table") {
            node = new FingerTableNode(ring.transports[i]);
        } else if (mode == "successor_walk") {
            node = new SuccessorWalkNode(ring.transports[i]);
        } else if (mode == "cuckoo") {
            filter = new CuckooRoutingFilter(256, fp_bits);
            node = new FilterNode(ring.transports[i], filter);
        } else if (mode == "quotient") {
            int q_bits = 10;
            filter = new QuotientRoutingFilter(q_bits, fp_bits);
            node = new FilterNode(ring.transports[i], filter);
        }

        ring.nodes.push_back(node);
        ring.filters.push_back(filter);
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

void run_lookups(ChordNode* node, int count) {
    for (int i = 0; i < count; ++i) {
        uint64_t key = fnv1a_hash("experiment_key_" + std::to_string(i));
        node->lookup(key);
    }
}


void experiment_maintenance_cost(bool use_ipc) {
    std::cout << "=== Experiment 1: Maintenance Cost ===" << std::endl;

    std::ofstream csv("results/exp1_maintenance.csv");
    csv << "routing_mode,phase,num_nodes,total_messages,maintenance_messages" << std::endl;

    std::vector<std::string> modes = {"finger_table", "cuckoo", "quotient"};
    int n = 64;
    int fp_bits = 12;

    for (const auto& mode : modes) {
        Metrics metrics;
        ExperimentRing ring = build_ring(n, mode, fp_bits, NULL, use_ipc);

        for (int i = 0; i < n; ++i) {
            ring.nodes[i]->set_metrics(&metrics);
        }

        metrics.reset();
        for (int round = 0; round < 100; ++round) {
            for (int j = 0; j < n; ++j) {
                ring.nodes[j]->stabilize();
                if (mode == "finger_table") {
                    ring.nodes[j]->do_maintenance();
                }
            }
        }

        std::cout << "  " << mode << " (stable): total=" << metrics.total_messages()
                  << " maintenance=" << metrics.total_maintenance_messages() << std::endl;
        csv << mode << ",stable," << n << ","
            << metrics.total_messages() << ","
            << metrics.total_maintenance_messages() << std::endl;

        metrics.reset();
        for (int i = 0; i < 5; ++i) {
            NodeId id = UINT64_MAX - (i + 1) * 1000;
            NodeInfo info(id, "sim", 3000 + i);
            Transport* t;
            if (!use_ipc){
                t = new SimTransport(info);
            } else {
                std::string path = IPCTransport::path_for_node(info);
                t = new IPCTransport(info, path);
            }
            t->start();

            ChordNode* new_node = NULL;
            RoutingFilter* new_filter = NULL;

            if (mode == "finger_table") {
                new_node = new FingerTableNode(t);
            } else if (mode == "cuckoo") {
                new_filter = new CuckooRoutingFilter(256, fp_bits);
                new_node = new FilterNode(t, new_filter);
            } else if (mode == "quotient") {
                new_filter = new QuotientRoutingFilter(10, fp_bits);
                new_node = new FilterNode(t, new_filter);
            }

            new_node->set_metrics(&metrics);
            new_node->join(ring.infos[0]);

            for (int round = 0; round < 10; ++round) {
                for (size_t j = 0; j < ring.nodes.size(); ++j) {
                    ring.nodes[j]->stabilize();
                    ring.nodes[j]->do_maintenance();
                }
                new_node->stabilize();
                new_node->do_maintenance();
            }

            ring.nodes.push_back(new_node);
            ring.infos.push_back(info);
            ring.transports.push_back(t);
            ring.filters.push_back(new_filter);
        }

        std::cout << "  " << mode << " (5 joins): total=" << metrics.total_messages()
                  << " maintenance=" << metrics.total_maintenance_messages() << std::endl;
        csv << mode << ",joins," << n << ","
            << metrics.total_messages() << ","
            << metrics.total_maintenance_messages() << std::endl;

        ring.teardown();
    }

    csv.close();
    std::cout << std::endl;
}


void experiment_latency_under_churn(bool use_ipc) {
    std::cout << "=== Experiment 2: Latency Under Membership Changes ===" << std::endl;

    std::ofstream csv("results/exp2_latency_timeseries.csv");
    csv << "routing_mode,lookup_index,latency_us,hop_count" << std::endl;

    std::vector<std::string> modes = {"finger_table", "cuckoo", "quotient"};
    int n = 64;
    int fp_bits = 12;
    int total_lookups = 200;
    int lookup_idx = 0;

    for (const auto& mode : modes) {
        Metrics metrics;
        ExperimentRing ring = build_ring(n, mode, fp_bits, &metrics, use_ipc);

        auto start_time = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < 50; ++i) {
            auto now = std::chrono::high_resolution_clock::now();
            double ts = std::chrono::duration<double, std::milli>(now - start_time).count();

            double latency = 0;
            int hops = 0;
            {
                ScopedTimer timer(latency);
                uint64_t key = fnv1a_hash("churn_key_" + std::to_string(i));
                ring.nodes[0]->lookup(key);
            }

            csv << mode << "," << lookup_idx++ << "," << latency << "," << metrics.avg_hop_count() << std::endl;
            
        }

        for (int i = 0; i < 5; ++i) {
            NodeId id = UINT64_MAX - (i + 1) * 2000;
            NodeInfo info(id, "sim", 4000 + i);
            Transport* t;
            if (!use_ipc) {
                t = new SimTransport(info);
            } else {
                std::string path = IPCTransport::path_for_node(info);
                t = new IPCTransport(info, path);
            }
            t->start();

            ChordNode* new_node = NULL;
            RoutingFilter* new_filter = NULL;

            if (mode == "finger_table") {
                new_node = new FingerTableNode(t);
            } else if (mode == "cuckoo") {
                new_filter = new CuckooRoutingFilter(256, fp_bits);
                new_node = new FilterNode(t, new_filter);
            } else if (mode == "quotient") {
                new_filter = new QuotientRoutingFilter(10, fp_bits);
                new_node = new FilterNode(t, new_filter);
            }

            new_node->join(ring.infos[0]);

            for (int round = 0; round < 10; ++round) {
                for (size_t j = 0; j < ring.nodes.size(); ++j) {
                    ring.nodes[j]->stabilize();
                    ring.nodes[j]->do_maintenance();
                }
                new_node->stabilize();
                new_node->do_maintenance();

                auto now = std::chrono::high_resolution_clock::now();
                double ts = std::chrono::duration<double, std::milli>(now - start_time).count();

                double latency = 0;
                {
                    ScopedTimer timer(latency);
                    uint64_t key = fnv1a_hash("churn_key_" + std::to_string(50 + i * 10 + round));
                    ring.nodes[0]->lookup(key);
                }

                csv << mode << "," << lookup_idx++ << "," << latency << ",0" << std::endl;
            }

            ring.nodes.push_back(new_node);
            ring.infos.push_back(info);
            ring.transports.push_back(t);
            ring.filters.push_back(new_filter);
        }

        for (int i = 0; i < 50; ++i) {
            for (size_t j = 0; j < ring.nodes.size(); ++j) {
                ring.nodes[j]->stabilize();
                ring.nodes[j]->do_maintenance();
            }

            auto now = std::chrono::high_resolution_clock::now();
            double ts = std::chrono::duration<double, std::milli>(now - start_time).count();

            double latency = 0;
            {
                ScopedTimer timer(latency);
                uint64_t key = fnv1a_hash("churn_key_" + std::to_string(100 + i));
                ring.nodes[0]->lookup(key);
            }

            csv << mode << "," << lookup_idx++ << "," << latency << ",0" << std::endl;
        }

        ring.teardown();
    }

    csv.close();
    std::cout << "  Written to results/exp2_latency_timeseries.csv" << std::endl;
    std::cout << std::endl;
}


void experiment_fp_sensitivity(bool use_ipc) {
    std::cout << "=== Experiment 3: FP Rate Sensitivity ===" << std::endl;

    std::ofstream csv("results/exp3_fp_sensitivity.csv");
    csv << "filter_type,fp_bits,theoretical_fp_rate,avg_hops,avg_latency_us,p99_latency_us,memory_bytes" << std::endl;

    int n = 64;
    int num_lookups = 200;

    std::vector<int> cuckoo_bits = {6, 7, 9, 10, 11, 12, 16};
    

    for (int bits : cuckoo_bits) {
        Metrics metrics;
        ExperimentRing ring = build_ring(n, "cuckoo", bits, &metrics, use_ipc);
        run_lookups(ring.nodes[0], num_lookups);

        double theoretical_fp = (2.0 * 4.0) / (1 << bits);
        size_t mem = ring.filters[0] ? ring.filters[0]->memory_usage() : 0;

        std::cout << "  Cuckoo " << bits << "-bit: "
                  << "hops=" << metrics.avg_hop_count()
                  << " fp_rate=" << (theoretical_fp * 100) << "%"
                  << " mem=" << mem << "B" << std::endl;

        csv << "cuckoo," << bits << "," << theoretical_fp << ","
            << metrics.avg_hop_count() << "," << metrics.avg_latency_us() << ","
            << metrics.p99_latency_us() << "," << mem << std::endl;

        ring.teardown();
    }

    std::vector<int> quotient_bits = {4, 5, 6, 7, 8, 9, 14};
    for (int bits : quotient_bits) {
        Metrics metrics;
        ExperimentRing ring = build_ring(n, "quotient", bits, &metrics, use_ipc);
        run_lookups(ring.nodes[0], num_lookups);

        double theoretical_fp = 1.0 / (1 << bits);
        size_t mem = ring.filters[0] ? ring.filters[0]->memory_usage() : 0;

        std::cout << "  Quotient " << bits << "-bit: "
                  << "hops=" << metrics.avg_hop_count()
                  << " fp_rate=" << (theoretical_fp * 100) << "%"
                  << " mem=" << mem << "B" << std::endl;

        csv << "quotient," << bits << "," << theoretical_fp << ","
            << metrics.avg_hop_count() << "," << metrics.avg_latency_us() << ","
            << metrics.p99_latency_us() << "," << mem << std::endl;

        ring.teardown();
    }

    csv.close();
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc > 2 || (argc == 2 && std::string(argv[1]) != "--ipc")) {
        std::cerr << "Usage: " << argv[0] << " [--ipc]" << std::endl;
        return 1;
    }
    bool use_ipc = (argc == 2 && std::string(argv[1]) == "--ipc");
    system("mkdir -p results");

    experiment_maintenance_cost(use_ipc);
    experiment_latency_under_churn(use_ipc);
    experiment_fp_sensitivity(use_ipc);

    std::cout << "All experiments complete. Results in results/" << std::endl;
    return 0;
}