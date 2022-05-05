#include "killallgo_mcts_solver.h"

namespace minizero::solver {

using namespace network;
using namespace env::go;
using namespace env::killallgo;

void KillAllGoMCTSNode::reset()
{
    MCTSNode::reset();
    solver_result_ = SolveResult::kSolverUnknown;
}

std::string KillAllGoMCTSNode::toString() const
{
    // TODO
    return MCTSNode::toString();
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
    while (!reachMaximumSimulation() && !getRootNode()->isSolved()) {
        std::vector<KillAllGoMCTSNode*> selection_path = select();
        network_->pushBack(env_.getFeatures());
        std::shared_ptr<AlphaZeroNetworkOutput> network_output = std::static_pointer_cast<AlphaZeroNetworkOutput>(network_->forward()[0]);
        if (!env_.isTerminal()) { expand(selection_path.back(), calculateActionCandidate(network_output)); }
        backup(selection_path, network_output->value_);
        env_ = env_backup_;
    }
    return getRootNode()->getSolverResult();
}

std::vector<KillAllGoMCTSNode*> KillAllGoMCTSSolver::select()
{
    KillAllGoMCTSNode* node = getRootNode();
    std::vector<KillAllGoMCTSNode*> node_path{node};
    while (!node->isLeaf()) {
        node = selectChildByPUCTScore(node);
        env_.act(node->getAction());
        node_path.push_back(node);
    }
    return node_path;
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

    if (env_.isTerminal()) { setTerminalRZone(node_path.back()); }

    for (int i = static_cast<int>(node_path.size() - 1); i > 0; --i) {
        KillAllGoMCTSNode* pNode = node_path[i];
        KillAllGoMCTSNode* pParent = node_path[i - 1];

        if (pNode->getSolverResult() == SolveResult::kSolverWin) {
            pParent->setSolverResult(SolveResult::kSolverLoss);
        } else if (pNode->getSolverResult() == SolveResult::kSolverLoss) {
            if (isAllChildrenSolutionLoss(pParent)) {
                pParent->setSolverResult(SolveResult::kSolverWin);
            } else {
                break;
            }
        }
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
        if (child->isSolved()) { continue; }
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

bool KillAllGoMCTSSolver::isAllChildrenSolutionLoss(const KillAllGoMCTSNode* p_node) const
{
    const KillAllGoMCTSNode* p_child = p_node->getFirstChild();
    for (int i = 0; i < p_node->getNumChildren(); ++i, ++p_child) {
        if (p_child->getSolverResult() != SolveResult::kSolverLoss) { return false; }
    }
    return true;
}

void KillAllGoMCTSSolver::setTerminalRZone(KillAllGoMCTSNode* p_leaf)
{
#if KILLALLGO
    GoPair<GoBitboard> stone_bitboard = env_.getStoneBitboard();
    GoBitboard white_benson_bitboard = env_.getBensonBitboard().get(env::Player::kPlayer2);
    GoBitboard black_bitboard = stone_bitboard.get(env::Player::kPlayer1) & white_benson_bitboard;
    GoBitboard white_bitboard = stone_bitboard.get(env::Player::kPlayer2) & white_benson_bitboard;
    KillAllGoMCTSNodeExtraData zoneData(white_benson_bitboard, GoPair<GoBitboard>(black_bitboard, white_bitboard));
    int index = tree_extra_data_.store(zoneData);
    p_leaf->setExtraDataIndex(index);
    if (p_leaf->getAction().getPlayer() == env::Player::kPlayer1) {
        p_leaf->setSolverResult(SolveResult::kSolverLoss);
    } else {
        p_leaf->setSolverResult(SolveResult::kSolverWin);
    }
#endif
}

} // namespace minizero::solver