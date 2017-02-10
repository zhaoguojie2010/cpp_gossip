//
// Created by bloodstone on 17/1/29.
//

#ifndef CPPGOSSIP_HANDLER_HPP
#define CPPGOSSIP_HANDLER_HPP

#include "src/message/header.hpp"
#include "src/logger.hpp"
#include "src/message/message_generated.h"
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
    /*
    auto node_info = message::GetNodeState(
        reinterpret_cast<uint8_t*>(buff)
    );
     */
    return 0;
}

int handle_packet(char *buff, std::size_t size,
    char *resp_buff, std::size_t resp_size) {
    return 0;
}

}

#endif //CPPGOSSIP_HANDLER_HPP
