#pragma once

#include "alphazero_network.h"
#include "base_actor.h"
#include "mcts.h"
#include "muzero_network.h"

namespace minizero::actor {

class MCTSSearchData {
public:
    std::string search_info_;
    MCTSNode* selected_node_;
    std::vector<MCTSNode*> node_path_;
    void clear();
};

class ZeroActor : public BaseActor {
public:
    ZeroActor(long long tree_node_size)
        : tree_node_size_(tree_node_size)
    {
        alphazero_network_ = nullptr;
        muzero_network_ = nullptr;
    }

    virtual void reset() override;
    virtual void resetSearch() override;
    virtual Action think(bool with_play = false, bool display_board = false) override;
    virtual void beforeNNEvaluation() override;
    virtual void afterNNEvaluation(const std::shared_ptr<network::NetworkOutput>& network_output) override;
    virtual bool isSearchDone() const override { return getMCTS()->reachMaximumSimulation(); }
    virtual Action getSearchAction() const override { return mcts_search_data_.selected_node_->getAction(); }
    virtual bool isResign() const override { return enable_resign_ && getMCTS()->isResign(mcts_search_data_.selected_node_); }
    virtual std::string getSearchInfo() const override { return mcts_search_data_.search_info_; }
    virtual std::string getActionComment() const override { return getMCTS()->getSearchDistributionString(); }
    virtual void setNetwork(const std::shared_ptr<network::Network>& network) override;
    virtual std::shared_ptr<Search> createSearch() override { return std::make_shared<MCTS>(tree_node_size_); }
    std::shared_ptr<MCTS> getMCTS() { return std::static_pointer_cast<MCTS>(search_); }
    const std::shared_ptr<MCTS> getMCTS() const { return std::static_pointer_cast<MCTS>(search_); }

protected:
    virtual void step();
    virtual void handleSearchDone();
    virtual MCTSNode* decideActionNode();
    virtual void addNoiseToNodeChildren(MCTSNode* node);
    virtual std::vector<MCTSNode*> selection() { return getMCTS()->select(); }

    std::vector<MCTS::ActionCandidate> calculateAlphaZeroActionPolicy(const Environment& env_transition, const std::shared_ptr<network::AlphaZeroNetworkOutput>& alphazero_output);
    std::vector<MCTS::ActionCandidate> calculateMuZeroActionPolicy(MCTSNode* leaf_node, const std::shared_ptr<network::MuZeroNetworkOutput>& muzero_output);
    Environment getEnvironmentTransition(const std::vector<MCTSNode*>& node_path);

    bool enable_resign_;
    long long tree_node_size_;
    MCTSSearchData mcts_search_data_;
    std::shared_ptr<network::AlphaZeroNetwork> alphazero_network_;
    std::shared_ptr<network::MuZeroNetwork> muzero_network_;
};

} // namespace minizero::actor