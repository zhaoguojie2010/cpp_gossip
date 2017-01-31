//
// Created by bloodstone on 17/1/29.
//

#ifndef CPPGOSSIP_BROADCAST_HPP
#define CPPGOSSIP_BROADCAST_HPP
#include "src/message/message.pb.h"
namespace gossip {

// TODO: create msg_ object inside the constructor instead
// of creating outside then passing the ptr
struct broadcast_message {
    broadcast_message(message::alive *msg)
    : state_(message::ALIVE),
      msg_(msg){}

    broadcast_message(message::suspect *msg)
    : state_(message::SUSPECT),
      msg_(msg){}

    broadcast_message(message::dead *msg)
    : state_(message::DEAD),
      msg_(msg){}

    broadcast_message(broadcast_message &&other)
    : state_(other.state_) {
        msg_ = other.msg_;
        other.msg_ = nullptr;
    }

    broadcast_message& operator=(broadcast_message&& other) {
        if (msg_ != other.msg_) {
            if (msg_ != nullptr) {
                release();
            }
            msg_ = other.msg_;
            other.msg_ = nullptr;
        }
        return *this;
    }

    message::STATE state_;
    void *msg_;

    void release() {
        switch (state_) {
            case message::ALIVE:
                delete static_cast<message::alive*>(msg_);
                break;
            case message::SUSPECT:
                delete static_cast<message::suspect*>(msg_);
                break;
            case message::DEAD:
                delete static_cast<message::dead*>(msg_);
                break;
            default:
                ;
        }
    }


    ~broadcast_message() {
        release();
    }
};

class broadcast_queue {
public:
    broadcast_queue()
    : broadcast_multi_(3) {

    }
    // get the msg that need to be broadcasted from the queue
    std::shared_ptr<broadcast_message> pop() {
        return nullptr;
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
