#pragma once

#include "environment.h"
#include "mcts_tree.h"
#include "network.h"
#include <vector>

namespace minizero::actor {

class Actor {
public:
    Actor(long long tree_node_size)
        : is_enable_resign_(true),
          tree_(tree_node_size)
    {
    }

    virtual ~Actor() = default;

    inline void clearTree()
    {
        tree_.reset();
        evaluation_jobs_ = {{}, -1};
    }

    inline void setIsEnableResign(bool is_enable_resign) { is_enable_resign_ = is_enable_resign; }
    inline bool isEnableResign() const { return is_enable_resign_; }
    inline bool isTerminal() const { return env_.isTerminal(); }
    inline bool reachMaximumSimulation() const { return tree_.reachMaximumSimulation(); }
    inline const MCTSTree& getMCTSTree() const { return tree_; }
    inline const Environment& getEnvironment() const { return env_; }
    inline const int getEvaluationJobIndex() const { return evaluation_jobs_.second; }

    virtual void reset() = 0;
    virtual bool act(const Action& action) = 0;
    virtual bool act(const std::vector<std::string>& action_string_args) = 0;
    virtual MCTSTreeNode* runMCTS(std::shared_ptr<network::Network>& network) = 0;
    virtual void beforeNNEvaluation(const std::shared_ptr<network::Network>& network) = 0;
    virtual void afterNNEvaluation(const std::shared_ptr<network::NetworkOutput>& network_output) = 0;
    virtual std::string getRecord() const = 0;

protected:
    bool is_enable_resign_;
    MCTSTree tree_;
    Environment env_;
    std::pair<std::vector<MCTSTreeNode*>, int> evaluation_jobs_;
};

} // namespace minizero::actor