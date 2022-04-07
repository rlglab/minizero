#include "mcts_tree.h"
#include "random.h"
#include <cmath>

namespace minizero::actor {

using namespace minizero::utils;

MCTSTreeNode::MCTSTreeNode()
{
    reset();
}

void MCTSTreeNode::reset()
{
    num_children_ = 0;
    hidden_state_external_data_index_ = -1;
    mean_ = 0.0f;
    count_ = 0.0f;
    policy_ = 0.0f;
    policy_logit_ = 0.0f;
    policy_noise_ = 0.0f;
    value_ = 0.0f;
    first_child_ = nullptr;
}

void MCTSTreeNode::add(float value, float weight /*= 1.0f*/)
{
    if (count_ + weight <= 0) {
        reset();
    } else {
        count_ += weight;
        mean_ += weight * (value - mean_) / count_;
    }
}

void MCTSTreeNode::remove(float value, float weight /*= 1.0f*/)
{
    if (count_ + weight <= 0) {
        reset();
    } else {
        count_ -= weight;
        mean_ -= weight * (value - mean_) / count_;
    }
}

float MCTSTreeNode::getPUCTScore(int total_simulation, float init_q_value /*= -1.0f*/)
{
    float puct_bias = config::actor_mcts_puct_init + log((1 + total_simulation + config::actor_mcts_puct_base) / config::actor_mcts_puct_base);
    float value_u = (puct_bias * getPolicy() * sqrt(total_simulation)) / (1 + getCount());
    float value_q = (getCount() == 0 ? init_q_value : (action_.getPlayer() == env::Player::kPlayer1 ? getMean() : -getMean()));
    return value_u + value_q;
}

std::string MCTSTreeNode::toString() const
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

void MCTSTree::reset()
{
    current_tree_size_ = 1;
    getRootNode()->reset();
    hidden_state_external_data_.reset();
}

MCTSTreeNode* MCTSTree::selectChildByMaxCount(const MCTSTreeNode* node) const
{
    assert(node && !node->isLeaf());

    float max_count = 0.0f;
    MCTSTreeNode* selected_node = nullptr;
    MCTSTreeNode* child = node->getFirstChild();
    for (int i = 0; i < node->getNumChildren(); ++i, ++child) {
        if (child->getCount() <= max_count) { continue; }
        max_count = child->getCount();
        selected_node = child;
    }

    assert(selected_node != nullptr);
    return selected_node;
}

MCTSTreeNode* MCTSTree::selectChildBySoftmaxCount(const MCTSTreeNode* node, float temperature /*= 1.0f*/) const
{
    assert(node && !node->isLeaf());

    float sum = 0.0f;
    MCTSTreeNode* selected_node = nullptr;
    MCTSTreeNode* child = node->getFirstChild();
    for (int i = 0; i < node->getNumChildren(); ++i, ++child) {
        float count = std::pow(child->getCount(), 1 / temperature);
        if (count == 0) { continue; }
        sum += count;
        float rand = Random::randReal(sum);
        if (rand < count) { selected_node = child; }
    }

    assert(selected_node != nullptr);
    return selected_node;
}

std::string MCTSTree::getSearchDistributionString() const
{
    const MCTSTreeNode* root = getRootNode();
    MCTSTreeNode* child = root->getFirstChild();
    std::ostringstream oss;
    for (int i = 0; i < root->getNumChildren(); ++i, ++child) {
        if (child->getCount() == 0) { continue; }
        oss << (oss.str().empty() ? "" : ",")
            << child->getAction().getActionID() << ":" << child->getCount();
    }
    return oss.str();
}

std::vector<MCTSTreeNode*> MCTSTree::select()
{
    MCTSTreeNode* node = getRootNode();
    std::vector<MCTSTreeNode*> node_path{node};
    while (!node->isLeaf()) {
        node = selectChildByPUCTScore(node);
        node_path.push_back(node);
    }
    return node_path;
}

void MCTSTree::expand(MCTSTreeNode* leaf_node, const std::vector<ActionCandidate>& action_candidates)
{
    assert(leaf_node && action_candidates.size() > 0);

    MCTSTreeNode* child_node = allocateNewNodes(action_candidates.size());
    leaf_node->setFirstChild(child_node);
    leaf_node->setNumChildren(action_candidates.size());
    for (const auto& candidate : action_candidates) {
        child_node->reset();
        child_node->setAction(candidate.action_);
        child_node->setPolicy(candidate.policy_);
        child_node->setPolicyLogit(candidate.policy_logit_);
        ++child_node;
    }
}

void MCTSTree::backup(std::vector<MCTSTreeNode*>& node_path, const float value)
{
    assert(node_path.size() > 0);

    node_path.back()->setValue(value);
    for (int i = static_cast<int>(node_path.size() - 1); i >= 0; --i) {
        MCTSTreeNode* node = node_path[i];
        node->add(value);
    }
}

MCTSTreeNode* MCTSTree::selectChildByPUCTScore(const MCTSTreeNode* node) const
{
    assert(node && !node->isLeaf());

    float best_score = -1.0f;
    MCTSTreeNode* best_node = nullptr;
    MCTSTreeNode* child_node = node->getFirstChild();
    int total_simulation = node->getCount();
    float init_q_value = calculateInitQValue(node);
    for (int i = 0; i < node->getNumChildren(); ++i, ++child_node) {
        float score = child_node->getPUCTScore(total_simulation, init_q_value);
        if (score <= best_score) { continue; }
        best_score = score;
        best_node = child_node;
    }

    assert(best_node != nullptr);
    return best_node;
}

float MCTSTree::calculateInitQValue(const MCTSTreeNode* node) const
{
    assert(node && !node->isLeaf());

    // init Q value = avg Q value of all visited children + one loss
    float sum_of_win = 0.0f, sum = 0.0f;
    MCTSTreeNode* child = node->getFirstChild();
    for (int i = 0; i < node->getNumChildren(); ++i, ++child) {
        if (child->getCount() == 0) { continue; }
        sum_of_win += child->getMean();
        sum += 1;
    }
    sum_of_win = (node->getFirstChild()->getAction().getPlayer() == env::Player::kPlayer1 ? sum_of_win : -sum_of_win);
    return (sum_of_win - 1) / (sum + 1);
}

MCTSTreeNode* MCTSTree::allocateNewNodes(int size)
{
    assert(current_tree_size_ + size <= static_cast<int>(nodes_.size()));
    MCTSTreeNode* node = &nodes_[current_tree_size_];
    current_tree_size_ += size;
    return node;
}

} // namespace minizero::actor