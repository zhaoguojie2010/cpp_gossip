//
// Created by bloodstone on 17/2/9.
//

#ifndef CPPGOSSIP_HEADER_HPP
#define CPPGOSSIP_HEADER_HPP

#include <cstring>
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

inline void DecodeHeader(uint8_t *buff, Header &header) {
    header.Type_ = *(reinterpret_cast<uint32_t*>(buff));
    header.Type_ = be32toh(header.Type_);
    header.Body_length_ = *(reinterpret_cast<uint32_t*>(buff+4));
    header.Body_length_ = be32toh(header.Body_length_);
}

inline void EncodeHeader(uint8_t *buff, const Header &header) {
    uint32_t type = htobe32(header.Type_);
    std::memcpy(buff, reinterpret_cast<uint8_t*>(&type), 4);
    uint32_t length = htobe32(header.Body_length_);
    std::memcpy(buff+4, reinterpret_cast<uint8_t*>(length), 4);
}

}
}

#endif //CPPGOSSIP_HEADER_HPP
