//
// Created by bloodstone on 17/1/23.
//

#ifndef CPPGOSSIP_GOSSIPER_H
#define CPPGOSSIP_GOSSIPER_H

#include <vector>
#include <mutex>
#include <unordered_map>
#include <queue>
#include <string>

#include "config.h"
#include "src/message/message.pb.h"
#include "src/network/tcp/server.h"
//#include "broadcast.h"

namespace gossip {

class gossiper {
public:
    gossiper(confptr);

    // Join randomly choose one node of the peers and sync state
    // with it.
    // Returned value: indicates how many members the cluster has
    // including ourselves when it's gt 0.
    // Join failed if it returns 0
    int Join(std::vector<std::string> &peers);

    // GetAliveNodes

    // Leave

public:
    // setAlive starts the random probe & gossip routine in a
    // new thread
    bool setAlive();

private:
    confptr p_cfg;

    uint64 seq_num_; // typically use for ping
    uint64 dominant_;
    bool is_leaving_;

    //udpSvcPtr p_udp_svc_;
    tcpSvrPtr p_tcp_svc_;

    std::mutex node_lock_;
    std::unordered_map<std::string, message::nodeState> node_map_;

    //std::queue<broadcastMsg> bc_queue_;
};
}

#endif //CPPGOSSIP_GOSSIPER_H
