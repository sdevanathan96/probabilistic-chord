#include <map>
#include <mutex>
#include <thread>
#include <chrono>
#include "transport/types.h"
#include "transport/message.h"
#include "transport/sim_network.h"
#include "transport/sim_transport.h"

SimNetwork& SimNetwork::instance() {
    static SimNetwork instance;
    return instance;
}

SimNetwork::SimNetwork() : latency_ms_(0) {}

void SimNetwork::register_node(NodeId id, SimTransport* transport){
    std::lock_guard<std::mutex> lock(mutex_);
    nodes_[id] = transport;
}

void SimNetwork::unregister_node(NodeId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    nodes_.erase(id);
}

void SimNetwork::set_latency_ms(int ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    latency_ms_ = ms;
}

bool SimNetwork::route(const NodeInfo& from, NodeId dest_id, const Message& msg) {
    SimTransport* dest_transport = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodes_.find(dest_id);
        if (it == nodes_.end()) {
            return false;
        }
        dest_transport = it->second;
    }
    if (latency_ms_ > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(latency_ms_));
    }
    dest_transport->deliver(from, msg);
    return true;
}