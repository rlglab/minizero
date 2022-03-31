#pragma once

#include "configuration.h"
#include "environment.h"
#include "mcts_tree.h"
#include "network.h"
#include "random.h"
#include <vector>

namespace minizero::actor {

using namespace minizero;

class Actor {
public:
    Actor(long long tree_node_size)
        : is_enable_resign_(true),
          tree_(tree_node_size)
    {
    }

    virtual ~Actor() = default;

    inline void reset()
    {
        env_.reset();
        tree_.reset();
        evaluation_jobs_ = {{}, -1};
        action_distributions_.clear();
        is_enable_resign_ = utils::Random::randReal() < config::zero_disable_resign_ratio ? false : true;
    }

    bool act(const Action& action)
    {
        bool can_act = env_.act(action);
        if (can_act) {
            action_distributions_.push_back(tree_.getActionDistributionString());
            tree_.reset();
            evaluation_jobs_ = {{}, -1};
        }
        return can_act;
    }

    bool act(const std::vector<std::string>& action_string_args)
    {
        bool can_act = env_.act(action_string_args);
        if (can_act) {
            action_distributions_.push_back(tree_.getActionDistributionString());
            tree_.reset();
            evaluation_jobs_ = {{}, -1};
        }
        return can_act;
    }

    std::string getRecord() const
    {
        EnvironmentLoader env_loader;
        env_loader.loadFromEnvironment(env_, action_distributions_);
        env_loader.addTag("EV", config::nn_file_name.substr(config::nn_file_name.find_last_of('/') + 1));

        // if the game is not ended, then treat the game as a resign game, where the next player is the lose side
        if (!isTerminal()) { env_loader.addTag("RE", std::to_string(env_.getEvalScore(true))); }
        return "SelfPlay " + std::to_string(env_loader.getActionPairs().size()) + " " + env_loader.toString();
    }

    inline void setIsEnableResign(bool is_enable_resign) { is_enable_resign_ = is_enable_resign; }
    inline bool isEnableResign() const { return is_enable_resign_; }
    inline bool isTerminal() const { return env_.isTerminal(); }
    inline bool reachMaximumSimulation() const { return tree_.reachMaximumSimulation(); }
    inline const MCTSTree& getMCTSTree() const { return tree_; }
    inline const Environment& getEnvironment() const { return env_; }
    inline const int getEvaluationJobIndex() const { return evaluation_jobs_.second; }

    virtual MCTSTreeNode* runMCTS(std::shared_ptr<network::Network>& network) = 0;
    virtual void beforeNNEvaluation(const std::shared_ptr<network::Network>& network) = 0;
    virtual void afterNNEvaluation(const std::shared_ptr<network::NetworkOutput>& network_output) = 0;

protected:
    bool is_enable_resign_;
    MCTSTree tree_;
    Environment env_;
    std::vector<std::string> action_distributions_;
    std::pair<std::vector<MCTSTreeNode*>, int> evaluation_jobs_;
};

} // namespace minizero::actor