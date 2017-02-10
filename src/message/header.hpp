//
// Created by bloodstone on 17/2/9.
//

#ifndef CPPGOSSIP_HEADER_HPP
#define CPPGOSSIP_HEADER_HPP

namespace gossip {
namespace message {

#pragma pack(4)
struct Header {
    uint32_t Type_;
    uint32_t Body_length_;
};
#pragma pack()

const static uint32_t HEADER_SIZE = sizeof(struct Header);

}
}

#endif //CPPGOSSIP_HEADER_HPP
