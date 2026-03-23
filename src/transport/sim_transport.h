#ifndef CHORD_SIM_TRANSPORT_H
#define CHORD_SIM_TRANSPORT_H

#include <map>
#include <mutex>
#include "transport.h"
#include "sim_network.h"



class SimTransport : public Transport {
public:
    explicit SimTransport(const NodeInfo& local);
    ~SimTransport();



    bool send(const NodeInfo& dest, const Message& msg);
    void register_handler(MessageType type, MessageHandler handler);
    void start();
    void stop();
    const NodeInfo& local_info() const;



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