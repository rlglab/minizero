#pragma once

#include "mcts.h"
#include <memory>
#include <string>
#include <vector>

namespace minizero::actor {

class GumbelZero {
public:
    std::string getMCTSPolicy(const std::shared_ptr<MCTS>& mcts) const;
    MCTSNode* decideActionNode(const std::shared_ptr<MCTS>& mcts);
    std::vector<MCTSNode*> selection(const std::shared_ptr<MCTS>& mcts);
    void sequentialHalving(const std::shared_ptr<MCTS>& mcts);
    void sortCandidatesByScore(const std::shared_ptr<MCTS>& mcts);

private:
    int sample_size_;
    int simulation_budget_;
    std::vector<MCTSNode*> candidates_;
};

} // namespace minizero::actor
