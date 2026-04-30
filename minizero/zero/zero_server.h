#pragma once

#include "base_server.h"
#include "configuration.h"
#include "time_system.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread.hpp>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <queue>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace minizero::zero {

class ZeroLogger {
public:
    ZeroLogger() {}
    void createLog();

    inline void addWorkerLog(const std::string& log_str) { addLog(log_str, worker_log_); }
    inline void addTrainingLog(const std::string& log_str) { addLog(log_str, training_log_); }

    template <typename T>
    void addTrainingLog(const std::string& log_str, T log_value, const std::string& metric_group = "", const std::string& metric_name = "", int iteration = -1)
    {
        static_assert(std::is_arithmetic<T>::value, "log_value must be numeric");

        std::string value_str;
        if constexpr (std::is_integral<T>::value) {
            value_str = std::to_string(log_value);
        } else {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(6) << static_cast<double>(log_value);
            value_str = oss.str();
        }

        addTrainingLog(log_str + " " + value_str);

        if (!metric_group.empty() && !metric_name.empty() && iteration >= 0) { addSelfPlayMetric(metric_group, metric_name, value_str, iteration); }
    }

    void flushSelfPlayMetricsJsonl(int iteration);
    inline std::fstream& getSelfPlayFileStream() { return self_play_game_; }

private:
    void addLog(const std::string& log_str, std::fstream& log_file);
    void addSelfPlayMetric(const std::string& metric_group, const std::string& metric_name, const std::string& metric_value, int iteration);
    void appendSelfPlayMetricsJsonl(int iteration);

    std::fstream worker_log_;
    std::fstream training_log_;
    std::fstream self_play_game_;
    int pending_selfplay_metrics_iteration_ = -1;
    std::map<std::string, std::map<std::string, std::string>> pending_selfplay_metrics_;
};

class ZeroSelfPlayData {
public:
    bool is_terminal_;
    int data_length_;
    int game_length_;
    float return_;
    std::string game_record_;

    ZeroSelfPlayData() {}
    ZeroSelfPlayData(std::string input_data);
};

class ZeroWorkerSharedData {
public:
    ZeroWorkerSharedData(boost::mutex& worker_mutex)
        : worker_mutex_(worker_mutex)
    {
    }

    bool getSelfPlayData(ZeroSelfPlayData& sp_data);
    bool isOptimizationPahse();
    int getModelIetration();

    bool is_optimization_phase_;
    int num_op_worker_;
    int total_games_;
    int model_iteration_;
    ZeroLogger logger_;
    std::string updated_conf_str_;
    std::queue<ZeroSelfPlayData> sp_data_queue_;
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

    void handleReceivedMessage(const std::string& message) override;
    void close() override;
    void syncConfig();

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

    virtual void run();
    boost::shared_ptr<ZeroWorkerHandler> handleAcceptNewConnection() override { return boost::make_shared<ZeroWorkerHandler>(io_service_, shared_data_); }
    void sendInitialMessage(boost::shared_ptr<ZeroWorkerHandler> connection) override {}

protected:
    virtual void initialize();
    virtual void selfPlay();
    virtual void broadcastSelfPlayJob();
    virtual void optimization();
    virtual std::string getUpdatedConfig();
    void syncConfig();
    void stopJob(const std::string& job_type);
    void close();
    void keepAlive();
    void startKeepAlive();

    int iteration_;
    ZeroWorkerSharedData shared_data_;
    boost::asio::deadline_timer keep_alive_timer_;

    std::vector<int> latest_game_lengths_;
    std::vector<float> latest_game_returns_;
};

} // namespace minizero::zero
