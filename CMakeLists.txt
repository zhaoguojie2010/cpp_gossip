cmake_minimum_required(VERSION 3.6)
project(cppgossip)

set(CMAKE_CXX_STANDARD 11)
include_directories(. ./thirdparty/asio/include ./src ./src/message ./src/network)
add_executable(main ./examples/main.cpp)
