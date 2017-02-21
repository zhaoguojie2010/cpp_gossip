//
// Created by bloodstone on 17/1/23.
//

#ifndef CPPGOSSIP_CONFIG_H
#define CPPGOSSIP_CONFIG_H

#include <string>
#include <cstdio>

namespace gossip {

std::string getLocalAddr() {
    return "localhost";
}

struct config {
    config(short port)
    : Port_(port),
      Addr_(getLocalAddr()),
      Indirect_checks_(1000),
      Sync_state_timeout_(1000),
      Probe_interval_(1000),
      Probe_timeout_(1000),
      Gossip_interval_(1000) {
        Name_ = "localhost" + std::to_string(port);
    }

    std::string Name_;
    std::string Addr_;
    short Port_;

    short Indirect_checks_;

    uint32_t Sync_state_timeout_;

    uint32_t Probe_interval_; // in ms
    uint32_t Probe_timeout_;

    uint32_t Gossip_interval_;
    uint32_t Gossip_node_num_;

};

typedef std::shared_ptr<config> confptr;

confptr DefaultConfig() {
    auto conf = std::make_shared<config>(29011);
    return conf;
}
}
#endif //CPPGOSSIP_CONFIG_H
