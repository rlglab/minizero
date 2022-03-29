#include "alphazero_actor.h"
#include "alphazero_network.h"
#include "configuration.h"
#include "random.h"

namespace minizero::actor {

using namespace minizero;
using namespace network;

void AlphaZeroActor::reset()
{
    env_.reset();
    clearTree();
    action_distributions_.clear();
    is_enable_resign_ = utils::Random::randReal() < config::zero_disable_resign_ratio ? false : true;
}

bool AlphaZeroActor::act(const Action& action)
{
    bool can_act = env_.act(action);
    if (can_act) {
        action_distributions_.push_back(tree_.getActionDistributionString());
        clearTree();
    }
    return can_act;
}

bool AlphaZeroActor::act(const std::vector<std::string>& action_string_args)
{
    bool can_act = env_.act(action_string_args);
    if (can_act) {
        action_distributions_.push_back(tree_.getActionDistributionString());
        clearTree();
    }
    return can_act;
}

MCTSTreeNode* AlphaZeroActor::runMCTS(std::shared_ptr<Network>& network)
{
    std::shared_ptr<AlphaZeroNetwork> az_network = std::static_pointer_cast<network::AlphaZeroNetwork>(network);
    clearTree();
    while (!reachMaximumSimulation()) {
        beforeNNEvaluation(network);
        std::vector<std::shared_ptr<NetworkOutput>> outputs = az_network->forward();
        afterNNEvaluation(outputs[getEvaluationJobIndex()]);
    }
    return getMCTSTree().decideActionNode();
}

void AlphaZeroActor::beforeNNEvaluation(const std::shared_ptr<network::Network>& network)
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

std::string AlphaZeroActor::getRecord() const
{
    EnvironmentLoader env_loader;
    env_loader.loadFromEnvironment(env_, action_distributions_);
    env_loader.addTag("EV", config::nn_file_name.substr(config::nn_file_name.find_last_of('/') + 1));

    // if the game is not ended, then treat the game as a resign game, where the next player is the lose side
    if (!isTerminal()) { env_loader.addTag("RE", std::to_string(env_.getEvalScore(true))); }
    return "SelfPlay " + std::to_string(env_loader.getActionPairs().size()) + " " + env_loader.toString();
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
