#pragma once

#include "actor.h"
#include "alphazero_network.h"
#include "environment.h"
#include <vector>

namespace minizero::actor {

class AlphaZeroActor : public Actor {
public:
    AlphaZeroActor(long long tree_node_size) : Actor(tree_node_size) {}

    void reset() override;
    bool act(const Action& action) override;
    bool act(const std::vector<std::string>& action_string_args) override;
    MCTSTreeNode* runMCTS(std::shared_ptr<network::Network>& network) override;
    void beforeNNEvaluation(const std::shared_ptr<network::Network>& network) override;
    void afterNNEvaluation(const std::shared_ptr<network::NetworkOutput>& network_output) override;
    std::string getRecord() const override;

private:
    std::vector<std::pair<Action, float>> calculateActionPolicy(const std::vector<float>& policy, const Environment& env_transition);
    Environment getEnvironmentTransition(const std::vector<MCTSTreeNode*>& node_path);

    std::vector<std::string> action_distributions_;
};

} // namespace minizero::actor