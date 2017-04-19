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
#include "src/node.hpp"
#include "src/utils.hpp"
#include "src/message/header.hpp"
#include "src/message/message_generated.h"
#include "src/hybrid_runner.hpp"
#include "src/logger.hpp"
#include "src/broadcast.hpp"
#include "thirdparty/asio.hpp"
#include "thirdparty/asio/steady_timer.hpp"
#include "src/network/udp/async_client.hpp"
#include "src/network/tcp/blocking_client.hpp"

namespace gossip {

template<int THREAD_NUM>
class gossiper {
public:
    typedef std::shared_ptr<node_state> node_state_ptr;
    typedef std::shared_ptr<suspicion> suspicion_ptr;
    typedef std::function<void(std::string)> event_notifier;

public:
    ~gossiper() {
        std::cout << "~gossiper\n";
    }

    gossiper(config& conf)
    : conf_(conf),
      seq_num_(0),
      dominant_(0),
      node_num_(0),
      MAX_APPENDED_MSG_(10),
      notify_join_(nullptr),
      notify_leave_(nullptr),
      is_leaving_(false),
      hybrid_runner_(conf.Port_,
                     std::bind(&gossiper::handleHeader, this, std::placeholders::_1,
                               std::placeholders::_2, std::placeholders::_3),
                     std::bind(&gossiper::handleBody, this, std::placeholders::_1,
                               std::placeholders::_2, std::placeholders::_3,
                               std::placeholders::_4, std::placeholders::_5),
                     message::HEADER_SIZE,
                     std::bind(&gossiper::handlePacket, this, std::placeholders::_1,
                               std::placeholders::_2, std::placeholders::_3,
                               std::placeholders::_4)) {
        //node_num_.store(0, std::memory_order_relaxed);
    }

    // sync with peer and start the probe and gossip routine
    // peer example: 192.168.1.39:29011
    void Join(const std::string &peer) {
        auto vec = Split(peer, ":");
        if (vec.size() != 2) {
            logger->error("invalid peer: {}", peer);
            return;
        }
        std::string host = vec[0];
        std::string port = vec[1];

        // create node state
        node_state a;
        a.Name_ = conf_.Name_;
        a.IP_ = conf_.Addr_;
        a.Port_ = std::to_string(conf_.Port_);
        a.Dominant_ = nextDominant();
        a.State_ = message::STATE_ALIVE;
        aliveNode(a, false);

        if (host != conf_.Addr_ || std::to_string(conf_.Port_) != port) {
            // sync state
            logger->debug("sync state...");
            try {
                syncState(host, port);
            } catch (const std::system_error &e) {
                logger->error("{0} ", e.what());
                logger->error("{}", e.code().message());
            }
        }

        alive();
    }

    // register event handler
    // WARNING: make sure that neither notify_join or notify_leave
    // will block, and since they runs in a separate thread, make
    // sure they are thread-safe
    // TODO: if they do block, create a thread pool for gossiping
    void RegisterNotifier(event_notifier notify_join,
        event_notifier notify_leave) {
        notify_join_ = notify_join;
        notify_leave_ = notify_leave;
    }

    // GetAliveNodes
    std::vector<std::string> GetAliveNodes() {
        node_lock_.lock();
        auto rst = nodes_;
        node_lock_.unlock();
        return std::move(rst);
    }

    // Leave

private:
    // alive starts the random probe & gossip routine in a
    // new thread
    bool alive() {
        // randomly probe every 1 sec
        hybrid_runner_.AddTicker(conf_.Probe_interval_, std::bind(&gossiper::probe, this));
        // gossip every 1 sec
        hybrid_runner_.AddTicker(conf_.Gossip_interval_, std::bind(&gossiper::gossip, this));
        hybrid_runner_.Run(THREAD_NUM);
        return true;
    }

