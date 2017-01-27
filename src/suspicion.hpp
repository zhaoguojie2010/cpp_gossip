//
// Created by bloodstone on 17/1/27.
//

#ifndef CPPGOSSIP_SUSPICION_HPP
#define CPPGOSSIP_SUSPICION_HPP

#include "string"
#include <set>
#include <functional>
namespace gossip {

template <class TIMER>
class suspicion {
public:
    typedef std::function<void()> callback;
public:
    suspicion(uint32 node_num, callback convict, TIMER t)
    : confirm_(0),
      need_confirm_(std::log(node_num) * 3),
      timer_(t),
      convict_(convict) {}

    bool Confirm(std::string from) {
        return false;
    }


private:
    TIMER timer_;

    // indicates the number of confirms we'd like to see
    // before the suspicion is convicted.
    uint32 need_confirm_;

    // indicates the number of confirms we've seen so far
    uint32 confirm_;

    // rule out duplicated confirms from the same node
    std::set<std::string> who_confirmed_;

    // confirm the suspect to be dead
    callback convict_;
};

}

#endif //CPPGOSSIP_SUSPICION_HPP
