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
        std::cout << "example: ./a.out <local addr> <join addr>";
        return 0;
    }
    std::string addr(argv[1]);
    gossip::config conf(addr);
    gossip::gossiper<1> g(conf);
    g.RegisterNotifier(notify_join, notify_leave);
    g.Join(std::string(argv[2]));

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
