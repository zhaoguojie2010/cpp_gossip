//
// Created by bloodstone on 17/1/21.
//

#ifndef CPPGOSSIP_SERVER_H
#define CPPGOSSIP_SERVER_H

#include <memory>
#include <functional>
#include <thread>
#include "src/types.hpp"
#include "thirdparty/asio/include/asio.hpp"

namespace gossip {
using asio::ip::tcp;

typedef std::function<uint32(char*, uint32)> header_handler;
typedef std::function<uint32(char*, uint32, char*, uint32)> body_handler;

class session: public std::enable_shared_from_this<session> {
public:
    session(tcp::socket socket, header_handler handle_header,
        body_handler handle_body, uint32 header_size)
    : socket_(std::move(socket)),
      header_size_(header_size),
      handle_header_(handle_header),
      handle_body_(handle_body)
    {}

    // Go starts the session with a read op
    void Go() {
        do_read_header();
    }

private:
    // do_read_header reads the fixed length header so that
    // we know what we are dealing with.
    void do_read_header() {
        auto self(shared_from_this());
        asio::async_read(socket_,
            asio::buffer(buff_, header_size_),
            [this, self](std::error_code ec, std::size_t){
                if (!ec) {
                    do_read_body();
                }
            });
    }

    void do_read_body() {
        auto self(shared_from_this());
        auto length = handle_header_(buff_, header_size_);
        asio::async_read(socket_,
            asio::buffer(buff_, length),
            [this, self, &length](std::error_code ec, std::size_t) {
                if (!ec) {
                    uint32 resp_length = handle_body_(buff_, length, buff_, buff_size_);
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

    header_handler handle_header_;
    body_handler handle_body_;
};

class TcpSvr {
public:
    TcpSvr(short port, header_handler handle_header,
        body_handler handle_body, uint32 header_size)
    : io_svc_(),
      acceptor_(io_svc_, tcp::endpoint(tcp::v4(), port)),
      header_size_(header_size),
      handle_header_(handle_header),
      handle_body_(handle_body),
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
                    std::make_shared<session>(std::move(
                        socket_), handle_header_, handle_body_, header_size_)->Go();
                }
            });
    }


private:
    asio::io_service io_svc_;
    tcp::acceptor acceptor_;
    tcp::socket socket_;
    uint32 header_size_;
    header_handler handle_header_;
    body_handler handle_body_;
};
typedef std::shared_ptr<TcpSvr> tcpSvrPtr;

}
#endif //CPPGOSSIP_SERVER_H
