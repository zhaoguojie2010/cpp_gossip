#include <iostream>
#include "gossip.h"

int main() {
    std::cout << "Hello, World!" << std::endl;
    gossip::confptr conf = std::make_shared<gossip::config>();
    return 0;
}