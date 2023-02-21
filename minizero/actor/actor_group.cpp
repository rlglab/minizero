#include "actor_group.h"
#include "configuration.h"
#include "create_actor.h"
#include "create_network.h"
#include "random.h"
#include "utils.h"
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <torch/cuda.h>
#include <unordered_map>

namespace minizero::actor {

using namespace network;
using namespace utils;

int ThreadSharedData::getAvailableActorIndex()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return (actor_index_ < static_cast<int>(actors_.size()) ? actor_index_++ : actors_.size());
}

void ThreadSharedData::syncObservation(int actor_id)
{
    if (!observation_map_.count(actor_id)) { return; }

    std::vector<float> observation;
    std::vector<std::string> observation_string = utils::stringToVector(observation_map_[actor_id]);
    for (const auto& s : observation_string) { observation.push_back(std::stof(s)); }
    actors_[actor_id]->getEnvironment().setObservation(observation);

    std::lock_guard<std::mutex> lock(mutex_);
    observation_map_.erase(actor_id);
}

void SlaveThread::initialize()
{
    int seed = config::program_auto_seed ? std::random_device()() : config::program_seed + id_;
    Random::seed(seed);
}

void SlaveThread::runJob()
{
    if (getSharedData()->do_cpu_job_) {
        while (doCPUJob()) {}
    } else {
        doGPUJob();
    }
}

bool SlaveThread::doCPUJob()
{
    size_t actor_id = getSharedData()->getAvailableActorIndex();
    if (actor_id >= getSharedData()->actors_.size()) { return false; }

    getSharedData()->syncObservation(actor_id);
    std::shared_ptr<BaseActor>& actor = getSharedData()->actors_[actor_id];
    int network_id = actor_id % getSharedData()->networks_.size();
    int network_output_id = actor->getNNEvaluationBatchIndex();
    if (network_output_id >= 0) {
        assert(network_output_id < static_cast<int>(getSharedData()->network_outputs_[network_id].size()));
        actor->afterNNEvaluation(getSharedData()->network_outputs_[network_id][network_output_id]);
        if (actor->isSearchDone() && !actor->isResign()) { actor->act(actor->getSearchAction(), actor->getActionComment()); }
    }
    if (!actor->isSearchDone()) { actor->beforeNNEvaluation(); }
    return true;
}

void SlaveThread::doGPUJob()
{
    if (id_ >= static_cast<int>(getSharedData()->networks_.size())) { return; }

    std::shared_ptr<Network>& network = getSharedData()->networks_[id_];
    if (network->getNetworkTypeName() == "alphazero") {
        std::shared_ptr<AlphaZeroNetwork> az_network = std::static_pointer_cast<AlphaZeroNetwork>(network);
        if (az_network->getBatchSize() > 0) { getSharedData()->network_outputs_[id_] = az_network->forward(); }
    } else if (network->getNetworkTypeName() == "muzero" || network->getNetworkTypeName() == "muzero_reward") {
        std::shared_ptr<MuZeroNetwork> muzero_network = std::static_pointer_cast<MuZeroNetwork>(network);
        if (muzero_network->getInitialInputBatchSize() > 0) {
            getSharedData()->network_outputs_[id_] = std::static_pointer_cast<MuZeroNetwork>(network)->initialInference();
        } else if (muzero_network->getRecurrentInputBatchSize() > 0) {
            getSharedData()->network_outputs_[id_] = std::static_pointer_cast<MuZeroNetwork>(network)->recurrentInference();
        }
    }
}

void ActorGroup::run()
{
    initialize();
    while (true) {
        handleCommand();

        if (!running_) { continue; }
        getSharedData()->actor_index_ = 0;
        for (auto& t : slave_threads_) { t->start(); }
        for (auto& t : slave_threads_) { t->finish(); }
        handleFinishedGame();
        getSharedData()->do_cpu_job_ = !getSharedData()->do_cpu_job_;
    }
}

void ActorGroup::initialize()
{
    int num_threads = std::max(static_cast<int>(torch::cuda::device_count()), config::actor_num_threads);
    createSlaveThreads(num_threads);
    createNeuralNetworks();
    createActors();
    createEnvironmentServer();
    running_ = false;
    getSharedData()->do_cpu_job_ = true;

    // create one thread to handle I/O
    commands_.clear();
    thread_groups_.create_thread(boost::bind(&ActorGroup::handleIO, this));

    // initialize ignored command
    std::vector<std::string> ignored_commands = utils::stringToVector(config::zero_actor_ignored_command);
    for (const auto& command : ignored_commands) { ignored_commands_.insert(command); }
}

void ActorGroup::createNeuralNetworks()
{
    int num_networks = std::min(static_cast<int>(torch::cuda::device_count()), config::actor_num_parallel_games);
    assert(num_networks > 0);
    getSharedData()->networks_.resize(num_networks);
    getSharedData()->network_outputs_.resize(num_networks);
    for (int gpu_id = 0; gpu_id < num_networks; ++gpu_id) {
        getSharedData()->networks_[gpu_id] = createNetwork(config::nn_file_name, gpu_id);
    }
}

void ActorGroup::createEnvironmentServer()
{
    if (!config::actor_use_remote_environment) { return; }

    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), config::actor_server_port);
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();
    acceptor_.accept(socket_);
    boost::asio::write(socket_, boost::asio::buffer(config::zero_training_directory + "\n"));
    boost::asio::write(socket_, boost::asio::buffer("num_actors " + std::to_string(getSharedData()->actors_.size()) + "\n"));
    handleRemoteEnvMessage();
}

