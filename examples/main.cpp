#include <iostream>
#include <thread>
#include <chrono>
#include "gossip.hpp"

void notify_join(std::string node) {
    std::cout << "node " << node << " joined.\n";
}

void notify_leave(std::string node) {
    std::cout << "node " << node << " left.\n";
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "example: ./a.out <local port> <join port>";
        return 0;
    }
    short local_port = std::atoi(argv[1]);
    gossip::config conf(local_port);
    gossip::gossiper<3> g(conf);
    g.RegisterNotifier(notify_join, notify_leave);
    g.Join("127.0.0.1:" + std::string(argv[2]));

    /*
    std::thread t([]() {
        gossip::config conf1(29013);
        gossip::gossiper g1(conf1);
        g1.Join("localhost:29011");
    });
     */
    std::this_thread::sleep_for(std::chrono::seconds(10000));
    return 0;
}
