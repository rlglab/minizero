#include "zero_server.h"
#include <boost/algorithm/string.hpp>
#include <iostream>

namespace minizero::server {

using namespace minizero;

void ZeroLogger::CreateLog()
{
    std::string worker_file_name = config::zero_training_directory + "/Worker.log";
    std::string training_file_name = config::zero_training_directory + "/Training.log";
    worker_log_.open(worker_file_name.c_str(), std::ios::out | std::ios::app);
    training_log_.open(training_file_name.c_str(), std::ios::out | std::ios::app);

    for (int i = 0; i < 100; ++i) {
        worker_log_ << "=";
        training_log_ << "=";
    }
    worker_log_ << std::endl;
    training_log_ << std::endl;
}

void ZeroLogger::AddLog(const std::string& log_str, std::fstream& log_file)
{
    log_file << TimeSystem::GetTimeString("[Y/m/d_H:i:s.f] ") << log_str << std::endl;
    std::cout << TimeSystem::GetTimeString("[Y/m/d_H:i:s.f] ") << log_str << std::endl;
}

std::string ZeroWorkerSharedData::GetSelfPlayGame()
{
    boost::lock_guard<boost::mutex> lock(mutex_);
    std::string self_play_game = "";
    if (!self_play_queue_.empty()) {
        self_play_game = self_play_queue_.front();
        self_play_queue_.pop();
    }
    return self_play_game;
}

bool ZeroWorkerSharedData::IsOptimizationPahse()
{
    boost::lock_guard<boost::mutex> lock(mutex_);
    return is_optimization_phase_;
}

int ZeroWorkerSharedData::GetModelIetration()
{
    boost::lock_guard<boost::mutex> lock(mutex_);
    return model_iteration_;
}

void ZeroWorkerHandler::HandleReceivedMessage(const std::string& message)
{
    std::vector<std::string> args;
    boost::split(args, message, boost::is_any_of(" "), boost::token_compress_on);

    if (args[0] == "Info") {
        name_ = args[1];
        type_ = args[2];
        boost::lock_guard<boost::mutex> lock(shared_data_.worker_mutex_);
        shared_data_.logger_.AddWorkerLog("[Worker Connection] " + GetName() + " " + GetType());
        is_idle_ = true;
    } else if (args[0] == "SelfPlay") {
        if (message.find("SelfPlay", message.find("SelfPlay", 0) + 1) != std::string::npos) { return; }
        std::string game_record = message.substr(message.find(args[0]) + args[0].length() + 1);
        boost::lock_guard<boost::mutex> lock(shared_data_.mutex_);
        shared_data_.self_play_queue_.push(game_record);
    } else if (args[0] == "Optimization_Done") {
        boost::lock_guard<boost::mutex> lock(shared_data_.mutex_);
        shared_data_.model_iteration_ = stoi(args[1]);
        shared_data_.is_optimization_phase_ = false;
    } else {
        std::string error_message = message;
        std::replace(error_message.begin(), error_message.end(), '\r', ' ');
        std::replace(error_message.begin(), error_message.end(), '\n', ' ');
        shared_data_.logger_.AddWorkerLog("[Worker Error] " + error_message);
        Close();
    }
}

void ZeroWorkerHandler::Close()
{
    if (IsClosed()) { return; }

    boost::lock_guard<boost::mutex> lock(shared_data_.worker_mutex_);
    shared_data_.logger_.AddWorkerLog("[Worker Disconnection] " + GetName() + " " + GetType());
    ConnectionHandler::Close();
}

void ZeroServer::Run()
{
    Initialize();
    StartAccept();
    std::cout << TimeSystem::GetTimeString("[Y/m/d_H:i:s.f] ") << "Server initialize over." << std::endl;

    for (iteration_ = config::zero_start_iteration; iteration_ <= config::zero_end_iteration; ++iteration_) {
        SelfPlay();
        Optimization();
    }
}

boost::shared_ptr<ZeroWorkerHandler> ZeroServer::HandleAcceptNewConnection()
{
    boost::shared_ptr<ZeroWorkerHandler> worker = boost::make_shared<ZeroWorkerHandler>(io_service_, shared_data_);
    return worker;
}

void ZeroServer::SendInitialMessage(boost::shared_ptr<ZeroWorkerHandler> connection)
{
    connection->Write("Info");
}

void ZeroServer::Initialize()
{
    seed_ = config::auto_seed ? static_cast<int>(time(NULL)) : config::seed;
    shared_data_.logger_.CreateLog();

    std::string nn_file_name = config::nn_file_name;
    nn_file_name = nn_file_name.substr(nn_file_name.find("weight_iter_") + std::string("weight_iter_").size());
    nn_file_name = nn_file_name.substr(0, nn_file_name.find("."));
    shared_data_.model_iteration_ = stoi(nn_file_name);
}

void ZeroServer::SelfPlay()
{
    // setup
    std::string self_play_file_name = config::zero_training_directory + "/sgf/" + std::to_string(iteration_) + ".sgf";
    shared_data_.logger_.GetSelfPlayFileStream().open(self_play_file_name.c_str(), std::ios::out);
    shared_data_.logger_.AddTrainingLog("[Iteration] =====" + std::to_string(iteration_) + "=====");
    shared_data_.logger_.AddTrainingLog("[SelfPlay] Start " + std::to_string(shared_data_.GetModelIetration()));

    int num_collect_game = 0, game_length = 0;
    while (num_collect_game < config::zero_num_games_per_iteration) {
        BroadCastSelfPlayJob();

        // read one selfplay game
        std::string self_play_game = shared_data_.GetSelfPlayGame();
        if (self_play_game.empty()) {
            boost::this_thread::sleep(boost::posix_time::milliseconds(100));
            continue;
        } else if (self_play_game.find("weight_iter_" + std::to_string(shared_data_.GetModelIetration())) == std::string::npos) {
            // discard previous self-play games
            continue;
        }

        // save record
        std::string move_number = self_play_game.substr(0, self_play_game.find("(") - 1);
        std::string game_string = self_play_game.substr(self_play_game.find("("));
        shared_data_.logger_.GetSelfPlayFileStream() << num_collect_game << " "
                                                     << move_number << " "
                                                     << game_string << std::endl;
        ++num_collect_game;
        game_length += stoi(move_number);

        // display progress
        if (num_collect_game % static_cast<int>(config::zero_num_games_per_iteration * 0.25) == 0) {
            shared_data_.logger_.AddTrainingLog("[SelfPlay Progress] " +
                                                std::to_string(num_collect_game) + " / " +
                                                std::to_string(config::zero_num_games_per_iteration));
        }
    }

    StopJob();
    shared_data_.logger_.GetSelfPlayFileStream().close();
    shared_data_.logger_.AddTrainingLog("[SelfPlay] Finished.");
    shared_data_.logger_.AddTrainingLog("[SelfPlay Game Lengths] " + std::to_string(game_length * 1.0f / num_collect_game));
}

void ZeroServer::BroadCastSelfPlayJob()
{
    std::string job_command = "Job_SelfPlay ";
    job_command += config::zero_training_directory + " ";
    job_command += "seed=" + std::to_string(seed_);
    job_command += ":nn_file_name=" + config::zero_training_directory + "/model/weight_iter_" + std::to_string(shared_data_.GetModelIetration()) + ".pt";
    seed_ += kSeedStep;

    boost::lock_guard<boost::mutex> lock(worker_mutex_);
    for (auto worker : connections_) {
        if (!worker->IsIdle()) { continue; }
        worker->SetIdle(false);
        worker->Write(job_command);
    }
}

void ZeroServer::Optimization()
{
    shared_data_.logger_.AddTrainingLog("[Optimization] Start.");

    std::string job_command = "Job_Optimization ";
    job_command += config::zero_training_directory + " ";
    job_command += "weight_iter_" + std::to_string(shared_data_.GetModelIetration()) + ".pkl";
    job_command += " " + std::to_string(std::max(1, iteration_ - config::zero_replay_buffer));
    job_command += " " + std::to_string(iteration_);

    shared_data_.is_optimization_phase_ = true;
    while (shared_data_.IsOptimizationPahse()) {
        boost::lock_guard<boost::mutex> lock(worker_mutex_);
        for (auto worker : connections_) {
            if (!worker->IsIdle()) { continue; }
            worker->SetIdle(false);
            worker->Write(job_command);
        }
    }
    StopJob();

    shared_data_.logger_.AddTrainingLog("[Optimization] Finished.");
}

void ZeroServer::StopJob()
{
    boost::lock_guard<boost::mutex> lock(worker_mutex_);
    for (auto worker : connections_) {
        worker->SetIdle(true);
        worker->Write("Job_Done");
    }
}

void ZeroServer::KeepAlive()
{
    boost::lock_guard<boost::mutex> lock(worker_mutex_);
    for (auto worker : connections_) {
        worker->Write("keep_alive");
    }
    StartKeepAlive();
}

void ZeroServer::StartKeepAlive()
{
    keep_alive_timer_.expires_from_now(boost::posix_time::minutes(1));
    keep_alive_timer_.async_wait(boost::bind(&ZeroServer::KeepAlive, this));
}

} // namespace minizero::server