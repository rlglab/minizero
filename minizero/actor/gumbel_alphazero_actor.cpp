#include "gumbel_alphazero_actor.h"
#include "alphazero_network.h"

namespace minizero::actor {

using namespace minizero;
using namespace network;

Action GumbelAlphaZeroActor::think(std::shared_ptr<network::Network>& network, bool with_play /*= false*/, bool display_board /*= false*/)
{
    std::shared_ptr<AlphaZeroNetwork> az_network = std::static_pointer_cast<AlphaZeroNetwork>(network);
    resetSearch();
    beforeNNEvaluation(network);
    afterNNEvaluation(az_network->forward()[getEvaluationJobIndex()]);

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

MCTSTreeNode* GumbelAlphaZeroActor::decideActionNode()
{
    float best_score = -std::numeric_limits<float>::max();
    MCTSTreeNode* selected_node = nullptr;
    MCTSTreeNode* child = tree_.getRootNode()->getFirstChild();
    for (int i = 0; i < tree_.getRootNode()->getNumChildren(); ++i, ++child) {
        if (child->getCount() == 0) { continue; }
        float value = (child->getAction().getPlayer() == env::Player::kPlayer1 ? child->getValue() : -child->getValue());
        float score = child->getPolicyLogit() + (config::actor_gumbel_sigma_visit_c + 1) * config::actor_gumbel_sigma_scale_c * value;
        if (score < best_score) { continue; }
        best_score = score;
        selected_node = child;
    }

    assert(selected_node != nullptr);
    return selected_node;
}

std::string GumbelAlphaZeroActor::getActionComment()
{
    // calculate completed Q-values
    std::ostringstream oss;
    float pi_sum = 0.0f, q_sum = 0.0f;
    MCTSTreeNode* child = tree_.getRootNode()->getFirstChild();
    for (int i = 0; i < tree_.getRootNode()->getNumChildren(); ++i, ++child) {
        if (child->getCount() == 0) { continue; }
        float value = (child->getAction().getPlayer() == env::Player::kPlayer1 ? child->getValue() : -child->getValue());
        float policy_without_noise = child->getPolicyLogit() - child->getPolicyNoise();
        float score = policy_without_noise + (config::actor_gumbel_sigma_visit_c + 1) * config::actor_gumbel_sigma_scale_c * value;
        oss << (oss.str().empty() ? "" : ",")
            << child->getAction().getActionID() << ":" << score;
        pi_sum += child->getPolicy();
        q_sum += child->getPolicy() * value;
    }

    // for non-visited nodes
    float value_pi = (child->getAction().getPlayer() == env::Player::kPlayer1 ? tree_.getRootNode()->getValue() : -tree_.getRootNode()->getValue());
    float non_visited_node_value = 1.0 / (1 + config::actor_num_simulation) * (value_pi + (config::actor_num_simulation / pi_sum) * q_sum);
    child = tree_.getRootNode()->getFirstChild();
    for (int i = 0; i < tree_.getRootNode()->getNumChildren(); ++i, ++child) {
        if (child->getCount() != 0) { continue; }
        float policy_without_noise = child->getPolicyLogit() - child->getPolicyNoise();
        float score = policy_without_noise + (config::actor_gumbel_sigma_visit_c + 1) * config::actor_gumbel_sigma_scale_c * non_visited_node_value;
        oss << (oss.str().empty() ? "" : ",")
            << child->getAction().getActionID() << ":" << score;
    }
    return oss.str();
}

void GumbelAlphaZeroActor::beforeNNEvaluation(const std::shared_ptr<network::Network>& network)
{
    std::vector<MCTSTreeNode*> node_path{tree_.getRootNode()};
    if (!tree_.getRootNode()->isLeaf()) {
        float best_score = -std::numeric_limits<float>::max();
        MCTSTreeNode* best_child = nullptr;
        MCTSTreeNode* child = tree_.getRootNode()->getFirstChild();
        for (int i = 0; i < tree_.getRootNode()->getNumChildren(); ++i, ++child) {
            if (child->getCount() > 0 || child->getPolicyLogit() <= best_score) { continue; }
            best_score = child->getPolicyLogit();
            best_child = child;
        }
        if (best_child) { node_path.push_back(best_child); }
    }
    Environment env_transition = getEnvironmentTransition(node_path);
    std::shared_ptr<AlphaZeroNetwork> alphazero_network = std::static_pointer_cast<AlphaZeroNetwork>(network);
    evaluation_jobs_ = {node_path, alphazero_network->pushBack(env_transition.getFeatures())};
}

void GumbelAlphaZeroActor::afterNNEvaluation(const std::shared_ptr<network::NetworkOutput>& network_output)
{
    std::vector<MCTSTreeNode*> node_path = evaluation_jobs_.first;
    Environment env_transition = getEnvironmentTransition(evaluation_jobs_.first);
    std::shared_ptr<AlphaZeroNetworkOutput> output = std::static_pointer_cast<AlphaZeroNetworkOutput>(network_output);
    if (tree_.getRootNode()->isLeaf()) {
        MCTSTreeNode* leaf_node = node_path.back();
        tree_.expand(leaf_node, calculateActionPolicy(output->policy_, output->policy_logits_, env_transition));
        addNoiseToNodeChildren(leaf_node);
    }

    if (!env_transition.isTerminal()) {
        tree_.backup(node_path, output->value_);
    } else {
        tree_.backup(node_path, env_transition.getEvalScore());
    }
}

std::vector<MCTSTree::ActionCandidate> GumbelAlphaZeroActor::calculateActionPolicy(const std::vector<float>& policy, const std::vector<float>& policy_logits, const Environment& env_transition)
{
    std::vector<MCTSTree::ActionCandidate> action_candidates;
    for (size_t action_id = 0; action_id < policy.size(); ++action_id) {
        Action action(action_id, env_transition.getTurn());
        if (!env_transition.isLegalAction(action)) { continue; }
        action_candidates.push_back(MCTSTree::ActionCandidate(action, policy[action_id], policy_logits[action_id]));
    }
    sort(action_candidates.begin(), action_candidates.end(), [](const MCTSTree::ActionCandidate& lhs, const MCTSTree::ActionCandidate& rhs) {
        return lhs.policy_logit_ > rhs.policy_logit_;
    });
    return action_candidates;
}

Environment GumbelAlphaZeroActor::getEnvironmentTransition(const std::vector<MCTSTreeNode*>& node_path)
{
    Environment env = env_;
    for (size_t i = 1; i < node_path.size(); ++i) { env.act(node_path[i]->getAction()); }
    return env;
}

} // namespace minizero::actor