#include "actor_group.h"
#include "alphazero_actor.h"
#include "alphazero_network.h"
#include "muzero_network.h"
#include "random.h"
#include <iostream>
#include <torch/cuda.h>

namespace minizero::actor {

using namespace network;
using namespace utils;

int ThreadSharedData::GetNextActorIndex()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return actor_index_++;
}

void ThreadSharedData::OutputRecord(const std::string& record)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << record << std::endl;
}

void SlaveThread::RunThread()
{
    int seed = config::auto_seed ? std::random_device()() : config::seed;
    Random::Seed(seed);
    while (true) {
        start_barrier_.wait();
        if (shared_data_.do_cpu_job_) {
            DoCPUJob();
        } else {
            DoGPUJob();
        }
        finish_barrier_.wait();
    }
}

void SlaveThread::DoCPUJob()
{
    size_t actor_id = shared_data_.GetNextActorIndex();
    while (actor_id < shared_data_.actors_.size()) {
        std::shared_ptr<Actor>& actor = shared_data_.actors_[actor_id];
        int network_id = actor_id % shared_data_.networks_.size();
        int network_output_id = actor->GetEvaluationJobIndex();
        if (network_output_id >= 0) {
            actor->AfterNNEvaluation(shared_data_.network_outputs_[network_id][network_output_id]);
            HandleSearchEndAndEnvEnd(actor, actor_id == 0);
        }
        actor->BeforeNNEvaluation(shared_data_.networks_[network_id]);
        actor_id = shared_data_.GetNextActorIndex();
    }
}

void SlaveThread::HandleSearchEndAndEnvEnd(const std::shared_ptr<Actor>& actor, bool display /*= false*/)
{
    if (!actor->ReachMaximumSimulation()) { return; }
    Action action = actor->GetMCTSTree().DecideAction();
    const MCTSTreeNode* root = actor->GetMCTSTree().GetRootNode();
    MCTSTreeNode* child = root->GetFirstChild();
    MCTSTreeNode* action_node = nullptr;
    for (int i = 0; i < root->GetNumChildren(); ++i, ++child) {
        if (child->GetAction().GetActionID() != action.GetActionID()) { continue; }
        action_node = child;
        break;
    }
    assert(action_node && action_node->GetCount() > 0);

    std::string root_node_info = root->ToString();
    float win_rate = (action.GetPlayer() == env::Player::kPlayer1 ? action_node->GetMean() : -action_node->GetMean());
    if (!actor->IsEnableResign() || win_rate >= config::actor_resign_threshold) { actor->Act(action); }
    if (display) {
        std::cerr << actor->GetEnvironment().ToString();
        std::cerr << "move number: " << actor->GetEnvironment().GetActionHistory().size()
                  << ", action id: " << action.GetActionID() << std::endl;
        std::cerr << "  root node info: " << root_node_info << std::endl;
        std::cerr << "action node info: " << action_node->ToString() << std::endl
                  << std::endl;
    }

    if ((actor->IsEnableResign() && win_rate < config::actor_resign_threshold) || actor->IsTerminal()) {
        shared_data_.OutputRecord(actor->GetRecord());
        actor->Reset();
    }
}

void SlaveThread::DoGPUJob()
{
    if (id_ >= static_cast<int>(shared_data_.networks_.size())) { return; }

    std::shared_ptr<Network>& network = shared_data_.networks_[id_];
    if (network->GetNetworkTypeName() == "alphazero") {
        shared_data_.network_outputs_[id_] = std::static_pointer_cast<AlphaZeroNetwork>(network)->Forward();
    } else if (network->GetNetworkTypeName() == "muzero") {
        // TODO
    }
}

ActorGroup::ActorGroup()
{
    // threads
    for (int id = 0; id < config::actor_num_threads; ++id) {
        slave_threads_.emplace_back(std::make_shared<SlaveThread>(id, shared_data_));
        thread_groups_.create_thread(boost::bind(&SlaveThread::RunThread, slave_threads_.back()));
    }

    // networks
    assert(torch::cuda::device_count() > 0);
    shared_data_.networks_.resize(torch::cuda::device_count());
    shared_data_.network_outputs_.resize(torch::cuda::device_count());
    for (size_t gpu_id = 0; gpu_id < torch::cuda::device_count(); ++gpu_id) {
        shared_data_.networks_[gpu_id] = CreateNetwork(config::nn_file_name, gpu_id);
    }

    // actors
    std::shared_ptr<Network>& network = shared_data_.networks_[0];
    long long mcts_tree_node_size = static_cast<long long>(config::actor_num_simulation);
    if (network->GetNetworkTypeName() == "alphazero") {
        mcts_tree_node_size *= std::static_pointer_cast<AlphaZeroNetwork>(network)->GetActionSize();
        for (int i = 0; i < config::actor_num_parallel_games; ++i) {
            shared_data_.actors_.emplace_back(std::make_shared<AlphaZeroActor>(mcts_tree_node_size));
            shared_data_.actors_.back()->Reset();
        }
    } else if (network->GetNetworkTypeName() == "muzero") {
        mcts_tree_node_size *= std::static_pointer_cast<MuZeroNetwork>(network)->GetActionSize();
        // TODO
    }
}

void ActorGroup::Run()
{
    shared_data_.do_cpu_job_ = true;
    while (true) {
        shared_data_.actor_index_ = 0;
        for (auto t : slave_threads_) { t->Start(); }
        for (auto t : slave_threads_) { t->Finish(); }
        shared_data_.do_cpu_job_ = !shared_data_.do_cpu_job_;
    }
}

} // namespace minizero::actor