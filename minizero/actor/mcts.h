#pragma once

#include "configuration.h"
#include "environment.h"
#include "random.h"
#include "tree.h"
#include <cmath>
#include <vector>

namespace minizero::actor {

class MCTSNode : public BaseTreeNode {
public:
    MCTSNode() { reset(); }

    void reset() override
    {
        num_children_ = 0;
        extra_data_index_ = -1;
        mean_ = 0.0f;
        count_ = 0.0f;
        policy_ = 0.0f;
        policy_logit_ = 0.0f;
        policy_noise_ = 0.0f;
        value_ = 0.0f;
        first_child_ = nullptr;
    }

    void add(float value, float weight = 1.0f)
    {
        if (count_ + weight <= 0) {
            reset();
        } else {
            count_ += weight;
            mean_ += weight * (value - mean_) / count_;
        }
    }

    void remove(float value, float weight = 1.0f)
    {
        if (count_ + weight <= 0) {
            reset();
        } else {
            count_ -= weight;
            mean_ -= weight * (value - mean_) / count_;
        }
    }

    float getPUCTScore(int total_simulation, float init_q_value = -1.0f)
    {
        float puct_bias = config::actor_mcts_puct_init + log((1 + total_simulation + config::actor_mcts_puct_base) / config::actor_mcts_puct_base);
        float value_u = (puct_bias * getPolicy() * sqrt(total_simulation)) / (1 + getCount());
        float value_q = (getCount() == 0 ? init_q_value : (action_.getPlayer() == env::Player::kPlayer1 ? getMean() : -getMean()));
        return value_u + value_q;
    }

    std::string toString() const override
    {
        std::ostringstream oss;
        oss.precision(4);
        oss << std::fixed << "p = " << policy_
            << ", p_logit = " << policy_logit_
            << ", p_noise = " << policy_noise_
            << ", v = " << value_
            << ", mean = " << mean_
            << ", count = " << count_;
        return oss.str();
    }

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

template <class Node, class Tree, class TreeExtraData>
class BaseMCTS {
public:
    class ActionCandidate {
    public:
        Action action_;
        float policy_;
        float policy_logit_;
        ActionCandidate(const Action& action, const float& policy, const float& policy_logit)
            : action_(action), policy_(policy), policy_logit_(policy_logit) {}
    };

    BaseMCTS(long long tree_node_size)
        : tree_(tree_node_size) {}

    virtual void reset()
    {
        tree_.reset();
        tree_extra_data_.reset();
    }

    virtual bool isResign(const Node* selected_node) const
    {
        const Action& action = selected_node->getAction();
        float root_win_rate = (action.getPlayer() == env::Player::kPlayer1 ? tree_.getRootNode()->getMean() : -tree_.getRootNode()->getMean());
        float action_win_rate = (action.getPlayer() == env::Player::kPlayer1 ? selected_node->getMean() : -selected_node->getMean());
        return (root_win_rate < config::actor_resign_threshold && action_win_rate < config::actor_resign_threshold);
    }

    virtual Node* selectChildByMaxCount(const Node* node) const
    {
        assert(node && !node->isLeaf());
        float max_count = 0.0f;
        Node* selected = nullptr;
        Node* child = node->getFirstChild();
        for (int i = 0; i < node->getNumChildren(); ++i, ++child) {
            if (child->getCount() <= max_count) { continue; }
            max_count = child->getCount();
            selected = child;
        }
        assert(selected != nullptr);
        return selected;
    }

    virtual Node* selectChildBySoftmaxCount(const Node* node, float temperature = 1.0f, float value_threshold = 0.1f) const
    {
        assert(node && !node->isLeaf());
        Node* selected = nullptr;
        Node* child = node->getFirstChild();
        Node* best_child = selectChildByMaxCount(node);
        float sum = 0.0f;
        for (int i = 0; i < node->getNumChildren(); ++i, ++child) {
            float count = std::pow(child->getCount(), 1 / temperature);
            if (count == 0 || (child->getMean() < best_child->getMean() - value_threshold)) { continue; }
            sum += count;
            float rand = utils::Random::randReal(sum);
            if (rand < count) { selected = child; }
        }
        assert(selected != nullptr);
        return selected;
    }

