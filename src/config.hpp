//
// Created by bloodstone on 17/1/23.
//

#ifndef CPPGOSSIP_CONFIG_H
#define CPPGOSSIP_CONFIG_H

#include <string>
#include <cstdio>
#include <exception>
#include "utils.hpp"

namespace gossip {

std::string getLocalAddr() {
    return "127.0.0.1";
}

struct config {
    config(const std::string &addr)
    : Indirect_checks_(1000),
      Sync_state_timeout_(0),
      Probe_interval_(1000),
      Probe_timeout_(1000),
      Gossip_interval_(1000) {
        Name_ = addr;
        auto fields = Split(addr, ":");
        if (fields.size() != 2) {
            std::cout << "invalid addr " << addr << std::endl;
            std::terminate();
        }
        Addr_ = fields[0];
        try {
            Port_ = std::stoi(fields[1]);
        } catch(std::exception) {
            std::cout << "invalid port " << fields[1] << std::endl;
            std::terminate();
        }
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
    auto conf = std::make_shared<config>("127.0.0.1:29011");
    return conf;
}
}
#endif //CPPGOSSIP_CONFIG_H
