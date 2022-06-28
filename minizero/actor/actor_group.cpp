#include "actor_group.h"
#include "alphazero_network.h"
#include "configuration.h"
#include "muzero_network.h"
#include "random.h"
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
    shared_data_.do_cpu_job_ = true;
    while (true) {
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

void ActorGroup::handleFinishedGame()
{
    for (size_t actor_id = 0; actor_id < shared_data_.actors_.size(); ++actor_id) {
        std::shared_ptr<BaseActor>& actor = shared_data_.actors_[actor_id];
        if (!actor->isSearchDone()) { continue; }
        if (actor_id == 0 && config::actor_num_simulation >= 100) { std::cerr << actor->getEnvironment().toString() << actor->getSearchInfo() << std::endl; }
        if (actor->isResign() || actor->isEnvTerminal()) {
            if (actor_id == 0 && config::actor_num_simulation < 100) { std::cerr << actor->getEnvironment().toString() << actor->getSearchInfo() << std::endl; }
            std::cout << actor->getRecord() << std::endl;
            actor->reset();
        } else {
            actor->resetSearch();
        }
    }
}

} // namespace minizero::actor