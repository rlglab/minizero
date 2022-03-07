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
    void CreateLog();

    inline void AddTrainingLog(const std::string log) { training_log_ << TimeSystem::GetTimeString("[Y/m/d_H:i:s.f] ") << log << std::endl; }

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

    std::string GetSelfPlayGame();
    bool IsOptimizationPahse();
    int GetModelIetration();

    bool is_optimization_phase_;
    int total_games_;
    int model_iteration_;
    ZeroLogger logger_;
    std::queue<std::string> self_play_queue_;
    boost::mutex mutex_;
    boost::mutex& worker_mutex_;
};

class ZeroWorkerHandler : public ConnectionHandler {
public:
    ZeroWorkerHandler(boost::asio::io_service& io_service, ZeroWorkerSharedData& shared_data)
        : ConnectionHandler(io_service),
          is_idle_(false),
          shared_data_(shared_data)
    {
    }

    void HandleReceivedMessage(const std::string& message);
    void Close();

    inline bool IsIdle() const { return is_idle_; }
    inline std::string GetName() const { return name_; }
    inline void SetIdle(bool is_idle) { is_idle_ = is_idle; }

private:
    bool is_idle_;
    std::string name_;
    ZeroWorkerSharedData& shared_data_;
};

class ZeroServer : public BaseServer<ZeroWorkerHandler> {
public:
    ZeroServer()
        : BaseServer(minizero::config::zero_server_port),
          shared_data_(worker_mutex_),
          keep_alive_timer_(io_service_)
    {
        StartKeepAlive();
    }

    void Run();
    boost::shared_ptr<ZeroWorkerHandler> HandleAcceptNewConnection();

private:
    void Initialize();
    void SelfPlay();
    void BroadCastSelfPlayJob();
    std::string GetOneSelfPlayGame();
    void Optimization();
    void StopJob();
    void KeepAlive();
    void StartKeepAlive();

    int seed_;
    int iteration_;
    boost::mutex worker_mutex_;
    ZeroWorkerSharedData shared_data_;
    boost::asio::deadline_timer keep_alive_timer_;

    const int kSeedStep = 48;
};

} // namespace minizero::server