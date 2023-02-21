#pragma once

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/thread.hpp>
#include <queue>
#include <string>
#include <vector>

namespace minizero::utils {

class ConnectionHandler : public boost::enable_shared_from_this<ConnectionHandler> {
public:
    ConnectionHandler(boost::asio::io_service& io_service)
        : is_closed_(false),
          socket_(io_service),
          strand_(io_service)
    {
    }

    virtual ~ConnectionHandler() = default;

    void write(std::string message)
    {
        if (message.empty() || isClosed()) { return; }

        message += (message.back() == '\n' ? "" : "\n");
        strand_.dispatch(boost::bind(&ConnectionHandler::doWrite, shared_from_this(), message));
    }

    void startRead()
    {
        boost::asio::async_read_until(socket_,
                                      read_buffer_, '\n',
                                      boost::bind(&ConnectionHandler::handleRead,
                                                  shared_from_this(),
                                                  boost::asio::placeholders::error,
                                                  boost::asio::placeholders::bytes_transferred));
    }

    virtual void close()
    {
        if (is_closed_) { return; }

        is_closed_ = true;
        socket_.close();
    }

    inline bool isClosed() const { return is_closed_; }
    inline boost::asio::ip::tcp::socket& getSocket() { return socket_; }

    virtual void handleReceivedMessage(const std::string& message) = 0;

private:
    void doWrite(const std::string& message)
    {
        message_queue_.push(message);
        if (message_queue_.size() == 1) { writeNext(); }
    }

    void writeNext()
    {
        const std::string& message = message_queue_.front();
        boost::asio::async_write(socket_,
                                 boost::asio::buffer(message),
                                 strand_.wrap(boost::bind(&ConnectionHandler::handleWrite,
                                                          shared_from_this(),
                                                          boost::asio::placeholders::error)));
    }

    void handleWrite(const boost::system::error_code& error)
    {
        message_queue_.pop();
        if (error) {
            close();
            return;
        }
        if (!message_queue_.empty()) { writeNext(); }
    }

    void handleRead(const boost::system::error_code& error, size_t bytes_read)
    {
        if (error) {
            close();
            return;
        }

        std::istream is(&read_buffer_);
        std::string line;
        std::getline(is, line);
        handleReceivedMessage(line);
        startRead();
    }

    bool is_closed_;
    std::queue<std::string> message_queue_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::io_service::strand strand_;
    boost::asio::streambuf read_buffer_;
};

template <class _ConnectionHandler>
class BaseServer {
public:
    BaseServer(int port)
        : work_(io_service_),
          acceptor_(io_service_)
    {
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();

        const int num_threads = 1;
        for (int i = 0; i < num_threads; ++i) {
            thread_pool_.create_thread(boost::bind(&BaseServer::run, this));
        }
    }

    virtual ~BaseServer() { thread_pool_.join_all(); }

    void startAccept()
    {
        boost::shared_ptr<_ConnectionHandler> connection = handleAcceptNewConnection();
        acceptor_.async_accept(connection->getSocket(),
                               boost::bind(&BaseServer::handleAccept,
                                           this,
                                           connection,
                                           boost::asio::placeholders::error));
    }

    void run() { io_service_.run(); }
    void stop() { io_service_.stop(); }

protected:
    void handleAccept(boost::shared_ptr<_ConnectionHandler> connection, const boost::system::error_code& error)
    {
        if (!error) {
            boost::lock_guard<boost::mutex> lock(worker_mutex_);
            sendInitialMessage(connection);
            connection->startRead();
            connections_.push_back(connection);
            cleanUpClosedConnection();
        }
        startAccept();
    }

    void cleanUpClosedConnection()
    {
        std::vector<boost::shared_ptr<_ConnectionHandler>> connections;
        for (auto connection : connections_) {
            if (connection->isClosed()) { continue; }
            connections.push_back(connection);
        }
        connections_ = connections;
    }

    virtual boost::shared_ptr<_ConnectionHandler> handleAcceptNewConnection() = 0;
    virtual void sendInitialMessage(boost::shared_ptr<_ConnectionHandler> connection) = 0;

    boost::mutex worker_mutex_;
    boost::thread_group thread_pool_;
    boost::asio::io_service io_service_;
    boost::asio::io_service::work work_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::vector<boost::shared_ptr<_ConnectionHandler>> connections_;
};

} // namespace minizero::utils
