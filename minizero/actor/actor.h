#pragma once

#include "environment.h"
#include "mcts_tree.h"
#include "network.h"
#include <vector>

namespace minizero::actor {

using namespace minizero;

class Actor {
public:
    Actor(long long tree_node_size)
        : tree_(tree_node_size)
    {
    }

    virtual ~Actor() = default;

    void reset();
    void resetSearch();
    bool act(const Action& action, const std::string action_comment = "");
    bool act(const std::vector<std::string>& action_string_args, const std::string action_comment = "");
    void displayBoard(const MCTSTreeNode* selected_node);
    std::string getRecord() const;

    inline bool isTerminal() const { return env_.isTerminal(); }
    inline bool reachMaximumSimulation() const { return tree_.reachMaximumSimulation(); }
    inline const MCTSTree& getMCTSTree() const { return tree_; }
    inline const Environment& getEnvironment() const { return env_; }
    inline const int getEvaluationJobIndex() const { return evaluation_jobs_.second; }

    virtual Action think(std::shared_ptr<network::Network>& network, bool with_play = false, bool display_board = false) = 0;
    virtual MCTSTreeNode* decideActionNode() = 0;
    virtual std::string getActionComment() = 0;
    virtual void beforeNNEvaluation(const std::shared_ptr<network::Network>& network) = 0;
    virtual void afterNNEvaluation(const std::shared_ptr<network::NetworkOutput>& network_output) = 0;

protected:
    void addNoiseToNodeChildren(MCTSTreeNode* node);

    MCTSTree tree_;
    Environment env_;
    std::vector<std::string> action_comments_;
    std::pair<std::vector<MCTSTreeNode*>, int> evaluation_jobs_;
};

std::shared_ptr<Actor> createActor(long long tree_node_size, const std::string& network_type_name);

} // namespace minizero::actor