#include "alphazero_actor.h"
#include "alphazero_network.h"

namespace minizero::actor {

using namespace minizero;
using namespace network;

Action AlphaZeroActor::think(std::shared_ptr<network::Network>& network, bool with_play /*= false*/, bool display_board /*= false*/)
{
    std::shared_ptr<AlphaZeroNetwork> az_network = std::static_pointer_cast<AlphaZeroNetwork>(network);
    resetSearch();
    while (!reachMaximumSimulation()) {
        beforeNNEvaluation(network);
        std::vector<std::shared_ptr<NetworkOutput>> outputs = az_network->forward();
        afterNNEvaluation(outputs[getEvaluationJobIndex()]);
    }

    MCTSTreeNode* selected_node = decideActionNode();
    if (with_play) { act(selected_node->getAction()); }
    if (display_board) { displayBoard(selected_node); }
    return selected_node->getAction();
}

MCTSTreeNode* AlphaZeroActor::decideActionNode()
{
    if (config::actor_select_action_by_count) {
        return tree_.selectChildByMaxCount(tree_.getRootNode());
    } else if (config::actor_select_action_by_softmax_count) {
        return tree_.selectChildBySoftmaxCount(tree_.getRootNode(), config::actor_select_action_softmax_temperature);
    }

    assert(false);
    return nullptr;
}

std::string AlphaZeroActor::getActionComment()
{
    return tree_.getSearchDistributionString();
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
        MCTSTreeNode* leaf_node = node_path.back();
        tree_.expand(leaf_node, calculateActionPolicy(output->policy_, output->policy_logits_, env_transition));
        tree_.backup(node_path, output->value_);
        if (leaf_node == getMCTSTree().getRootNode()) { addNoiseToNodeChildren(leaf_node); }
    } else {
        tree_.backup(node_path, env_transition.getEvalScore());
    }
}

std::vector<MCTSTree::ActionCandidate> AlphaZeroActor::calculateActionPolicy(const std::vector<float>& policy, const std::vector<float>& policy_logits, const Environment& env_transition)
{
    std::vector<MCTSTree::ActionCandidate> action_candidates;
    for (size_t action_id = 0; action_id < policy.size(); ++action_id) {
        Action action(action_id, env_transition.getTurn());
        if (!env_transition.isLegalAction(action)) { continue; }
        action_candidates.push_back(MCTSTree::ActionCandidate(action, policy[action_id], policy_logits[action_id]));
    }
    sort(action_candidates.begin(), action_candidates.end(), [](const MCTSTree::ActionCandidate& lhs, const MCTSTree::ActionCandidate& rhs) {
        return lhs.policy_ > rhs.policy_;
    });
    return action_candidates;
}

Environment AlphaZeroActor::getEnvironmentTransition(const std::vector<MCTSTreeNode*>& node_path)
{
    Environment env = env_;
    for (size_t i = 1; i < node_path.size(); ++i) { env.act(node_path[i]->getAction()); }
    return env;
}

} // namespace minizero::actor
