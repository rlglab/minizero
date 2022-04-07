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
    mean_ = 0.0f;
    count_ = 0.0f;
    policy_ = 0.0f;
    value_ = 0.0f;
    first_child_ = nullptr;
    hidden_state_.clear();
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
        << ", v = " << value_
        << ", mean = " << mean_
        << ", count = " << count_;
    return oss.str();
}

MCTSTree::MCTSTree(long long tree_node_size)
{
    nodes_.resize(tree_node_size);
}

void MCTSTree::reset()
{
    current_tree_size_ = 1;
    getRootNode()->reset();
}

Action MCTSTree::decideAction() const
{
    return decideActionNode()->getAction();
}

MCTSTreeNode* MCTSTree::decideActionNode() const
{
    if (config::actor_select_action_by_count) {
        return selectChildByMaxCount(getRootNode());
    } else if (config::actor_select_action_by_softmax_count) {
        return selectChildBySoftmaxCount(getRootNode(), config::actor_select_action_softmax_temperature);
    }

    assert(false);
    return nullptr;
}

std::string MCTSTree::getActionDistributionString() const
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

void MCTSTree::expand(MCTSTreeNode* leaf_node, const std::vector<std::pair<Action, float>>& action_policy)
{
    assert(leaf_node && action_policy.size() > 0);

    MCTSTreeNode* child_node = allocateNewNodes(action_policy.size());
    leaf_node->setFirstChild(child_node);
    leaf_node->setNumChildren(action_policy.size());
    for (const auto& p : action_policy) {
        child_node->reset();
        child_node->setPolicy(p.second);
        child_node->setAction(p.first);
        ++child_node;
    }

    // add noise to root's children
    if (config::actor_use_dirichlet_noise && leaf_node == getRootNode()) { addNoiseToNode(leaf_node); }
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

MCTSTreeNode* MCTSTree::selectChildByMaxCount(const MCTSTreeNode* node) const
{
    assert(node && !node->isLeaf());

    float max_count = 0.0f;
    MCTSTreeNode* selected = nullptr;
    MCTSTreeNode* child = node->getFirstChild();
    for (int i = 0; i < node->getNumChildren(); ++i, ++child) {
        if (child->getCount() <= max_count) { continue; }
        max_count = child->getCount();
        selected = child;
    }
    return selected;
}

MCTSTreeNode* MCTSTree::selectChildBySoftmaxCount(const MCTSTreeNode* node, float temperature /*= 1.0f*/) const
{
    assert(node && !node->isLeaf());

    float sum = 0.0f;
    MCTSTreeNode* selected = nullptr;
    MCTSTreeNode* child = node->getFirstChild();
    for (int i = 0; i < node->getNumChildren(); ++i, ++child) {
        float count = std::pow(child->getCount(), 1 / temperature);
        if (count == 0) { continue; }
        sum += count;
        float rand = Random::randReal(sum);
        if (rand < count) { selected = child; }
    }
    return selected;
}

void MCTSTree::addNoiseToNode(MCTSTreeNode* node)
{
    assert(node && !node->isLeaf());

    const float epsilon = config::actor_dirichlet_noise_epsilon;
    std::vector<float> dirichlet_noise = calculateDirichletNoise(node->getNumChildren(), config::actor_dirichlet_noise_alpha);
    MCTSTreeNode* child = node->getFirstChild();
    for (int i = 0; i < node->getNumChildren(); ++i, ++child) {
        child->setPolicy((1 - epsilon) * child->getPolicy() + epsilon * dirichlet_noise[i]);
    }
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

std::vector<float> MCTSTree::calculateDirichletNoise(int size, float alpha) const
{
    std::gamma_distribution<float> gamma_distribution(alpha);
    std::vector<float> dirichlet_noise;
    for (int i = 0; i < size; ++i) { dirichlet_noise.emplace_back(gamma_distribution(Random::generator_)); }
    float sum = std::accumulate(dirichlet_noise.begin(), dirichlet_noise.end(), 0.0f);
    if (sum < std::numeric_limits<float>::min()) { return dirichlet_noise; }
    for (int i = 0; i < size; ++i) { dirichlet_noise[i] /= sum; }
    return dirichlet_noise;
}

MCTSTreeNode* MCTSTree::allocateNewNodes(int size)
{
    assert(current_tree_size_ + size <= static_cast<int>(nodes_.size()));
    MCTSTreeNode* node = &nodes_[current_tree_size_];
    current_tree_size_ += size;
    return node;
}

} // namespace minizero::actor