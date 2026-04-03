#ifndef CHORD_NODE_H
#define CHORD_NODE_H

#include <mutex>
#include "transport/transport.h"
#include "transport/types.h"
#include "chord/ring_utils.h"
#include "chord/finger_table.h"

enum class RoutingMode {
    SUCCESSOR_WALK,
    FINGER_TABLE
};

class ChordNode {
public:
    explicit ChordNode(Transport* transport, RoutingMode mode = RoutingMode::FINGER_TABLE);
    ~ChordNode();

    void create();
    void join(const NodeInfo& bootstrap);
    NodeInfo lookup(uint64_t key);
    void leave();

    void stabilize();
    void fix_fingers();
    void notify(const NodeInfo& candidate);

    NodeInfo get_info() const;
    NodeInfo get_successor() const;
    NodeInfo get_predecessor() const;
    RoutingMode get_routing_mode() const;

    void print_finger_table() const;


private:
    void handle_find_successor(const NodeInfo& from, const Message& msg);

    void handle_stabilize_req(const NodeInfo& from, const Message& msg);

    void handle_notify(const NodeInfo& from, const Message& msg);

    void handle_leave_notify(const NodeInfo& from, const Message& msg);

    Message make_find_successor_req(uint64_t key);

    bool send_rpc(const NodeInfo& dest, const Message& request,
                  MessageType response_type, Message& response,
                  int timeout_ms = 3000);
    NodeInfo find_successor_for(uint64_t key);

    Transport* transport_;
    NodeInfo self_;
    NodeInfo successor_;
    NodeInfo predecessor_;
    RoutingMode routing_mode_;
    FingerTable finger_table_;
    mutable std::mutex mutex_;

    Message pending_response_;
    bool response_ready_;
    std::mutex rpc_mutex_;
    std::condition_variable rpc_cv_;
};

#endif // CHORD_NODE_H