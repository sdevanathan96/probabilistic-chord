#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
#include <cstring>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include "transport/transport.h"
#include "transport/ipc_transport.h"

IPCTransport::IPCTransport(const NodeInfo& local, const std::string& socket_path) {
    local_ = local;
    socket_path_ = socket_path;
    running_ = false;
}

IPCTransport::~IPCTransport(){
    if (running_) stop();
}

void IPCTransport::start(){
    listen_fd_ = create_listener();
    if (listen_fd_ < 0) {
        throw std::runtime_error("Failed to create listener socket");
    }
    running_ = true;
    listener_thread_ = std::thread(&IPCTransport::listen_loop, this);
}

void IPCTransport::stop(){
    running_ = false;
    close(listen_fd_);
    if(listener_thread_.joinable()) {
        listener_thread_.join();
    }
    for (int fd : client_fds_) {
        close(fd);
    }
    for (const auto& pair : conn_pool_) {
        close(pair.second);
    }
    unlink(socket_path_.c_str());
}

int IPCTransport::create_listener() {
    unlink(socket_path_.c_str());

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    if (listen(fd, 5) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

void IPCTransport::listen_loop() {
    while (running_) {
        std::vector<struct pollfd> poll_fds;
        poll_fds.push_back({listen_fd_, POLLIN, 0});
        {
            std::lock_guard<std::mutex> lock(client_mutex_);
            for (int fd : client_fds_) {
                poll_fds.push_back({fd, POLLIN, 0});
            }
        }
        int ready = poll(poll_fds.data(), poll_fds.size(), 100);
        if (ready <= 0) continue;

        if (poll_fds[0].revents & POLLIN) {
            handle_new_connection();
        }

        for (size_t i = 1; i < poll_fds.size(); ++i) {
            if (poll_fds[i].revents & POLLIN) {
                handle_client_data(poll_fds[i].fd);
            }
        }
    }
}

void IPCTransport::handle_new_connection() {
    int client_fd = accept(listen_fd_, nullptr, nullptr);
    if (client_fd < 0) return;

    std::lock_guard<std::mutex> lock(client_mutex_);
    client_fds_.push_back(client_fd);
}

void IPCTransport::handle_client_data(int fd) {
    Message msg;
    if (!read_message(fd, msg)) {
        close(fd);
        std::lock_guard<std::mutex> lock(client_mutex_);
        client_fds_.erase(
            std::remove(client_fds_.begin(), client_fds_.end(), fd),
            client_fds_.end()
        );
        return;
    }

    NodeInfo from;
    size_t offset = 0;
    if (!unpack_node_info(msg.payload, offset, from)) return;

    msg.payload = std::vector<uint8_t>(
        msg.payload.begin() + offset,
        msg.payload.end()
    );

    MessageHandler handler;
    {
        std::lock_guard<std::mutex> lock(handler_mutex_);
        auto it = handlers_.find(msg.type);
        if (it == handlers_.end()) return;
        handler = it->second;
    }
    std::thread([handler, from, msg]() {
        handler(from, msg);
    }).detach();
}

bool IPCTransport::read_exact(int fd, uint8_t* buf, size_t n) {
    size_t total = 0;
    while (total < n) {
        ssize_t r = read(fd, buf + total, n - total);
        if (r <= 0) return false;
        total += r;
    }
    return true;
}

bool IPCTransport::read_message(int fd, Message &msg){
    uint8_t header[5];
    if (!read_exact(fd, header, 5)) return false;

    uint32_t payload_len = 0;
    for (int i=0; i<4; ++i) {
        payload_len |= (static_cast<uint32_t>(header[i]) << (24 - i * 8));
    }
    MessageType type = static_cast<MessageType>(header[4]);
    std::vector<uint8_t> payload(payload_len);
    if (!read_exact(fd, payload.data(), payload_len)) return false;
    msg.type = type;
    msg.payload = std::move(payload);
    return true;
}

bool IPCTransport::write_all(int fd, const uint8_t* buf, size_t n) {
    size_t total = 0;
    while (total < n) {
        ssize_t w = write(fd, buf + total, n - total);
        if (w <= 0) return false;
        total += w;
    }
    return true;
}

bool IPCTransport::write_message(int fd, const Message& msg) {
    std::vector<uint8_t> data = serialize(msg);
    return write_all(fd, data.data(), data.size());
}

std::string IPCTransport::path_for_node(const NodeInfo& node) {
    return "/tmp/chord_node_" + std::to_string(node.id) + ".sock";
}

void IPCTransport::register_handler(MessageType type, MessageHandler handler){
    std::lock_guard<std::mutex> lock(handler_mutex_);
    handlers_[type] = handler;
}

const NodeInfo& IPCTransport::local_info() const {
    return local_;
}

int IPCTransport::get_or_connect(const std::string& dest_socket_path){
    {
        std::lock_guard<std::mutex> lock(conn_mutex_);
        auto it = conn_pool_.find(dest_socket_path);
        if (it != conn_pool_.end()) {
            return it->second;
        }
    }
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, dest_socket_path.c_str(), sizeof(addr.sun_path) - 1);
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    {
        std::lock_guard<std::mutex> lock(conn_mutex_);
        conn_pool_[dest_socket_path] = fd;
    }
    return fd;
}

bool IPCTransport::send(const NodeInfo& dest, const Message& msg) {
    std::string dest_path = path_for_node(dest);
    int fd = get_or_connect(dest_path);
    if (fd < 0) return false;
    std::vector<uint8_t> sender_bytes = pack_node_info(local_);
    std::vector<uint8_t> new_payload;
    new_payload.reserve(sender_bytes.size() + msg.payload.size());
    new_payload.insert(new_payload.end(), sender_bytes.begin(), sender_bytes.end());
    new_payload.insert(new_payload.end(), msg.payload.begin(), msg.payload.end());

    Message wire_msg(msg.type, new_payload);
    return write_message(fd, wire_msg);
}

const std::string& IPCTransport::socket_path() const {
    return socket_path_;
}