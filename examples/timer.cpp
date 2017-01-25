//
// Created by bloodstone on 17/1/23.
//

#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>
#include <utility>

#include "asio.hpp"
#include "asio/steady_timer.hpp"

using asio::ip::tcp;

class session
    : public std::enable_shared_from_this<session>
{
public:
    session(tcp::socket socket)
        : socket_(std::move(socket))
    {
    }

    void start()
    {
        do_read();
    }

private:
    void do_read()
    {
        auto self(shared_from_this());
        socket_.async_read_some(asio::buffer(data_, max_length),
                                [this, self](std::error_code ec, std::size_t length)
                                {
                                    if (!ec)
                                    {
                                        do_write(length);
                                    }
                                });
    }

    void do_write(std::size_t length)
    {
        auto self(shared_from_this());
        asio::async_write(socket_, asio::buffer(data_, length),
                          [this, self](std::error_code ec, std::size_t /*length*/)
                          {
                              if (!ec)
                              {
                                  do_read();
                              }
                          });
    }

    tcp::socket socket_;
    enum { max_length = 1024 };
    char data_[max_length];
};

class server
{
public:
    server(asio::io_service& io_service, short port)
        : acceptor_(io_service, tcp::endpoint(tcp::v4(), port)),
          socket_(io_service)
    {
        do_accept();
    }

private:
    void do_accept()
    {
        acceptor_.async_accept(socket_,
                               [this](std::error_code ec)
                               {
                                   if (!ec)
                                   {
                                       std::make_shared<session>(std::move(socket_))->start();
                                   }

                                   do_accept();
                               });
    }

    tcp::acceptor acceptor_;
    tcp::socket socket_;
};

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 2)
        {
            std::cerr << "Usage: async_tcp_echo_server <port>\n";
            return 1;
        }

        asio::io_service io_service;

        server s(io_service, std::atoi(argv[1]));
        std::thread t([&](){
            io_service.run();
        });

        asio::steady_timer timer(io_service);
        timer.expires_from_now(std::chrono::seconds(2));
        timer.async_wait([](const asio::error_code&){
            std::cout << "11111\n";
        });
        t.join();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}

