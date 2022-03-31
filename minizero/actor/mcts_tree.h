#pragma once

#include "configuration.h"
#include "environment.h"
#include <vector>

namespace minizero::actor {

template <class Data>
class MCTSTreeExternalData {
public:
    MCTSTreeExternalData(int size)
    {
        index_ = 0;
        data_.resize(size);
    }

    inline void reset() { index_ = 0; }
    inline int storeData(const Data& data)
    {
        assert(index_ < static_cast<int>(data_.size()));
        data_[index_] = data;
        return index_++;
    }

    inline int getCurrentSize() { return index_; }
    inline const Data& getData(int index) const { return data_[index]; }

private:
    int index_;
    std::vector<Data> data_;
};

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
    inline void setHiddenStateExternalDataIndex(const int hidden_state_external_data_index) { hidden_state_external_data_index_ = hidden_state_external_data_index; }
    inline void setMean(float mean) { mean_ = mean; }
    inline void setCount(float count) { count_ = count; }
    inline void setPolicy(float policy) { policy_ = policy; }
    inline void setValue(float value) { value_ = value; }
    inline void setFirstChild(MCTSTreeNode* first_child) { first_child_ = first_child; }

    // getter
    inline Action getAction() const { return action_; }
    inline bool isLeaf() const { return (num_children_ == 0); }
    inline int getNumChildren() const { return num_children_; }
    inline int getHiddenStateExternalDataIndex() const { return hidden_state_external_data_index_; }
    inline float getMean() const { return mean_; }
    inline float getCount() const { return count_; }
    inline float getPolicy() const { return policy_; }
    inline float getValue() const { return value_; }
    inline MCTSTreeNode* getFirstChild() const { return first_child_; }

private:
    Action action_;
    int num_children_;
    int hidden_state_external_data_index_;
    float mean_;
    float count_;
    float policy_;
    float value_;
    MCTSTreeNode* first_child_;
};

class MCTSTree {
public:
    MCTSTree(long long tree_node_size)
        : hidden_state_external_data_(config::actor_num_simulation)
    {
        nodes_.resize(1 + tree_node_size);
    }

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
    inline MCTSTreeExternalData<std::vector<float>>& getHiddenStateExternalData() { return hidden_state_external_data_; }

private:
    MCTSTreeNode* selectChildByPUCTScore(const MCTSTreeNode* node) const;
    MCTSTreeNode* selectChildByMaxCount(const MCTSTreeNode* node) const;
    MCTSTreeNode* selectChildBySoftmaxCount(const MCTSTreeNode* node, float temperature = 1.0f) const;
    void addNoiseToNode(MCTSTreeNode* node);
    float calculateInitQValue(const MCTSTreeNode* node) const;
    std::vector<float> calculateDirichletNoise(int size, float alpha) const;
    MCTSTreeNode* allocateNewNodes(int size);

    int current_tree_size_;
    std::vector<MCTSTreeNode> nodes_;
    MCTSTreeExternalData<std::vector<float>> hidden_state_external_data_;
};

} // namespace minizero::actor