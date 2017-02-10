//
// Created by bloodstone on 17/1/29.
//

#ifndef CPPGOSSIP_BROADCAST_HPP
#define CPPGOSSIP_BROADCAST_HPP

#include "src/node.hpp"
namespace gossip {


class broadcast_queue {
public:
    broadcast_queue()
    : broadcast_multi_(3) {

    }
    // get the msg that need to be broadcasted from the queue
    std::shared_ptr<node_state> pop() {
        return nullptr;
    }

    // push the msg into the queue
    void push(std::shared_ptr<node_state> msg, const uint32_t node_num) {

    }

private:
    // this is used to determine how many node each msg
    // will be broadcasted to. the formula is as follow:
    // broadcast_multi_ * log(node_num)
    uint32_t broadcast_multi_;
};

}

#endif //CPPGOSSIP_BROADCAST_HPP
