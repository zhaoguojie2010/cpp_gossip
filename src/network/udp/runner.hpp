//
// Created by bloodstone on 17/2/3.
//

#ifndef CPPGOSSIP_SERVER_HPP
#define CPPGOSSIP_SERVER_HPP

#include <cstdlib>
#include <iostream>
#include "asio.hpp"

namespace gossip {
namespace udp {

using asio::ip::udp;

typedef std::function<int(char */*data*/,
                          int/*data length*/,
                          char */*response buff*/,
                          int/*maximum length of response*/)> packet_handler;

class Server {
public:
    Server(short port, packet_handler handle_packet, asio::io_service &io_svc)
        : socket_(io_svc, udp::endpoint(udp::v4(), port)),
          handle_packet_(handle_packet) {}

    void Start() {
        doReceive();
    }

    void doReceive() {
        socket_.async_receive_from(
            asio::buffer(data_, mtu_), sender_endpoint_,
            [this](std::error_code ec, std::size_t bytes_recvd) {
                if (!ec && bytes_recvd > 0) {
                    int response_length = handle_packet_(data_, bytes_recvd, data_, mtu_);
                    // TODO: handle error
                    if (response_length > 0)
                        doSend(response_length);
                } else {
                    doReceive();
                }
            });
    }

    void doSend(std::size_t length) {
        socket_.async_send_to(
            asio::buffer(data_, length), sender_endpoint_,
            [this](std::error_code /*ec*/, std::size_t /*bytes_sent*/) {
                doReceive();
            });
    }

private:
    udp::socket socket_;
    udp::endpoint sender_endpoint_;
    const static int mtu_ = 1460;
    char data_[mtu_];
    packet_handler handle_packet_;
};

}
}

#endif //CPPGOSSIP_SERVER_HPP
