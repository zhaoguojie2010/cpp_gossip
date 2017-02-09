//
// Created by bloodstone on 17/1/29.
//

#ifndef CPPGOSSIP_HANDLER_HPP
#define CPPGOSSIP_HANDLER_HPP

#include "thirdparty/portable_endian.h"
#include "src/message/header.hpp"
#include "src/logger.hpp"

namespace gossip {

std::size_t handle_header(char* buff, std::size_t size) {
    if (size != HEADER_SIZE) {
        logger->info("invalid header size: ", 4);
        return 0;
    }
    buff[HEADER_SIZE] = 0;
    // don't need endian conversion since the first byte received
    // is always the higher bit
    // return be32toh(std::atoi(buff));
    return std::atoi(buff);
}

std::size_t handle_body(char*, std::size_t, char*, std::size_t) {
    return 0;
}

int handle_packet(char *buff, std::size_t size,
    char *resp_buff, std::size_t resp_size) {
    return 0;
}

}

#endif //CPPGOSSIP_HANDLER_HPP
