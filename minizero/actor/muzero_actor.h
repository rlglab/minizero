#pragma once

#include "actor.h"

namespace minizero::actor {

class MuZeroActor : public Actor {
public:
    MuZeroActor(long long tree_node_size): Actor(tree_node_size) {}

    MCTSTreeNode* runMCTS(std::shared_ptr<network::Network>& network) override;
    void beforeNNEvaluation(const std::shared_ptr<network::Network>& network) override;
    void afterNNEvaluation(const std::shared_ptr<network::NetworkOutput>& network_output) override;

private:
    std::vector<std::pair<Action, float>> calculateActionPolicy(const std::vector<float>& policy, const env::Player& turn);
};

} // namespace minizero::actor