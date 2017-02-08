//
// Created by bloodstone on 17/1/29.
//

#ifndef CPPGOSSIP_HANDLER_HPP
#define CPPGOSSIP_HANDLER_HPP

namespace gossip {

uint32 handle_header(char*, uint32) {
    return 0;
}

uint32 handle_body(char*, uint32, char*, uint32) {
    return 0;
}

int handle_packet(char *buff, std::size_t size,
    char *resp_buff, std::size_t resp_size) {
    return 0;
}

}

#endif //CPPGOSSIP_HANDLER_HPP
