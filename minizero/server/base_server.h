#pragma once

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/thread.hpp>
#include <queue>
#include <string>
#include <vector>

class ConnectionHandler : public boost::enable_shared_from_this<ConnectionHandler> {
public:
    ConnectionHandler(boost::asio::io_service& io_service)
        : is_closed_(false),
          socket_(io_service),
          strand_(io_service)
    {
    }

    virtual ~ConnectionHandler() = default;

    void Write(std::string message)
    {
        if (message.empty() || IsClosed()) { return; }

        message += (message.back() == '\n' ? "" : "\n");
        strand_.dispatch(boost::bind(&ConnectionHandler::DoWrite, shared_from_this(), message));
    }

    void StartRead()
    {
        boost::asio::async_read_until(socket_,
                                      read_buffer_, '\n',
                                      boost::bind(&ConnectionHandler::HandleRead,
                                                  shared_from_this(),
                                                  boost::asio::placeholders::error,
                                                  boost::asio::placeholders::bytes_transferred));
    }

    virtual void Close()
    {
        if (is_closed_) { return; }

        is_closed_ = true;
        socket_.close();
    }

    inline bool IsClosed() { return is_closed_; }
    inline boost::asio::ip::tcp::socket& GetSocket() { return socket_; }

    virtual void HandleReceivedMessage(const std::string& message) = 0;

private:
    void DoWrite(const std::string& message)
    {
        message_queue_.push(message);
        if (message_queue_.size() == 1) { WriteNext(); }
    }

    void WriteNext()
    {
        const std::string& message = message_queue_.front();
        boost::asio::async_write(socket_,
                                 boost::asio::buffer(message),
                                 strand_.wrap(boost::bind(&ConnectionHandler::HandleWrite,
                                                          shared_from_this(),
                                                          boost::asio::placeholders::error)));
    }

    void HandleWrite(const boost::system::error_code& error)
    {
        message_queue_.pop();
        if (error) {
            Close();
            return;
        }
        if (!message_queue_.empty()) { WriteNext(); }
    }

    void HandleRead(const boost::system::error_code& error, size_t bytes_read)
    {
        if (error) {
            Close();
            return;
        }

        std::istream is(&read_buffer_);
        std::string line;
        std::getline(is, line);
        HandleReceivedMessage(line);
        StartRead();
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
            thread_pool_.create_thread(boost::bind(&BaseServer::Run, this));
        }
    }

    ~BaseServer() { thread_pool_.join_all(); }

    void StartAccept()
    {
        boost::shared_ptr<_ConnectionHandler> connection = HandleAcceptNewConnection();
        acceptor_.async_accept(connection->GetSocket(),
                               boost::bind(&BaseServer::HandleAccept,
                                           this,
                                           connection,
                                           boost::asio::placeholders::error));
    }

    void Run() { io_service_.run(); }
    void Stop() { io_service_.stop(); }

protected:
    void HandleAccept(boost::shared_ptr<_ConnectionHandler> connection, const boost::system::error_code& error)
    {
        if (!error) {
            boost::lock_guard<boost::mutex> lock(worker_mutex_);
            SendInitialMessage(connection);
            connection->StartRead();
            connections_.push_back(connection);
            CleanUpClosedConnection();
        }
        StartAccept();
    }

    void CleanUpClosedConnection()
    {
        std::vector<boost::shared_ptr<_ConnectionHandler>> connections;
        for (auto connection : connections_) {
            if (connection->IsClosed()) { continue; }
            connections.push_back(connection);
        }
        connections_ = connections;
    }

    virtual boost::shared_ptr<_ConnectionHandler> HandleAcceptNewConnection() = 0;
    virtual void SendInitialMessage(boost::shared_ptr<_ConnectionHandler> connection) = 0;

    boost::mutex worker_mutex_;
    boost::thread_group thread_pool_;
    boost::asio::io_service io_service_;
    boost::asio::io_service::work work_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::vector<boost::shared_ptr<_ConnectionHandler>> connections_;
};