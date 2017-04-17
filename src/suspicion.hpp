//
// Created by bloodstone on 17/1/27.
//

#ifndef CPPGOSSIP_SUSPICION_HPP
#define CPPGOSSIP_SUSPICION_HPP

#include "string"
#include <set>
#include <functional>
#include <cstdint>

#include "thirdparty/asio.hpp"
#include "thirdparty/asio/steady_timer.hpp"
namespace gossip {

class suspicion :public asio::steady_timer {
public:
    typedef std::function<void()> callback;
public:
    suspicion(uint32_t node_num, callback convict,
              asio::io_service *io_svc, uint32_t timeout)
    : asio::steady_timer(*io_svc),
      confirm_(0),
      need_confirm_(std::ceil(std::log2(node_num))),
      convict_(convict) {
        expires_from_now(std::chrono::milliseconds(timeout));
        async_wait([this](const asio::error_code&) {
            convict_();
        });
    }

    bool Confirm(std::string from) {
        bool result = false;
        mtx_.lock();
        auto search = who_confirmed_.find(from);
        if (search == who_confirmed_.end()) {
            who_confirmed_.insert(from);
            ++confirm_;
        }
        if (confirm_ >= need_confirm_) {
            result = true;
            // clean up the timer and convict
            cancel();
            convict_();
        }
        mtx_.unlock();
        return result;
    }

    ~suspicion() {
        cancel();
    }

private:
    // indicates the number of confirms we'd like to see
    // before the suspicion is convicted.
    uint32_t need_confirm_;

    // indicates the number of confirms we've seen so far
    uint32_t confirm_;

    std::mutex mtx_;
    // rule out duplicated confirms from the same node
    std::set<std::string> who_confirmed_;

    // confirm the suspect to be dead
    callback convict_;
};

}

#endif //CPPGOSSIP_SUSPICION_HPP
