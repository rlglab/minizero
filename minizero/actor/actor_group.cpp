#include "actor_group.h"
#include "alphazero_actor.h"
#include "alphazero_network.h"
#include "muzero_actor.h"
#include "muzero_network.h"
#include "random.h"
#include "time_system.h"
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
            handleSearchEndAndEnvEnd(actor, actor_id == 0);
        }
        actor->beforeNNEvaluation(shared_data_.networks_[network_id]);
        actor_id = shared_data_.getNextActorIndex();
    }
}

void SlaveThread::handleSearchEndAndEnvEnd(const std::shared_ptr<Actor>& actor, bool display /*= false*/)
{
    if (!actor->reachMaximumSimulation()) { return; }
    const MCTSTreeNode* root = actor->getMCTSTree().getRootNode();
    const MCTSTreeNode* action_node = actor->getMCTSTree().decideActionNode();
    Action action = action_node->getAction();

    std::string root_node_info = root->toString();
    float win_rate = (action.getPlayer() == env::Player::kPlayer1 ? action_node->getMean() : -action_node->getMean());
    if (!actor->isEnableResign() || win_rate >= config::actor_resign_threshold) { actor->act(action); }
    if (display) {
        std::string action_id_str = ((!actor->isEnableResign() || win_rate >= config::actor_resign_threshold) ? std::to_string(action.getActionID()) : "resign");
        std::cerr << actor->getEnvironment().toString();
        std::cerr << TimeSystem::getTimeString("[Y/m/d H:i:s.f] ")
                  << "move number: " << actor->getEnvironment().getActionHistory().size()
                  << ", action id: " << action_id_str
                  << ", action player: " << env::playerToChar(action.getPlayer()) << std::endl;
        std::cerr << "  root node info: " << root_node_info << std::endl;
        std::cerr << "action node info: " << action_node->toString() << std::endl
                  << std::endl;
    }

    if ((actor->isEnableResign() && win_rate < config::actor_resign_threshold) || actor->isTerminal()) {
        shared_data_.outputRecord(actor->getRecord());
        actor->reset();
    }
}

void SlaveThread::doGPUJob()
{
    if (id_ >= static_cast<int>(shared_data_.networks_.size()) || id_ >= config::actor_num_parallel_games) { return; }

    std::shared_ptr<Network>& network = shared_data_.networks_[id_];
    if (network->getNetworkTypeName() == "alphazero") {
        shared_data_.network_outputs_[id_] = std::static_pointer_cast<AlphaZeroNetwork>(network)->forward();
    } else if (network->getNetworkTypeName() == "muzero") {
        if (shared_data_.actors_[0]->getMCTSTree().getRootNode()->getCount() == 0) {
            // root forward, need to call initial inference
            shared_data_.network_outputs_[id_] = std::static_pointer_cast<MuZeroNetwork>(network)->initialInference();
        } else {
            // recurrent inference
            shared_data_.network_outputs_[id_] = std::static_pointer_cast<MuZeroNetwork>(network)->recurrentInference();
        }
    }
}

ActorGroup::ActorGroup()
{
    // threads
    for (int id = 0; id < std::max(static_cast<int>(torch::cuda::device_count()), config::actor_num_threads); ++id) {
        slave_threads_.emplace_back(std::make_shared<SlaveThread>(id, shared_data_));
        thread_groups_.create_thread(boost::bind(&SlaveThread::runThread, slave_threads_.back()));
    }

    // networks
    assert(torch::cuda::device_count() > 0);
    shared_data_.networks_.resize(torch::cuda::device_count());
    shared_data_.network_outputs_.resize(torch::cuda::device_count());
    for (size_t gpu_id = 0; gpu_id < torch::cuda::device_count(); ++gpu_id) {
        shared_data_.networks_[gpu_id] = createNetwork(config::nn_file_name, gpu_id);
    }

    // actors
    std::shared_ptr<Network>& network = shared_data_.networks_[0];
    long long mcts_tree_node_size = static_cast<long long>(config::actor_num_simulation);
    if (network->getNetworkTypeName() == "alphazero") {
        mcts_tree_node_size *= std::static_pointer_cast<AlphaZeroNetwork>(network)->getActionSize();
        for (int i = 0; i < config::actor_num_parallel_games; ++i) {
            shared_data_.actors_.emplace_back(std::make_shared<AlphaZeroActor>(mcts_tree_node_size));
            shared_data_.actors_.back()->reset();
        }
    } else if (network->getNetworkTypeName() == "muzero") {
        mcts_tree_node_size *= std::static_pointer_cast<MuZeroNetwork>(network)->getActionSize();
        for (int i = 0; i < config::actor_num_parallel_games; ++i) {
            shared_data_.actors_.emplace_back(std::make_shared<MuZeroActor>(mcts_tree_node_size));
            shared_data_.actors_.back()->reset();
        }
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