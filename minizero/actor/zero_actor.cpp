#include "zero_actor.h"

namespace minizero::actor {

using namespace minizero;
using namespace network;

Action ZeroActor::think(bool with_play /*= false*/, bool display_board /*= false*/)
{
    resetSearch();
    while (!mcts_.reachMaximumSimulation()) { step(); }
    MCTSNode* selected_node = decideActionNode();
    if (with_play) { act(selected_node->getAction()); }
    if (display_board) { displayBoard(selected_node); }
    return selected_node->getAction();
}

MCTSNode* ZeroActor::decideActionNode()
{
    if (config::actor_select_action_by_count) {
        return mcts_.selectChildByMaxCount(mcts_.getRootNode());
    } else if (config::actor_select_action_by_softmax_count) {
        return mcts_.selectChildBySoftmaxCount(mcts_.getRootNode(), config::actor_select_action_softmax_temperature);
    }

    assert(false);
    return nullptr;
}

std::string ZeroActor::getActionComment()
{
    return mcts_.getSearchDistributionString();
}

void ZeroActor::beforeNNEvaluation()
{
    std::vector<MCTSNode*> node_path = mcts_.select();
    if (alphazero_network_) {
        Environment env_transition = getEnvironmentTransition(node_path);
        evaluation_jobs_ = {node_path, alphazero_network_->pushBack(env_transition.getFeatures())};
    } else if (muzero_network_) {
        if (mcts_.getNumSimulation() == 0) { // initial inference for root node
            evaluation_jobs_ = {node_path, muzero_network_->pushBackInitialData(env_.getFeatures())};
        } else { // for non-root nodes
            MCTSNode* leaf_node = node_path.back();
            MCTSNode* parent_node = node_path[node_path.size() - 2];
            assert(parent_node && parent_node->getExtraDataIndex() != -1);
            const std::vector<float>& hidden_state = mcts_.getTreeExtraData().getExtraData(parent_node->getExtraDataIndex()).hidden_state_;
            evaluation_jobs_ = {node_path, muzero_network_->pushBackRecurrentData(hidden_state, env_.getActionFeatures(leaf_node->getAction()))};
        }
    } else {
        assert(false);
    }
}

void ZeroActor::afterNNEvaluation(const std::shared_ptr<NetworkOutput>& network_output)
{
    const std::vector<MCTSNode*>& node_path = evaluation_jobs_.first;
    MCTSNode* leaf_node = node_path.back();
    if (alphazero_network_) {
        Environment env_transition = getEnvironmentTransition(evaluation_jobs_.first);
        if (!env_transition.isTerminal()) {
            std::shared_ptr<AlphaZeroNetworkOutput> alphazero_output = std::static_pointer_cast<AlphaZeroNetworkOutput>(network_output);
            mcts_.expand(leaf_node, calculateAlphaZeroActionPolicy(env_transition, alphazero_output));
            mcts_.backup(node_path, alphazero_output->value_);
        } else {
            mcts_.backup(node_path, env_transition.getEvalScore());
        }
    } else if (muzero_network_) {
        std::shared_ptr<MuZeroNetworkOutput> muzero_output = std::static_pointer_cast<MuZeroNetworkOutput>(network_output);
        mcts_.expand(leaf_node, calculateMuZeroActionPolicy(leaf_node, muzero_output));
        mcts_.backup(node_path, muzero_output->value_);
        leaf_node->setExtraDataIndex(mcts_.getTreeExtraData().store(MCTSNodeExtraData(muzero_output->hidden_state_)));
    } else {
        assert(false);
    }
    if (leaf_node == mcts_.getRootNode()) { addNoiseToNodeChildren(leaf_node); }
}

void ZeroActor::setNetwork(const std::shared_ptr<network::Network>& network)
{
    assert(network);
    alphazero_network_ = nullptr;
    muzero_network_ = nullptr;
    if (network->getNetworkTypeName() == "alphazero") {
        alphazero_network_ = std::static_pointer_cast<AlphaZeroNetwork>(network);
    } else if (network->getNetworkTypeName() == "muzero") {
        muzero_network_ = std::static_pointer_cast<MuZeroNetwork>(network);
    } else {
        assert(false);
    }
    assert((alphazero_network_ && !muzero_network_) || (!alphazero_network_ && muzero_network_));
}

void ZeroActor::step()
{
    beforeNNEvaluation();
    if (alphazero_network_) {
        afterNNEvaluation(alphazero_network_->forward()[getEvaluationJobIndex()]);
    } else if (muzero_network_) {
        if (mcts_.getNumSimulation() == 0) { // initial inference for root node
            afterNNEvaluation(muzero_network_->initialInference()[getEvaluationJobIndex()]);
        } else { // for non-root nodes
            afterNNEvaluation(muzero_network_->recurrentInference()[getEvaluationJobIndex()]);
        }
    } else {
        assert(false);
    }
}

std::vector<MCTS::ActionCandidate> ZeroActor::calculateAlphaZeroActionPolicy(const Environment& env_transition, const std::shared_ptr<network::AlphaZeroNetworkOutput>& alphazero_output)
{
    assert(alphazero_network_);
    std::vector<MCTS::ActionCandidate> action_candidates;
    for (size_t action_id = 0; action_id < alphazero_output->policy_.size(); ++action_id) {
        Action action(action_id, env_transition.getTurn());
        if (!env_transition.isLegalAction(action)) { continue; }
        action_candidates.push_back(MCTS::ActionCandidate(action, alphazero_output->policy_[action_id], alphazero_output->policy_logits_[action_id]));
    }
    sort(action_candidates.begin(), action_candidates.end(), [](const MCTS::ActionCandidate& lhs, const MCTS::ActionCandidate& rhs) {
        return lhs.policy_ > rhs.policy_;
    });
    return action_candidates;
}

std::vector<MCTS::ActionCandidate> ZeroActor::calculateMuZeroActionPolicy(MCTSNode* leaf_node, const std::shared_ptr<network::MuZeroNetworkOutput>& muzero_output)
{
    assert(muzero_network_);
    std::vector<MCTS::ActionCandidate> action_candidates;
    env::Player turn = (leaf_node == mcts_.getRootNode() ? env_.getTurn() : leaf_node->getAction().nextPlayer());
    for (size_t action_id = 0; action_id < muzero_output->policy_.size(); ++action_id) {
        const Action action(action_id, turn);
        if (leaf_node == mcts_.getRootNode() && !env_.isLegalAction(action)) { continue; }
        action_candidates.push_back(MCTS::ActionCandidate(action, muzero_output->policy_[action_id], muzero_output->policy_logits_[action_id]));
    }
    sort(action_candidates.begin(), action_candidates.end(), [](const MCTS::ActionCandidate& lhs, const MCTS::ActionCandidate& rhs) {
        return lhs.policy_ > rhs.policy_;
    });
    return action_candidates;
}

Environment ZeroActor::getEnvironmentTransition(const std::vector<MCTSNode*>& node_path)
{
    Environment env = env_;
    for (size_t i = 1; i < node_path.size(); ++i) { env.act(node_path[i]->getAction()); }
    return env;
}

} // namespace minizero::actor
