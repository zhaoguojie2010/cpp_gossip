#include <iostream>
#include "gossip.hpp"

int main() {
    std::cout << "Hello, World!" << std::endl;
    gossip::config conf(29011);
    gossip::gossiper g(conf);
    g.Join("localhost:29011");
    int a;
    std::cin >> a;
    return 0;
}