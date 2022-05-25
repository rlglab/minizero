#include "killallgo_actor_group.h"
#include "random.h"
#include <iostream>
#include <string>
#include <torch/cuda.h>

namespace solver {

using namespace minizero;
using namespace minizero::network;
using namespace minizero::utils;

int ThreadSharedData::getAvailableActorIndex()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return actor_index_++;
}

void ThreadSharedData::outputRecord(const std::string& record)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << record << std::endl;
}

void SlaveThread::initialize()
{
    int seed = config::auto_seed ? std::random_device()() : config::seed;
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

    std::shared_ptr<KillallGoActor>& actor = shared_data_.actors_[actor_id];
    int network_id = actor_id % shared_data_.networks_.size();
    int network_output_id = actor->getNNEvaluationBatchIndex();
    if (network_output_id >= 0) {
        assert(network_output_id < static_cast<int>(shared_data_.network_outputs_[network_id].size()));
        actor->afterNNEvaluation(shared_data_.network_outputs_[network_id][network_output_id]);
        if (actor->isSearchDone()) { // handle search end
            if (!actor->isResign()) { actor->act(actor->getSearchAction(), actor->getActionComment()); }
            if (actor_id == 0 && config::actor_num_simulation >= 100) { actor->displayBoard(); }
            if (actor->isResign() || actor->isEnvTerminal()) {
                if (actor_id == 0 && config::actor_num_simulation < 100) { actor->displayBoard(); }
                shared_data_.outputRecord(actor->getRecord());
                actor->reset();
            } else {
                actor->resetSearch();
            }
        }
    }
    actor->beforeNNEvaluation();
    return true;
}

void SlaveThread::doGPUJob()
{
    if (id_ >= static_cast<int>(shared_data_.networks_.size())) { return; }
    shared_data_.network_outputs_[id_] = shared_data_.networks_[id_]->forward();
}

void KillallGoActorGroup::run()
{
    initialize();
    shared_data_.do_cpu_job_ = true;
    while (true) {
        shared_data_.actor_index_ = 0;
        for (auto& t : slave_threads_) { t->start(); }
        for (auto& t : slave_threads_) { t->finish(); }
        shared_data_.do_cpu_job_ = !shared_data_.do_cpu_job_;
    }
}

void KillallGoActorGroup::initialize()
{
    int num_threads = std::max(static_cast<int>(torch::cuda::device_count()), config::actor_num_threads);
    createSlaveThreads(num_threads);
    createNeuralNetworks();
    createActors();
}

void KillallGoActorGroup::createNeuralNetworks()
{
    int num_networks = std::min(static_cast<int>(torch::cuda::device_count()), config::actor_num_parallel_games);
    assert(num_networks > 0);
    shared_data_.networks_.resize(num_networks);
    shared_data_.network_outputs_.resize(num_networks);
    for (int gpu_id = 0; gpu_id < num_networks; ++gpu_id) {
        shared_data_.networks_[gpu_id] = std::make_shared<ProofCostNetwork>();
        shared_data_.networks_[gpu_id]->loadModel(config::nn_file_name, gpu_id);
    }
}

void KillallGoActorGroup::createActors()
{
    assert(shared_data_.networks_.size() > 0);
    long long tree_node_size = static_cast<long long>(config::actor_num_simulation + 1) * shared_data_.networks_[0]->getActionSize();
    for (int i = 0; i < config::actor_num_parallel_games; ++i) {
        shared_data_.actors_.emplace_back(std::make_shared<KillallGoActor>(tree_node_size));
        shared_data_.actors_.back()->setNetwork(shared_data_.networks_[i % shared_data_.networks_.size()]);
        shared_data_.actors_.back()->reset();
    }
}

} // namespace solver