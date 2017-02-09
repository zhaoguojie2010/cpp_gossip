//
// Created by bloodstone on 17/2/7.
//

#ifndef CPPGOSSIP_HYBRID_SERVER_HPP
#define CPPGOSSIP_HYBRID_SERVER_HPP


#include <set>
#include <thread>
#include <vector>
#include "asio.hpp"
#include "src/ticker.hpp"
#include "src/network/tcp/runner.hpp"
#include "src/network/udp/runner.hpp"
#include "src/types.hpp"

namespace gossip {

using asio::ip::tcp;
using asio::ip::udp;


// hybridRunner consists of:
// 1. tcp server
// 2. udp broadcaster
//    NOTE: it's not real udp broadcast, instead, it sends
//    multiple identical udp packets to different peers
// 3. udp server
// 4. ticker
class HybridRunner {
public:
    HybridRunner(short port, ::gossip::tcp::header_handler handle_header,
                 ::gossip::tcp::body_handler handle_body, uint32 header_size,
                 ::gossip::udp::packet_handler handle_packet)
    : io_service_(),
      tcp_svr_(port, handle_header,
               handle_body, header_size, io_service_),
      udp_svr_(port, handle_packet, io_service_) {

    }

    void AddTicker(uint32 interval, typename Ticker::callback cb) {
        auto ticker = new AsioTicker(io_service_);
        ticker->Tick(interval, cb);
        tickers_.push_back(ticker);
    }

    void Run() {
        // start the tcp server
        tcp_svr_.Start();
        // start the udp server
        udp_svr_.Start();


        std::thread ([this]() {
            io_service_.run();
        }).detach();
    }

    asio::io_service* GetIoSvc() {
        return &io_service_;
    }

    ~HybridRunner() {
        std::for_each(tickers_.begin(), tickers_.end(), [](Ticker* t) {
            delete t;
        });
    }

private:
    asio::io_service io_service_;
    ::gossip::tcp::Server tcp_svr_;
    ::gossip::udp::Server udp_svr_;

    std::vector<Ticker*> tickers_;
};

}

#endif //CPPGOSSIP_HYBRID_SERVER_HPP
