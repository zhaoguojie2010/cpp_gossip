//
// Created by bloodstone on 17/1/29.
//

#ifndef CPPGOSSIP_BROADCAST_HPP
#define CPPGOSSIP_BROADCAST_HPP
#include "src/message/message.pb.h"
namespace gossip {

struct broadcast_message {
    broadcast_message(message::STATE state, void *msg)
    : state_(state),
      msg_(msg) {}
    message::STATE state_;
    void *msg_;

    ~broadcast_message() {
        delete msg_;
    }
};

class broadcast_queue {
public:
    broadcast_queue()
    : broadcast_multi_(3) {

    }
    // get the msg that need to be broadcasted from the queue
    std::shared_ptr<broadcast_message> pop() {

    }

    // push the msg into the queue
    void push(std::shared_ptr<broadcast_message> msg, const uint32 node_num) {

    }

private:
    // this is used to determine how many node each msg
    // will be broadcasted to. the formula is as follow:
    // broadcast_multi_ * log(node_num)
    uint32 broadcast_multi_;
};

}

#endif //CPPGOSSIP_BROADCAST_HPP