    void aliveNode(const node_state &a, bool need_bc) {
        const std::string alive_node_name = a.Name_;
        if (is_leaving_ && alive_node_name == conf_.Name_) {
            return;
        }

        auto state = getNodeState(alive_node_name);
        bool need_notify = false;
        // if we've never seen this node, then create one and
        // add it to node_map_
        if (state == nullptr) {
            state = std::make_shared<node_state>(a);
            state->Dominant_ = 0;
            state->State_ = message::STATE_DEAD;
        }

        if (a.Dominant_ <= state->Dominant_) {
            return;
        }

        // if node come back alive from dead or it's a new alive node,
        // 1. do the notification
        // 2. add the node
        if (state->State_ == message::STATE_DEAD) {
            need_notify = true;
            addNodeState(alive_node_name, state);
        }

        updateNodeState(alive_node_name, a);

        // clear suspicion if any
        suspicion_lock_.lock();
        suspicions_.erase(alive_node_name);
        suspicion_lock_.unlock();

        if (need_bc) {
            //logger->info("push alive node:{} state:{}, domi:{}", alive_node_name, a.State_, a.Dominant_);
            //logger->info("updated node:{} state:{}, domi:{}", alive_node_name, node_map_[alive_node_name]->State_, node_map_[alive_node_name]->Dominant_);
            node_state alive(a);
            bc_queue_.Push(std::make_shared<node_state>(std::move(alive)),
                           node_num_.load(std::memory_order_relaxed));
        }

        if (alive_node_name == conf_.Name_) {
            dominant_ = a.Dominant_;
        }


        // notify join
        if (need_notify && alive_node_name != conf_.Name_ && notify_join_ != nullptr) {
            notify_join_(alive_node_name);
        }
    }

