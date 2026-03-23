#ifndef CHORD_TRANSPORT_H
#define CHORD_TRANSPORT_H

#include <functional>
#include "types.h"
#include "message.h"

// ── Transport Interface ──────────────────────────────────────
// This is the key abstraction that decouples Chord logic from
// the networking layer. All Chord code talks through this
// interface — it never touches sockets or knows whether it's
// running over real TCP or in-process simulation.
//
// Two implementations:
//   SimTransport  — in-process, for development + large-scale tests
//   TCPTransport  — real sockets, for small-ring validation (Day 2)

typedef std::function<void(const NodeInfo& from, const Message& msg)> MessageHandler;

class Transport {
public:
    virtual ~Transport() {}

    // Send a message to a destination node.
    // Returns true if the message was delivered (or queued).
    virtual bool send(const NodeInfo& dest, const Message& msg) = 0;

    // Register a handler for a specific message type.
    // When a message of this type arrives, the handler is called
    // with the sender's info and the message.
    virtual void register_handler(MessageType type, MessageHandler handler) = 0;

    // Start listening for incoming messages.
    virtual void start() = 0;

    // Stop the transport and clean up.
    virtual void stop() = 0;

    // The identity of this transport's node.
    virtual const NodeInfo& local_info() const = 0;
};

#endif // CHORD_TRANSPORT_H