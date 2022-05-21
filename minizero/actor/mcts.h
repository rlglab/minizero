#pragma once

#include "configuration.h"
#include "environment.h"
#include "tree.h"
#include <vector>

namespace minizero::actor {

class MCTSNode : public BaseTreeNode {
public:
    MCTSNode() { reset(); }

    void reset() override;
    void add(float value, float weight = 1.0f);
    void remove(float value, float weight = 1.0f);
    float getPUCTScore(int total_simulation, float init_q_value = -1.0f);
    std::string toString() const override;
    bool displayInTreeLog() const override { return count_ > 0; }

    // setter
    inline void setExtraDataIndex(int extra_data_index) { extra_data_index_ = extra_data_index; }
    inline void setMean(float mean) { mean_ = mean; }
    inline void setCount(float count) { count_ = count; }
    inline void setPolicy(float policy) { policy_ = policy; }
    inline void setPolicyLogit(float policy_logit) { policy_logit_ = policy_logit; }
    inline void setPolicyNoise(float policy_noise) { policy_noise_ = policy_noise; }
    inline void setValue(float value) { value_ = value; }
    inline void setFirstChild(MCTSNode* first_child) { BaseTreeNode::setFirstChild(first_child); }

    // getter
    inline int getExtraDataIndex() const { return extra_data_index_; }
    inline float getMean() const { return mean_; }
    inline float getCount() const { return count_; }
    inline float getPolicy() const { return policy_; }
    inline float getPolicyLogit() const { return policy_logit_; }
    inline float getPolicyNoise() const { return policy_noise_; }
    inline float getValue() const { return value_; }
    inline MCTSNode* getFirstChild() const { return static_cast<MCTSNode*>(BaseTreeNode::getFirstChild()); }

protected:
    int extra_data_index_;
    float mean_;
    float count_;
    float policy_;
    float policy_logit_;
    float policy_noise_;
    float value_;
};

class MCTSNodeExtraData {
public:
    MCTSNodeExtraData(const std::vector<float>& hidden_state)
        : hidden_state_(hidden_state) {}
    std::vector<float> hidden_state_;
};

typedef Tree<MCTSNode> MCTSTree;
typedef TreeExtraData<MCTSNodeExtraData> MCTSTreeExtraData;

class MCTS {
public:
    class ActionCandidate {
    public:
        Action action_;
        float policy_;
        float policy_logit_;
        ActionCandidate(const Action& action, const float& policy, const float& policy_logit)
            : action_(action), policy_(policy), policy_logit_(policy_logit) {}
    };

    MCTS(long long tree_node_size)
        : tree_(tree_node_size) {}

    void reset();
    bool isResign(const MCTSNode* selected_node) const;
    MCTSNode* selectChildByMaxCount(const MCTSNode* node) const;
    MCTSNode* selectChildBySoftmaxCount(const MCTSNode* node, float temperature = 1.0f) const;
    std::string getSearchDistributionString() const;
    std::vector<MCTSNode*> select();
    std::vector<MCTSNode*> selectFromNode(MCTSNode* start_node);
    void expand(MCTSNode* leaf_node, const std::vector<ActionCandidate>& action_candidates);
    void backup(const std::vector<MCTSNode*>& node_path, const float value);

    inline int getNumSimulation() const { return tree_.getRootNode()->getCount(); }
    inline bool reachMaximumSimulation() const { return (getNumSimulation() == config::actor_num_simulation + 1); }
    inline MCTSTree& getTree() { return tree_; }
    inline const MCTSTree& getTree() const { return tree_; }
    inline MCTSNode* getRootNode() { return tree_.getRootNode(); }
    inline const MCTSNode* getRootNode() const { return tree_.getRootNode(); }
    inline MCTSTreeExtraData& getTreeExtraData() { return tree_extra_data_; }
    inline const MCTSTreeExtraData& getTreeExtraData() const { return tree_extra_data_; }

private:
    MCTSNode* selectChildByPUCTScore(const MCTSNode* node) const;
    float calculateInitQValue(const MCTSNode* node) const;

    MCTSTree tree_;
    MCTSTreeExtraData tree_extra_data_;
};

} // namespace minizero::actor