    void suspectNode(const node_state &s) {
        auto suspect_node_name = s.Name_;
        auto state = getNodeState(suspect_node_name);

        if (state == nullptr || s.Dominant_ < state->Dominant_) {
            return;
        }

        //logger->info("suspecting node {0}, dominant = {1}", s.Name_, state->Dominant_);
        std::function<void()> broadcastSuspect = [this, &s]() {
            node_state sus(s);
            bc_queue_.Push(std::make_shared<node_state>(std::move(sus)),
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
                                              convict, hybrid_runner_.GetIoSvc(), 1000);
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

        // if it's us, object
        if (dead_node_name == conf_.Name_ && !is_leaving_) {
            fuckyou(d.Dominant_);
            return;
        }

        updateNodeState(dead_node_name, d);
        removeNodeState(dead_node_name);

        // start to broadcast
        node_state dead(d);
        bc_queue_.Push(std::make_shared<node_state>(std::move(dead)),
                       node_num_.load(std::memory_order_relaxed));

        // notify leave
        if (dead_node_name != conf_.Name_ && notify_leave_ != nullptr) {
            notify_leave_(dead_node_name);
        }
    }

    // probe randomly ping one known node via udp
    void probe() {
        auto candi = roundrobinNode(1);
        //logger->info("prepare to probe node: {0}", candi[0]);
        auto node = getNodeState(candi[0]);
        if (node->State_ == message::STATE_ALIVE &&
            node->Name_ != conf_.Name_) {
            probeNode(*node);
        }
    }

    void probeNode(const node_state &node) {
        std::size_t mtu = 1460;
        std::string host = node.IP_;
        short port = std::stoi(node.Port_);
        auto client = std::make_shared<udp::AsyncClient>(*hybrid_runner_.GetIoSvc());
        uint64_t seq_num = nextSeqNum();
        // Waterfall defines the series of behaviors after ping packets
        // is sent
        client->Waterfall(
            // if ping finish, start to receive pong
            std::bind(&udp::AsyncClient::AsyncReceiveFrom,
                      client, host, port, conf_.Probe_timeout_),
            // if sending ping timeout, just give up
            nullptr,
            // if pong is received, check if it's valid
            [this,seq_num](char *resp, std::size_t size) {
                message::Header header;
                message::DecodeHeader(reinterpret_cast<uint8_t*>(resp), header);
                if (header.Type_ != message::TYPE_PONG) {
                    logger->error("wrong ack type");
                    return;
                }
                auto pong = flatbuffers::GetRoot<message::Pong>(resp+message::HEADER_SIZE);
                if (pong->seqNo() != seq_num) {
                    logger->error("mismatched ping ack seqNo");
                }

                // check if dead alert is appended
                if (size > message::HEADER_SIZE+header.Body_length_) {
                    auto dead_node_state = flatbuffers::GetRoot<message::NodeState>(
                        resp+message::HEADER_SIZE+header.Body_length_);
                    auto name = dead_node_state->node()->name()->str();
                    auto state = dead_node_state->state();
                    if (name == conf_.Name_ && state == message::STATE_DEAD) {
                        // there is a dead alert appended, object
                        fuckyou(dead_node_state->dominant());
                    }
                }
            },
            // TODO: if pong timeout, start sending indirect ping packets
            // std::bind(&gossiper::indirectPing, this)
            // right now, if we didn't get pong in time, just treat the target
            // node as a suspect
            std::bind(&gossiper::suspectNode, this, node)
        );

        uint8_t send_buff[mtu];
        int size = generatePing(seq_num, send_buff, mtu);

        // send ping
        client->AsyncSendTo(reinterpret_cast<char*>(send_buff),
                            size, host, port, conf_.Probe_timeout_);
    }

    int generatePing(uint64_t seqNo, uint8_t *send_buff, int send_buff_size) {
        //  generate ping
        flatbuffers::FlatBufferBuilder builder(1024);
        auto from = builder.CreateString(conf_.Name_);
        auto ping = message::CreatePing(builder, seqNo, from);
        builder.Finish(ping);
        uint8_t *buff = builder.GetBufferPointer();
        int size = builder.GetSize();
        message::Header header;
        header.Type_ = message::TYPE_PING;
        header.Body_length_ = size;
        message::EncodeHeader(send_buff, header);
        if (message::HEADER_SIZE+size > send_buff_size) {
            std::cerr << "ping packet too large, body len = " << size << std::endl;
            return -1;
        }
        std::memcpy(send_buff+message::HEADER_SIZE, buff, size);

        return message::HEADER_SIZE + size + appendGossipMsg(
            send_buff+message::HEADER_SIZE+size, send_buff_size-size-message::HEADER_SIZE);
    }

    int appendGossipMsg(uint8_t *buff, int buff_size) {
        int default_mgs_num = MAX_APPENDED_MSG_;
        flatbuffers::FlatBufferBuilder builder(1024);
        std::vector<flatbuffers::Offset<message::NodeState>> ns_vec;
        while (default_mgs_num > 0) {
            auto node = bc_queue_.Peek();
            if (node == nullptr)
                break;
            //logger->info("append port = {}, name = {}, state = {}, distinct = {}, dominant = {}", node->Port_, node->Name_, node->State_, bc_queue_.Distinct(), node->Dominant_);
            auto n = message::CreateNode(builder, builder.CreateString(node->Name_),
                                            builder.CreateString(node->IP_),
                                            std::stoi(node->Port_));
            auto ns = message::CreateNodeState(
                builder, n, node->State_, node->Dominant_,
                builder.CreateString(node->From_), node->Timestamp_);
            ns_vec.push_back(ns);
            --default_mgs_num;
        }
        
        if (default_mgs_num == MAX_APPENDED_MSG_) {
            return 0;
        }

        auto nss = builder.CreateVector(ns_vec);
        auto gossipMsg = message::CreateNodeStates(builder, nss);
        builder.Finish(gossipMsg);
        int size = builder.GetSize();
        if (size > buff_size) {
            // fbs size too large, give up sending peeked msg and
            // shrink the MAX_APPENDED_MSG_
            bc_queue_.ResetPeek();
            shrinkMaxAppendedMsg();
            return 0;
        }
        // now pop the peeked msg
        bc_queue_.ApplyPeek();
        uint8_t *body = builder.GetBufferPointer();
        std::memcpy(buff, body, size);
        return size;
    }

    // Sometimes a node fails to join a cluster. This typically happens when a node
    // restart immediately after it crashes/shutdown and the seed node already
    // sends a ping to the current node and is waiting for the pong. At this point,
    // the seed node hasn't realized that the node leaved the cluster for a while. So
    // when the pong finally timed out, the other nodes in the cluster will simply
    // mark this node as dead, while this node thinks it joined the cluster successfully.
    // To prevent this, when current node received a ping from other node and the local
    // node state of the other node is dead, the current node append a dead alter to the
    // pong so that the other node has a chance to object.
    int appendDeadAlert(node_state_ptr node, uint8_t *buff, int buff_size) {
        flatbuffers::FlatBufferBuilder builder(1024);
        auto n = message::CreateNode(builder, builder.CreateString(node->Name_),
                                     builder.CreateString(node->IP_),
                                     std::stoi(node->Port_));
        auto ns = message::CreateNodeState(
            builder, n, node->State_, node->Dominant_,
            builder.CreateString(node->From_), node->Timestamp_);
        builder.Finish(ns);
        int size = builder.GetSize();
        if (size > buff_size) {
            return 0;
        }
        uint8_t *body = builder.GetBufferPointer();
        std::memcpy(buff, body, size);
        return size;
    }

    void indirectPing() {

    }

    // broadcast local state to other nodes via udp
    void gossip() {
        logger->debug("start to gossip...\n");
    }

    // convert flatbuffers node state to internal node state
    void convert(node_state &to, const message::NodeState &from) {
        to.Name_ = from.node()->name()->str();
        to.IP_ = from.node()->ip()->str();
        to.Port_ = std::to_string(from.node()->port());
        to.Dominant_ = from.dominant();
        to.State_ = from.state();
        to.From_ = from.from()->str();
        to.Timestamp_ = from.timeStamp();
    }

    node_state&& convert(const message::NodeState &from) {
        node_state tmp;
        tmp.Name_ = from.node()->name()->str();
        tmp.IP_ = from.node()->ip()->str();
        tmp.Port_ = std::to_string(from.node()->port());
        tmp.Dominant_ = from.dominant();
        tmp.State_ = from.state();
        tmp.From_ = from.from()->str();
        tmp.Timestamp_ = from.timeStamp();
        return std::move(tmp);
    }

    // merge state with remote node via tcp
    void mergeStates(const message::NodeStates *remote_states, bool need_bc) {
        auto nss = remote_states->nodes();
        auto len = nss->Length();
        for(int i=0; i<len; ++i) {
            auto ns = nss->Get(i);
            //std::cout << "merging " << ns->node()->name()->str() << " state: " << ns->state() << std::endl;
            node_state state(convert(*ns));
            switch (state.State_) {
                case message::STATE_ALIVE:
                    aliveNode(state, need_bc);
                    break;
                case message::STATE_SUSPECT:
                    suspectNode(state);
                    break;
                case message::STATE_DEAD:
                    deadNode(state);
                    break;
                default:
                std::cerr << "merging wrong state: " << state.State_ << std::endl;
                    break;
            }
        }
        //printNodeState();
    }

    void printNodeState() {
        node_lock_.lock();
        std::cout << "node states: ";
        for(auto it=node_map_.begin(); it!=node_map_.end(); it++) {
            std::cout << it->first << " ";
        }
        std::cout << std::endl;
        node_lock_.unlock();
    }

    std::pair<uint8_t*, int> encodeLocalState(flatbuffers::FlatBufferBuilder &builder) {
        // generate local state
        std::vector<flatbuffers::Offset<message::NodeState>> ns_vec;
        node_lock_.lock();
        for (auto it = node_map_.begin(); it != node_map_.end(); it++) {
            auto node = message::CreateNode(builder, builder.CreateString(it->second->Name_),
                                            builder.CreateString(it->second->IP_),
                                            std::stoi(it->second->Port_));
            auto ns = message::CreateNodeState(
                builder, node, it->second->State_, it->second->Dominant_,
                builder.CreateString(it->second->From_), it->second->Timestamp_);
            ns_vec.push_back(ns);
        }
        node_lock_.unlock();
        auto nss = builder.CreateVector(ns_vec);
        auto local_states = message::CreateNodeStates(builder, nss);
        builder.Finish(local_states);
        uint8_t *body = builder.GetBufferPointer();
        int size = builder.GetSize();
        return std::make_pair(body, size);
    };

    void syncState(const std::string &host, const std::string &port) {
        tcp::BlockingClient client;
        client.Connect(host, port, conf_.Sync_state_timeout_);
        // generate local state
        flatbuffers::FlatBufferBuilder builder(2048);
        auto p = encodeLocalState(builder);
        auto body = p.first;
        auto size = p.second;

        uint8_t buff[message::HEADER_SIZE+size];
        message::Header header;
        header.Type_ = message::TYPE_SYNCSTATE;
        header.Body_length_ = size;
        message::EncodeHeader(buff, header);
        std::memcpy(buff+message::HEADER_SIZE, body, size);
        client.Write(reinterpret_cast<char*>(buff),
                     message::HEADER_SIZE+size, conf_.Sync_state_timeout_);

        // read header first
        client.ReadFull(reinterpret_cast<char*>(buff),
                        message::HEADER_SIZE, conf_.Sync_state_timeout_);
        message::DecodeHeader(buff, header);

        // read remote states body
        char resp[header.Body_length_];
        client.ReadFull(resp, header.Body_length_, conf_.Sync_state_timeout_);
        auto remote_states = message::GetNodeStates(resp);

        //merge
        mergeStates(remote_states, false);
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
        node_lock_.lock();
        int num = nodes_.size();
        for (int i = 0; i < num; i++) {
            if (nodes_[i].compare(node_name) == 0) {
                std::swap(nodes_[i], nodes_[num-1]);
                nodes_.pop_back();
                node_num_.fetch_sub(1, std::memory_order_relaxed);
                break;
            }
        }
        // keep the dead in the node_map_ so that when it come back
        // alive it could issue an objection to update its dominant to
        // prevent flipping join/leave notifications.
        //node_map_.erase(node_name);
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
        //logger->info("fuck i'm alive");
        node_state alive;
        alive.Name_ = conf_.Name_;
        alive.IP_ = conf_.Addr_;
        alive.Port_ = std::to_string(conf_.Port_);
        alive.State_ = message::STATE_ALIVE;

        auto d = nextDominant();
        if (dominant >= d) {
            d = nextDominant(dominant-d+1);
        }
        alive.Dominant_ = d;

        // broadcast
        bc_queue_.Push(std::make_shared<node_state>(std::move(alive)),
            node_num_.load(std::memory_order_relaxed));
    }

    std::vector<std::string> roundrobinNode(int num) {
        std::vector<std::string> rst;
        static int cursor;

        node_lock_.lock();
        int size = nodes_.size();
        if (nodes_.size() <= num) {
            rst = nodes_;
        } else {
            for (int i = 0; i < num; i++) {
                rst.push_back(nodes_[(cursor+i)%size]);
            }
            cursor = (cursor + num)%size;
        }
        node_lock_.unlock();
        return rst;
    }

    void handleHeader(char* buff, std::size_t size,
                      message::Header &header) {
        if (size != message::HEADER_SIZE) {
            logger->error("invalid header size: {}", message::HEADER_SIZE);
            return;
        }
        message::DecodeHeader(reinterpret_cast<uint8_t *>(buff), header);
    }

    std::size_t handleBody(uint32_t type,
                           char *buff, std::size_t size,
                           char *resp_buff, std::size_t resp_size) {
        std::size_t result;
        uint8_t *body;
        message::Header header;
        switch (type) {
            case message::TYPE_PING: {
                auto ping = flatbuffers::GetRoot<message::Ping>(buff);
                flatbuffers::FlatBufferBuilder builder(1024);
                auto ack = message::CreatePong(builder, ping->seqNo());
                builder.Finish(ack);
                body = builder.GetBufferPointer();
                result = builder.GetSize();
                header.Type_ = message::TYPE_PONG;

                break;
            }
            case message::TYPE_INDIRECTPING: {
                auto indirect_ping = flatbuffers::GetRoot<message::IndirectPing>(buff);
                break;
            }
            case message::TYPE_SYNCSTATE: {
                auto remote_states = message::GetNodeStates(buff);
                mergeStates(remote_states, true);
                flatbuffers::FlatBufferBuilder builder(2048);
                auto p = encodeLocalState(builder);
                body = p.first;
                result = p.second;
                header.Type_ = message::TYPE_SYNCSTATE;
                break;
            }
            default:
                ;
        }
        if (result > resp_size) {
            result = -1;
        } else {
            header.Body_length_ = result;
            message::EncodeHeader(reinterpret_cast<uint8_t*>(resp_buff), header);
            std::memcpy(resp_buff+message::HEADER_SIZE, body, result);
        }
        return result+message::HEADER_SIZE;
    }

    int handlePacket(char *buff, std::size_t size,
                     char *resp_buff, std::size_t resp_size) {
        int rst = 0;
        message::Header header;
        message::DecodeHeader(reinterpret_cast<uint8_t*>(buff), header);
        switch (header.Type_) {
            case message::TYPE_PING: {
                auto ping = flatbuffers::GetRoot<message::Ping>(buff+message::HEADER_SIZE);
                auto ns = getNodeState(ping->from()->str());
                if (header.Body_length_+message::HEADER_SIZE < size) {
                    // there's gossip msg appended to the ping msg
                    auto gossipMsg = message::GetNodeStates(buff+message::HEADER_SIZE+header.Body_length_);
                    mergeStates(gossipMsg, true);
                }
                // generate pong
                flatbuffers::FlatBufferBuilder builder;
                auto ack = message::CreatePong(builder, ping->seqNo());
                builder.Finish(ack);
                uint8_t *tmp_buff = builder.GetBufferPointer();
                int size = builder.GetSize();
                if (message::HEADER_SIZE+size > resp_size) {
                    rst = -1;
                    break;
                }
                header.Type_ = message::TYPE_PONG;
                header.Body_length_ = size;
                message::EncodeHeader(reinterpret_cast<uint8_t*>(resp_buff), header);
                std::memcpy(resp_buff+message::HEADER_SIZE, tmp_buff, size);
                rst = size + message::HEADER_SIZE;

                // append dead alert if needed
                if (ns != nullptr && ns->State_ == message::STATE_DEAD) {
                    int append_size = appendDeadAlert(ns, reinterpret_cast<uint8_t*>(resp_buff)+rst, resp_size-rst);
                    rst += append_size;
                }
                break;
            }
            case message::TYPE_INDIRECTPING: {
                break;
            }
            default: {
                logger->error("invalid diagram type");
                break;
            }
        }
        return rst;
    }

    void shrinkMaxAppendedMsg() {
        if (MAX_APPENDED_MSG_ == 1) {
            logger->error("appened msg too large, cannot fit in udp packets");
        } else {
            MAX_APPENDED_MSG_ /= 2;
        }
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

    BroadcastQueue bc_queue_;
    int MAX_APPENDED_MSG_;

    // callback functions gossiper calls when node join/leave the cluster
    event_notifier notify_join_;
    event_notifier notify_leave_;
};
}

#endif //CPPGOSSIP_GOSSIPER_H
