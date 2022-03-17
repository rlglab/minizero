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

    inline void ClearTree()
    {
        tree_.Reset();
        evaluation_jobs_ = {{}, -1};
    }

    inline void SetIsEnableResign(bool is_enable_resign) { is_enable_resign_ = is_enable_resign; }
    inline bool IsEnableResign() const { return is_enable_resign_; }
    inline bool IsTerminal() const { return env_.IsTerminal(); }
    inline bool ReachMaximumSimulation() const { return tree_.ReachMaximumSimulation(); }
    inline const MCTSTree& GetMCTSTree() const { return tree_; }
    inline const Environment& GetEnvironment() const { return env_; }
    inline const int GetEvaluationJobIndex() const { return evaluation_jobs_.second; }

    virtual void Reset() = 0;
    virtual bool Act(const Action& action) = 0;
    virtual void BeforeNNEvaluation(const std::shared_ptr<network::Network>& network) = 0;
    virtual void AfterNNEvaluation(const std::shared_ptr<network::NetworkOutput>& network_output) = 0;
    virtual std::string GetRecord() const = 0;

protected:
    bool is_enable_resign_;
    MCTSTree tree_;
    Environment env_;
    std::pair<std::vector<MCTSTreeNode*>, int> evaluation_jobs_;
};

} // namespace minizero::actor