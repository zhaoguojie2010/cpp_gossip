//
// Created by bloodstone on 17/1/23.
//

#ifndef CPPGOSSIP_CONFIG_H
#define CPPGOSSIP_CONFIG_H

#include <string>
#include <cstdio>
#include "types.hpp"

namespace gossip {

struct config {
    config(short port)
    : Bind_port_(port),
      Bind_addr_("0.0.0.0"),
      Indirect_checks_(1000),
      Probe_interval_(1000),
      Probe_timeout_(1000),
      Gossip_interval_(1000) {
        Name_ = "localhost" + std::to_string(port);
    }

    std::string Name_;
    std::string Bind_addr_;
    short Bind_port_;

    short Indirect_checks_;

    uint32 Probe_interval_; // in ms
    uint32 Probe_timeout_;

    uint32 Gossip_interval_;
    uint32 Gossip_node_num_;

};

typedef std::shared_ptr<config> confptr;

confptr DefaultConfig() {
    auto conf = std::make_shared<config>(29011);
    return conf;
}
}
#endif //CPPGOSSIP_CONFIG_H
