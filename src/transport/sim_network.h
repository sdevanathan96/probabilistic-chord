#ifndef CHORD_SIM_NETWORK_H
#define CHORD_SIM_NETWORK_H

#include <map>
#include <mutex>
#include "types.h"
#include "message.h"

class SimTransport;  // forward declaration



class SimNetwork {
public:
    // Get the singleton instance.
    static SimNetwork& instance();

    // Register a transport so it can receive messages.
    // Called by SimTransport::start().
    void register_node(NodeId id, SimTransport* transport);

    // Unregister a transport.
    // Called by SimTransport::stop().
    void unregister_node(NodeId id);

    // Route a message from `from` to the transport registered
    // under `dest_id`. Returns false if dest_id is not found.
    //
    // This should look up the destination SimTransport and call
    // its deliver() method. Optionally add a small sleep here
    // to simulate network latency.
    bool route(const NodeInfo& from, NodeId dest_id, const Message& msg);

    // Set simulated one-way latency in milliseconds (default: 0).
    void set_latency_ms(int ms);

private:
    SimNetwork();
    SimNetwork(const SimNetwork&);             // no copy
    SimNetwork& operator=(const SimNetwork&);  // no assign

    std::map<NodeId, SimTransport*> nodes_;
    std::mutex mutex_;
    int latency_ms_;
};

#endif // CHORD_SIM_NETWORK_H