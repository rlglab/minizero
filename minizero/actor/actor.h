#pragma once

#include "environment.h"
#include "mcts.h"
#include "network.h"
#include <vector>

namespace minizero::actor {

using namespace minizero;

class Actor {
public:
    Actor(long long tree_node_size)
        : mcts_(tree_node_size)
    {
    }

    virtual ~Actor() = default;

    void reset();
    void resetSearch();
    bool act(const Action& action, const std::string action_comment = "");
    bool act(const std::vector<std::string>& action_string_args, const std::string action_comment = "");
    void displayBoard(const MCTSNode* selected_node);
    std::string getRecord() const;

    inline int getCurrentSimulation() const { return mcts_.getNumSimulation(); }
    inline bool isResign(const MCTSNode* selected_node) const { return mcts_.isResign(selected_node); }
    inline bool isTerminal() const { return env_.isTerminal(); }
    inline bool reachMaximumSimulation() const { return mcts_.reachMaximumSimulation(); }
    inline const Environment& getEnvironment() const { return env_; }
    inline const int getEvaluationJobIndex() const { return evaluation_jobs_.second; }

    virtual Action think(bool with_play = false, bool display_board = false) = 0;
    virtual MCTSNode* decideActionNode() = 0;
    virtual std::string getActionComment() = 0;
    virtual void beforeNNEvaluation() = 0;
    virtual void afterNNEvaluation(const std::shared_ptr<network::NetworkOutput>& network_output) = 0;
    virtual void setNetwork(const std::shared_ptr<network::Network>& network) = 0;

protected:
    void addNoiseToNodeChildren(MCTSNode* node);

    MCTS mcts_;
    Environment env_;
    std::vector<std::string> action_comments_;
    std::pair<std::vector<MCTSNode*>, int> evaluation_jobs_;
};

std::shared_ptr<Actor> createActor(long long tree_node_size, const std::shared_ptr<network::Network>& network);

} // namespace minizero::actor