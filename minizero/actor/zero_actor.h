#pragma once

#include "alphazero_network.h"
#include "base_actor.h"
#include "mcts.h"
#include "muzero_network.h"

namespace minizero::actor {

class MCTSSearchData {
public:
    MCTSNode* selected_node_;
    std::vector<MCTSNode*> node_path_;
    void clear();
};

class ZeroActor : public BaseActor {
public:
    ZeroActor(long long tree_node_size)
        : mcts_(tree_node_size)
    {
        alphazero_network_ = nullptr;
        muzero_network_ = nullptr;
    }

    void reset() override;
    void resetSearch() override;
    Action think(bool with_play = false, bool display_board = false) override;
    void beforeNNEvaluation() override;
    void afterNNEvaluation(const std::shared_ptr<network::NetworkOutput>& network_output) override;
    void displayBoard() const override;
    bool isSearchDone() const override { return mcts_.reachMaximumSimulation(); }
    Action getSearchAction() const override { return mcts_search_data_.selected_node_->getAction(); }
    bool isResign() const override { return enable_resign_ && mcts_.isResign(mcts_search_data_.selected_node_); }
    virtual std::string getActionComment() const override { return mcts_.getSearchDistributionString(); }
    void setNetwork(const std::shared_ptr<network::Network>& network) override;

protected:
    void step();
    virtual MCTSNode* decideActionNode();
    void addNoiseToNodeChildren(MCTSNode* node);

    std::vector<MCTS::ActionCandidate> calculateAlphaZeroActionPolicy(const Environment& env_transition, const std::shared_ptr<network::AlphaZeroNetworkOutput>& alphazero_output);
    std::vector<MCTS::ActionCandidate> calculateMuZeroActionPolicy(MCTSNode* leaf_node, const std::shared_ptr<network::MuZeroNetworkOutput>& muzero_output);
    Environment getEnvironmentTransition(const std::vector<MCTSNode*>& node_path);

    MCTS mcts_;
    bool enable_resign_;
    MCTSSearchData mcts_search_data_;
    std::shared_ptr<network::AlphaZeroNetwork> alphazero_network_;
    std::shared_ptr<network::MuZeroNetwork> muzero_network_;
};

} // namespace minizero::actor