#pragma once

#include "actor.h"
#include "alphazero_network.h"
#include "environment.h"
#include <vector>

namespace minizero::actor {

class AlphaZeroActor : public Actor {
public:
    AlphaZeroActor(long long tree_node_size) : Actor(tree_node_size) {}

    void Reset() override;
    bool Act(const Action& action) override;
    void BeforeNNEvaluation(const std::shared_ptr<network::Network>& network) override;
    void AfterNNEvaluation(const std::shared_ptr<network::NetworkOutput>& network_output) override;
    std::string GetRecord() const override;

private:
    std::vector<std::pair<Action, float>> CalculateActionPolicy(const std::vector<float>& policy, const Environment& env_transition);
    Environment GetEnvironmentTransition(const std::vector<MCTSTreeNode*>& node_path);

    std::vector<std::string> action_distributions_;
};

} // namespace minizero::actor