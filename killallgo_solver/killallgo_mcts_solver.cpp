/*#include "killallgo_mcts_solver.h"
#include "killallgo_configuration.h"

namespace solver {

using namespace minizero;
using namespace minizero::env::go;
using namespace minizero::env::killallgo;

void KillAllGoMCTSSolver::backup(const std::vector<KillAllGoMCTSNode*>& node_path, const float value)
{
    KillAllGoMCTS::backup(node_path, value);

    if (!isBensonTerminal()) { return; }
    setTerminalNode(node_path.back());

    for (int i = static_cast<int>(node_path.size() - 1); i > 0; --i) {
        KillAllGoMCTSNode* pNode = node_path[i];
        KillAllGoMCTSNode* pParent = node_path[i - 1];

        if (pNode->getSolverResult() == SolveResult::kSolverWin) {
            pParent->setSolverResult(SolveResult::kSolverLoss);
            if (use_rzone) {
                undo();
                updateWinnerRZone(pParent, pNode);
            }
        } else if (pNode->getSolverResult() == SolveResult::kSolverLoss) {
            if (use_rzone) {
                undo();
                pruneNodesOutsideRZone(pParent, pNode);
            }
            if (isAllChildrenSolutionLoss(pParent)) {
                pParent->setSolverResult(SolveResult::kSolverWin);
                if (use_rzone) {
                    updateLoserRZone(pParent);
                }
            } else {
                break;
            }
        }
    }
}

std::string KillAllGoMCTSSolver::getTreeString(const std::string& env_string) const
{
    assert(!env_string.empty() && env_string.back() == ')');
    std::ostringstream oss;
    const KillAllGoMCTSNode* pRoot = tree_.getRootNode();
    std::string env_prefix = env_string.substr(0, env_string.size() - 1);
    oss << env_prefix << "C[" << pRoot->toString() << getRZoneString(pRoot) << "]" << getTreeString_r(pRoot) << ")";
    return oss.str();
}

KillAllGoMCTSNode* KillAllGoMCTSSolver::selectChildByPUCTScore(const KillAllGoMCTSNode* node) const
{
    assert(node && !node->isLeaf());

    KillAllGoMCTSNode* selected = nullptr;
    KillAllGoMCTSNode* child = node->getFirstChild();
    float best_score = -1.0f;
    int total_simulation = node->getCount();
    float init_q_value = calculateInitQValue(node);
    for (int i = 0; i < node->getNumChildren(); ++i, ++child) {
        if (child->isSolved()) { continue; }
        float score = child->getPUCTScore(total_simulation, tree_value_map_, init_q_value);
        if (score <= best_score) { continue; }
        best_score = score;
        selected = child;
    }

    assert(selected != nullptr);
    return selected;
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
    return env_.getBensonBitboard().get(env::Player::kPlayer2).any();
}

void KillAllGoMCTSSolver::pruneNodesOutsideRZone(const KillAllGoMCTSNode* parent, const KillAllGoMCTSNode* node)
{
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
}

void KillAllGoMCTSSolver::setTerminalNode(KillAllGoMCTSNode* p_leaf)
{
    GoBitboard white_benson_bitboard = env_.getBensonBitboard().get(env::Player::kPlayer2);
    setNodeRZone(p_leaf, white_benson_bitboard);
    if (p_leaf->getAction().getPlayer() == env::Player::kPlayer1) {
        p_leaf->setSolverResult(SolveResult::kSolverLoss);
    } else {
        p_leaf->setSolverResult(SolveResult::kSolverWin);
    }
}

void KillAllGoMCTSSolver::setNodeRZone(KillAllGoMCTSNode* node, GoBitboard& rzone_bitboard)
{
    const GoPair<GoBitboard>& stone_bitboard = env_.getStoneBitboard();
    GoBitboard black_bitboard = stone_bitboard.get(env::Player::kPlayer1) & rzone_bitboard;
    GoBitboard white_bitboard = stone_bitboard.get(env::Player::kPlayer2) & rzone_bitboard;
    KillAllGoMCTSNodeExtraData zone_data(rzone_bitboard, GoPair<GoBitboard>(black_bitboard, white_bitboard));
    int index = tree_extra_data_.store(zone_data);
    node->setExtraDataIndex(index);
}

void KillAllGoMCTSSolver::updateWinnerRZone(KillAllGoMCTSNode* parent, const KillAllGoMCTSNode* child)
{
    GoBitboard child_rzone_bitboard = tree_extra_data_.getExtraData(child->getExtraDataIndex()).getRZone();
    GoBitboard parent_rzone_bitboard = env_.getWinnerRZoneBitboard(child_rzone_bitboard, child->getAction());
    setNodeRZone(parent, parent_rzone_bitboard);
}

void KillAllGoMCTSSolver::updateLoserRZone(KillAllGoMCTSNode* parent)
{
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

std::string KillAllGoMCTSSolver::getTreeString_r(const KillAllGoMCTSNode* pNode) const
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

std::string KillAllGoMCTSSolver::getRZoneString(const KillAllGoMCTSNode* pNode) const
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

} // namespace solver*/