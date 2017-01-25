#include <iostream>
#include "gossip.hpp"

int main() {
    std::cout << "Hello, World!" << std::endl;
    gossip::config conf(29011);
    gossip::gossiper g(conf);
    return 0;
}