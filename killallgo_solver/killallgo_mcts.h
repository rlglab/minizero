#pragma once

#include "killallgo.h"
#include "mcts.h"
#include "proof_cost_network.h"
#include <map>

namespace solver {

enum class SolveResult {
    kSolverWin = 1,
    kSolverDraw = 0,
    kSolverLoss = -1,
    kSolverUnknown = -2
};

class KillAllGoMCTSNode : public minizero::actor::MCTSNode {
public:
    KillAllGoMCTSNode() {}

    void reset() override;
    std::string toString() const override;
    float getNormalizedMean(const std::map<float, int>& tree_value_map);
    float getNormalizedPUCTScore(int total_simulation, const std::map<float, int>& tree_value_map, float init_q_value = -1.0f);

    // setter
    inline void setEqualLoss(int equal_loss) { equal_loss_ = equal_loss; }
    inline void setSolverResult(SolveResult result) { solver_result_ = result; }
    inline void setFirstChild(KillAllGoMCTSNode* first_child) { minizero::actor::BaseTreeNode::setFirstChild(first_child); }

    // getter
    inline int getEqualLoss() const { return equal_loss_; }
    inline SolveResult getSolverResult() const { return solver_result_; }
    inline KillAllGoMCTSNode* getFirstChild() const { return static_cast<KillAllGoMCTSNode*>(minizero::actor::BaseTreeNode::getFirstChild()); }
    inline bool isSolved() const { return solver_result_ != SolveResult::kSolverUnknown; }

private:
    int equal_loss_; // move to extra data (?)
    SolveResult solver_result_;
};

class KillAllGoMCTSNodeExtraData {
public:
    KillAllGoMCTSNodeExtraData(const minizero::env::go::GoBitboard& rzone_bitboard, const minizero::env::go::GoPair<minizero::env::go::GoBitboard>& stone_bitboard)
        : rzone_bitboard_(rzone_bitboard), stone_bitboard_(stone_bitboard) {}

public:
    minizero::env::go::GoBitboard getRZone() const { return rzone_bitboard_; }
    minizero::env::go::GoBitboard getRZoneStone(minizero::env::Player player) const { return stone_bitboard_.get(player); }

private:
    minizero::env::go::GoBitboard rzone_bitboard_;
    minizero::env::go::GoPair<minizero::env::go::GoBitboard> stone_bitboard_;
};

typedef minizero::actor::Tree<KillAllGoMCTSNode> KillAllGoMCTSTree;
typedef minizero::actor::TreeExtraData<KillAllGoMCTSNodeExtraData> KillAllGoMCTSTreeExtraData;

class KillAllGoMCTS : public minizero::actor::BaseMCTS<KillAllGoMCTSNode, KillAllGoMCTSTree, KillAllGoMCTSTreeExtraData> {
public:
    KillAllGoMCTS(long long tree_node_size)
        : BaseMCTS(tree_node_size) {}

    virtual void reset() override;
    virtual KillAllGoMCTSNode* selectChildBySoftmaxCount(const KillAllGoMCTSNode* node, float temperature = 1.0f, float value_threshold = 0.1f) const override;
    virtual void backup(const std::vector<KillAllGoMCTSNode*>& node_path, const float value) override;

protected:
    virtual KillAllGoMCTSNode* selectChildByPUCTScore(const KillAllGoMCTSNode* node) const;
    virtual float calculateInitQValue(const KillAllGoMCTSNode* node) const;

    std::map<float, int> tree_value_map_;
};

} // namespace solver