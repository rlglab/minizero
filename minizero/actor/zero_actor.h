#pragma once

#include "actor.h"
#include "alphazero_network.h"
#include "muzero_network.h"

namespace minizero::actor {

class ZeroActor : public Actor {
public:
    ZeroActor(long long tree_node_size) : Actor(tree_node_size)
    {
        alphazero_network_ = nullptr;
        muzero_network_ = nullptr;
    }

    Action think(bool with_play = false, bool display_board = false) override;
    MCTSNode* decideActionNode() override;
    std::string getActionComment() override;
    void beforeNNEvaluation() override;
    void afterNNEvaluation(const std::shared_ptr<network::NetworkOutput>& network_output) override;
    void setNetwork(const std::shared_ptr<network::Network>& network) override;

protected:
    void step();
    std::vector<MCTS::ActionCandidate> calculateAlphaZeroActionPolicy(const Environment& env_transition, const std::shared_ptr<network::AlphaZeroNetworkOutput>& alphazero_output);
    std::vector<MCTS::ActionCandidate> calculateMuZeroActionPolicy(MCTSNode* leaf_node, const std::shared_ptr<network::MuZeroNetworkOutput>& muzero_output);
    Environment getEnvironmentTransition(const std::vector<MCTSNode*>& node_path);

    std::shared_ptr<network::AlphaZeroNetwork> alphazero_network_;
    std::shared_ptr<network::MuZeroNetwork> muzero_network_;
};

} // namespace minizero::actor