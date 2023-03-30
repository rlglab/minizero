#include "actor_group.h"
#include "configuration.h"
#include "create_actor.h"
#include "create_network.h"
#include "random.h"
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <torch/cuda.h>
#include <utility>

namespace minizero::actor {

using namespace network;
using namespace utils;

int ThreadSharedData::getAvailableActorIndex()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return (actor_index_ < static_cast<int>(actors_.size()) ? actor_index_++ : actors_.size());
}

void ThreadSharedData::outputGame(const std::shared_ptr<BaseActor>& actor)
{
    int game_length = actor->getEnvironment().getActionHistory().size();
    std::pair<int, int> data_range = calculateTrainingDataRange(actor);

    std::ostringstream oss;
    oss << "SelfPlay "
        << (data_range.second - data_range.first + 1) << " "                                                               // data length
        << game_length << " "                                                                                              // game length
        << actor->getEnvironment().getEvalScore(!actor->isEnvTerminal()) << " "                                            // return
        << actor->getRecord({{"DLEN", std::to_string(data_range.first) + "-" + std::to_string(data_range.second)}}) << " " // game record
        << "#";                                                                                                            // end mark for a valid game

    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << oss.str() << std::endl;
}

std::pair<int, int> ThreadSharedData::calculateTrainingDataRange(const std::shared_ptr<BaseActor>& actor)
{
    int game_length = actor->getEnvironment().getActionHistory().size();
    int data_start = 0, data_end = game_length - 1;
    if (config::zero_actor_intermediate_sequence_length > 0) {
        data_end = std::max(0, (actor->getEnvironment().isTerminal() ? data_end : game_length - (config::learner_n_step_return - 1)));
        data_start = std::max(0, (actor->getEnvironment().isTerminal() ? data_end - data_end % config::zero_actor_intermediate_sequence_length : data_end + 1 - config::zero_actor_intermediate_sequence_length));
    }
    return {data_start, data_end};
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

    std::shared_ptr<BaseActor>& actor = getSharedData()->actors_[actor_id];
    int network_id = actor_id % getSharedData()->networks_.size();
    int network_output_id = actor->getNNEvaluationBatchIndex();
    if (network_output_id >= 0) {
        assert(network_output_id < static_cast<int>(getSharedData()->network_outputs_[network_id].size()));
        actor->afterNNEvaluation(getSharedData()->network_outputs_[network_id][network_output_id]);
        if (actor->isSearchDone()) { handleSearchDone(actor_id); }
    }
    actor->beforeNNEvaluation();
    return true;
}

void SlaveThread::doGPUJob()
{
    if (id_ >= static_cast<int>(getSharedData()->networks_.size())) { return; }

    std::shared_ptr<Network>& network = getSharedData()->networks_[id_];
    if (network->getNetworkTypeName() == "alphazero") {
        std::shared_ptr<AlphaZeroNetwork> az_network = std::static_pointer_cast<AlphaZeroNetwork>(network);
        if (az_network->getBatchSize() > 0) { getSharedData()->network_outputs_[id_] = az_network->forward(); }
    } else if (network->getNetworkTypeName() == "muzero" || network->getNetworkTypeName() == "muzero_atari") {
        std::shared_ptr<MuZeroNetwork> muzero_network = std::static_pointer_cast<MuZeroNetwork>(network);
        if (muzero_network->getInitialInputBatchSize() > 0) {
            getSharedData()->network_outputs_[id_] = std::static_pointer_cast<MuZeroNetwork>(network)->initialInference();
        } else if (muzero_network->getRecurrentInputBatchSize() > 0) {
            getSharedData()->network_outputs_[id_] = std::static_pointer_cast<MuZeroNetwork>(network)->recurrentInference();
        }
    }
}

void SlaveThread::handleSearchDone(int actor_id)
{
    assert(actor_id >= 0 && actor_id < static_cast<int>(getSharedData()->actors_.size()) && getSharedData()->actors_[actor_id]->isSearchDone());

    std::shared_ptr<BaseActor>& actor = getSharedData()->actors_[actor_id];
    if (!actor->isResign()) { actor->act(actor->getSearchAction()); }
    bool is_endgame = (actor->isResign() || actor->isEnvTerminal());
    bool display_game = (actor_id == 0 && (config::actor_num_simulation >= 50 || (config::actor_num_simulation < 50 && is_endgame)));
    if (display_game) { std::cerr << actor->getEnvironment().toString() << actor->getSearchInfo() << std::endl; }
    if (is_endgame) {
        getSharedData()->outputGame(actor);
        actor->reset();
    } else {
        int game_length = actor->getEnvironment().getActionHistory().size();
        int sequence_length = config::zero_actor_intermediate_sequence_length;
        if (sequence_length > 0 && game_length > sequence_length && (game_length - config::learner_n_step_return + 1) % sequence_length == sequence_length - 1) { getSharedData()->outputGame(actor); }
        actor->resetSearch();
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
        getSharedData()->do_cpu_job_ = !getSharedData()->do_cpu_job_;
    }
}

void ActorGroup::initialize()
{
    int num_threads = std::max(static_cast<int>(torch::cuda::device_count()), config::actor_num_threads);
    createSlaveThreads(num_threads);
    createNeuralNetworks();
    createActors();
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
