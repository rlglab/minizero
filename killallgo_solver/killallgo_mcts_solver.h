/*#pragma once

#include "killallgo_mcts.h"

namespace solver {

class KillAllGoMCTSSolver : public KillAllGoMCTS {
public:
    KillAllGoMCTSSolver(long long tree_node_size)
        : KillAllGoMCTS(tree_node_size) {}

    virtual void backup(const std::vector<KillAllGoMCTSNode*>& node_path, const float value);
    std::string getTreeString(const std::string& env_string) const;

private:
    KillAllGoMCTSNode* selectChildByPUCTScore(const KillAllGoMCTSNode* node) const;
    bool isAllChildrenSolutionLoss(const KillAllGoMCTSNode* node) const;
    bool isBensonTerminal() const;
    void pruneNodesOutsideRZone(const KillAllGoMCTSNode* parent, const KillAllGoMCTSNode* child);
    void setTerminalNode(KillAllGoMCTSNode* p_leaf);
    void setNodeRZone(KillAllGoMCTSNode* node, minizero::env::go::GoBitboard& bitboard_rzone);
    void updateWinnerRZone(KillAllGoMCTSNode* parent, const KillAllGoMCTSNode* child);
    void updateLoserRZone(KillAllGoMCTSNode* parent);
    void undo();
    std::string getTreeString_r(const KillAllGoMCTSNode* pNode) const;
    std::string getRZoneString(const KillAllGoMCTSNode* pNode) const;
};

} // namespace solver*/