#include "actor_group.h"
#include "alphazero_network.h"
#include "configuration.h"
#include "create_actor.h"
#include "create_network.h"
#include "muzero_network.h"
#include "random.h"
#include "utils.h"
#include <iostream>
#include <string>
#include <torch/cuda.h>

namespace minizero::actor {

using namespace network;
using namespace utils;

int ThreadSharedData::getAvailableActorIndex()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return actor_index_++;
}

void SlaveThread::initialize()
{
    int seed = config::program_auto_seed ? std::random_device()() : config::program_seed;
    Random::seed(seed);
}

void SlaveThread::runJob()
{
    if (shared_data_.do_cpu_job_) {
        while (doCPUJob()) {}
    } else {
        doGPUJob();
    }
}

bool SlaveThread::doCPUJob()
{
    size_t actor_id = shared_data_.getAvailableActorIndex();
    if (actor_id >= shared_data_.actors_.size()) { return false; }

    std::shared_ptr<BaseActor>& actor = shared_data_.actors_[actor_id];
    int network_id = actor_id % shared_data_.networks_.size();
    int network_output_id = actor->getNNEvaluationBatchIndex();
    if (network_output_id >= 0) {
        assert(network_output_id < static_cast<int>(shared_data_.network_outputs_[network_id].size()));
        actor->afterNNEvaluation(shared_data_.network_outputs_[network_id][network_output_id]);
        if (actor->isSearchDone() && !actor->isResign()) { actor->act(actor->getSearchAction(), actor->getActionComment()); }
    }
    if (!actor->isSearchDone()) { actor->beforeNNEvaluation(); }
    return true;
}

void SlaveThread::doGPUJob()
{
    if (id_ >= static_cast<int>(shared_data_.networks_.size())) { return; }

    std::shared_ptr<Network>& network = shared_data_.networks_[id_];
    if (network->getNetworkTypeName() == "alphazero") {
        std::shared_ptr<AlphaZeroNetwork> az_network = std::static_pointer_cast<AlphaZeroNetwork>(network);
        if (az_network->getBatchSize() > 0) { shared_data_.network_outputs_[id_] = az_network->forward(); }
    } else if (network->getNetworkTypeName() == "muzero" || network->getNetworkTypeName() == "muzero_reward") {
        std::shared_ptr<MuZeroNetwork> muzero_network = std::static_pointer_cast<MuZeroNetwork>(network);
        if (muzero_network->getInitialInputBatchSize() > 0) {
            shared_data_.network_outputs_[id_] = std::static_pointer_cast<MuZeroNetwork>(network)->initialInference();
        } else if (muzero_network->getRecurrentInputBatchSize() > 0) {
            shared_data_.network_outputs_[id_] = std::static_pointer_cast<MuZeroNetwork>(network)->recurrentInference();
        }
    }
}

void ActorGroup::run()
{
    initialize();
    while (true) {
        handleCommand();

        if (!running_) { continue; }
        shared_data_.actor_index_ = 0;
        for (auto& t : slave_threads_) { t->start(); }
        for (auto& t : slave_threads_) { t->finish(); }
        handleFinishedGame();
        shared_data_.do_cpu_job_ = !shared_data_.do_cpu_job_;
    }
}

void ActorGroup::initialize()
{
    int num_threads = std::max(static_cast<int>(torch::cuda::device_count()), config::actor_num_threads);
    createSlaveThreads(num_threads);
    createNeuralNetworks();
    createActors();
    running_ = false;
    shared_data_.do_cpu_job_ = true;

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
    shared_data_.networks_.resize(num_networks);
    shared_data_.network_outputs_.resize(num_networks);
    for (int gpu_id = 0; gpu_id < num_networks; ++gpu_id) {
        shared_data_.networks_[gpu_id] = createNetwork(config::nn_file_name, gpu_id);
    }
}

void ActorGroup::createActors()
{
    assert(shared_data_.networks_.size() > 0);
    std::shared_ptr<Network>& network = shared_data_.networks_[0];
    long long tree_node_size = static_cast<long long>(config::actor_num_simulation + 1) * network->getActionSize();
    for (int i = 0; i < config::actor_num_parallel_games; ++i) {
        shared_data_.actors_.emplace_back(createActor(tree_node_size, shared_data_.networks_[i % shared_data_.networks_.size()]));
    }
}

void ActorGroup::handleIO()
{
    std::string command;
    const int buffer_size = 10000000;
    command.reserve(buffer_size);
    while (getline(std::cin, command)) {
        std::lock_guard<std::mutex> lock(shared_data_.mutex_);
        commands_.push_back(command);
    }
}

void ActorGroup::handleFinishedGame()
{
    for (size_t actor_id = 0; actor_id < shared_data_.actors_.size(); ++actor_id) {
        std::shared_ptr<BaseActor>& actor = shared_data_.actors_[actor_id];
        if (!actor->isSearchDone()) { continue; }
        bool is_endgame = (actor->isResign() || actor->isEnvTerminal());
        bool display_game = (actor_id == 0 && (config::actor_num_simulation >= 100 || (config::actor_num_simulation < 100 && is_endgame)));
        if (display_game) { std::cerr << actor->getEnvironment().toString() << actor->getSearchInfo() << std::endl; }
        if (is_endgame) {
            std::cout << actor->getRecord() << std::endl;
            actor->reset();
        } else {
            actor->resetSearch();
        }
    }
}

void ActorGroup::handleCommand()
{
    if (commands_.empty()) { return; }

    std::lock_guard<std::mutex> lock(shared_data_.mutex_);
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
        if (command_prefix == "reset_actors") {
            std::cerr << "[command] " << command << std::endl;
            for (auto& actor : shared_data_.actors_) { actor->reset(); }
            shared_data_.do_cpu_job_ = true;
        } else if (command_prefix == "load_model") {
            std::cerr << "[command] " << command << std::endl;
            std::vector<std::string> args = utils::stringToVector(command);
            assert(args.size() == 2);
            config::nn_file_name = args[1];
            for (auto& network : shared_data_.networks_) { network->loadModel(config::nn_file_name, network->getGPUID()); }
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
}

} // namespace minizero::actor