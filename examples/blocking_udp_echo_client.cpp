//
// blocking_udp_echo_client.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2016 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdlib>
#include <cstring>
#include <iostream>
#include "asio.hpp"
#include "src/config.hpp"
#include "src/suspicion.hpp"
#include "src/node.hpp"
#include "src/utils.hpp"
#include "src/message/header.hpp"
#include "src/message/message_generated.h"
#include "src/hybrid_runner.hpp"
#include "src/logger.hpp"
#include "src/broadcast.hpp"
#include "thirdparty/asio/include/asio.hpp"
#include "thirdparty/asio/include/asio/steady_timer.hpp"
#include "src/network/udp/async_client.hpp"
#include "src/network/tcp/blocking_client.hpp"

using asio::ip::udp;

enum { max_length = 1024 };

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 3)
    {
      std::cerr << "Usage: blocking_udp_echo_client <host> <port>\n";
      return 1;
    }

    asio::io_service io_service;

    udp::socket s(io_service, udp::endpoint(udp::v4(), 0));

    udp::resolver resolver(io_service);
    udp::endpoint endpoint = *resolver.resolve({udp::v4(), argv[1], argv[2]});
    const int mtu = 1460;


    flatbuffers::FlatBufferBuilder builder(1024);
    auto from = builder.CreateString("localhost");
    auto ping = gossip::message::CreatePing(builder, 1, from);
    builder.Finish(ping);
    uint8_t *buff = builder.GetBufferPointer();
    int size = builder.GetSize();
    gossip::message::Header header;
    header.Type_ = gossip::message::TYPE_PING;
    header.Body_length_ = size;
    uint8_t send_buff[mtu];
    gossip::message::EncodeHeader(send_buff, header);
    if (gossip::message::HEADER_SIZE+size > mtu) {
        std::cerr << "udb packet too large, body len = " << size << std::endl;
        return -1;
    }
    std::memcpy(send_buff+gossip::message::HEADER_SIZE, buff, size);

    std::cout << "send upd packet " << size+gossip::message::HEADER_SIZE << std::endl;
    s.send_to(asio::buffer(reinterpret_cast<char*>(send_buff), size+gossip::message::HEADER_SIZE), endpoint);

    char reply[1460];
    udp::endpoint sender_endpoint;
    size_t reply_length = s.receive_from(
        asio::buffer(reply, max_length), sender_endpoint);
    gossip::message::DecodeHeader(reinterpret_cast<uint8_t*>(reply), header);
    std::cout << "header length = " << header.Body_length_ << std::endl;
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
