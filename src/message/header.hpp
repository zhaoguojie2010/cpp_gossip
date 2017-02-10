//
// Created by bloodstone on 17/2/9.
//

#ifndef CPPGOSSIP_HEADER_HPP
#define CPPGOSSIP_HEADER_HPP

#include "thirdparty/portable_endian.h"

namespace gossip {
namespace message {

#pragma pack(4)
struct Header {
    uint32_t Type_;
    uint32_t Body_length_;
};
#pragma pack()

const static uint32_t HEADER_SIZE = sizeof(struct Header);

inline void DecodeHeader(uint8_t *buff, uint32_t size, Header &header) {
    header.Type_ = *(reinterpret_cast<uint32_t*>(buff));
    header.Type_ = be32toh(header.Type_);
    header.Body_length_ = *(reinterpret_cast<uint32_t*>(buff+4));
    header.Body_length_ = be32toh(header.Body_length_);
}

}
}

#endif //CPPGOSSIP_HEADER_HPP
