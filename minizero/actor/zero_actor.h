#pragma once

#include "alphazero_network.h"
#include "base_actor.h"
#include "gumbel_zero.h"
#include "mcts.h"
#include "muzero_network.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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
    ZeroActor(uint64_t tree_node_size)
        : tree_node_size_(tree_node_size)
    {
        alphazero_network_ = nullptr;
        muzero_network_ = nullptr;
    }

    void reset() override;
    void resetSearch() override;
    Action think(bool with_play = false, bool display_board = false) override;
    void beforeNNEvaluation() override;
    void afterNNEvaluation(const std::shared_ptr<network::NetworkOutput>& network_output) override;
    bool isSearchDone() const override { return getMCTS()->reachMaximumSimulation(); }
    Action getSearchAction() const override { return mcts_search_data_.selected_node_->getAction(); }
    bool isResign() const override { return enable_resign_ && getMCTS()->isResign(mcts_search_data_.selected_node_); }
    std::string getSearchInfo() const override { return mcts_search_data_.search_info_; }
    void setNetwork(const std::shared_ptr<network::Network>& network) override;
    std::shared_ptr<Search> createSearch() override { return std::make_shared<MCTS>(tree_node_size_); }

protected:
    std::vector<std::pair<std::string, std::string>> getActionInfo() const override;
    std::string getMCTSPolicy() const override { return (config::actor_use_gumbel ? gumbel_zero_.getMCTSPolicy(getMCTS()) : getMCTS()->getSearchDistributionString()); }
    std::string getMCTSValue() const override { return std::to_string(getMCTS()->getRootNode()->getMean()); }
    std::string getEnvReward() const override;
    std::string getMCTSChange() const override
    {
        std::vector<Action> action_history = env_.getActionHistory();
        return action_history.size() > 1 ? (action_history.back().getPlayer() == action_history[action_history.size() - 2].getPlayer() ? "0" : "1") : "1";
    }

    virtual void step();
    virtual void handleSearchDone();
    virtual MCTSNode* decideActionNode();
    virtual void addNoiseToNodeChildren(MCTSNode* node);
    virtual std::vector<MCTSNode*> selection() { return (config::actor_use_gumbel ? gumbel_zero_.selection(getMCTS()) : getMCTS()->select()); }

    std::vector<MCTS::ActionCandidate> calculateAlphaZeroActionPolicy(const Environment& env_transition, const std::shared_ptr<network::AlphaZeroNetworkOutput>& alphazero_output, const utils::Rotation& rotation);
    std::vector<MCTS::ActionCandidate> calculateMuZeroActionPolicy(MCTSNode* leaf_node, const std::shared_ptr<network::MuZeroNetworkOutput>& muzero_output);
    virtual Environment getEnvironmentTransition(const std::vector<MCTSNode*>& node_path);

    bool enable_resign_;
    GumbelZero gumbel_zero_;
    uint64_t tree_node_size_;
    MCTSSearchData mcts_search_data_;
    utils::Rotation feature_rotation_;
    std::shared_ptr<network::AlphaZeroNetwork> alphazero_network_;
    std::shared_ptr<network::MuZeroNetwork> muzero_network_;
};

} // namespace minizero::actor
