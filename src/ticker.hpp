//
// Created by bloodstone on 17/2/7.
//

#ifndef CPPGOSSIP_TICKER_HPP
#define CPPGOSSIP_TICKER_HPP

#include <chrono>
#include "asio.hpp"

namespace gossip {

class Ticker {
public:
    typedef std::function<void()> callback;
public:
    // Tick calls op every timeout milliseconds
    virtual void Tick(uint32_t timeout, callback op) = 0;

    // Stop
    virtual void Stop() = 0;

    virtual ~Ticker() {}
};

class AsioTicker: public Ticker {
public:
    AsioTicker(asio::io_service &io_svc)
    : timer_(io_svc){

    }

    virtual void Tick(uint32_t timeout, callback op) {
        op_ = op;
        timeout_ = std::chrono::milliseconds(timeout);
        timer_.expires_from_now(timeout_);
        timer_.async_wait(std::bind(&AsioTicker::checkDeadline, this));
    }

    virtual void Stop() {
        timer_.cancel();
    }

    virtual ~AsioTicker() {
        Stop();
    }

private:
    void checkDeadline()
    {
        if (timer_.expires_at() <= std::chrono::steady_clock::now())
        {
            // timer_ has expired, just call the callback
            if (op_ != nullptr)
                op_();

            // reset timer
            timer_.expires_from_now(timeout_);
        }
        timer_.async_wait(std::bind(&AsioTicker::checkDeadline, this));
    }

    asio::steady_timer timer_;
    callback op_;
    std::chrono::milliseconds timeout_;

};

}

#endif //CPPGOSSIP_TICKER_HPP
