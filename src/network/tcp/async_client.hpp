//
// Created by bloodstone on 17/2/6.
//
// This is a modified version of asio example code

#ifndef CPPGOSSIP_ASYNC_CLIENT_HPP
#define CPPGOSSIP_ASYNC_CLIENT_HPP

namespace gossip {
namespace tcp {
using asio::ip::tcp;

class AsyncClient {
public:
    AsyncClient(const std::string &host, short port, asio::io_service &io_svc)
        : stopped_(false),
          socket_(io_svc),
          resolver_(io_svc),
          host_(host),
          port_(port),
          deadline_(io_svc) {
        connect(host, std::to_string(port));
    }

    // This function terminates all the actors to shut down the connection. It
    // may be called by the user of the client class, or by the class itself in
    // response to graceful termination or an unrecoverable error.
    void Close() {
        stopped_ = true;
        asio::error_code ignored_ec;
        socket_.close(ignored_ec);
        deadline_.cancel();
    }

    void Write(char *buff, int size, int timeout = 0) {
        if (timeout != 0) {
            std::string error_info = "tcp write timeout";
            deadline_.expires_from_now(std::chrono::milliseconds(timeout));
            deadline_.async_wait(std::bind(&AsyncClient::checkDeadline, this, error_info));
        }
        asio::async_write(socket_, asio::buffer(buff, size),
                          std::bind(&AsyncClient::handleWrite, this,
                                    std::placeholders::_1));
    }

    void ReadFull(char *buff, int size, int timeout = 0) {
        if (timeout != 0) {
            std::string error_info = "tcp read timeout";
            deadline_.expires_from_now(std::chrono::milliseconds(timeout));
            deadline_.async_wait(std::bind(&AsyncClient::checkDeadline, this, error_info));
        }
        asio::async_read(socket_, asio::buffer(buff, size),
                         std::bind(&AsyncClient::handleRead, this,
                                   std::placeholders::_1));
    }

private:
    void connect(const std::string &host, const std::string &port) {
        auto endpoint = resolver_.resolve(tcp::resolver::query(host, port));
        startConnect(endpoint);
    }

    void startConnect(tcp::resolver::iterator endpoint_iter) {
        if (endpoint_iter != tcp::resolver::iterator()) {
            std::cout << "Trying " << endpoint_iter->endpoint() << "...\n";
            std::string error_info = "connect to " +
                                     endpoint_iter->endpoint().address().to_string() +
                                     ":" + std::to_string(endpoint_iter->endpoint().port()) + " timeout";

            // Set a deadline for the connect operation.
            deadline_.expires_from_now(std::chrono::milliseconds(1000));
            deadline_.async_wait(std::bind(&AsyncClient::checkDeadline, this, error_info));

            // Start the asynchronous connect operation.
            socket_.async_connect(endpoint_iter->endpoint(),
                                  std::bind(&AsyncClient::handleConnect,
                                            this, std::placeholders::_1,
                                            endpoint_iter));
        } else {
            // There are no more endpoints to try. Shut down the client.
            Close();
        }
    }

    void handleConnect(const asio::error_code &ec,
                       tcp::resolver::iterator endpoint_iter) {
        if (stopped_)
            return;

        // The async_connect() function automatically opens the socket at the start
        // of the asynchronous operation. If the socket is closed at this time then
        // the timeout handler must have run first.
        if (!socket_.is_open()) {
            std::cout << "Connect timed out\n";

            // Try the next available endpoint.
            startConnect(++endpoint_iter);
        }

            // Check if the connect operation failed before the deadline expired.
        else if (ec) {
            std::cout << "Connect error: " << ec.message() << "\n";

            // We need to close the socket used in the previous connection attempt
            // before starting a new one.
            socket_.close();

            // Try the next available endpoint.
            startConnect(++endpoint_iter);
        }

            // Otherwise we have successfully established a connection.
        else {
            std::cout << "Connected to " << endpoint_iter->endpoint() << "\n";
            deadline_.cancel();
        }
    }

    void handleWrite(const asio::error_code &ec) {
        if (stopped_)
            return;

        deadline_.cancel();

        if (ec) {
            std::cerr << "Error on tcp write: " << ec.message() << "\n";
            Close();
        }
    }

    void handleRead(const asio::error_code &ec) {
        if (stopped_)
            return;

        deadline_.cancel();

        if (ec) {
            std::cerr << "Error on tcp write: " << ec.message() << "\n";
            Close();
        }
    }

    void checkDeadline(const std::string &error_info) {
        if (stopped_)
            return;

        // Check whether the deadline has passed. We compare the deadline against
        // the current time since a new asynchronous operation may have moved the
        // deadline before this actor had a chance to run.
        if (deadline_.expires_at() <= std::chrono::steady_clock::now()) {
            std::cerr << error_info << std::endl;
            // The deadline has passed. The socket is closed so that any outstanding
            // asynchronous operations are cancelled.
            socket_.close();

            // There is no longer an active deadline. The expiry is set to positive
            // infinity so that the actor takes no action until a new deadline is set.
            // deadline_.expires_at(std::chrono::time_point<std::chrono::steady_clock>::max());
        }
    }


private:
    bool stopped_;
    std::string host_;
    short port_;
    tcp::socket socket_;
    tcp::resolver resolver_;
    asio::streambuf input_buffer_;
    asio::steady_timer deadline_;
};

}
}

#endif //CPPGOSSIP_ASYNC_CLIENT_HPP
