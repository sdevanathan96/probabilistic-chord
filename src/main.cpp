#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include "transport/sim_transport.h"
#include "transport/ipc_transport.h"
#include "chord/node.h"
#include "chord/ring_utils.h"

static std::atomic<bool> running(true);

void signal_handler(int sig) {
    running.store(false);
}

void usage(const char* prog) {
    std::cerr << "Usage:" << std::endl;
    std::cerr << "  " << prog << " sim" << std::endl;
    std::cerr << "    Run a quick SimTransport demo (3 nodes, same process)" << std::endl;
    std::cerr << std::endl;
    std::cerr << "  " << prog << " ipc --id <node_id> [--bootstrap <socket_path>]" << std::endl;
    std::cerr << "    Run a single Chord node over IPC." << std::endl;
    std::cerr << "    If --bootstrap is omitted, creates a new ring." << std::endl;
    std::cerr << "    Socket path: /tmp/chord_node_<id>.sock" << std::endl;
}

void run_sim_demo() {
    std::cout << "=== SimTransport Demo ===" << std::endl;

    NodeInfo n1(1, "sim", 1);
    NodeInfo n2(2, "sim", 2);
    NodeInfo n3(3, "sim", 3);

    SimTransport t1(n1);
    SimTransport t2(n2);
    SimTransport t3(n3);

    t1.start();
    t2.start();
    t3.start();

    ChordNode node1(&t1);
    ChordNode node2(&t2);
    ChordNode node3(&t3);

    node1.create();
    std::cout << "[Node " << n1.id << "] Created ring" << std::endl;

    node2.join(n1);
    std::cout << "[Node " << n2.id << "] Joined via Node " << n1.id << std::endl;

    node3.join(n1);
    std::cout << "[Node " << n3.id << "] Joined via Node " << n1.id << std::endl;

    for (int round = 0; round < 10; ++round) {
        node1.stabilize();
        node1.fix_fingers();
        node2.stabilize();
        node2.fix_fingers();
        node3.stabilize();
        node3.fix_fingers();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "Ring formed:" << std::endl;
    std::cout << "  Node " << n1.id << " -> successor: " << node1.get_successor().id << std::endl;
    std::cout << "  Node " << n2.id << " -> successor: " << node2.get_successor().id << std::endl;
    std::cout << "  Node " << n3.id << " -> successor: " << node3.get_successor().id << std::endl;

    uint64_t key = hash_key("hello");
    NodeInfo owner = node1.lookup(key);
    std::cout << "Lookup(\"hello\") -> Node " << owner.id << std::endl;

    t1.stop();
    t2.stop();
    t3.stop();
    std::cout << "Done." << std::endl;
}

void run_ipc_node(NodeId id, const std::string& bootstrap_path) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    NodeInfo self(id, "ipc", static_cast<uint16_t>(id));
    std::string socket_path = IPCTransport::path_for_node(self);

    IPCTransport transport(self, socket_path);
    transport.start();

    ChordNode node(&transport);

    if (bootstrap_path.empty()) {
        node.create();
        std::cout << "[Node " << id << "] Created new ring at " << socket_path << std::endl;
    } else {
        std::string prefix = "/tmp/chord_node_";
        std::string suffix = ".sock";
        std::string id_str = bootstrap_path.substr(
            prefix.size(),
            bootstrap_path.size() - prefix.size() - suffix.size()
        );
        NodeId bootstrap_id = std::stoull(id_str);
        NodeInfo bootstrap_info(bootstrap_id, "ipc", static_cast<uint16_t>(bootstrap_id));

        node.join(bootstrap_info);
        std::cout << "[Node " << id << "] Joined ring via " << bootstrap_path << std::endl;
    }

    std::cout << "[Node " << id << "] Running (Ctrl+C to stop)..." << std::endl;
    while (running.load()) {
        node.stabilize();
        node.fix_fingers();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::cout << std::endl;
    std::cout << "[Node " << id << "] Shutting down..." << std::endl;
    std::cout << "  Successor:   " << node.get_successor().id << std::endl;
    std::cout << "  Predecessor: " << node.get_predecessor().id << std::endl;

    transport.stop();
    std::cout << "[Node " << id << "] Stopped." << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];

    if (mode == "sim") {
        run_sim_demo();
        return 0;
    }

    if (mode == "ipc") {
        NodeId id = 0;
        std::string bootstrap_path;

        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--id" && i + 1 < argc) {
                id = std::stoull(argv[++i]);
            } else if (arg == "--bootstrap" && i + 1 < argc) {
                bootstrap_path = argv[++i];
            }
        }

        if (id == 0) {
            std::cerr << "Error: --id is required and must be > 0" << std::endl;
            usage(argv[0]);
            return 1;
        }

        run_ipc_node(id, bootstrap_path);
        return 0;
    }

    std::cerr << "Unknown mode: " << mode << std::endl;
    usage(argv[0]);
    return 1;
}