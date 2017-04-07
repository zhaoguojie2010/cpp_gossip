//
// Created by bloodstone on 17/2/8.
//

#ifndef CPPGOSSIP_ASYNC_UDP_CLIENT_HPP
#define CPPGOSSIP_ASYNC_UDP_CLIENT_HPP

#include <chrono>
#include <iostream>
#include <memory>
#include "asio.hpp"
#include "asio/steady_timer.hpp"
#include "src/logger.hpp"

namespace gossip {
namespace udp {

class AsyncClient: public std::enable_shared_from_this<AsyncClient> {
public:
    typedef std::function<void()> timeout_callback;
    typedef std::function<void(char*, std::size_t)> recv_callback;
    typedef std::function<void()> send_callback;
public:
    ~AsyncClient() {
        std::cout << "~AsyncClient\n";
    }

    AsyncClient(asio::io_service &io_svc)
    : socket_(io_svc),
      send_finish_cb_(nullptr),
      send_timeout_cb_(nullptr),
      receive_finish_cb_(nullptr),
      receive_timeout_cb_(nullptr),
      deadline_(io_svc) {}

    void AsyncSendTo(char *buff, std::size_t size,
                const std::string &host, short port,
                uint32_t timeout = 0) {
        if (size > mtu_) {
            logger->error("datagram size too large");
            return;
        }
        auto self(shared_from_this());
        if (timeout > 0) {
            deadline_.expires_from_now(std::chrono::milliseconds(timeout));
            deadline_.async_wait([this, self](asio::error_code) {
                checkDeadline("udp send timeout", [this]() {
                    // send timeout, cancel the sending operation
                    socket_.cancel();
                    if (send_timeout_cb_ != nullptr) {
                        send_timeout_cb_();
                    }
                });
            });
        }
        asio::error_code ec;
        auto addr = asio::ip::address::from_string(host, ec);
        if (ec) {
            std::cerr << ec.message() << std::endl;
            return;
        }
        asio::ip::udp::endpoint peer(addr, port);
        socket_.open(peer.protocol());
        socket_.async_send_to(
            asio::buffer(buff, size), peer,
            [this, self, timeout](const asio::error_code &ec, std::size_t) {
                // now the sending op is finished, cancel the timer
                if (timeout > 0) {
                    deadline_.cancel();
                }
                if (ec) {
                    std::cerr << "async_send_to: " << ec.message() << std::endl;
                    return;
                }
                if (send_finish_cb_ != nullptr) {
                    send_finish_cb_();
                }
            });
    }

    // Waterfall defines the behavior of the client
    AsyncClient& Waterfall(send_callback sfc, timeout_callback stc,
        recv_callback rfc, timeout_callback rtc) {
        send_finish_cb_ = sfc;
        send_timeout_cb_ = stc;
        receive_finish_cb_ = rfc;
        receive_timeout_cb_ = rtc;
        return *this;
    }

    void AsyncReceiveFrom(
        const std::string &host, short port,
        uint32_t timeout = 0) {
        auto peer = asio::ip::udp::endpoint(asio::ip::address::from_string(host), port);
        asyncReceiveFrom(peer, timeout);
    }

private:
    void handleSendTo(const asio::error_code &ec, std::size_t bytes_transfered,
        asio::ip::udp::endpoint &peer) {
        // cancel the timeout timer
        deadline_.cancel();

        if (ec) {
            std::cerr << ec.message() << std::endl;
            return;
        }

        // start async receiving
        asyncReceiveFrom(peer, 0);
    }

    void asyncReceiveFrom(
        asio::ip::udp::endpoint &peer, uint32_t timeout = 0) {
        auto self(shared_from_this());
        if (timeout > 0) {
            deadline_.expires_from_now(std::chrono::milliseconds(timeout));
            deadline_.async_wait([this, self](asio::error_code) {
                checkDeadline("udp receive timeout", [this]() {
                    socket_.cancel();
                    if (receive_timeout_cb_ != nullptr) {
                        receive_timeout_cb_();
                    }
                });
            });
        }

        socket_.async_receive_from(
            asio::buffer(data_, mtu_), peer,
            [this, self, timeout](const asio::error_code &ec, std::size_t bytes_received) {
                // now the receiving op is finished, cancel the timer
                if (timeout > 0) {
                    deadline_.cancel();
                }
                if (ec) {
                    std::cerr << "xxx" << ec.message() << std::endl;
                    return;
                }
                if (receive_finish_cb_ != nullptr) {
                    receive_finish_cb_(data_, bytes_received);
                }
            });
    }

    void checkDeadline(const std::string &error_info, timeout_callback op) {
        if (deadline_.expires_at() <= std::chrono::steady_clock::now()) {
            std::cout << error_info << std::endl;
            if (op != nullptr) {
                op();
            }
        }
    }
private:
    asio::steady_timer deadline_;
    asio::ip::udp::socket socket_;
    const static int mtu_ = 1460;
    char data_[mtu_];
    send_callback send_finish_cb_;
    timeout_callback send_timeout_cb_;
    recv_callback receive_finish_cb_;
    timeout_callback receive_timeout_cb_;
};

}
}

#endif //CPPGOSSIP_ASYNC_UDP_CLIENT_HPP
