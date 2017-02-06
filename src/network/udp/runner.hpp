//
// Created by bloodstone on 17/2/3.
//

#ifndef CPPGOSSIP_SERVER_HPP
#define CPPGOSSIP_SERVER_HPP

#include <cstdlib>
#include <iostream>
#include "asio.hpp"

using asio::ip::udp;

typedef std::function<int(char*/*data*/,
                          int/*data length*/,
                          int/*maximum length of response*/)> handle_packet;

class server
{
public:
    server(asio::io_service& io_service, short port)
        : socket_(io_service, udp::endpoint(udp::v4(), port)) {
        do_receive();
    }

    void do_receive() {
        socket_.async_receive_from(
            asio::buffer(data_, max_length), sender_endpoint_,
            [this](std::error_code ec, std::size_t bytes_recvd)
            {
                if (!ec && bytes_recvd > 0)
                {
                    int response_length = handle_packet(data_, bytes_recvd, mtu_);
                    do_send(response_length);
                }
                else
                {
                    do_receive();
                }
            });
    }

    void do_send(std::size_t length) {
        socket_.async_send_to(
            asio::buffer(data_, length), sender_endpoint_,
            [this](std::error_code /*ec*/, std::size_t /*bytes_sent*/)
            {
                do_receive();
            });
    }

private:
    udp::socket socket_;
    udp::endpoint sender_endpoint_;
    enum { max_length = 1024 };
    char data_[max_length];
    const int mtu_ = 1460;
};

#endif //CPPGOSSIP_SERVER_HPP
