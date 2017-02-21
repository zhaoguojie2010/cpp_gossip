//
// Created by bloodstone on 17/2/2.
//

#ifndef CPPGOSSIP_UTILS_HPP
#define CPPGOSSIP_UTILS_HPP

#include <string>
#include <vector>

#if defined(__GNUC__)

#elif defined(_MSC_VER)

#elif defined(__clang__)

#endif

std::vector<std::string> Split(const std::string &s, const std::string &sep) {
    std::vector<std::string> rst;
    int start = 0, mark = 0;
    int skip = sep.size();
    mark = s.find(sep);
    while(mark != std::string::npos) {
        rst.push_back(s.substr(start, mark-start));
        start = mark+skip;
        mark = s.find(sep, start);
    }
    if (start < s.size()) {
        rst.push_back(s.substr(start));
    }
    return rst;
}

#endif //CPPGOSSIP_UTILS_HPP
