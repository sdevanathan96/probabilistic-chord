#ifndef CHORD_TRANSPORT_H
#define CHORD_TRANSPORT_H

#include <functional>
#include "types.h"
#include "message.h"


typedef std::function<void(const NodeInfo& from, const Message& msg)> MessageHandler;

class Transport {
public:
    virtual ~Transport() {}

    virtual bool send(const NodeInfo& dest, const Message& msg) = 0;

    virtual void register_handler(MessageType type, MessageHandler handler) = 0;

    virtual void start() = 0;

    virtual void stop() = 0;

    virtual const NodeInfo& local_info() const = 0;
};

#endif // CHORD_TRANSPORT_H