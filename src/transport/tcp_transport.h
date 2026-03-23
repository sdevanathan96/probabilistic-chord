#ifndef CHORD_TCP_TRANSPORT_H
#define CHORD_TCP_TRANSPORT_H

#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include "transport/transport.h"


class TCPTransport : public Transport {
public:
    explicit TCPTransport(const NodeInfo& local);
    ~TCPTransport();

    bool send(const NodeInfo& dest, const Message& msg);
    void register_handler(MessageType type, MessageHandler handler);
    void start();
    void stop();
    const NodeInfo& local_info() const;

private:
    void listen_loop();

    int create_listener();

    bool read_message(int fd, Message& msg);

    int get_or_connect(const std::string& ip, uint16_t port);

    bool write_message(int fd, const Message& msg);


    NodeInfo local_;

    std::map<MessageType, MessageHandler> handlers_;
    std::mutex handler_mutex_;

    std::map<std::string, int> conn_pool_;
    std::mutex conn_mutex_;

    int listen_fd_;
    std::thread listener_thread_;
    std::atomic<bool> running_;

    std::vector<int> client_fds_;
    std::mutex client_mutex_;
};

#endif // CHORD_TCP_TRANSPORT_H