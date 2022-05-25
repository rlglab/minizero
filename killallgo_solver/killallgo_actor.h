#pragma once

#include "base_actor.h"
#include "killallgo_mcts.h"
#include "proof_cost_network.h"

namespace solver {

class KillallGoMCTSSearchData {
public:
    KillAllGoMCTSNode* selected_node_;
    std::vector<KillAllGoMCTSNode*> node_path_;
    void clear();
};

class KillallGoActor : public minizero::actor::BaseActor {
public:
    KillallGoActor(long long tree_node_size)
        : mcts_(tree_node_size)
    {
        pcn_network_ = nullptr;
    }

    void resetSearch() override;
    Action think(bool with_play = false, bool display_board = false) override;
    void beforeNNEvaluation() override;
    void afterNNEvaluation(const std::shared_ptr<minizero::network::NetworkOutput>& network_output) override;
    void displayBoard() const override;
    bool isSearchDone() const override { return mcts_.reachMaximumSimulation(); }
    Action getSearchAction() const override { return mcts_search_data_.selected_node_->getAction(); }
    bool isResign() const override { return false; }
    virtual std::string getActionComment() const override { return mcts_.getSearchDistributionString(); }
    void setNetwork(const std::shared_ptr<minizero::network::Network>& network) override { pcn_network_ = std::static_pointer_cast<ProofCostNetwork>(network); }

protected:
    void step();
    virtual KillAllGoMCTSNode* decideActionNode();
    void addNoiseToNodeChildren(KillAllGoMCTSNode* node);

    std::vector<KillAllGoMCTS::ActionCandidate> calculateActionPolicy(const Environment& env_transition, const std::shared_ptr<ProofCostNetworkOutput>& pcn_output);
    Environment getEnvironmentTransition(const std::vector<KillAllGoMCTSNode*>& node_path);

    KillAllGoMCTS mcts_;
    KillallGoMCTSSearchData mcts_search_data_;
    std::shared_ptr<ProofCostNetwork> pcn_network_;
};

} // namespace solver