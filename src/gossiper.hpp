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
#include <functional>
#include <ctime>
#include <cstdlib>

#include "src/config.hpp"
#include "src/suspicion.hpp"
#include "src/handler.hpp"
#include "src/node.hpp"
#include "src/message/header.hpp"
#include "src/message/message_generated.h"
#include "src/hybrid_runner.hpp"
#include "src/logger.hpp"
#include "src/broadcast.hpp"
#include "thirdparty/asio/include/asio.hpp"
#include "thirdparty/asio/include/asio/steady_timer.hpp"
#include "src/network/udp/async_client.hpp"

namespace gossip {

class gossiper {
public:
    gossiper(config& conf)
    : conf_(conf),
      seq_num_(0),
      dominant_(0),
      node_num_(0),
      is_leaving_(false),
      hybrid_runner_(conf.Port_, handle_header,
                     handle_body, message::HEADER_SIZE, handle_packet) {}

    // sync with the first available peer and call Alive()
    // Returned value: indicates how many members the cluster has
    // including ourselves when it's gt 0.
    // Join failed if it returns 0
    // peer example: 192.168.1.39:29011
    int AliveAndJoin(std::vector<std::string> &peers) {
        // select a random peer
        std::srand(std::time(0));
        const std::string &seed = peers[std::rand()%peers.size()];
        return 0;
    }

    // setAlive starts the random probe & gossip routine in a
    // new thread
    bool Alive() {
        node_state alive;
        alive.Name_ = conf_.Name_;
        alive.IP_ = conf_.Addr_;
        alive.Port_ = conf_.Port_;
        alive.Dominant_ = nextDominant();
        aliveNode(alive, true);

        // randomly probe every 1 sec
        hybrid_runner_.AddTicker(1000, std::bind(&gossiper::probe, this));
        // gossip every 1 sec
        hybrid_runner_.AddTicker(1000, std::bind(&gossiper::gossip, this));
        hybrid_runner_.Run();
        return true;
    }

    // GetAliveNodes

    // Leave

public:
    typedef std::shared_ptr<node_state> node_state_ptr;
    typedef std::shared_ptr<suspicion> suspicion_ptr;

private:
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
            state->Dominant_ = 0;
            state->State_ = message::STATE_DEAD;

            addNodeState(alive_node_name, state);

            // notify join
            // TODO:
        }

        if (a.Dominant_ <= state->Dominant_) {
            return;
        }

        updateNodeState(alive_node_name, a);

        // clear suspicion if any
        suspicion_lock_.lock();
        suspicions_.erase(alive_node_name);
        suspicion_lock_.unlock();

        // if it's about us and this is not a initialization, then update
        // ourselves
        if (!bootstrap && alive_node_name == conf_.Name_) {
            dominant_ = a.Dominant_;
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

        updateNodeState(suspect_node_name, s);

        // set up suspicion
        suspicion::callback convict = [this, &s]() {
            node_state d(s);
            d.State_ = message::STATE_DEAD;
            deadNode(d);
        };
        suspicion_lock_.lock();
        // make sure the suspicion is not created by other threads(if any)
        if (suspicions_.find(suspect_node_name) == suspicions_.end()) {
            sus = std::make_shared<suspicion>(node_num_.load(std::memory_order_relaxed),
                                              convict, hybrid_runner_.GetIoSvc(), 2000);
            suspicions_[suspect_node_name] = sus;
        }
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

        updateNodeState(dead_node_name, d);
        removeNodeState(dead_node_name);

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
        logger->info("start to probe...\n");
        auto candi = randomNode(1);
        auto node = getNodeState(candi[0]);
        if (node->State_ == message::STATE_ALIVE &&
            node->Name_ != conf_.Name_) {
            probeNode(*node);
        }
    }

    void probeNode(const node_state &node) {
        std::size_t mtu = 1460;
        char buff[mtu];
        std::string host = node.IP_;
        short port = std::stoi(node.Port_);
        auto client = std::make_shared<udp::AsyncClient>(*hybrid_runner_.GetIoSvc());

        // Waterfall defines the series of behaviors after ping packets
        // is sent
        client->Waterfall(
            // if ping finish, start to receive pong
            std::bind(&udp::AsyncClient::AsyncReceiveFrom,
                      client, host, port, 1000),
            // if sending ping timeout, just give up
            nullptr,
            // if pong is received, check if it's valid
            nullptr, // TODO: check if ack is valid
            // if pong timeout, start sending indirect ping packets
            nullptr
        );
        // TODO: get ping
        //int size = 10;
        //client->AsyncSendTo(buff, size, host, port, 1000);
    }

    // broadcast local state to other nodes via udp
    void gossip() {
        logger->info("start to gossip...\n");
    }

    // sync state with remote node via tcp
    void syncState(const message::Node &remote_node) {

    }

    uint64_t nextDominant(uint64_t shift = 1) {
        return dominant_.fetch_add(shift, std::memory_order_relaxed) + shift;
    }

    uint64_t nextSeqNum() {
        return seq_num_.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    // return a copy of node_state
    node_state_ptr getNodeState(std::string node_name) {
        node_state_ptr rst = nullptr;
        node_lock_.lock();
        auto search = node_map_.find(node_name);
        if (search != node_map_.end()) {
            auto tmp = *(search->second);
            rst = std::make_shared<node_state>(tmp);
        }
        node_lock_.unlock();
        return rst;
    }


    void updateNodeState(std::string target_node_name, const node_state &from) {
        node_lock_.lock();
        auto target = node_map_[target_node_name];
        *target = from;
        target->Timestamp_ = 0; // TODO: get timestamp
        node_lock_.unlock();
    }

    void addNodeState(const std::string &node_name, node_state_ptr state) {
        node_lock_.lock();
        node_map_[node_name] = state;
        nodes_.push_back(node_name);
        node_num_.fetch_add(1, std::memory_order_relaxed);
        node_lock_.unlock();
    }

    void removeNodeState(const std::string &node_name) {
        // when a node died, we don't remove it from the
        // node_map_ yet, so that it could the last dominant
        // when it joins the cluster again
        node_lock_.lock();
        int num = nodes_.size();
        for (int i = 0; i < num; i++) {
            if (nodes_[i].compare(node_name) == 0) {
                std::swap(nodes_[i], nodes_[num-1]);
                nodes_.pop_back();
            }
        }
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
    void fuckyou(uint64_t dominant) {
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

    std::vector<std::string> randomNode(int num) {
        std::vector<std::string> rst(num, "");
        std::srand(std::time(0));
        node_lock_.lock();
        int siz = nodes_.size();
        for (int i=0; i<num; i++) {
            rst[i] = nodes_[rand()%siz];
        }
        node_lock_.unlock();
        return rst;
    }
private:
    config& conf_;

    std::atomic<uint64_t> seq_num_; // typically use for ping
    std::atomic<uint64_t> dominant_;
    bool is_leaving_;

    HybridRunner hybrid_runner_;

    std::mutex node_lock_;
    std::unordered_map<std::string, node_state_ptr> node_map_;
    // used to randomly select nodes to gossip
    std::vector<std::string> nodes_;
    std::atomic<uint32_t> node_num_;

    std::mutex suspicion_lock_;
    std::unordered_map<std::string, suspicion_ptr> suspicions_;

    broadcast_queue bc_queue_;
};
}

#endif //CPPGOSSIP_GOSSIPER_H
