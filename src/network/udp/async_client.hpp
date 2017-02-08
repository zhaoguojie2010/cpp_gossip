//
// Created by bloodstone on 17/2/8.
//

#ifndef CPPGOSSIP_ASYNC_CLIENT_HPP
#define CPPGOSSIP_ASYNC_CLIENT_HPP

#include <chrono>
#include "asio.hpp"
#include "asio/steady_timer.hpp"

namespace gossip {
namespace udp {

using asio::ip::udp;

class AsyncClient {
public:
    typedef std::function<void()> callback;
public:
    AsyncClient(asio::io_service &io_svc)
    : socket_(io_svc),
      send_finish_cb_(nullptr),
      send_timeout_cb_(nullptr),
      receive_finish_cb_(nullptr),
      receive_timeout_cb_(nullptr),
      deadline_(io_svc) {}

    void AsyncSendTo(char *buff, std::size_t size,
                const std::string &host, short port,
                uint32 timeout = 0) {
        if (timeout > 0) {
            deadline_.expires_from_now(std::chrono::milliseconds(timeout));
            deadline_.async_wait(std::bind(&AsyncClient::checkDeadline,
                                           this, "udp send timeout",
                                           [this]() {
                                               // send timeout, cancel the sending operation
                                               socket_.cancel();
                                               if (send_timeout_cb_ != nullptr) {
                                                   send_timeout_cb_();
                                               }
                                           }));
        }
        udp::endpoint peer(asio::ip::address::from_string(host), port);
        socket_.async_send_to(
            asio::buffer(buff, size), peer,
            [this](const asio::error_code &ec, std::size_t) {
                // now the sending op is finished, cancel the timer
                deadline_.cancel();
                if (ec) {
                    std::cerr << ec.message() << std::endl;
                    return;
                }
                if (send_finish_cb_ != nullptr) {
                    send_finish_cb_();
                }
            });
            //std::bind(&AsyncClient::handleSendTo, this,
            //    std::placeholders::_1, std::placeholders::_2,
            //    std::ref(peer)));
    }

    void RegisterCallback(callback sfc, callback stc,
        callback rfc, callback rtc) {
        send_finish_cb_ = sfc;
        send_timeout_cb_ = stc;
        receive_finish_cb_ = rfc;
        receive_timeout_cb_ = rtc;
    }

    void AsyncReceiveFrom(char *buff, std::size_t size,
        const std::string &host, short port,
        uint32 timeout = 0) {
        asyncReceiveFrom(buff, size,
                         udp::endpoint(asio::ip::address::from_string(host), port));
    }

private:
    void handleSendTo(const asio::error_code &ec, std::size_t bytes_transfered,
        udp::endpoint &peer) {
        // cancel the timeout timer
        deadline_.cancel();

        if (ec) {
            std::cerr << ec.message() << std::endl;
            return;
        }

        // start async receiving
        asyncReceiveFrom(data_, mtu_, peer);
    }

    void asyncReceiveFrom(char *buff, std::size_t size,
        udp::endpoint &peer, uint32 timeout = 0) {
        if (timeout > 0) {
            deadline_.expires_from_now(std::chrono::milliseconds(timeout));
            deadline_.async_wait(std::bind(&AsyncClient::checkDeadline,
                                           this, "udp receive timeout",
                                           [this]() {
                                               socket_.cancel();
                                               if (receive_timeout_cb_ != nullptr) {
                                                   receive_timeout_cb_();
                                               }
                                           }));
        }

        socket_.async_receive_from(
            asio::buffer(buff, size), peer,
            [this](const asio::error_code &ec, std::size_t) {
                // now the receiving op is finished, cancel the timer
                deadline_.cancel();
                if (ec) {
                    std::cerr << ec.message() << std::endl;
                    return;
                }
                if (receive_finish_cb_ != nullptr) {
                    receive_finish_cb_();
                }
            });
    }

    void checkDeadline(const std::string &error_info, callback op) {
        if (deadline_.expires_at() <= std::chrono::steady_clock::now()) {
            std::cout << error_info << std::endl;
            if (op != nullptr) {
                op();
            }
        }
    }
private:
    asio::steady_timer deadline_;
    udp::socket socket_;
    const int mtu_ = 1460;
    char data_[mtu_];
    callback send_finish_cb_;
    callback send_timeout_cb_;
    callback receive_finish_cb_;
    callback receive_timeout_cb_;
};

}
}

#endif //CPPGOSSIP_ASYNC_CLIENT_HPP