void ActorGroup::createActors()
{
    assert(getSharedData()->networks_.size() > 0);
    std::shared_ptr<Network>& network = getSharedData()->networks_[0];
    uint64_t tree_node_size = static_cast<uint64_t>(config::actor_num_simulation + 1) * network->getActionSize();
    for (int i = 0; i < config::actor_num_parallel_games; ++i) {
        getSharedData()->actors_.emplace_back(createActor(tree_node_size, getSharedData()->networks_[i % getSharedData()->networks_.size()]));
    }
}

void ActorGroup::handleIO()
{
    std::string command;
    const int buffer_size = 10000000;
    command.reserve(buffer_size);
    while (getline(std::cin, command)) {
        std::lock_guard<std::mutex> lock(getSharedData()->mutex_);
        commands_.push_back(command);
    }
}

void ActorGroup::handleFinishedGame()
{
    handleRemoteEnvAct();
    for (size_t actor_id = 0; actor_id < getSharedData()->actors_.size(); ++actor_id) {
        std::shared_ptr<BaseActor>& actor = getSharedData()->actors_[actor_id];
        if (!actor->isSearchDone()) { continue; }
        bool is_endgame = (actor->isResign() || actor->isEnvTerminal());
        bool display_game = (actor_id == 0 && (config::actor_num_simulation >= 50 || (config::actor_num_simulation < 50 && is_endgame)));
        if (display_game) { std::cerr << actor->getEnvironment().toString() << actor->getSearchInfo() << std::endl; }
        if (is_endgame) {
            std::cout << actor->getRecord() << std::endl;
            actor->reset();
        } else {
            actor->resetSearch();
        }
    }
}

void ActorGroup::handleRemoteEnvAct()
{
    if (!config::actor_use_remote_environment) { return; }

    std::string act_string = "";
    for (size_t actor_id = 0; actor_id < getSharedData()->actors_.size(); ++actor_id) {
        std::shared_ptr<BaseActor>& actor = getSharedData()->actors_[actor_id];
        if (!actor->isSearchDone()) { continue; }
        if (actor_id == 0) { std::cerr << actor->getEnvironment().toString() << actor->getSearchInfo() << std::endl; }
        act_string += " " + std::to_string(actor_id) + " " + std::to_string(actor->getEnvironment().getActionHistory().back().getActionID());
        actor->resetSearch();
    }
    if (act_string.empty()) { return; }

    boost::asio::write(socket_, boost::asio::buffer("act" + act_string + "\n"));
    handleRemoteEnvMessage();
}

void ActorGroup::handleRemoteEnvMessage()
{
    if (!config::actor_use_remote_environment) { return; }

    std::string recevied_message;
    boost::asio::read_until(socket_, boost::asio::dynamic_buffer(recevied_message), '\n');
    recevied_message.pop_back(); // remove new line character

    size_t index = 0, dim_index = 0;
    while ((dim_index = recevied_message.find(';', index)) != std::string::npos) {
        std::string message = recevied_message.substr(index, dim_index - index);
        std::string message_prefix = message.substr(0, message.find(" "));
        if (message_prefix == "obs") {
            size_t first_space = message.find(" ");
            size_t second_space = message.find(" ", first_space + 1);
            int actor_id = std::stoi(message.substr(first_space + 1, second_space - first_space - 1));
            getSharedData()->observation_map_[actor_id] = message.substr(second_space + 1);
        } else if (message_prefix == "game") {
            std::vector<std::string> game_string = utils::stringToVector(message);
            std::shared_ptr<BaseActor>& actor = getSharedData()->actors_[std::stoi(game_string[1])];
            std::unordered_map<std::string, std::string> tags;
            for (size_t i = 2; i < game_string.size(); i += 2) { tags.insert({game_string[i], game_string[i + 1]}); }
            std::cout << actor->getRecord(tags) << std::endl;
            actor->reset();
        }
        index = dim_index + 1;
    }
}

void ActorGroup::handleCommand()
{
    if (commands_.empty() || !getSharedData()->do_cpu_job_) { return; }

    std::lock_guard<std::mutex> lock(getSharedData()->mutex_);
    while (!commands_.empty()) {
        const std::string command = commands_.front();
        commands_.pop_front();

        // ignore specific command
        std::string command_prefix = ((command.find(" ") == std::string::npos) ? command : command.substr(0, command.find(" ")));
        if (ignored_commands_.count(command_prefix)) {
            std::cerr << "[ignored command] " << command << std::endl;
            continue;
        }

        // do command
        handleCommand(command_prefix, command);
    }
}

void ActorGroup::handleCommand(const std::string& command_prefix, const std::string& command)
{
    if (command_prefix == "reset_actors") {
        std::cerr << "[command] " << command << std::endl;
        for (auto& actor : getSharedData()->actors_) { actor->reset(); }
        getSharedData()->do_cpu_job_ = true;
    } else if (command_prefix == "load_model") {
        std::cerr << "[command] " << command << std::endl;
        std::vector<std::string> args = utils::stringToVector(command);
        assert(args.size() == 2);
        config::nn_file_name = args[1];
        for (auto& network : getSharedData()->networks_) { network->loadModel(config::nn_file_name, network->getGPUID()); }
    } else if (command_prefix == "start") {
        std::cerr << "[command] " << command << std::endl;
        running_ = true;
    } else if (command_prefix == "stop") {
        std::cerr << "[command] " << command << std::endl;
        running_ = false;
    } else if (command_prefix == "quit") {
        std::cerr << "[command] " << command << std::endl;
        exit(0);
    }
}

} // namespace minizero::actor
