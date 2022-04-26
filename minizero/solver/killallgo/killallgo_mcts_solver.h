#pragma once

#include "alphazero_network.h"
#include "killallgo.h"
#include "mcts.h"
#include "tree.h"

namespace minizero::solver {

enum class SolveResult {
    kSolverWin = 1,
    kSolverDraw = 0,
    kSolverLoss = -1,
    kSolverUnknown = -2
};

class KillAllGoMCTSNode : public actor::MCTSNode {
public:
    KillAllGoMCTSNode() {}

    void reset() override;

    inline void setSolverResult(SolveResult result) { solver_result_ = result; }
    inline void setFirstChild(KillAllGoMCTSNode* first_child) { killallgo_first_child_ = first_child; }
    inline SolveResult getSolverResult() const { return solver_result_; }
    inline bool isSolved() const { return solver_result_ != SolveResult::kSolverUnknown; }
    inline KillAllGoMCTSNode* getFirstChild() const { return killallgo_first_child_; }

private:
    SolveResult solver_result_;
    KillAllGoMCTSNode* killallgo_first_child_;
};

class KillAllGoMCTSNodeExtraData {
public:
    KillAllGoMCTSNodeExtraData(const env::go::GoBitboard& rzone_bitboard, const env::go::GoPair<env::go::GoBitboard>& stone_bitboard)
        : rzone_bitboard_(rzone_bitboard), stone_bitboard_(stone_bitboard) {}

private:
    env::go::GoBitboard rzone_bitboard_;
    env::go::GoPair<env::go::GoBitboard> stone_bitboard_;
};

typedef actor::Tree<KillAllGoMCTSNode> KillAllGoMCTSTree;
typedef actor::TreeExtraData<KillAllGoMCTSNodeExtraData> KillAllGoMCTSTreeExtraData;

class KillAllGoMCTSSolver {
public:
    class ActionCandidate {
    public:
        Action action_;
        float policy_;
        float policy_logit_;
        ActionCandidate(const Action& action, const float& policy, const float& policy_logit)
            : action_(action), policy_(policy), policy_logit_(policy_logit) {}
    };

    KillAllGoMCTSSolver(long long tree_node_size)
        : tree_(tree_node_size) {}

    void reset();
    void resetTree();
    SolveResult solve();
    std::vector<KillAllGoMCTSNode*> select();
    void expand(KillAllGoMCTSNode* leaf_node, const std::vector<ActionCandidate>& action_candidates);
    void backup(const std::vector<KillAllGoMCTSNode*>& node_path, const float value);

    inline void act(const env::killallgo::KillAllGoAction& action) { env_.act(action); }
    inline void setNetwork(const std::shared_ptr<network::AlphaZeroNetwork>& network) { network_ = network; }
    inline int getNumSimulation() const { return tree_.getRootNode()->getCount(); }
    inline bool reachMaximumSimulation() const { return (getNumSimulation() == config::actor_num_simulation + 1); }
    inline KillAllGoMCTSTree& getTree() { return tree_; }
    inline const KillAllGoMCTSTree& getTree() const { return tree_; }
    inline KillAllGoMCTSNode* getRootNode() { return tree_.getRootNode(); }
    inline const KillAllGoMCTSNode* getRootNode() const { return tree_.getRootNode(); }
    inline KillAllGoMCTSTreeExtraData& getTreeExtraData() { return tree_extra_data_; }
    inline const KillAllGoMCTSTreeExtraData& getTreeExtraData() const { return tree_extra_data_; }
    inline const env::killallgo::KillAllGoEnv& getEnvironment() const { return env_; }

private:
    KillAllGoMCTSNode* selectChildByPUCTScore(const KillAllGoMCTSNode* node) const;
    float calculateInitQValue(const KillAllGoMCTSNode* node) const;
    std::vector<ActionCandidate> calculateActionCandidate(const std::shared_ptr<network::AlphaZeroNetworkOutput>& alphazero_output);
    bool isAllChildrenSolutionLoss(const KillAllGoMCTSNode* pNode) const;

    env::killallgo::KillAllGoEnv env_;
    env::killallgo::KillAllGoEnv env_backup_;
    std::shared_ptr<network::AlphaZeroNetwork> network_;
    KillAllGoMCTSTree tree_;
    KillAllGoMCTSTreeExtraData tree_extra_data_;
};

} // namespace minizero::solver