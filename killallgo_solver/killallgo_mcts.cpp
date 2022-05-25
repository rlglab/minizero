#include "killallgo_mcts.h"
#include "killallgo_configuration.h"

namespace solver {

using namespace minizero;
using namespace minizero::env::go;
using namespace minizero::env::killallgo;

void KillAllGoMCTSNode::reset()
{
    MCTSNode::reset();
    equal_loss_ = -1;
    solver_result_ = SolveResult::kSolverUnknown;
}

std::string KillAllGoMCTSNode::toString() const
{
    std::ostringstream oss;
    if (getNumChildren() == 0) { oss << "Leaf node, "; }
    oss.precision(4);
    oss << "Solve_Result: ";
    switch (solver_result_) {
        case SolveResult::kSolverWin: oss << "WIN, "; break;
        case SolveResult::kSolverDraw: oss << "DRAW, "; break;
        case SolveResult::kSolverLoss: oss << "LOSS, "; break;
        case SolveResult::kSolverUnknown: oss << "UNKNOWN, "; break;
        default: break;
    }
    oss << std::fixed << "p = " << policy_
        << ", p_logit = " << policy_logit_
        << ", p_noise = " << policy_noise_
        << ", v = " << value_
        << ", mean = " << mean_
        << ", count = " << count_
        << ", equal_loss = " << equal_loss_
        << ", extra_data_idnex = " << extra_data_index_;

    return oss.str();
}

float KillAllGoMCTSNode::getNormalizedMean(const std::map<float, int>& tree_value_map)
{
    if (count_ == 0 || tree_value_map.size() <= 1) { return -1.0f; }

    const float value_lower_bound = tree_value_map.begin()->first;
    const float value_upper_bound = tree_value_map.rbegin()->first;
    float value = (mean_ - value_lower_bound) / (value_upper_bound - value_lower_bound);
    return fmin(1, fmax(-1, 2 * value - 1)); // normalize to [-1, 1]
}

float KillAllGoMCTSNode::getNormalizedPUCTScore(int total_simulation, const std::map<float, int>& tree_value_map, float init_q_value /*= -1.0f*/)
{
    float puct_bias = config::actor_mcts_puct_init + log((1 + total_simulation + config::actor_mcts_puct_base) / config::actor_mcts_puct_base);
    float value_u = (puct_bias * getPolicy() * sqrt(total_simulation)) / (1 + getCount());
    float value_q = (getCount() == 0 ? init_q_value : (action_.getPlayer() == env::Player::kPlayer1 ? getNormalizedMean(tree_value_map) : -getNormalizedMean(tree_value_map)));
    return value_u + value_q;
}

void KillAllGoMCTS::reset()
{
    BaseMCTS::reset();
    tree_value_map_.clear();
}

KillAllGoMCTSNode* KillAllGoMCTS::selectChildBySoftmaxCount(const KillAllGoMCTSNode* node, float temperature /*= 1.0f*/, float value_threshold /*= 0.1f*/) const
{
    assert(node && !node->isLeaf());
    KillAllGoMCTSNode* selected = nullptr;
    KillAllGoMCTSNode* child = node->getFirstChild();
    KillAllGoMCTSNode* best_child = selectChildByMaxCount(node);
    float sum = 0.0f;
    for (int i = 0; i < node->getNumChildren(); ++i, ++child) {
        float count = std::pow(child->getCount(), 1 / temperature);
        if (count == 0 || fabs(best_child->getNormalizedMean(tree_value_map_) - child->getNormalizedMean(tree_value_map_)) > value_threshold) { continue; }
        sum += count;
        float rand = utils::Random::randReal(sum);
        if (rand < count) { selected = child; }
    }
    assert(selected != nullptr);
    return selected;
}

void KillAllGoMCTS::backup(const std::vector<KillAllGoMCTSNode*>& node_path, const float value)
{
    assert(node_path.size() > 0);

    // value from root's perspective
    float root_value = value;
    for (int i = static_cast<int>(node_path.size() - 1); i > 0; --i) {
        if (node_path[i]->getAction().getPlayer() == env::Player::kPlayer1) { root_value += std::log10(50); }
    }
    root_value = fmax(0, fmin(nn_value_size, root_value));

    // update value
    node_path.back()->setValue(value);
    for (int i = static_cast<int>(node_path.size() - 1); i >= 0; --i) {
        KillAllGoMCTSNode* node = node_path[i];
        float original_value = node->getMean();
        node->add(root_value);

        // update tree value map
        if (tree_value_map_.count(original_value)) {
            --tree_value_map_[original_value];
            if (tree_value_map_[original_value] == 0) { tree_value_map_.erase(original_value); }
        }
        ++tree_value_map_[original_value];
    }
}

KillAllGoMCTSNode* KillAllGoMCTS::selectChildByPUCTScore(const KillAllGoMCTSNode* node) const
{
    assert(node && !node->isLeaf());
    float best_score = -1.0f;
    KillAllGoMCTSNode* selected = nullptr;
    KillAllGoMCTSNode* child = node->getFirstChild();
    int total_simulation = node->getCount();
    float init_q_value = calculateInitQValue(node);
    for (int i = 0; i < node->getNumChildren(); ++i, ++child) {
        float score = child->getNormalizedPUCTScore(total_simulation, tree_value_map_, init_q_value);
        if (score <= best_score) { continue; }
        best_score = score;
        selected = child;
    }
    assert(selected != nullptr);
    return selected;
}

float KillAllGoMCTS::calculateInitQValue(const KillAllGoMCTSNode* node) const
{
    assert(node && !node->isLeaf());

    // init Q value = avg Q value of all visited children + one loss
    float sum_of_win = 0.0f, sum = 0.0f;
    KillAllGoMCTSNode* child = node->getFirstChild();
    for (int i = 0; i < node->getNumChildren(); ++i, ++child) {
        if (child->getCount() == 0) { continue; }
        sum_of_win += child->getNormalizedMean(tree_value_map_);
        sum += 1;
    }
    sum_of_win = (node->getFirstChild()->getAction().getPlayer() == env::Player::kPlayer1 ? sum_of_win : -sum_of_win);
    return (sum_of_win - 1) / (sum + 1);
}

} // namespace solver