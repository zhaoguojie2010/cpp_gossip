//
// Created by bloodstone on 17/2/8.
//

#ifndef CPPGOSSIP_BROADCAST_HPP
#define CPPGOSSIP_BROADCAST_HPP

#include <set>
#include <memory>
#include "asio.hpp"

namespace gossip {
namespace udp {

using asio::ip::udp;

class Broadcaster {
public:
    typedef std::shared_ptr<udp::endpoint> peerPtr;
public:
    Broadcaster(asio::io_service &io_svc)
        : socket_(io_svc) {}

    void Add(const std::string &host, short port) {
        peerPtr p = std::make_shared<udp::endpoint>(
            asio::ip::address::from_string(
                host), port);
        peers_.insert(p);
    }

    void Broadcast(char *buff, std::size_t size) {
        std::for_each(peers_.begin(), peers_.end(), [this](peerPtr &p) {
            // block sending the broadcast message.
            // it's ok to block because we don't need ack.
            socket_.send_to(asio::buffer(buff, size), *p);
        });
    }

    void Clear() {
        endpoints_.clear();
    }
private:
    std::set<udp::endpoint> peers_;
    udp::socket socket_;

};

}
}

#endif //CPPGOSSIP_BROADCAST_HPP
