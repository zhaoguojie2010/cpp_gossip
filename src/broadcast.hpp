//
// Created by bloodstone on 17/1/29.
//

#ifndef CPPGOSSIP_BROADCAST_HPP
#define CPPGOSSIP_BROADCAST_HPP

#include <mutex>
#include <cmath>
#include <utility>
#include <string>
#include "src/node.hpp"
namespace gossip {


class BroadcastQueue {
public:
    BroadcastQueue()
    : broadcast_multi_(3),
      peeked_(0) {
        cur_ = m_.begin();
        peek_ = cur_;
    }

    // get the msg that need to be broadcasted from the queue
    std::shared_ptr<node_state> Pop() {
        mtx_.lock();
        auto rst = pop();
        mtx_.unlock();
        return rst;
    }

    // Peek acts like Pop() except it doesn't reduce the msg counter.
    // One should always call ResetPeek() or ApplyPeek() before calling 
    // Pop() if Peek() was called.
    // One can peek no more than m_.size() nodes at once, if exceeded, 
    // nullptr will be returned.
    std::shared_ptr<node_state> Peek() {
        mtx_.lock();
        if (peek_ == m_.end())
            peek_ = m_.begin();
        if (peek_ == m_.end()) {
            mtx_.unlock();
            return nullptr;
        }

        if (peeked_ >= m_.size()) {
            mtx_.unlock();
            return nullptr;
        }
        ++peeked_;

        auto rst = peek_->second.first;
        ++peek_;
        mtx_.unlock();
        return rst;
    }

    // ResetPeek set the peek_ to cur_
    void ResetPeek() {
        mtx_.lock();
        peek_ = cur_;
        peeked_ = 0;
        mtx_.unlock();
    }

    // ApplyPeek pops all peeked_ msg
    void ApplyPeek() {
        mtx_.lock();
        while (peeked_ > 0) {
            pop();
            --peeked_;
        }
        peek_ = cur_;
        mtx_.unlock();
    }

    // Push the msg into the queue, node_num is used to
    // decide how many times the new state needs to be
    // broadcasted. see the comment of broadcast_multi_
    void Push(std::shared_ptr<node_state> msg, uint32_t node_num) {
        //std::cout << "push " << msg->Name_ << ", num = " << node_num << std::endl;
        if (node_num <= 1) {
            node_num = 2;
        }
        auto key = msg->Name_;
        int times = broadcast_multi_ * std::ceil(std::log10(node_num));
        mtx_.lock();
        if (m_.find(key) == m_.end()) {
            m_[key] = std::make_pair(msg, times);
        } else {
            auto old = m_[key].first;
            if (msg->Dominant_ > old->Dominant_ ||
                (msg->State_ > old->State_ && msg->Dominant_ == old->Dominant_)) {
                m_[key] = std::make_pair(msg, times);
            }
        }
        mtx_.unlock();
    }

    int Distinct() {
        mtx_.lock();
        int d = m_.size();
        mtx_.unlock();
        return d;
    }

public:
    typedef std::pair<std::shared_ptr<node_state>, int> content_type;

private:
    std::shared_ptr<node_state> pop() {
        if (cur_ == m_.end()) 
            cur_ = m_.begin();
        if (cur_ == m_.end()) {
            return nullptr;
        }
        
        auto rst = cur_->second.first;
        // reduce msg counter
        if (--cur_->second.second < 1) {
            // if counter is 0, remove the msg
            m_.erase(cur_++);
        } else {
            cur_++;
        }
        return rst;
    }

private:
    // this is used to determine how many node each msg
    // will be broadcasted to. the formula is as follow:
    // broadcast_multi_ * ceil(log10(node_num))
    uint32_t broadcast_multi_;
    std::unordered_map<std::string, content_type> m_;
    std::unordered_map<std::string, content_type>::iterator cur_;
    std::unordered_map<std::string, content_type>::iterator peek_;
    int peeked_;
    std::mutex mtx_;
};

}

#endif //CPPGOSSIP_BROADCAST_HPP
