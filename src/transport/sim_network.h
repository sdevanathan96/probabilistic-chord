#ifndef CHORD_SIM_NETWORK_H
#define CHORD_SIM_NETWORK_H

#include <map>
#include <mutex>
#include "types.h"
#include "message.h"

class SimTransport;



class SimNetwork {
public:
    static SimNetwork& instance();
    void register_node(NodeId id, SimTransport* transport);
    void unregister_node(NodeId id);
    bool route(const NodeInfo& from, NodeId dest_id, const Message& msg);
    void set_latency_ms(int ms);

private:
    SimNetwork();
    SimNetwork(const SimNetwork&);
    SimNetwork& operator=(const SimNetwork&);

    std::map<NodeId, SimTransport*> nodes_;
    std::mutex mutex_;
    int latency_ms_;
};

#endif // CHORD_SIM_NETWORK_H