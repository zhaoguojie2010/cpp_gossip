//
// Created by bloodstone on 17/1/21.
//

#ifndef CPPGOSSIP_SERVER_H
#define CPPGOSSIP_SERVER_H

#include <memory>
#include <functional>
#include <thread>
#include <chrono>
#include "src/message/header.hpp"
#include "src/network/tcp/async_client.hpp"
#include "src/network/tcp/blocking_client.hpp"
#include "thirdparty/asio.hpp"

namespace gossip {
namespace tcp {
using asio::ip::tcp;

typedef std::function<void(char*, std::size_t, message::Header&)> header_handler;
typedef std::function<std::size_t(std::size_t ,char*, std::size_t, char*, std::size_t)> body_handler;

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, header_handler handle_header,
            body_handler handle_body, uint32_t header_size)
        : socket_(std::move(socket)),
          header_size_(header_size),
          handle_header_(handle_header),
          handle_body_(handle_body) {}

    // Go starts the Session with a read op
    void Go() {
        doReadHeader();
    }

private:
    // doReadHeader reads the fixed length header so that
    // we know what we are dealing with.
    void doReadHeader() {
        auto self(shared_from_this());
        asio::async_read(socket_,
                         asio::buffer(in_buff_, header_size_),
                         [this, self](std::error_code ec, std::size_t) {
                             if (!ec) {
                                 doReadBody();
                             }
                         });
    }

    void doReadBody() {
        auto self(shared_from_this());
        message::Header header;
        handle_header_(in_buff_, header_size_, header);
        asio::async_read(socket_,
                         asio::buffer(in_buff_, header.Body_length_),
                         [this, self, &header](std::error_code ec, std::size_t) {
                             if (!ec) {
                                 uint32_t resp_length = handle_body_(
                                     header.Type_,
                                     in_buff_, header.Body_length_,
                                     out_buff_, buff_size_);
                                 doWriteResponse(resp_length);
                             }
                         });
    }

    void doWriteResponse(uint32_t length) {
        auto self(shared_from_this());
        asio::async_write(socket_,
                          asio::buffer(out_buff_, length),
                          [this, self](std::error_code ec, std::size_t) {
                              // do nothing, wait for the Session to end
                          });
    }

private:
    tcp::socket socket_;
    uint32_t header_size_;
    const static int buff_size_ = 65536;
    char in_buff_[buff_size_];
    char out_buff_[buff_size_];

    header_handler handle_header_;
    body_handler handle_body_;
    // TODO: expand bufff size
};

class Server {
public:
    Server(short port, header_handler handle_header,
           body_handler handle_body, uint32_t header_size,
           asio::io_service &io_svc)
        : io_svc_(io_svc),
          acceptor_(io_svc_, tcp::endpoint(tcp::v4(), port)),
          header_size_(header_size),
          handle_header_(handle_header),
          handle_body_(handle_body),
          socket_(io_svc_) {}

    void Start() {
        doAccept();
    }

    asio::io_service *GetIoSvc() {
        return &io_svc_;
    }

private:
    void doAccept() {
        acceptor_.async_accept(socket_,
                               [this](std::error_code ec) {
                                   if (!ec) {
                                       std::make_shared<Session>(std::move(
                                           socket_), handle_header_, handle_body_, header_size_)->Go();
                                   }
                                   doAccept();
                               });
    }


private:
    asio::io_service &io_svc_;
    tcp::acceptor acceptor_;
    tcp::socket socket_;
    uint32_t header_size_;
    header_handler handle_header_;
    body_handler handle_body_;
};

class TcpRunner {
public:
    TcpRunner()
        : io_svc_(),
          tcp_svr_(nullptr) {

    }

    asio::io_service *GetIoSvc() {
        return &io_svc_;
    }

    bool PrepareServer(short port, header_handler handle_header,
                       body_handler handle_body, uint32_t header_size) {
        if (tcp_svr_)
            return false;
        tcp_svr_ = new Server(port, handle_header, handle_body, header_size, io_svc_);
        return true;
    }

    void Run() {
        std::thread([this]() {
            io_svc_.run();
        });
    }

private:
    asio::io_service io_svc_;
    Server *tcp_svr_;
};

typedef std::shared_ptr<Server> tcpSvrPtr;

}
}
#endif //CPPGOSSIP_SERVER_H
