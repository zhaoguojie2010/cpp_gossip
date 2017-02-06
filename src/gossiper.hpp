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

#include "src/config.hpp"
#include "src/suspicion.hpp"
#include "src/handler.hpp"
#include "src/node.hpp"
#include "src/message/message_generated.h"
#include "src/network/tcp/runner.hpp"
#include "src/broadcast.hpp"
#include "thirdparty/asio/include/asio.hpp"
#include "thirdparty/asio/include/asio/steady_timer.hpp"

namespace gossip {

class gossiper {
public:
    gossiper(config& conf)
    : conf_(conf),
      seq_num_(0),
      dominant_(0),
      node_num_(0),
      is_leaving_(false),
      tcp_runner_() {
        tcp_runner_.PrepareServer(conf.Port_, handle_header, handle_body, 0); // TODO: get header size
        setAlive();
    }

    // Join randomly choose one node of the peers and sync state
    // with it.
    // Returned value: indicates how many members the cluster has
    // including ourselves when it's gt 0.
    // Join failed if it returns 0
    // peer example: 192.168.1.39:29011
    int Join(std::vector<std::string> &peers) {
        // select a random peer
        std::srand(std::time(0));
        const std::string &seed = peers[std::rand()%peers.size()];
        return 0;
    }

    // GetAliveNodes

    // Leave

public:
    typedef std::shared_ptr<node_state> node_state_ptr;
    typedef std::shared_ptr<suspicion> suspicion_ptr;

private:
    // setAlive starts the random probe & gossip routine in a
    // new thread
    bool setAlive() {
        node_state alive;
        alive.Name_ = conf_.Name_;
        alive.IP_ = conf_.Addr_;
        alive.Port_ = conf_.Port_;
        alive.Dominant_ = nextDominant();
        aliveNode(alive, true);
        return true;
    }

    void aliveNode(const node_state &a, bool bootstrap) {
        const std::string alive_node_name = a.Name_;
        if (is_leaving_ && alive_node_name == conf_.Name_) {
            return;
        }

        auto state = getNodeState(alive_node_name);
        // if we've never seen this node, then create one and
        // add it to node_map_
        if (state == nullptr) {
            state = std::make_shared<node_state>(a);

            setNodeState(alive_node_name, state);
            node_num_.fetch_add(1, std::memory_order_relaxed);

            // notify join
            // TODO:
        }

        if (a.Dominant_ <= state->Dominant_) {
            return;
        }

        updateNodeState(state, a);

        // clear suspicion if any
        suspicion_lock_.lock();
        suspicions_.erase(alive_node_name);
        suspicion_lock_.unlock();

        // if it's about us and this is not a initialization, then update
        // ourselves
        if (!bootstrap && alive_node_name == state->Name_) {
            dominant_ = state->Dominant_;
        } else {
            // it's not about us or we just init ourselves, start broadcasting
            node_state alive(a);
            bc_queue_.push(std::make_shared<node_state>(std::move(alive)),
                           node_num_.load(std::memory_order_relaxed));
        }
    }

    void suspectNode(const node_state &s) {
        auto suspect_node_name = s.Name_;
        auto state = getNodeState(suspect_node_name);

        if (state == nullptr || s.Dominant_ < state->Dominant_) {
            return;
        }

        std::function<void()> broadcastSuspect = [this, &s]() {
            node_state sus(s);
            bc_queue_.push(std::make_shared<node_state>(std::move(sus)),
                           node_num_.load(std::memory_order_relaxed));
        };

        auto sus = getSuspicion(suspect_node_name);
        if (sus != nullptr) {
            // if the node has already been a suspect, try to verify if
            // it's dead
            if (!sus->Confirm(s.From_)) {
                // it's not been confirmed to be dead, so just gossip it
                broadcastSuspect();
            }
            return;
        }

        // if the state is dead, just ignore it. if it's suspect,
        // the suspicion must have been created, so ignore it too
        if (state->State_ != message::STATE_ALIVE) {
            return;
        }

        if (conf_.Name_ == suspect_node_name) {
            // if it's us, issue a objection
            fuckyou(s.Dominant_);
        } else {
            // otherwise, just gossip it
            broadcastSuspect();
        }

        updateNodeState(state, s);

        // set up suspicion
        node_lock_.lock();
        auto node_num = node_map_.size();
        node_lock_.unlock();

        suspicion::callback convict = [this, &s]() {
            node_state d(s);
            d.State_ = message::STATE_DEAD;
            deadNode(d);
        };
        sus = std::make_shared<suspicion>(node_num, convict, tcp_runner_.GetIoSvc(), 2000);
        suspicion_lock_.lock();
        suspicions_[suspect_node_name] = sus;
        suspicion_lock_.unlock();
    }

    void deadNode(const node_state &d) {
        auto dead_node_name = d.Name_;
        auto state = getNodeState(dead_node_name);
        if (state == nullptr || d.Dominant_ < state->Dominant_) {
            return;
        }

        suspicion_lock_.lock();
        suspicions_.erase(dead_node_name);
        suspicion_lock_.unlock();
        node_num_.fetch_sub(1, std::memory_order_relaxed);

        if (state->State_ == message::STATE_DEAD) {
            return;
        }

        // if it's us, object
        if (dead_node_name == conf_.Name_ && !is_leaving_) {
            fuckyou(d.Dominant_);
            return;
        }
        // start to broadcast
        node_state dead(d);
        bc_queue_.push(std::make_shared<node_state>(std::move(dead)),
                       node_num_.load(std::memory_order_relaxed));

        // notify leave
        // TODO:
    }

    // probe randomly ping one known node via udp
    void probe() {

    }

    // broadcast local state to other nodes via udp
    void gossip() {

    }

    // sync state with remote node via tcp
    void syncState(const message::Node &remote_node) {

    }

    uint64 nextDominant(uint64 shift = 1) {
        return dominant_.fetch_add(shift, std::memory_order_relaxed) + shift;
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


    void updateNodeState(node_state_ptr to, const node_state &from) {
        node_lock_.lock();
        *to = from;
        to->Timestamp_ = 0; // TODO: get timestamp
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

    // object if we're accused of being suspect or dead
    void fuckyou(uint64 dominant) {
        node_state alive;
        alive.Name_ = conf_.Name_;
        alive.IP_ = conf_.Addr_;
        alive.Port_ = conf_.Port_;
        alive.State_ = message::STATE_ALIVE;

        auto d = nextDominant();
        if (dominant >= d) {
            d = nextDominant(dominant-d+1);
        }
        alive.Dominant_ = d;

        // broadcast
        bc_queue_.push(std::make_shared<node_state>(std::move(alive)),
            node_num_.load(std::memory_order_relaxed));
    }
private:
    config& conf_;

    std::atomic<uint64> seq_num_; // typically use for ping
    std::atomic<uint64> dominant_;
    bool is_leaving_;

    //udpSvcPtr udp_svc_;
    TcpRunner tcp_runner_;

    std::mutex node_lock_;
    std::unordered_map<std::string, node_state_ptr> node_map_;
    std::atomic<uint32> node_num_;

    std::mutex suspicion_lock_;
    std::unordered_map<std::string, suspicion_ptr> suspicions_;

    broadcast_queue bc_queue_;
};
}

#endif //CPPGOSSIP_GOSSIPER_H
