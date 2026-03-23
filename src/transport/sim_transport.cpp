#include <map>
#include <mutex>
#include "transport/message.h"
#include "transport/types.h"
#include "transport/transport.h"
#include "transport/sim_network.h"
#include "transport/sim_transport.h"

SimTransport::SimTransport(const NodeInfo& local) {
    local_ = local;
    running_ = false;
}

SimTransport::~SimTransport() {
    if (running_) stop();
}

void SimTransport::start(){
    running_ = true;
    SimNetwork::instance().register_node(local_.id, this);
}

void SimTransport::stop(){
    running_ = false;
    SimNetwork::instance().unregister_node(local_.id);
}

const NodeInfo& SimTransport::local_info() const {
    return local_;
}

void SimTransport::register_handler(MessageType type, MessageHandler handler){
    std::lock_guard<std::mutex> lock(mutex_);
    handlers_[type] = handler;
}

bool SimTransport::send(const NodeInfo& dest, const Message& msg) {
    return SimNetwork::instance().route(local_, dest.id, msg);
}

void SimTransport::deliver(const NodeInfo& from, const Message& msg) {
    MessageHandler handler;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = handlers_.find(msg.type);
        if (it == handlers_.end()) {
            return;
        }
        handler = it->second;
    }
    handler(from, msg);
}