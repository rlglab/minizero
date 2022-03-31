#include "alphazero_actor.h"
#include "alphazero_network.h"

namespace minizero::actor {

using namespace minizero;
using namespace network;

MCTSTreeNode* AlphaZeroActor::runMCTS(std::shared_ptr<Network>& network)
{
    std::shared_ptr<AlphaZeroNetwork> az_network = std::static_pointer_cast<AlphaZeroNetwork>(network);
    tree_.reset();
    evaluation_jobs_ = {{}, -1};
    while (!reachMaximumSimulation()) {
        beforeNNEvaluation(network);
        std::vector<std::shared_ptr<NetworkOutput>> outputs = az_network->forward();
        afterNNEvaluation(outputs[getEvaluationJobIndex()]);
    }
    return getMCTSTree().decideActionNode();
}

void AlphaZeroActor::beforeNNEvaluation(const std::shared_ptr<Network>& network)
{
    std::vector<MCTSTreeNode*> node_path = tree_.select();
    Environment env_transition = getEnvironmentTransition(node_path);
    std::shared_ptr<AlphaZeroNetwork> alphazero_network = std::static_pointer_cast<AlphaZeroNetwork>(network);
    evaluation_jobs_ = {node_path, alphazero_network->pushBack(env_transition.getFeatures())};
}

void AlphaZeroActor::afterNNEvaluation(const std::shared_ptr<NetworkOutput>& network_output)
{
    std::vector<MCTSTreeNode*> node_path = evaluation_jobs_.first;
    Environment env_transition = getEnvironmentTransition(evaluation_jobs_.first);
    if (!env_transition.isTerminal()) {
        std::shared_ptr<AlphaZeroNetworkOutput> output = std::static_pointer_cast<AlphaZeroNetworkOutput>(network_output);
        tree_.expand(node_path.back(), calculateActionPolicy(output->policy_, env_transition));
        tree_.backup(node_path, output->value_);
    } else {
        tree_.backup(node_path, env_transition.getEvalScore());
    }
}

std::vector<std::pair<Action, float>> AlphaZeroActor::calculateActionPolicy(const std::vector<float>& policy, const Environment& env_transition)
{
    std::vector<std::pair<Action, float>> action_policy;
    for (size_t action_id = 0; action_id < policy.size(); ++action_id) {
        Action action(action_id, env_transition.getTurn());
        if (!env_transition.isLegalAction(action)) { continue; }
        action_policy.push_back({action, policy[action_id]});
    }
    return action_policy;
}

Environment AlphaZeroActor::getEnvironmentTransition(const std::vector<MCTSTreeNode*>& node_path)
{
    Environment env = env_;
    for (size_t i = 1; i < node_path.size(); ++i) { env.act(node_path[i]->getAction()); }
    return env;
}

} // namespace minizero::actor
