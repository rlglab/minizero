#pragma once

#include "configuration.h"
#include "environment.h"
#include <vector>

namespace minizero::actor {

class MCTSTreeNode {
public:
    MCTSTreeNode();

    void reset();

    void add(float value, float weight = 1.0f);
    void remove(float value, float weight = 1.0f);
    float getPUCTScore(int total_simulation, float init_q_value = -1.0f);
    std::string toString() const;

    // setter
    inline void setAction(Action action) { action_ = action; }
    inline void setNumChildren(int num_children) { num_children_ = num_children; }
    inline void setMean(float mean) { mean_ = mean; }
    inline void setCount(float count) { count_ = count; }
    inline void setPolicy(float policy) { policy_ = policy; }
    inline void setValue(float value) { value_ = value; }
    inline void setFirstChild(MCTSTreeNode* first_child) { first_child_ = first_child; }
    inline void setHiddenState(const std::vector<float>& hidden_state) { hidden_state_ = hidden_state; }

    // getter
    inline Action getAction() const { return action_; }
    inline bool isLeaf() const { return (num_children_ == 0); }
    inline int getNumChildren() const { return num_children_; }
    inline float getMean() const { return mean_; }
    inline float getCount() const { return count_; }
    inline float getPolicy() const { return policy_; }
    inline float getValue() const { return value_; }
    inline MCTSTreeNode* getFirstChild() const { return first_child_; }
    inline const std::vector<float>& getHiddenState() const { return hidden_state_; }

private:
    Action action_;
    int num_children_;
    float mean_;
    float count_;
    float policy_;
    float value_;
    MCTSTreeNode* first_child_;
    std::vector<float> hidden_state_;
};

class MCTSTree {
public:
    MCTSTree(long long tree_node_size);

    void reset();
    Action decideAction() const;
    MCTSTreeNode* decideActionNode() const;
    std::string getActionDistributionString() const;
    std::vector<MCTSTreeNode*> select();
    void expand(MCTSTreeNode* leaf_node, const std::vector<std::pair<Action, float>>& action_policy);
    void backup(std::vector<MCTSTreeNode*>& node_path, const float value);

    inline bool reachMaximumSimulation() const { return (getRootNode()->getCount() == config::actor_num_simulation); }
    inline MCTSTreeNode* getRootNode() { return &nodes_[0]; }
    inline const MCTSTreeNode* getRootNode() const { return &nodes_[0]; }

// private:
    MCTSTreeNode* selectChildByPUCTScore(const MCTSTreeNode* node) const;
    MCTSTreeNode* selectChildByMaxCount(const MCTSTreeNode* node) const;
    MCTSTreeNode* selectChildBySoftmaxCount(const MCTSTreeNode* node, float temperature = 1.0f) const;
    void addNoiseToNode(MCTSTreeNode* node);
    float calculateInitQValue(const MCTSTreeNode* node) const;
    std::vector<float> calculateDirichletNoise(int size, float alpha) const;
    MCTSTreeNode* allocateNewNodes(int size);

    int current_tree_size_;
    std::vector<MCTSTreeNode> nodes_;
};

} // namespace minizero::actor