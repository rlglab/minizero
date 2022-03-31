#include "muzero_actor.h"
#include "muzero_network.h"

namespace minizero::actor {

using namespace minizero;
using namespace network;

MCTSTreeNode* MuZeroActor::runMCTS(std::shared_ptr<Network>& network)
{
    std::shared_ptr<MuZeroNetwork> muzero_network = std::static_pointer_cast<MuZeroNetwork>(network);
    tree_.reset();
    evaluation_jobs_ = {{}, -1};
    while (!reachMaximumSimulation()) {
        beforeNNEvaluation(network);
        std::vector<std::shared_ptr<NetworkOutput>> outputs;
        if (getMCTSTree().getRootNode()->getCount() == 0) {
            outputs = muzero_network->initialInference();
        } else {
            outputs = muzero_network->recurrentInference();
        }
        afterNNEvaluation(outputs[getEvaluationJobIndex()]);
    }
    return getMCTSTree().decideActionNode();
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
        MCTSTreeExternalData<std::vector<float>>& hidden_state_external_data = tree_.getHiddenStateExternalData();
        assert(leaf_parent_node->getHiddenStateExternalDataIndex() != -1);
        evaluation_jobs_ = {node_path, muzero_network->pushBackRecurrentData(hidden_state_external_data.getData(leaf_parent_node->getHiddenStateExternalDataIndex()),
                                                                             env_.getActionFeatures(node_path.back()->getAction()))};
    }
}

void MuZeroActor::afterNNEvaluation(const std::shared_ptr<NetworkOutput>& network_output)
{
    std::vector<MCTSTreeNode*> node_path = evaluation_jobs_.first;
    std::shared_ptr<MuZeroNetworkOutput> output = std::static_pointer_cast<MuZeroNetworkOutput>(network_output);
    MCTSTreeNode* leaf_node = node_path.back();
    MCTSTreeExternalData<std::vector<float>>& hidden_state_external_data = tree_.getHiddenStateExternalData();
    leaf_node->setHiddenStateExternalDataIndex(hidden_state_external_data.storeData(output->hidden_state_));
    env::Player turn = (leaf_node == getMCTSTree().getRootNode() ? env_.getTurn() : leaf_node->getAction().nextPlayer());
    tree_.expand(leaf_node, calculateActionPolicy(output->policy_, turn));
    tree_.backup(node_path, output->value_);
}

std::vector<std::pair<Action, float>> MuZeroActor::calculateActionPolicy(const std::vector<float>& policy, const env::Player& turn)
{
    std::vector<std::pair<Action, float>> action_policy;
    for (size_t action_id = 0; action_id < policy.size(); ++action_id) {
        const Action action(action_id, turn);
        if (getMCTSTree().getRootNode()->getCount() == 0 && !env_.isLegalAction(action)) { continue; }
        action_policy.push_back({action, policy[action_id]});
    }
    return action_policy;
}

} // namespace minizero::actor