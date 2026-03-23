#include <iostream>
#include <thread>
#include <chrono>
#include "transport/sim_transport.h"
#include "transport/message.h"
#include "transport/types.h"

int main() {

    NodeInfo n1(1, "sim", 1);
    NodeInfo n2(2, "sim", 2);
    NodeInfo n3(3, "sim", 3);

    SimTransport t1(n1);
    SimTransport t2(n2);
    SimTransport t3(n3);

    t2.register_handler(MessageType::PING,
        [&](const NodeInfo& from, const Message& msg) {
            std::cout << "[Node 2] Got PING from Node " << from.id
                      << ", sending PONG" << std::endl;
            t2.send(from, Message(MessageType::PONG, {}));
        });
    t3.register_handler(MessageType::PING,
        [&](const NodeInfo& from, const Message& msg) {
            std::cout << "[Node 3] Got PING from Node " << from.id
                      << ", sending PONG" << std::endl;
            t3.send(from, Message(MessageType::PONG, {}));
        });
    t1.register_handler(MessageType::PONG,
        [&](const NodeInfo& from, const Message& msg) {
            std::cout << "[Node 1] Got PONG from Node " << from.id
                      << std::endl;
        });

    t1.start();
    t2.start();
    t3.start();

    std::cout << "[Node 1] Sending PING to Node 2" << std::endl;
    t1.send(n2, Message(MessageType::PING, {}));

    std::cout << "[Node 1] Sending PING to Node 3" << std::endl;
    bool ok = t1.send(n3, Message(MessageType::PING, {}));
    std::cout << "[Node 1] Send to Node 3 returned: " << ok << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    t1.stop();
    t2.stop();
    t3.stop();

    std::cout << "Done." << std::endl;
    return 0;
}