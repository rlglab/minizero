#include "mcts.h"
#include "random.h"
#include <cmath>

namespace minizero::actor {

using namespace minizero::utils;

void MCTSNode::reset()
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

void MCTSNode::add(float value, float weight /*= 1.0f*/)
{
    if (count_ + weight <= 0) {
        reset();
    } else {
        count_ += weight;
        mean_ += weight * (value - mean_) / count_;
    }
}

void MCTSNode::remove(float value, float weight /*= 1.0f*/)
{
    if (count_ + weight <= 0) {
        reset();
    } else {
        count_ -= weight;
        mean_ -= weight * (value - mean_) / count_;
    }
}

float MCTSNode::getPUCTScore(int total_simulation, float init_q_value /*= -1.0f*/)
{
    float puct_bias = config::actor_mcts_puct_init + log((1 + total_simulation + config::actor_mcts_puct_base) / config::actor_mcts_puct_base);
    float value_u = (puct_bias * getPolicy() * sqrt(total_simulation)) / (1 + getCount());
    float value_q = (getCount() == 0 ? init_q_value : (action_.getPlayer() == env::Player::kPlayer1 ? getMean() : -getMean()));
    return value_u + value_q;
}

std::string MCTSNode::toString() const
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

void MCTS::reset()
{
    tree_.reset();
    tree_extra_data_.reset();
}

bool MCTS::isResign(const MCTSNode* selected_node) const
{
    const Action& action = selected_node->getAction();
    float root_win_rate = (action.getPlayer() == env::Player::kPlayer1 ? tree_.getRootNode()->getMean() : -tree_.getRootNode()->getMean());
    float action_win_rate = (action.getPlayer() == env::Player::kPlayer1 ? selected_node->getMean() : -selected_node->getMean());
    return (root_win_rate < config::actor_resign_threshold && action_win_rate < config::actor_resign_threshold);
}

MCTSNode* MCTS::selectChildByMaxCount(const MCTSNode* node) const
{
    assert(node && !node->isLeaf());

    float max_count = 0.0f;
    MCTSNode* selected = nullptr;
    MCTSNode* child = node->getFirstChild();
    for (int i = 0; i < node->getNumChildren(); ++i, ++child) {
        if (child->getCount() <= max_count) { continue; }
        max_count = child->getCount();
        selected = child;
    }

    assert(selected != nullptr);
    return selected;
}

MCTSNode* MCTS::selectChildBySoftmaxCount(const MCTSNode* node, float temperature /*= 1.0f*/) const
{
    assert(node && !node->isLeaf());

    float sum = 0.0f;
    MCTSNode* selected = nullptr;
    MCTSNode* child = node->getFirstChild();
    for (int i = 0; i < node->getNumChildren(); ++i, ++child) {
        float count = std::pow(child->getCount(), 1 / temperature);
        if (count == 0) { continue; }
        sum += count;
        float rand = Random::randReal(sum);
        if (rand < count) { selected = child; }
    }

    assert(selected != nullptr);
    return selected;
}

std::string MCTS::getSearchDistributionString() const
{
    const MCTSNode* root = getRootNode();
    MCTSNode* child = root->getFirstChild();
    std::ostringstream oss;
    for (int i = 0; i < root->getNumChildren(); ++i, ++child) {
        if (child->getCount() == 0) { continue; }
        oss << (oss.str().empty() ? "" : ",")
            << child->getAction().getActionID() << ":" << child->getCount();
    }
    return oss.str();
}

std::vector<MCTSNode*> MCTS::select()
{
    return selectFromNode(getRootNode());
}

std::vector<MCTSNode*> MCTS::selectFromNode(MCTSNode* start_node)
{
    assert(start_node);
    MCTSNode* node = start_node;
    std::vector<MCTSNode*> node_path{node};
    while (!node->isLeaf()) {
        node = selectChildByPUCTScore(node);
        node_path.push_back(node);
    }
    return node_path;
}

void MCTS::expand(MCTSNode* leaf_node, const std::vector<ActionCandidate>& action_candidates)
{
    assert(leaf_node && action_candidates.size() > 0);
    MCTSNode* child = tree_.allocateNodes(action_candidates.size());
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

void MCTS::backup(const std::vector<MCTSNode*>& node_path, const float value)
{
    assert(node_path.size() > 0);
    node_path.back()->setValue(value);
    for (int i = static_cast<int>(node_path.size() - 1); i >= 0; --i) {
        MCTSNode* node = node_path[i];
        node->add(value);
    }
}

MCTSNode* MCTS::selectChildByPUCTScore(const MCTSNode* node) const
{
    assert(node && !node->isLeaf());

    float best_score = -1.0f;
    MCTSNode* selected = nullptr;
    MCTSNode* child = node->getFirstChild();
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

float MCTS::calculateInitQValue(const MCTSNode* node) const
{
    assert(node && !node->isLeaf());

    // init Q value = avg Q value of all visited children + one loss
    float sum_of_win = 0.0f, sum = 0.0f;
    MCTSNode* child = node->getFirstChild();
    for (int i = 0; i < node->getNumChildren(); ++i, ++child) {
        if (child->getCount() == 0) { continue; }
        sum_of_win += child->getMean();
        sum += 1;
    }
    sum_of_win = (node->getFirstChild()->getAction().getPlayer() == env::Player::kPlayer1 ? sum_of_win : -sum_of_win);
    return (sum_of_win - 1) / (sum + 1);
}

} // namespace minizero::actor