//
// Created by bloodstone on 17/1/23.
//

#ifndef CPPGOSSIP_CONFIG_H
#define CPPGOSSIP_CONFIG_H

#include <string>
#include "types.h"

namespace gossip {

struct config {
    std::string Name_;
    std::string Bind_addr;
    short Bind_port;

    short Indirect_checks_;

    uint32 Probe_interval_; // in ms
    uint32 probe_timeout_;

    uint32 Gossip_interval_;
    uint32 Gossip_node_num_;

};
typedef std::shared_ptr<config> confptr;
}
#endif //CPPGOSSIP_CONFIG_H
