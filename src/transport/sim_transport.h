#ifndef CHORD_SIM_TRANSPORT_H
#define CHORD_SIM_TRANSPORT_H

#include <map>
#include <mutex>
#include "transport.h"
#include "sim_network.h"

// ── SimTransport ─────────────────────────────────────────────
// In-process transport that delivers messages through SimNetwork.
//
// How it works:
//   - send() passes the message to SimNetwork::route(), which
//     looks up the destination and calls deliver() on it.
//   - deliver() is called by SimNetwork when this node receives
//     a message. It looks up the registered handler for the
//     message type and invokes it.
//   - start() registers this transport with SimNetwork.
//   - stop() unregisters it.
//
// Thread safety: deliver() can be called from any thread (since
// the sender's thread calls SimNetwork::route() which calls
// deliver() on the receiver). Protect the handlers map with
// a mutex, or accept that handlers are registered before
// start() is called and treat the map as read-only after that.

class SimTransport : public Transport {
public:
    explicit SimTransport(const NodeInfo& local);
    ~SimTransport();

    // ── Transport interface ──────────────────────────────────

    bool send(const NodeInfo& dest, const Message& msg);
    void register_handler(MessageType type, MessageHandler handler);
    void start();
    void stop();
    const NodeInfo& local_info() const;

    // ── Called by SimNetwork ─────────────────────────────────

    // Deliver an incoming message to this node.
    // Look up the handler for msg.type and call it.
    // If no handler is registered for that type, drop the message
    // (or log a warning).
    void deliver(const NodeInfo& from, const Message& msg);

private:
    NodeInfo local_;
    std::map<MessageType, MessageHandler> handlers_;
    std::mutex mutex_;
    bool running_;
};

#endif // CHORD_SIM_TRANSPORT_H