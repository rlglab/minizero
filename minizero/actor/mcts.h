#pragma once

#include "configuration.h"
#include "environment.h"
#include "random.h"
#include "search.h"
#include "tree.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace minizero::actor {

class MCTSNode : public TreeNode {
public:
    MCTSNode() { reset(); }

    void reset() override;
    virtual void add(float value, float weight = 1.0f);
    virtual void remove(float value, float weight = 1.0f);
    virtual float getNormalizedMean(const std::map<float, int>& tree_value_bound) const;
    virtual float getNormalizedPUCTScore(int total_simulation, const std::map<float, int>& tree_value_bound, float init_q_value = -1.0f) const;
    std::string toString() const override;
    bool displayInTreeLog() const override { return count_ > 0; }

    // setter
    inline void setIsLegalAction(bool is_legal_action) { is_legal_action_ = is_legal_action; }
    inline void setIsLegalPlayer(bool is_legal_player) { is_legal_player_ = is_legal_player; }
    inline void setIsTerminal(bool is_terminal) { is_terminal_ = is_terminal; }
    inline void setHiddenStateDataIndex(int hidden_state_data_index) { hidden_state_data_index_ = hidden_state_data_index; }
    inline void setMean(float mean) { mean_ = mean; }
    inline void setCount(float count) { count_ = count; }
    inline void addVirtualLoss(float num = 1.0f) { virtual_loss_ += num; }
    inline void removeVirtualLoss(float num = 1.0f) { virtual_loss_ -= num; }
    inline void setPolicy(float policy) { policy_ = policy; }
    inline void setPolicyLogit(float policy_logit) { policy_logit_ = policy_logit; }
    inline void setPolicyNoise(float policy_noise) { policy_noise_ = policy_noise; }
    inline void setValue(float value) { value_ = value; }
    inline void setReward(float reward) { reward_ = reward; }
    inline void setFirstChild(MCTSNode* first_child) { TreeNode::setFirstChild(first_child); }

    // getter
    inline bool getIsLegalAction() const { return is_legal_action_; }
    inline bool getIsLegalPlayer() const { return is_legal_player_; }
    inline bool getIsTerminal() const { return is_terminal_; }
    inline bool getIsLegal() const { return (is_legal_action_ && is_legal_player_ && !is_terminal_); }
    inline int getHiddenStateDataIndex() const { return hidden_state_data_index_; }
    inline float getMean() const { return mean_; }
    inline float getCount() const { return count_; }
    inline float getCountWithVirtualLoss() const { return count_ + virtual_loss_; }
    inline float getVirtualLoss() const { return virtual_loss_; }
    inline float getPolicy() const { return policy_; }
    inline float getPolicyLogit() const { return policy_logit_; }
    inline float getPolicyNoise() const { return policy_noise_; }
    inline float getValue() const { return value_; }
    inline float getReward() const { return reward_; }
    inline virtual MCTSNode* getChild(int index) const override { return (index < num_children_ ? static_cast<MCTSNode*>(first_child_) + index : nullptr); }

protected:
    bool is_legal_action_;
    bool is_legal_player_;
    bool is_terminal_;
    int hidden_state_data_index_;
    float mean_;
    float count_;
    float virtual_loss_;
    float policy_;
    float policy_logit_;
    float policy_noise_;
    float value_;
    float reward_;
};

class HiddenStateData {
public:
    HiddenStateData(const std::vector<float>& hidden_state)
        : hidden_state_(hidden_state) {}
    std::vector<float> hidden_state_;
};
typedef TreeData<HiddenStateData> TreeHiddenStateData;

class MCTS : public Tree, public Search {
public:
    class ActionCandidate {
    public:
        Action action_;
        float policy_;
        float policy_logit_;
        ActionCandidate(const Action& action, const float& policy, const float& policy_logit)
            : action_(action), policy_(policy), policy_logit_(policy_logit) {}
    };

    MCTS(uint64_t tree_node_size)
        : Tree(tree_node_size) {}

    void reset() override;
    virtual bool isResign(const MCTSNode* selected_node) const;
    virtual MCTSNode* selectChildByMaxCount(const MCTSNode* node) const;
    virtual MCTSNode* selectChildBySoftmaxCount(const MCTSNode* node, float temperature = 1.0f, float value_threshold = 0.1f) const;
    virtual std::string getSearchDistributionString() const;
    virtual std::vector<MCTSNode*> select() { return selectFromNode(getRootNode()); }
    virtual std::vector<MCTSNode*> selectFromNode(MCTSNode* start_node);
    virtual void expand(MCTSNode* leaf_node, const std::vector<ActionCandidate>& action_candidates);
    virtual void backup(const std::vector<MCTSNode*>& node_path, const float value, const float reward = 0.0f);

    void increaseLegalNodeCount() { ++legal_node_count_; }
    void increaseIllegalPlayerNodeCount() { ++illegal_player_node_count_; }
    void increaseIllegalActionNodeCount() { ++illegal_action_node_count_; }
    void increaseIllegalBothNodeCount() { ++illegal_both_node_count_; }
    void increaseTerminalNodeCount() { ++terminal_node_count_; }
    inline float getLegalRate() const { return (legal_node_count_ / static_cast<float>(config::actor_num_simulation) * 100); }
    inline float getIllegalPlayerRate() const { return (illegal_player_node_count_ / static_cast<float>(config::actor_num_simulation) * 100); }
    inline float getIllegalActionRate() const { return (illegal_action_node_count_ / static_cast<float>(config::actor_num_simulation) * 100); }
    inline float getIllegalBothRate() const { return (illegal_both_node_count_ / static_cast<float>(config::actor_num_simulation) * 100); }
    inline float getTerminalRate() const { return (terminal_node_count_ / static_cast<float>(config::actor_num_simulation) * 100); }
    inline MCTSNode* allocateNodes(int size) { return static_cast<MCTSNode*>(Tree::allocateNodes(size)); }
    inline int getNumSimulation() const { return getRootNode()->getCount(); }
    inline bool reachMaximumSimulation() const { return (getNumSimulation() == config::actor_num_simulation + 1); }
    inline MCTSNode* getRootNode() { return static_cast<MCTSNode*>(Tree::getRootNode()); }
    inline const MCTSNode* getRootNode() const { return static_cast<const MCTSNode*>(Tree::getRootNode()); }
    inline TreeHiddenStateData& getTreeHiddenStateData() { return tree_hidden_state_data_; }
    inline const TreeHiddenStateData& getTreeHiddenStateData() const { return tree_hidden_state_data_; }
    inline std::map<float, int>& getTreeValueBound() { return tree_value_bound_; }
    inline const std::map<float, int>& getTreeValueBound() const { return tree_value_bound_; }

    float change_;

protected:
    int legal_node_count_;
    int illegal_player_node_count_;
    int illegal_action_node_count_;
    int illegal_both_node_count_;
    int illegal_parent_player_node_count_;
    int illegal_parent_action_node_count_;
    int terminal_node_count_;

    TreeNode* createTreeNodes(uint64_t tree_node_size) override { return new MCTSNode[tree_node_size]; }
    TreeNode* getNodeIndex(int index) override { return getRootNode() + index; }

    virtual MCTSNode* selectChildByPUCTScore(const MCTSNode* node) const;
    virtual float calculateInitQValue(const MCTSNode* node) const;
    virtual void updateTreeValueBound(float old_value, float new_value);

    std::map<float, int> tree_value_bound_;
    TreeHiddenStateData tree_hidden_state_data_;
};

} // namespace minizero::actor
