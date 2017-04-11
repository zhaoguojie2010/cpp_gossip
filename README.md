##cppgosip
cppgossip is an header-only cross-platform gossip(aka. anti-entropy) protocol written in c++11


##Getting started

1. git@github.com:zhaoguojie2010/cppgossip.git
2. include "gossip.hpp"
3. create join/leave handler, the signature should be void(std::string)
4. create a config object: gossip::config conf(addr). addr format is ip:port
5. create a gossiper instance: gossip::gossiper<1> g(conf);
6. register join/leave handler: g.RegisterNotifier(notify_join, notify_leave);
7. join a seed node of the target cluster: g.Join(seed_addr);

* say now you have a main.cpp
``` c
#include <iostream>
#include <thread>
#include <chrono>
#include "gossip.hpp"
void notify_join(std::string node) {
    std::cout << "node " << node << " joined." << std::endl;
}
void notify_leave(std::string node) {
    std::cout << "node " << node << " left." << std::endl;
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

    std::this_thread::sleep_for(std::chrono::seconds(10000));
    return 0;
}
```

* compile
>g++ main.cpp -std=c++0x -I path/to/cppgossip -I path/to/cppgossip/thirdparty
* test
    * in one terminal:
    >./a.out 127.0.0.1:29011 127.0.0.1:29011
    * in another terminal:
    >./a.out 127.0.0.1:29013 127.0.0.1:29011
