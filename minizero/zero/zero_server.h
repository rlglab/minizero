#pragma once

#include "base_server.h"
#include "configuration.h"
#include "time_system.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread.hpp>
#include <ctime>
#include <fstream>
#include <queue>

namespace minizero::server {

class ZeroLogger {
public:
    ZeroLogger() {}
    void createLog();

    inline void addWorkerLog(const std::string& log_str) { addLog(log_str, worker_log_); }
    inline void addTrainingLog(const std::string& log_str) { addLog(log_str, training_log_); }
    inline std::fstream& getSelfPlayFileStream() { return self_play_game_; }

private:
    void addLog(const std::string& log_str, std::fstream& log_file);

    std::fstream worker_log_;
    std::fstream training_log_;
    std::fstream self_play_game_;
};

class ZeroWorkerSharedData {
public:
    ZeroWorkerSharedData(boost::mutex& worker_mutex)
        : worker_mutex_(worker_mutex)
    {
    }

    std::string getSelfPlayGame();
    bool isOptimizationPahse();
    int getModelIetration();

    bool is_optimization_phase_;
    int total_games_;
    int model_iteration_;
    ZeroLogger logger_;
    std::queue<std::string> self_play_queue_;
    boost::mutex mutex_;
    boost::mutex& worker_mutex_;
};

class ZeroWorkerHandler : public utils::ConnectionHandler {
public:
    ZeroWorkerHandler(boost::asio::io_service& io_service, ZeroWorkerSharedData& shared_data)
        : ConnectionHandler(io_service),
          is_idle_(false),
          shared_data_(shared_data)
    {
    }

    void handleReceivedMessage(const std::string& message);
    void close();

    inline bool isIdle() const { return is_idle_; }
    inline std::string getName() const { return name_; }
    inline std::string getType() const { return type_; }
    inline void setIdle(bool is_idle) { is_idle_ = is_idle; }

private:
    bool is_idle_;
    std::string name_;
    std::string type_;
    ZeroWorkerSharedData& shared_data_;
};

class ZeroServer : public utils::BaseServer<ZeroWorkerHandler> {
public:
    ZeroServer()
        : BaseServer(minizero::config::zero_server_port),
          shared_data_(worker_mutex_),
          keep_alive_timer_(io_service_)
    {
        startKeepAlive();
    }

    void run();
    boost::shared_ptr<ZeroWorkerHandler> handleAcceptNewConnection();
    void sendInitialMessage(boost::shared_ptr<ZeroWorkerHandler> connection);

private:
    void initialize();
    void selfPlay();
    void broadCastSelfPlayJob();
    void optimization();
    void stopJob();
    void keepAlive();
    void startKeepAlive();

    int iteration_;
    ZeroWorkerSharedData shared_data_;
    boost::asio::deadline_timer keep_alive_timer_;
};

} // namespace minizero::server