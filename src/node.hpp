//
// Created by bloodstone on 17/2/2.
//

#ifndef CPPGOSSIP_NODE_HPP
#define CPPGOSSIP_NODE_HPP

#include <cstdint>
#include "src/message/message_generated.h"
namespace gossip {

struct node_state {
    std::string Name_;
    std::string IP_;
    std::string Port_;
    uint64_t Dominant_;
    message::STATE State_;
    std::string From_;
    uint64_t Timestamp_;

    node_state(){}

    node_state(const node_state &other)
        : Name_(other.Name_),
          IP_(other.IP_),
          Port_(other.Port_),
          Dominant_(other.Dominant_),
          State_(other.State_),
          From_(other.From_),
          Timestamp_(other.Timestamp_)
    {}

    node_state(node_state &&other)
    : Name_(std::move(other.Name_)),
      IP_(std::move(other.IP_)),
      Port_(std::move(other.Port_)),
      Dominant_(other.Dominant_),
      State_(other.State_),
      From_(std::move(other.From_)),
      Timestamp_(other.Timestamp_)
    {}

    node_state& operator=(const node_state &other) {
        Name_ = other.Name_;
        IP_ = other.IP_;
        Port_ = other.Port_;
        Dominant_ = other.Dominant_;
        State_ = other.State_;
        From_ = other.From_;
        Timestamp_ = other.Timestamp_;
        return *this;
    }

    node_state& operator=(node_state &&other) {
        Name_ = std::move(other.Name_);
        IP_ = std::move(other.IP_);
        Port_ = std::move(other.Port_);
        Dominant_ = other.Dominant_;
        State_ = other.State_;
        From_ = std::move(other.From_);
        Timestamp_ = other.Timestamp_;
        return *this;
    }
};

}
#endif //CPPGOSSIP_NODE_HPP
