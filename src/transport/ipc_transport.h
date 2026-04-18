#ifndef CHORD_IPC_TRANSPORT_H
#define CHORD_IPC_TRANSPORT_H

#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>
#include "transport/transport.h"


class IPCTransport : public Transport {
public:
    IPCTransport(const NodeInfo& local, const std::string& socket_path);
    ~IPCTransport();

    bool send(const NodeInfo& dest, const Message& msg);
    void register_handler(MessageType type, MessageHandler handler);
    void start();
    void stop();
    const NodeInfo& local_info() const;
    const std::string& socket_path() const;
    static std::string path_for_node(const NodeInfo& node);

private:
    void handle_new_connection();
    void listen_loop();
    int create_listener();

    bool read_message(int fd, Message& msg);

    bool write_message(int fd, const Message& msg);

    bool read_exact(int fd, uint8_t* buf, size_t n);

    bool write_all(int fd, const uint8_t* buf, size_t n);

    NodeInfo local_;
    std::string socket_path_;
    std::map<MessageType, MessageHandler> handlers_;
    std::mutex handler_mutex_;

    int listen_fd_;
    std::thread listener_thread_;
    std::atomic<bool> running_;
};

#endif // CHORD_IPC_TRANSPORT_H