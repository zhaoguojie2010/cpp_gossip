//
// Created by bloodstone on 17/1/29.
//

#ifndef CPPGOSSIP_HANDLER_HPP
#define CPPGOSSIP_HANDLER_HPP

#include "src/message/header.hpp"
#include "src/logger.hpp"
#include "src/message/message_generated.h"
#include "flatbuffers/flatbuffers.h"
#include "thirdparty/portable_endian.h"

namespace gossip {

void handle_header(char* buff, std::size_t size,
                          message::Header &header) {
    if (size != message::HEADER_SIZE) {
        logger->info("invalid header size: ", message::HEADER_SIZE);
        return;
    }
    message::DecodeHeader(reinterpret_cast<uint8_t *>(buff), size, header);
}

std::size_t handle_body(uint32_t type,
                        char *buff, std::size_t size,
                        char *resp_buff, std::size_t resp_size) {
    std::size_t result;
    switch (type) {
        case message::TYPE_PING: {
            auto ping = flatbuffers::GetRoot<gossip::message::Ping>(buff);
            flatbuffers::FlatBufferBuilder builder(1024);
            auto ack = message::CreateAck(builder, ping->seqNo());
            builder.Finish(ack);
            uint8_t *tmp_buff = builder.GetBufferPointer();
            int size = builder.GetSize();

            if (size > resp_size) {
                result = -1;
            } else {
                std::memcpy(resp_buff, tmp_buff, size);
                result = size;
            }
            break;
        }
        case message::TYPE_INDIRECTPING: {
            auto indirect_ping = flatbuffers::GetRoot<gossip::message::IndirectPing>(buff);
            break;
        }
        case message::TYPE_SYNCSTATE: {
            break;
        }
        default:
            ;
    }
    return result;
}

int handle_packet(char *buff, std::size_t size,
    char *resp_buff, std::size_t resp_size) {
    return 0;
}

}

#endif //CPPGOSSIP_HANDLER_HPP
