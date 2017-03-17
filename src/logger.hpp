//
// Created by bloodstone on 17/2/9.
//

#ifndef CPPGOSSIP_LOGGER_HPP
#define CPPGOSSIP_LOGGER_HPP

#include "thirdparty/spdlog/spdlog.h"

namespace gossip {

std::shared_ptr<spdlog::logger> GetLogger() {
    return spdlog::basic_logger_mt("basic_logger", "logs/basic.txt");
}

std::shared_ptr<spdlog::logger> GetConsole() {
    return spdlog::stdout_color_mt("console");
}

std::shared_ptr<spdlog::logger> logger = GetConsole();

}

#endif //CPPGOSSIP_LOGGER_HPP
