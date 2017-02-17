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

void HandleHeader(char* buff, std::size_t size,
                          message::Header &header) {
    if (size != message::HEADER_SIZE) {
        logger->info("invalid header size: ", message::HEADER_SIZE);
        return;
    }
    message::DecodeHeader(reinterpret_cast<uint8_t *>(buff), header);
}

std::size_t HandleBody(uint32_t type,
                        char *buff, std::size_t size,
                        char *resp_buff, std::size_t resp_size) {
    std::size_t result;
    switch (type) {
        case message::TYPE_PING: {
            auto ping = flatbuffers::GetRoot<message::Ping>(buff);
            flatbuffers::FlatBufferBuilder builder(1024);
            auto ack = message::CreatePong(builder, ping->seqNo());
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
            auto indirect_ping = flatbuffers::GetRoot<message::IndirectPing>(buff);
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

int HandlePacket(char *buff, std::size_t size,
    char *resp_buff, std::size_t resp_size) {
    int rst = 0;
    message::Header header;
    message::DecodeHeader(reinterpret_cast<uint8_t*>(buff), header);
    switch (header.Type_) {
        case message::TYPE_PING: {
            auto ping = flatbuffers::GetRoot<message::Ping>(buff+message::HEADER_SIZE);
            // generate pong
            flatbuffers::FlatBufferBuilder builder(1024);
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
            message::DecodeHeader(reinterpret_cast<uint8_t*>(resp_buff), header);
            std::memcpy(resp_buff+message::HEADER_SIZE, tmp_buff, size);

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


}

#endif //CPPGOSSIP_HANDLER_HPP
