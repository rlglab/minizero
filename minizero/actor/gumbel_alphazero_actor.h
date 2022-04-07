#pragma once

#include "actor.h"

namespace minizero::actor {

class GumbelAlphaZeroActor : public Actor {
public:
    GumbelAlphaZeroActor(long long tree_node_size) : Actor(tree_node_size) {}

    Action think(std::shared_ptr<network::Network>& network, bool with_play = false, bool display_board = false) override;
    MCTSTreeNode* decideActionNode() override;
    std::string getActionComment() override;
    void beforeNNEvaluation(const std::shared_ptr<network::Network>& network) override;
    void afterNNEvaluation(const std::shared_ptr<network::NetworkOutput>& network_output) override;

    inline bool reachMaximumSimulation() const { return (tree_.getRootNode()->getCount() == config::actor_num_simulation + 1); }

private:
    std::vector<MCTSTree::ActionCandidate> calculateActionPolicy(const std::vector<float>& policy, const std::vector<float>& policy_logits, const Environment& env_transition);
    Environment getEnvironmentTransition(const std::vector<MCTSTreeNode*>& node_path);
};

} // namespace minizero::actor