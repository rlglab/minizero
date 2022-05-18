#include "killallgo_mcts_solver.h"
#include "configuration.h"

namespace minizero::solver {

using namespace network;
using namespace env::go;
using namespace env::killallgo;

void KillAllGoMCTSNode::reset()
{
    MCTSNode::reset();
    solver_result_ = SolveResult::kSolverUnknown;
    equal_loss_ = -1;
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
        << ", extra_data_idnex = " << extra_data_index_
        << "\n";

    return oss.str();
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
        if (getNumSimulation() % 1000 == 0) {
            std::cout << getNumSimulation() << std::endl;
        }
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

    if (!isBensonTerminal()) { return; }
    setTerminalNode(node_path.back());

    for (int i = static_cast<int>(node_path.size() - 1); i > 0; --i) {
        KillAllGoMCTSNode* pNode = node_path[i];
        KillAllGoMCTSNode* pParent = node_path[i - 1];

        if (pNode->getSolverResult() == SolveResult::kSolverWin) {
            pParent->setSolverResult(SolveResult::kSolverLoss);
            if (config::env_killallgo_use_rzone) {
                undo();
                updateWinnerRZone(pParent, pNode);
            }
        } else if (pNode->getSolverResult() == SolveResult::kSolverLoss) {
            if (config::env_killallgo_use_rzone) {
                undo();
                pruneNodesOutsideRZone(pParent, pNode);
            }
            if (isAllChildrenSolutionLoss(pParent)) {
                pParent->setSolverResult(SolveResult::kSolverWin);
                if (config::env_killallgo_use_rzone) {
                    updateLoserRZone(pParent);
                }
            } else {
                break;
            }
        }
    }
}

std::string KillAllGoMCTSSolver::getTreeString(const std::string& env_string)
{
    assert(!env_string.empty() && env_string.back() == ')');
    std::ostringstream oss;
    KillAllGoMCTSNode* pRoot = tree_.getRootNode();
    std::string env_prefix = env_string.substr(0, env_string.size() - 1);
    oss << env_prefix << "C[" << pRoot->toString() << getRZoneString(pRoot) << "]" << getTreeString_r(pRoot) << ")";
    return oss.str();
}

std::string KillAllGoMCTSSolver::getTreeString_r(KillAllGoMCTSNode* pNode)
{
    std::ostringstream oss;

    int numChildren = 0;
    KillAllGoMCTSNode* pChild = pNode->getFirstChild();
    for (int i = 0; i < pNode->getNumChildren(); ++i, ++pChild) {
        if (pChild->getCount() == 0) { continue; }
        ++numChildren;
    }

    pChild = pNode->getFirstChild();
    for (int i = 0; i < pNode->getNumChildren(); ++i, ++pChild) {
        if (!pChild->displayInTreeLog()) { continue; }
        if (numChildren > 1) { oss << "("; }
        oss << ";";
        oss << playerToChar(pChild->getAction().getPlayer())
            << "[" << minizero::utils::SGFLoader::actionIDToSGFString(pChild->getAction().getActionID(), 7) << "]"
            << "C[" << pChild->toString() << getRZoneString(pChild) << "]" << getTreeString_r(pChild);
        if (numChildren > 1) { oss << ")"; }
    }
    return oss.str();
}

std::string KillAllGoMCTSSolver::getRZoneString(KillAllGoMCTSNode* pNode)
{
    std::ostringstream oss;
    int index = pNode->getExtraDataIndex();
    if (index == -1) { return ""; }
    GoBitboard rzone_bitboard = tree_extra_data_.getExtraData(index).getRZone();
    for (int i = 6; i >= 0; --i) {
        for (int j = 0; j < 7; ++j) {
            if (rzone_bitboard.test(i * 7 + j)) {
                oss << "1 ";
            } else {
                oss << "0 ";
            }
        }
        oss << "\n";
    }

    return oss.str();
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

bool KillAllGoMCTSSolver::isBensonTerminal() const
{
#if KILLALLGO    
    return env_.getBensonBitboard().get(env::Player::kPlayer2).any();
#else
    return false;
#endif
}

void KillAllGoMCTSSolver::pruneNodesOutsideRZone(const KillAllGoMCTSNode* parent, const KillAllGoMCTSNode* node)
{
#if KILLALLGO
    int index = node->getExtraDataIndex();
    if (index == -1) { return; }
    const GoBitboard& child_rzone_bitboard = tree_extra_data_.getExtraData(index).getRZone();
    if (env_.isRelevantMove(child_rzone_bitboard, node->getAction())) { return; }

    KillAllGoMCTSNode* child = parent->getFirstChild();
    for (int i = 0; i < parent->getNumChildren(); ++i, ++child) {
        if (child->getSolverResult() != SolveResult::kSolverUnknown) { continue; }

        if (!child_rzone_bitboard.test(child->getAction().getActionID())) {
            child->setSolverResult(SolveResult::kSolverLoss);
            child->setEqualLoss(node->getAction().getActionID());
        }
    }
#endif
}

void KillAllGoMCTSSolver::setTerminalNode(KillAllGoMCTSNode* p_leaf)
{
#if KILLALLGO
    GoBitboard white_benson_bitboard = env_.getBensonBitboard().get(env::Player::kPlayer2);
    setNodeRZone(p_leaf, white_benson_bitboard);
    if (p_leaf->getAction().getPlayer() == env::Player::kPlayer1) {
        p_leaf->setSolverResult(SolveResult::kSolverLoss);
    } else {
        p_leaf->setSolverResult(SolveResult::kSolverWin);
    }
#endif
}

void KillAllGoMCTSSolver::setNodeRZone(KillAllGoMCTSNode* node, GoBitboard& rzone_bitboard)
{
#if KILLALLGO    
    const GoPair<GoBitboard>& stone_bitboard = env_.getStoneBitboard();
    GoBitboard black_bitboard = stone_bitboard.get(env::Player::kPlayer1) & rzone_bitboard;
    GoBitboard white_bitboard = stone_bitboard.get(env::Player::kPlayer2) & rzone_bitboard;
    KillAllGoMCTSNodeExtraData zone_data(rzone_bitboard, GoPair<GoBitboard>(black_bitboard, white_bitboard));
    int index = tree_extra_data_.store(zone_data);
    node->setExtraDataIndex(index);
#endif    
}

void KillAllGoMCTSSolver::updateWinnerRZone(KillAllGoMCTSNode* parent, const KillAllGoMCTSNode* child)
{
#if KILLALLGO
    GoBitboard child_rzone_bitboard = tree_extra_data_.getExtraData(child->getExtraDataIndex()).getRZone();
    GoBitboard parent_rzone_bitboard = env_.getWinnerRZoneBitboard(child_rzone_bitboard, child->getAction());

    setNodeRZone(parent, parent_rzone_bitboard);
#endif
}

void KillAllGoMCTSSolver::updateLoserRZone(KillAllGoMCTSNode* parent)
{
#if KILLALLGO
    KillAllGoMCTSNode* child = parent->getFirstChild();
    GoBitboard union_bitboard;
    for (int i = 0; i < parent->getNumChildren(); ++i, ++child) {
        if (child->getExtraDataIndex() == -1) { continue; }

        int index = child->getExtraDataIndex();
        GoBitboard child_rzone_bitboard = tree_extra_data_.getExtraData(index).getRZone();
        union_bitboard |= child_rzone_bitboard;
    }

    GoBitboard parent_rzone_bitboard = env_.getLoserRZoneBitboard(union_bitboard, parent->getAction().getPlayer());
    setNodeRZone(parent, parent_rzone_bitboard);
#endif
}

void KillAllGoMCTSSolver::undo()
{
    std::vector<Action> action_list = env_.getActionHistory();
    if (action_list.empty()) { return; }
    env_.reset();
    action_list.pop_back();
    for (unsigned int i = 0; i < action_list.size(); ++i) {
        env_.act(action_list[i]);
    }
}

} // namespace minizero::solver