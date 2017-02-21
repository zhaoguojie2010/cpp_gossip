//
// Created by bloodstone on 17/2/6.
// This is a modified version of asio example code

#ifndef CPPGOSSIP_BLOCKING_CLIENT_HPP
#define CPPGOSSIP_BLOCKING_CLIENT_HPP

#include <chrono>

namespace gossip {
namespace tcp {

class BlockingClient {
public:
    BlockingClient()
        : io_service_(),
          socket_(io_service_),
          deadline_(io_service_) {
        // No deadline is required until the first socket operation is started. We
        // set the deadline to positive infinity so that the actor takes no action
        // until a specific deadline is set.
        deadline_.expires_at(std::chrono::time_point<std::chrono::steady_clock>::max());

        // Start the persistent actor that checks for deadline expiry.
        check_deadline();
    }

    void Connect(const std::string &host,
                 const std::string &service,
                 int timeout = 0) {
        // Resolve the host name and service to a list of endpoints.
        tcp::resolver::query query(host, service);
        tcp::resolver::iterator iter = tcp::resolver(io_service_).resolve(query);

        // Set a deadline for the asynchronous operation. As a host name may
        // resolve to multiple endpoints, this function uses the composed operation
        // async_connect. The deadline applies to the entire operation, rather than
        // individual connection attempts.
        deadline_.expires_from_now(std::chrono::milliseconds(timeout));

        // Set up the variable that receives the result of the asynchronous
        // operation. The error code is set to would_block to signal that the
        // operation is incomplete. Asio guarantees that its asynchronous
        // operations will never fail with would_block, so any other value in
        // ec indicates completion.
        asio::error_code ec = asio::error::would_block;

        // Start the asynchronous operation itself. The lambda function
        // object is used as a callback and will update the ec variable when the
        // operation completes.
        asio::async_connect(
            socket_, iter,
            [&ec](const asio::error_code &ec1,
                  tcp::resolver::iterator endpoint_iter) {
                ec = ec1;
            });

        // Block until the asynchronous operation has completed.
        do io_service_.run_one(); while (ec == asio::error::would_block);

        // Determine whether a connection was successfully established. The
        // deadline actor may have had a chance to run and close our socket, even
        // though the connect operation notionally succeeded. Therefore we must
        // check whether the socket is still open before deciding if we succeeded
        // or failed.
        if (ec || !socket_.is_open())
            throw asio::system_error(
                ec ? ec : asio::error::operation_aborted);
    }

    void ReadFull(char *buff, std::size_t size, int timeout) {
        // Set a deadline for the asynchronous operation. Since this function uses
        // a composed operation (async_read_until), the deadline applies to the
        // entire operation, rather than individual reads from the socket.
        deadline_.expires_from_now(std::chrono::milliseconds(timeout));

        // Set up the variable that receives the result of the asynchronous
        // operation. The error code is set to would_block to signal that the
        // operation is incomplete. Asio guarantees that its asynchronous
        // operations will never fail with would_block, so any other value in
        // ec indicates completion.
        asio::error_code ec = asio::error::would_block;

        // Start the asynchronous operation itself. The lambda function
        // object is used as a callback and will update the ec variable when the
        // operation completes.
        asio::async_read(
            socket_, asio::buffer(buff, size),
            [&ec](const asio::error_code &ec1,
                  std::size_t /*lenght*/) {
                ec = ec1;
            });

        // Block until the asynchronous operation has completed.
        do io_service_.run_one(); while (ec == asio::error::would_block);

        if (ec)
            throw asio::system_error(ec);
    }

    void Write(char *buff, std::size_t size,
                   int timeout = 0) {
        // Set a deadline for the asynchronous operation. Since this function uses
        // a composed operation (async_write), the deadline applies to the entire
        // operation, rather than individual writes to the socket.
        deadline_.expires_from_now(std::chrono::milliseconds(timeout));

        // Set up the variable that receives the result of the asynchronous
        // operation. The error code is set to would_block to signal that the
        // operation is incomplete. Asio guarantees that its asynchronous
        // operations will never fail with would_block, so any other value in
        // ec indicates completion.
        asio::error_code ec = asio::error::would_block;

        // Start the asynchronous operation itself. The boost::lambda function
        // object is used as a callback and will update the ec variable when the
        // operation completes. The blocking_udp_BlockingClient.cpp example shows how you
        // can use boost::bind rather than boost::lambda.
        asio::async_write(
            socket_, asio::buffer(buff, size),
            [&ec](const asio::error_code &ec1,
                  std::size_t /*length*/) {
                ec = ec1;
            });

        // Block until the asynchronous operation has completed.
        do io_service_.run_one(); while (ec == asio::error::would_block);

        if (ec)
            throw asio::system_error(ec);
    }

    void Close() {
        asio::error_code ignored_ec;
        deadline_.cancel();
        socket_.close(ignored_ec);
    }

private:
    void check_deadline() {
        // Check whether the deadline has passed. We compare the deadline against
        // the current time since a new asynchronous operation may have moved the
        // deadline before this actor had a chance to run.
        if (deadline_.expires_at() <= std::chrono::steady_clock::now()) {
            // The deadline has passed. The socket is closed so that any outstanding
            // asynchronous operations are cancelled. This allows the blocked
            // connect(), read_line() or write_line() functions to return.
            asio::error_code ignored_ec;
            socket_.close(ignored_ec);

            // There is no longer an active deadline. The expiry is set to positive
            // infinity so that the actor takes no action until a new deadline is set.
            deadline_.expires_at(std::chrono::time_point<std::chrono::steady_clock>::max());
        }

        // Put the actor back to sleep.
        deadline_.async_wait(std::bind(&BlockingClient::check_deadline, this));
    }

    asio::io_service io_service_;
    tcp::socket socket_;
    asio::steady_timer deadline_;
};

}
}

#endif //CPPGOSSIP_BLOCKING_CLIENT_HPP