    virtual std::string getSearchDistributionString() const
    {
        const Node* root = getRootNode();
        Node* child = root->getFirstChild();
        std::ostringstream oss;
        for (int i = 0; i < root->getNumChildren(); ++i, ++child) {
            if (child->getCount() == 0) { continue; }
            oss << (oss.str().empty() ? "" : ",")
                << child->getAction().getActionID() << ":" << child->getCount();
        }
        return oss.str();
    }

    virtual std::vector<Node*> select() { return selectFromNode(getRootNode()); }

    virtual std::vector<Node*> selectFromNode(Node* start_node)
    {
        assert(start_node);
        Node* node = start_node;
        std::vector<Node*> node_path{node};
        while (!node->isLeaf()) {
            node = selectChildByPUCTScore(node);
            node_path.push_back(node);
        }
        return node_path;
    }

    virtual void expand(Node* leaf_node, const std::vector<ActionCandidate>& action_candidates)
    {
        assert(leaf_node && action_candidates.size() > 0);
        Node* child = tree_.allocateNodes(action_candidates.size());
        leaf_node->setFirstChild(child);
        leaf_node->setNumChildren(action_candidates.size());
        for (const auto& candidate : action_candidates) {
            child->reset();
            child->setAction(candidate.action_);
            child->setPolicy(candidate.policy_);
            child->setPolicyLogit(candidate.policy_logit_);
            ++child;
        }
    }

    virtual void backup(const std::vector<Node*>& node_path, const float value)
    {
        assert(node_path.size() > 0);
        node_path.back()->setValue(value);
        for (int i = static_cast<int>(node_path.size() - 1); i >= 0; --i) {
            Node* node = node_path[i];
            node->add(value);
        }
    }

    inline int getNumSimulation() const { return tree_.getRootNode()->getCount(); }
    inline bool reachMaximumSimulation() const { return (getNumSimulation() == config::actor_num_simulation + 1); }
    inline Tree& getTree() { return tree_; }
    inline const Tree& getTree() const { return tree_; }
    inline Node* getRootNode() { return tree_.getRootNode(); }
    inline const Node* getRootNode() const { return tree_.getRootNode(); }
    inline TreeExtraData& getTreeExtraData() { return tree_extra_data_; }
    inline const TreeExtraData& getTreeExtraData() const { return tree_extra_data_; }

protected:
    virtual Node* selectChildByPUCTScore(const Node* node) const
    {
        assert(node && !node->isLeaf());
        float best_score = -1.0f;
        Node* selected = nullptr;
        Node* child = node->getFirstChild();
        int total_simulation = node->getCount();
        float init_q_value = calculateInitQValue(node);
        for (int i = 0; i < node->getNumChildren(); ++i, ++child) {
            float score = child->getPUCTScore(total_simulation, init_q_value);
            if (score <= best_score) { continue; }
            best_score = score;
            selected = child;
        }
        assert(selected != nullptr);
        return selected;
    }

    virtual float calculateInitQValue(const Node* node) const
    {
        // init Q value = avg Q value of all visited children + one loss
        assert(node && !node->isLeaf());
        float sum_of_win = 0.0f, sum = 0.0f;
        Node* child = node->getFirstChild();
        for (int i = 0; i < node->getNumChildren(); ++i, ++child) {
            if (child->getCount() == 0) { continue; }
            sum_of_win += child->getMean();
            sum += 1;
        }
        sum_of_win = (node->getFirstChild()->getAction().getPlayer() == env::Player::kPlayer1 ? sum_of_win : -sum_of_win);
        return (sum_of_win - 1) / (sum + 1);
    }

    Tree tree_;
    TreeExtraData tree_extra_data_;
};

typedef Tree<MCTSNode> MCTSTree;
typedef TreeExtraData<MCTSNodeExtraData> MCTSTreeExtraData;
typedef BaseMCTS<MCTSNode, MCTSTree, MCTSTreeExtraData> MCTS;

} // namespace minizero::actor