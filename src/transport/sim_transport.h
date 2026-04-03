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

    void deliver(const NodeInfo& from, const Message& msg);

private:
    NodeInfo local_;
    std::map<MessageType, MessageHandler> handlers_;
    std::mutex mutex_;
    bool running_;
};

#endif // CHORD_SIM_TRANSPORT_H