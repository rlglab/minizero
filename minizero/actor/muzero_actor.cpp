#include "muzero_actor.h"
#include "muzero_network.h"

namespace minizero::actor {

using namespace minizero;
using namespace network;

Action MuZeroActor::think(std::shared_ptr<network::Network>& network, bool with_play /*= false*/, bool display_board /*= false*/)
{
    std::shared_ptr<MuZeroNetwork> muzero_network = std::static_pointer_cast<MuZeroNetwork>(network);
    resetSearch();

    // initial inference for root
    beforeNNEvaluation(network);
    afterNNEvaluation(muzero_network->initialInference()[getEvaluationJobIndex()]);

    // for non-root node
    while (!reachMaximumSimulation()) {
        beforeNNEvaluation(network);
        std::vector<std::shared_ptr<NetworkOutput>> outputs = muzero_network->recurrentInference();
        afterNNEvaluation(outputs[getEvaluationJobIndex()]);
    }

    MCTSTreeNode* selected_node = decideActionNode();
    if (with_play) { act(selected_node->getAction()); }
    if (display_board) { displayBoard(selected_node); }
    return selected_node->getAction();
}

MCTSTreeNode* MuZeroActor::decideActionNode()
{
    if (config::actor_select_action_by_count) {
        return tree_.selectChildByMaxCount(tree_.getRootNode());
    } else if (config::actor_select_action_by_softmax_count) {
        return tree_.selectChildBySoftmaxCount(tree_.getRootNode(), config::actor_select_action_softmax_temperature);
    }

    assert(false);
    return nullptr;
}

std::string MuZeroActor::getActionComment()
{
    return tree_.getSearchDistributionString();
}

void MuZeroActor::beforeNNEvaluation(const std::shared_ptr<Network>& network)
{
    std::vector<MCTSTreeNode*> node_path = tree_.select();
    std::shared_ptr<MuZeroNetwork> muzero_network = std::static_pointer_cast<MuZeroNetwork>(network);

    if (getMCTSTree().getRootNode()->getCount() == 0) {
        // root forward, need to call initial inference
        evaluation_jobs_ = {node_path, muzero_network->pushBackInitialData(env_.getFeatures())};
    } else {
        // recurrent inference
        MCTSTreeNode* leaf_parent_node = node_path[node_path.size() - 2];
        assert(leaf_parent_node->getHiddenStateExternalDataIndex() != -1);
        evaluation_jobs_ = {node_path, muzero_network->pushBackRecurrentData(tree_.getHiddenStateExternalData().getData(leaf_parent_node->getHiddenStateExternalDataIndex()),
                                                                             env_.getActionFeatures(node_path.back()->getAction()))};
    }
}

void MuZeroActor::afterNNEvaluation(const std::shared_ptr<NetworkOutput>& network_output)
{
    std::vector<MCTSTreeNode*> node_path = evaluation_jobs_.first;
    std::shared_ptr<MuZeroNetworkOutput> output = std::static_pointer_cast<MuZeroNetworkOutput>(network_output);

    MCTSTreeNode* leaf_node = node_path.back();
    env::Player turn = (leaf_node == getMCTSTree().getRootNode() ? env_.getTurn() : leaf_node->getAction().nextPlayer());
    tree_.expand(leaf_node, calculateActionPolicy(output->policy_, output->policy_logits_, turn));
    tree_.backup(node_path, output->value_);
    leaf_node->setHiddenStateExternalDataIndex(tree_.getHiddenStateExternalData().storeData(output->hidden_state_));
    if (leaf_node == getMCTSTree().getRootNode()) { addNoiseToNodeChildren(leaf_node); }
}

std::vector<MCTSTree::ActionCandidate> MuZeroActor::calculateActionPolicy(const std::vector<float>& policy, const std::vector<float>& policy_logits, const env::Player& turn)
{
    std::vector<MCTSTree::ActionCandidate> action_candidates;
    for (size_t action_id = 0; action_id < policy.size(); ++action_id) {
        const Action action(action_id, turn);
        if (getMCTSTree().getRootNode()->getCount() == 0 && !env_.isLegalAction(action)) { continue; }
        action_candidates.push_back(MCTSTree::ActionCandidate(action, policy[action_id], policy_logits[action_id]));
    }
    sort(action_candidates.begin(), action_candidates.end(), [](const MCTSTree::ActionCandidate& lhs, const MCTSTree::ActionCandidate& rhs) {
        return lhs.policy_ > rhs.policy_;
    });
    return action_candidates;
}

} // namespace minizero::actor