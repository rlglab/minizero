#include "alphazero_actor.h"
#include "alphazero_network.h"
#include "configuration.h"

#include <iostream>

namespace minizero::actor {

using namespace network;

void AlphaZeroActor::Reset()
{
    Actor::Reset();
    action_distributions_.clear();
}

bool AlphaZeroActor::Act(const Action& action)
{
    bool can_act = env_.Act(action);
    if (can_act) {
        action_distributions_.push_back(tree_.GetActionDistributionString());
        ClearTree();
    }
    return can_act;
}

void AlphaZeroActor::BeforeNNEvaluation(const std::shared_ptr<network::Network>& network)
{
    std::vector<MCTSTreeNode*> node_path = tree_.Select();
    Environment env_transition = GetEnvironmentTransition(node_path);
    std::shared_ptr<AlphaZeroNetwork> alphazero_network = std::static_pointer_cast<AlphaZeroNetwork>(network);
    evaluation_jobs_ = {node_path, alphazero_network->PushBack(env_transition.GetFeatures())};
}

void AlphaZeroActor::AfterNNEvaluation(const std::shared_ptr<NetworkOutput>& network_output)
{
    std::vector<MCTSTreeNode*> node_path = evaluation_jobs_.first;
    Environment env_transition = GetEnvironmentTransition(evaluation_jobs_.first);
    if (!env_transition.IsTerminal()) {
        std::shared_ptr<AlphaZeroNetworkOutput> output = std::static_pointer_cast<AlphaZeroNetworkOutput>(network_output);
        tree_.Expand(node_path.back(), CalculateActionPolicy(output->policy_, env_transition));
        tree_.Backup(node_path, output->value_);
    } else {
        tree_.Backup(node_path, env_transition.GetEvalScore());
    }
}

std::string AlphaZeroActor::GetRecord() const
{
    EnvironmentLoader env_loader;
    env_loader.LoadFromEnvironment(env_, action_distributions_);
    return env_loader.ToString();
}

std::vector<std::pair<Action, float>> AlphaZeroActor::CalculateActionPolicy(const std::vector<float>& policy, const Environment& env_transition)
{
    std::vector<std::pair<Action, float>> action_policy;
    for (size_t action_id = 0; action_id < policy.size(); ++action_id) {
        Action action(action_id, env_transition.GetTurn());
        if (!env_transition.IsLegalAction(action)) { continue; }
        action_policy.push_back({action, policy[action_id]});
    }
    return action_policy;
}

Environment AlphaZeroActor::GetEnvironmentTransition(const std::vector<MCTSTreeNode*>& node_path)
{
    Environment env = env_;
    for (size_t i = 1; i < node_path.size(); ++i) { env.Act(node_path[i]->GetAction()); }
    return env;
}

} // namespace minizero::actor
