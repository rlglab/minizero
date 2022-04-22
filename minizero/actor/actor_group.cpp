#include "actor_group.h"
#include "alphazero_network.h"
#include "muzero_network.h"
#include "random.h"
#include <iostream>
#include <torch/cuda.h>

namespace minizero::actor {

using namespace network;
using namespace utils;

int ThreadSharedData::getNextActorIndex()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return actor_index_++;
}

void ThreadSharedData::resetActor(int actor_id)
{
    assert(actor_id < static_cast<int>(actors_.size()));
    actors_[actor_id]->reset();
    actors_enable_resign_[actor_id] = (utils::Random::randReal() < config::zero_disable_resign_ratio ? 0 : 1);
}

void ThreadSharedData::outputRecord(const std::string& record)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << record << std::endl;
}

void SlaveThread::runThread()
{
    int seed = config::auto_seed ? std::random_device()() : config::seed;
    Random::seed(seed);
    while (true) {
        start_barrier_.wait();
        if (shared_data_.do_cpu_job_) {
            doCPUJob();
        } else {
            doGPUJob();
        }
        finish_barrier_.wait();
    }
}

void SlaveThread::doCPUJob()
{
    size_t actor_id = shared_data_.getNextActorIndex();
    while (actor_id < shared_data_.actors_.size()) {
        std::shared_ptr<Actor>& actor = shared_data_.actors_[actor_id];
        int network_id = actor_id % shared_data_.networks_.size();
        int network_output_id = actor->getEvaluationJobIndex();
        if (network_output_id >= 0) {
            assert(network_output_id < static_cast<int>(shared_data_.network_outputs_[network_id].size()));
            actor->afterNNEvaluation(shared_data_.network_outputs_[network_id][network_output_id]);
            handleSearchEndAndEnvEnd(actor_id);
        }
        actor->beforeNNEvaluation();
        actor_id = shared_data_.getNextActorIndex();
    }
}

void SlaveThread::handleSearchEndAndEnvEnd(int actor_id)
{
    std::shared_ptr<Actor>& actor = shared_data_.actors_[actor_id];
    if (!actor->reachMaximumSimulation()) { return; }

    const MCTSNode* selected_node = actor->decideActionNode();
    const Action& action = selected_node->getAction();
    bool is_resign = shared_data_.actors_enable_resign_[actor_id] && actor->isResign(selected_node);
    if (!is_resign) { actor->act(action, actor->getActionComment()); }
    if (actor_id == 0 && config::actor_num_simulation >= 100) { actor->displayBoard(selected_node); }
    if (is_resign || actor->isTerminal()) {
        if (actor_id == 0 && config::actor_num_simulation < 100) { actor->displayBoard(selected_node); }
        shared_data_.outputRecord(actor->getRecord());
        shared_data_.resetActor(actor_id);
    } else {
        actor->resetSearch();
    }
}

void SlaveThread::doGPUJob()
{
    if (id_ >= static_cast<int>(shared_data_.networks_.size())) { return; }

    std::shared_ptr<Network>& network = shared_data_.networks_[id_];
    if (network->getNetworkTypeName() == "alphazero") {
        shared_data_.network_outputs_[id_] = std::static_pointer_cast<AlphaZeroNetwork>(network)->forward();
    } else if (network->getNetworkTypeName() == "muzero") {
        if (shared_data_.actors_[0]->getCurrentSimulation() == 0) { // root forward, need to call initial inference
            shared_data_.network_outputs_[id_] = std::static_pointer_cast<MuZeroNetwork>(network)->initialInference();
        } else { // recurrent inference
            shared_data_.network_outputs_[id_] = std::static_pointer_cast<MuZeroNetwork>(network)->recurrentInference();
        }
    }
}

ActorGroup::ActorGroup()
{
    // create CPU & GPU threads
    for (int id = 0; id < std::max(static_cast<int>(torch::cuda::device_count()), config::actor_num_threads); ++id) {
        slave_threads_.emplace_back(std::make_shared<SlaveThread>(id, shared_data_));
        thread_groups_.create_thread(boost::bind(&SlaveThread::runThread, slave_threads_.back()));
    }

    // create networks
    int num_networks = std::min(static_cast<int>(torch::cuda::device_count()), config::actor_num_parallel_games);
    assert(num_networks > 0);
    shared_data_.networks_.resize(num_networks);
    shared_data_.network_outputs_.resize(num_networks);
    for (int gpu_id = 0; gpu_id < num_networks; ++gpu_id) {
        shared_data_.networks_[gpu_id] = createNetwork(config::nn_file_name, gpu_id);
    }

    // create actors
    std::shared_ptr<Network>& network = shared_data_.networks_[0];
    long long tree_node_size = static_cast<long long>(config::actor_num_simulation + 1) * network->getActionSize();
    shared_data_.actors_enable_resign_.resize(config::actor_num_parallel_games);
    for (int i = 0; i < config::actor_num_parallel_games; ++i) {
        shared_data_.actors_.emplace_back(createActor(tree_node_size, shared_data_.networks_[i % shared_data_.networks_.size()]));
        shared_data_.resetActor(i);
    }
}

void ActorGroup::run()
{
    shared_data_.do_cpu_job_ = true;
    while (true) {
        shared_data_.actor_index_ = 0;
        for (auto t : slave_threads_) { t->start(); }
        for (auto t : slave_threads_) { t->finish(); }
        shared_data_.do_cpu_job_ = !shared_data_.do_cpu_job_;
    }
}

} // namespace minizero::actor