#include "mcts_tree.h"
#include "random.h"
#include <cmath>

namespace minizero::actor {

using namespace minizero::utils;

MCTSTreeNode::MCTSTreeNode()
{
    Reset();
}

void MCTSTreeNode::Reset()
{
    num_children_ = 0;
    mean_ = 0.0f;
    count_ = 0.0f;
    policy_ = 0.0f;
    value_ = 0.0f;
    first_child_ = nullptr;
    hidden_state_.clear();
}

void MCTSTreeNode::Add(float value, float weight /*= 1.0f*/)
{
    if (count_ + weight <= 0) {
        Reset();
    } else {
        count_ += weight;
        mean_ += weight * (value - mean_) / count_;
    }
}

void MCTSTreeNode::Remove(float value, float weight /*= 1.0f*/)
{
    if (count_ + weight <= 0) {
        Reset();
    } else {
        count_ -= weight;
        mean_ -= weight * (value - mean_) / count_;
    }
}

float MCTSTreeNode::GetPUCTScore(int total_simulation)
{
    float puct_bias = config::actor_mcts_puct_init + log((1 + total_simulation + config::actor_mcts_puct_base) / config::actor_mcts_puct_base);
    float value_u = (puct_bias * GetPolicy() * sqrt(total_simulation)) / (1 + GetCount());
    float value_q = (GetCount() == 0 ? -1 : (action_.GetPlayer() == env::Player::kPlayer1 ? GetMean() : -GetMean()));
    return value_u + value_q;
}

std::string MCTSTreeNode::ToString() const
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

Action MCTSTree::DecideAction() const
{
    if (config::actor_select_action_by_count) {
        return SelectChildByMaxCount(GetRootNode())->GetAction();
    } else if (config::actor_select_action_by_softmax_count) {
        return SelectChildBySoftmaxCount(GetRootNode(), config::actor_select_action_softmax_temperature)->GetAction();
    }

    assert(false);
    return Action();
}

std::string MCTSTree::GetActionDistributionString() const
{
    const MCTSTreeNode* root = GetRootNode();
    MCTSTreeNode* child = root->GetFirstChild();
    std::ostringstream oss;
    for (int i = 0; i < root->GetNumChildren(); ++i, ++child) {
        if (child->GetCount() == 0) { continue; }
        oss << (oss.str().empty() ? "" : ",")
            << child->GetAction().GetActionID() << ":" << child->GetCount();
    }
    return oss.str();
}

void MCTSTree::Reset()
{
    current_tree_size_ = 1;
    GetRootNode()->Reset();
}

std::vector<MCTSTreeNode*> MCTSTree::Select()
{
    MCTSTreeNode* node = GetRootNode();
    std::vector<MCTSTreeNode*> node_path{node};
    while (!node->IsLeaf()) {
        node = SelectChildByPUCTScore(node);
        node_path.push_back(node);
    }
    return node_path;
}

void MCTSTree::Expand(MCTSTreeNode* leaf_node, const std::vector<std::pair<Action, float>>& action_policy)
{
    assert(leaf_node && action_policy.size() > 0);

    MCTSTreeNode* child_node = AllocateNewNodes(action_policy.size());
    leaf_node->SetFirstChild(child_node);
    leaf_node->SetNumChildren(action_policy.size());
    for (const auto& p : action_policy) {
        child_node->Reset();
        child_node->SetPolicy(p.second);
        child_node->SetAction(p.first);
        ++child_node;
    }
    AddNoiseToNode(leaf_node);
}

void MCTSTree::Backup(std::vector<MCTSTreeNode*>& node_path, const float value)
{
    assert(node_path.size() > 0);

    node_path.back()->SetValue(value);
    for (int i = static_cast<int>(node_path.size() - 1); i >= 0; --i) {
        MCTSTreeNode* node = node_path[i];
        node->Add(value);
    }
}

MCTSTreeNode* MCTSTree::SelectChildByPUCTScore(const MCTSTreeNode* node) const
{
    assert(node && !node->IsLeaf());

    float best_score = -1.0f;
    MCTSTreeNode* best_node = nullptr;
    MCTSTreeNode* child_node = node->GetFirstChild();
    int total_simulation = node->GetCount();
    for (int i = 0; i < node->GetNumChildren(); ++i, ++child_node) {
        float score = child_node->GetPUCTScore(total_simulation);
        if (score <= best_score) { continue; }

        best_score = score;
        best_node = child_node;
    }
    return best_node;
}

MCTSTreeNode* MCTSTree::SelectChildByMaxCount(const MCTSTreeNode* node) const
{
    assert(node && !node->IsLeaf());

    float max_count = 0.0f;
    MCTSTreeNode* selected = nullptr;
    MCTSTreeNode* child = node->GetFirstChild();
    for (int i = 0; i < node->GetNumChildren(); ++i, ++child) {
        if (child->GetCount() <= max_count) { continue; }
        max_count = child->GetCount();
        selected = child;
    }
    return selected;
}

MCTSTreeNode* MCTSTree::SelectChildBySoftmaxCount(const MCTSTreeNode* node, float temperature /*= 1.0f*/) const
{
    assert(node && !node->IsLeaf());

    float count_sum = 0.0f;
    MCTSTreeNode* selected = nullptr;
    MCTSTreeNode* child = node->GetFirstChild();
    for (int i = 0; i < node->GetNumChildren(); ++i, ++child) {
        float count = std::pow(child->GetCount(), 1 / temperature);
        if (count == 0) { continue; }
        count_sum += count;
        float rand = Random::RandReal(count_sum);
        if (rand < count) { selected = child; }
    }
    return selected;
}

void MCTSTree::AddNoiseToNode(MCTSTreeNode* node)
{
    assert(node && !node->IsLeaf());

    if (config::actor_use_dirichlet_noise && node == GetRootNode()) {
        const float epsilon = config::actor_dirichlet_noise_epsilon;
        std::vector<float> dirichlet_noise = CalculateDirichletNoise(node->GetNumChildren(), config::actor_dirichlet_noise_alpha);
        MCTSTreeNode* child = node->GetFirstChild();
        for (int i = 0; i < node->GetNumChildren(); ++i, ++child) {
            child->SetPolicy((1 - epsilon) * child->GetPolicy() + epsilon * dirichlet_noise[i]);
        }
    }
}

std::vector<float> MCTSTree::CalculateDirichletNoise(int size, float alpha) const
{
    std::vector<float> dirichlet_noise;
    std::gamma_distribution<float> gamma_distribution(alpha);
    for (int i = 0; i < size; ++i) { dirichlet_noise.emplace_back(gamma_distribution(Random::generator_)); }
    float sum = std::accumulate(dirichlet_noise.begin(), dirichlet_noise.end(), 0.0f);
    if (sum < std::numeric_limits<float>::min()) { return dirichlet_noise; }
    for (int i = 0; i < size; ++i) { dirichlet_noise[i] /= sum; }
    return dirichlet_noise;
}

MCTSTreeNode* MCTSTree::AllocateNewNodes(int size)
{
    assert(current_tree_size_ + size <= static_cast<int>(nodes_.size()));
    MCTSTreeNode* node = &nodes_[current_tree_size_];
    current_tree_size_ += size;
    return node;
}

} // namespace minizero::actor