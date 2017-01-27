//
// Created by bloodstone on 17/1/23.
//

#ifndef CPPGOSSIP_GOSSIPER_H
#define CPPGOSSIP_GOSSIPER_H

#include <vector>
#include <mutex>
#include <unordered_map>
#include <queue>
#include <atomic>
#include <string>
#include <ctime>
#include <cstdlib>

#include "config.hpp"
#include "src/suspicion.hpp"
#include "src/message/message.pb.h"
#include "src/network/tcp/server.hpp"
#include "thirdparty/asio/include/asio.hpp"
#include "thirdparty/asio/include/asio/steady_timer.hpp"
//#include "broadcast.h"

namespace gossip {

class gossiper {
public:
    gossiper(config& conf)
    : conf_(conf),
      seq_num_(0),
      dominant_(0),
      is_leaving_(false),
      tcp_svc_(conf.Port_) {
        setAlive();
    }

    // Join randomly choose one node of the peers and sync state
    // with it.
    // Returned value: indicates how many members the cluster has
    // including ourselves when it's gt 0.
    // Join failed if it returns 0
    int Join(std::vector<std::string> &peers) {
        // select a random peer
        std::srand(std::time(0));
        const std::string &seed = peers[std::rand()%peers.size()];
        return 0;
    }

    // GetAliveNodes

    // Leave

public:
    typedef std::shared_ptr<message::nodeState> node_state_ptr;
    typedef std::shared_ptr<asio::steady_timer> timer_ptr;
    typedef std::shared_ptr<suspicion<timer_ptr>> suspicion_ptr;

private:
    // setAlive starts the random probe & gossip routine in a
    // new thread
    bool setAlive() {
        message::alive alive;
        auto node = alive.mutable_node();
        node->set_ip(conf_.Addr_);
        node->set_port(conf_.Port_);
        node->set_name(conf_.Addr_ + std::to_string(conf_.Port_));
        alive.set_dominant(nextDominant());
        aliveNode(alive, true);
        return true;
    }

    void aliveNode(const message::alive& a, bool bootstrap) {
        const std::string alive_node_name = a.node().name();
        if (is_leaving_ && alive_node_name == conf_.Name_) {
            return;
        }

        auto state = getNodeState(alive_node_name);
        // if we've never seen this node, then create one and
        // add it to node_map_
        if (state == nullptr) {
            state = std::make_shared<message::nodeState>();
            updateNodeState(state, a, message::ALIVE);

            setNodeState(alive_node_name, state);
        }

        if (a.dominant() <= state->dominant()) {
            return;
        }

        updateNodeState(state, a, message::ALIVE);

        // TODO: clear timer if any

        // if it's about us and this is not a initialization, then update
        // ourselves
        if (!bootstrap && alive_node_name == state->node().name()) {
            dominant_ = a.dominant();
        } else {
            // it's not about us or we just init ourselves, start broadcasting
            // TODO:
        }
    }

    void suspectNode(const message::suspect& s) {
        auto suspect_node_name = s.node().name();
        auto state = getNodeState(suspect_node_name);

        if (state == nullptr || s.dominant() < state->dominant()) {
            return;
        }

        auto suspicion = getSuspicion(suspect_node_name);
        if (suspicion != nullptr) {
            // if the node has already been a suspect, try to verify if
            // it's dead
            if (suspicion->Confirm(s.from())) {
                // it's not been confirmed to be dead, so just gossip it
                // TODO: enqueue
            }
            return;
        }

        // if the state is dead, just ignore it. if it's suspect,
        // the suspicion must have been created, so ignore it too
        if (state->state() != message::ALIVE) {
            return;
        }

        if (conf_.Name_ == suspect_node_name) {
            // if it's us, issue a objection
            fuckyou();
        } else {
            // otherwise, just gossip it
            // TODO:
        }
    }

    void deadNode(const message::dead& d) {

    }

    // probe randomly ping one known node via udp
    void probe() {

    }

    // broadcast local state to other nodes via udp
    void gossip() {

    }

    // sync state with remote node via tcp
    void syncState(const message::node &remote_node) {

    }

    uint64 nextDominant() {
        return dominant_.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    uint64 nextSeqNum() {
        return seq_num_.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    node_state_ptr getNodeState(std::string node_name) {
        node_state_ptr rst = nullptr;
        node_lock_.lock();
        auto search = node_map_.find(node_name);
        if (search != node_map_.end()) {
            rst = search->second;
        }
        node_lock_.unlock();
        return rst;
    }


    template<typename T>
    void updateNodeState(node_state_ptr to, const T &from, message::STATE state) {
        node_lock_.lock();
        auto mutable_node = to->mutable_node();
        mutable_node->set_name(from.node().name());
        mutable_node->set_ip(from.node().ip());
        mutable_node->set_port(from.node().port());
        to->set_state(state);
        to->set_dominant(from.dominant());
        to->set_timestamp(0); // TODO: get timestamp
        node_lock_.unlock();
    }

    void setNodeState(std::string node_name, node_state_ptr state) {
        node_lock_.lock();
        node_map_[node_name] = state;
        node_lock_.unlock();
    }

    suspicion_ptr getSuspicion(std::string node_name) {
        suspicion_ptr result = nullptr;
        suspicion_lock_.lock();
        auto search = suspicions_.find(node_name);
        if (search != suspicions_.end()) {
            result = search->second;
        }
        suspicion_lock_.unlock();
        return result;
    }

    timer_ptr getTimer() {
        auto io_svc = tcp_svc_.GetIoSvc();
        return std::make_shared<asio::steady_timer>(*io_svc);
    }

    // object if we're accused of suspect or dead
    void fuckyou() {

    }
private:
    config& conf_;

    std::atomic<uint64> seq_num_; // typically use for ping
    std::atomic<uint64> dominant_;
    bool is_leaving_;

    //udpSvcPtr udp_svc_;
    TcpSvr tcp_svc_;

    std::mutex node_lock_;
    std::unordered_map<std::string, node_state_ptr> node_map_;

    std::mutex suspicion_lock_;
    std::unordered_map<std::string, suspicion_ptr> suspicions_;

    //std::queue<broadcastMsg> bc_queue_;
};
}

#endif //CPPGOSSIP_GOSSIPER_H
