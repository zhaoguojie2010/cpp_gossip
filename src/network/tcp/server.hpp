//
// Created by bloodstone on 17/1/21.
//

#ifndef CPPGOSSIP_SERVER_H
#define CPPGOSSIP_SERVER_H

#include <memory>
#include <functional>
#include <thread>
#include "src/types.hpp"
#include "src/message/message.pb.h"
#include "thirdparty/asio/include/asio.hpp"

namespace gossip {
using asio::ip::tcp;

typedef std::function<uint32(const message::Header&, char*, uint32)> Handler;

class session: public std::enable_shared_from_this<session> {
public:
    session(tcp::socket socket)
        : socket_(std::move(socket))
    {}

    session(tcp::socket socket, Handler handler)
        : socket_(std::move(socket)),
          handler_(handler)
    {}

    // Go starts the session with a read op
    void Go() {
        do_read_header(header_size_);
    }

private:
    // do_read_header reads the fixed length header so that
    // we know what we are dealing with.
    void do_read_header(uint32 header_size) {
        auto self(shared_from_this());
        asio::async_read(socket_,
            asio::buffer(buff_, header_size),
            [this, self](std::error_code ec, std::size_t){
                if (!ec) {
                    do_read_body();
                }
            });
    }

    void do_read_body() {
        auto self(shared_from_this());
        message::Header hdr;
        hdr.ParseFromArray(buff_, header_size_);
        asio::async_read(socket_,
            asio::buffer(buff_, hdr.length()),
            [this, self, &hdr](std::error_code ec, std::size_t) {
                if (!ec) {
                    auto resp_length = handler_(hdr, buff_, buff_size_);
                    do_write_response(resp_length);
                }
            });
    }

    void do_write_response(uint32 length) {
        auto self(shared_from_this());
        asio::async_write(socket_,
            asio::buffer(buff_, length),
            [this, self](std::error_code ec, std::size_t) {
                // do nothing, wait for the session to end
            });
    }

private:
    tcp::socket socket_;
    uint32 header_size_;
    enum { buff_size_ = 65536};
    char buff_[buff_size_];

    Handler handler_;
};

class TcpSvr {
public:
    TcpSvr(short port)
    : io_svc_(),
      acceptor_(io_svc_, tcp::endpoint(tcp::v4(), port)),
      socket_(io_svc_) {
        do_accept();
    }

    void Run() {
        std::thread([this]() {
            io_svc_.run();
        });
    }

    asio::io_service* GetIoSvc() {
        return &io_svc_;
    }
private:
    void do_accept() {
        acceptor_.async_accept(socket_,
            [this](std::error_code ec) {
                if (!ec) {
                    std::make_shared<session>(std::move(socket_))->Go();
                }
            });
    }


private:
    asio::io_service io_svc_;
    tcp::acceptor acceptor_;
    tcp::socket socket_;
};
typedef std::shared_ptr<TcpSvr> tcpSvrPtr;

}
#endif //CPPGOSSIP_SERVER_H
