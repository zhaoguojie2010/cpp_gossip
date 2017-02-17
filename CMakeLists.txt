cmake_minimum_required(VERSION 3.6)
project(cppgossip)

set(CMAKE_CXX_STANDARD 11)
include_directories(. ./thirdparty/asio/include ./src ./src/message ./src/network ./src/network/udp ./thirdparty)
aux_source_directory(./src/message SRC_DIR)
add_executable(main ./examples/main.cpp ${SRC_DIR})
link_directories(/usr/local/lib)
