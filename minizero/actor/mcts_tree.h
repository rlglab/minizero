#pragma once

#include "configuration.h"
#include "environment.h"
#include <vector>

namespace minizero::actor {

class MCTSTreeNode {
public:
    MCTSTreeNode();

    void Reset();

    void Add(float value, float weight = 1.0f);
    void Remove(float value, float weight = 1.0f);
    float GetPUCTScore(int total_simulation);
    std::string ToString() const;

    // setter
    inline void SetAction(Action action) { action_ = action; }
    inline void SetNumChildren(int num_children) { num_children_ = num_children; }
    inline void SetMean(float mean) { mean_ = mean; }
    inline void SetCount(float count) { count_ = count; }
    inline void SetPolicy(float policy) { policy_ = policy; }
    inline void SetValue(float value) { value_ = value; }
    inline void SetFirstChild(MCTSTreeNode* first_child) { first_child_ = first_child; }
    inline void SetHiddenState(const std::vector<float>& hidden_state) { hidden_state_ = hidden_state; }

    // getter
    inline Action GetAction() const { return action_; }
    inline bool IsLeaf() const { return (num_children_ == 0); }
    inline int GetNumChildren() const { return num_children_; }
    inline float GetMean() const { return mean_; }
    inline float GetCount() const { return count_; }
    inline float GetPolicy() const { return policy_; }
    inline float GetValue() const { return value_; }
    inline MCTSTreeNode* GetFirstChild() const { return first_child_; }
    inline const std::vector<float>& GetHiddenState() const { return hidden_state_; }

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

    void Reset();
    Action DecideAction() const;
    std::string GetActionDistributionString() const;
    std::vector<MCTSTreeNode*> Select();
    void Expand(MCTSTreeNode* leaf_node, const std::vector<std::pair<Action, float>>& action_policy);
    void Backup(std::vector<MCTSTreeNode*>& node_path, const float value);

    inline bool ReachMaximumSimulation() const { return (GetRootNode()->GetCount() == config::actor_num_simulation); }
    inline MCTSTreeNode* GetRootNode() { return &nodes_[0]; }
    inline const MCTSTreeNode* GetRootNode() const { return &nodes_[0]; }

private:
    MCTSTreeNode* SelectChildByPUCTScore(const MCTSTreeNode* node) const;
    MCTSTreeNode* SelectChildByMaxCount(const MCTSTreeNode* node) const;
    MCTSTreeNode* SelectChildBySoftmaxCount(const MCTSTreeNode* node, float temperature = 1.0f) const;
    void AddNoiseToNode(MCTSTreeNode* node);
    std::vector<float> CalculateDirichletNoise(int size, float alpha) const;
    MCTSTreeNode* AllocateNewNodes(int size);

    int current_tree_size_;
    std::vector<MCTSTreeNode> nodes_;
};

} // namespace minizero::actor