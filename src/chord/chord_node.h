#ifndef CHORD_CHORD_NODE_H
#define CHORD_CHORD_NODE_H

#include <mutex>
#include <condition_variable>
#include <string>
#include "transport/transport.h"
#include "transport/types.h"
#include "chord/ring_utils.h"
#include "bench/metrics.h"

class ChordNode {
public:
    explicit ChordNode(Transport* transport);
    virtual ~ChordNode();

    void create();
    void join(const NodeInfo& bootstrap);
    NodeInfo lookup(uint64_t key);
    void leave();

    void stabilize();
    void notify(const NodeInfo& candidate);
    virtual void do_maintenance();

    void set_metrics(Metrics* metrics);

    NodeInfo get_info() const;
    NodeInfo get_successor() const;
    NodeInfo get_predecessor() const;

protected:
    virtual NodeInfo find_next_hop(uint64_t key);

    virtual void on_create();

    virtual void on_join(const NodeInfo& successor);

    virtual void on_stabilize();

    virtual void on_leave();

    virtual std::string routing_mode_name() const;

    NodeInfo find_successor_for(uint64_t key);
    NodeInfo find_successor_for(uint64_t key, int& hop_count);
    NodeInfo find_successor_for(uint64_t key, bool is_maintenance);

    Message make_find_successor_req(uint64_t key);
    Message make_find_successor_req(uint64_t key, int hop_count);

    bool send_rpc(const NodeInfo& dest, const Message& request,
                  MessageType response_type, Message& response,
                  int timeout_ms = 3000);

    Transport* transport_;
    NodeInfo self_;
    NodeInfo successor_;
    NodeInfo predecessor_;
    mutable std::mutex mutex_;
    Metrics* metrics_;

private:
    void handle_find_successor(const NodeInfo& from, const Message& msg);
    void handle_stabilize_req(const NodeInfo& from, const Message& msg);
    void handle_notify(const NodeInfo& from, const Message& msg);
    void handle_leave_notify(const NodeInfo& from, const Message& msg);

    Message pending_response_;
    bool response_ready_;
    mutable std::mutex rpc_mutex_;
    std::condition_variable rpc_cv_;
};

#endif // CHORD_CHORD_NODE_H