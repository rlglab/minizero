#include "killallgo_mcts_solver.h"

namespace minizero::solver {

using namespace network;

void KillAllGoMCTSNode::reset()
{
    MCTSNode::reset();
    solver_result_ = SolveResult::kSolverUnknown;
}

void KillAllGoMCTSSolver::reset()
{
    env_.reset();
    resetTree();
}

void KillAllGoMCTSSolver::resetTree()
{
    tree_.reset();
    tree_extra_data_.reset();
    env_backup_ = env_;
}

SolveResult KillAllGoMCTSSolver::solve()
{
    resetTree();
    while (!reachMaximumSimulation()) { // other condition?
        std::vector<KillAllGoMCTSNode*> selection_path = select();
        network_->pushBack(env_.getFeatures());
        std::shared_ptr<AlphaZeroNetworkOutput> network_output = std::static_pointer_cast<AlphaZeroNetworkOutput>(network_->forward()[0]);
        expand(selection_path.back(), calculateActionCandidate(network_output));
        backup(selection_path, network_output->value_); // update benson and solver result?
        env_ = env_backup_;
    }
    return getRootNode()->getSolveResult();
}

std::vector<KillAllGoMCTSNode*> KillAllGoMCTSSolver::select()
{
    KillAllGoMCTSNode* node = getRootNode();
    std::vector<KillAllGoMCTSNode*> node_path{node};
    while (!node->isLeaf()) {
        node = selectChildByPUCTScore(node); // skip already solved node?
        env_.act(node->getAction());
        node_path.push_back(node);
    }
    return node_path;
}

void KillAllGoMCTSSolver::evalution()
{
}

void KillAllGoMCTSSolver::expand(KillAllGoMCTSNode* leaf_node, const std::vector<ActionCandidate>& action_candidates)
{
    assert(leaf_node && action_candidates.size() > 0);
    KillAllGoMCTSNode* child = tree_.allocateNodes(action_candidates.size());
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

void KillAllGoMCTSSolver::backup(const std::vector<KillAllGoMCTSNode*>& node_path, const float value)
{
    assert(node_path.size() > 0);
    node_path.back()->setValue(value);
    for (int i = static_cast<int>(node_path.size() - 1); i >= 0; --i) {
        KillAllGoMCTSNode* node = node_path[i];
        node->add(value);
    }
}

KillAllGoMCTSNode* KillAllGoMCTSSolver::selectChildByPUCTScore(const KillAllGoMCTSNode* node) const
{
    assert(node && !node->isLeaf());

    float best_score = -1.0f;
    KillAllGoMCTSNode* selected = nullptr;
    KillAllGoMCTSNode* child = node->getFirstChild();
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

float KillAllGoMCTSSolver::calculateInitQValue(const KillAllGoMCTSNode* node) const
{
    assert(node && !node->isLeaf());

    // init Q value = avg Q value of all visited children + one loss
    float sum_of_win = 0.0f, sum = 0.0f;
    KillAllGoMCTSNode* child = node->getFirstChild();
    for (int i = 0; i < node->getNumChildren(); ++i, ++child) {
        if (child->getCount() == 0) { continue; }
        sum_of_win += child->getMean();
        sum += 1;
    }
    sum_of_win = (node->getFirstChild()->getAction().getPlayer() == env::Player::kPlayer1 ? sum_of_win : -sum_of_win);
    return (sum_of_win - 1) / (sum + 1);
}

std::vector<KillAllGoMCTSSolver::ActionCandidate> KillAllGoMCTSSolver::calculateActionCandidate(const std::shared_ptr<AlphaZeroNetworkOutput>& alphazero_output)
{
    std::vector<KillAllGoMCTSSolver::ActionCandidate> candidates_;
    for (size_t action_id = 0; action_id < alphazero_output->policy_.size(); ++action_id) {
        Action action(action_id, env_.getTurn());
        if (!env_.isLegalAction(action)) { continue; }
        candidates_.push_back(KillAllGoMCTSSolver::ActionCandidate(action, alphazero_output->policy_[action_id], alphazero_output->policy_logits_[action_id]));
    }
    sort(candidates_.begin(), candidates_.end(), [](const KillAllGoMCTSSolver::ActionCandidate& lhs, const KillAllGoMCTSSolver::ActionCandidate& rhs) {
        return lhs.policy_ > rhs.policy_;
    });
    return candidates_;
}

} // namespace minizero::solver