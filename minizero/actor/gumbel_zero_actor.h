#pragma once

#include "alphazero_network.h"
#include "muzero_network.h"
#include "zero_actor.h"
#include <memory>
#include <string>
#include <vector>

namespace minizero::actor {

class GumbelZeroActor : public ZeroActor {
public:
    GumbelZeroActor(uint64_t tree_node_size) : ZeroActor(tree_node_size)
    {
    }

    void afterNNEvaluation(const std::shared_ptr<network::NetworkOutput>& network_output) override;

protected:
    std::string getMCTSPolicy() const override;

    MCTSNode* decideActionNode() override;
    std::vector<MCTSNode*> selection() override;
    virtual void sequentialHalving();
    virtual void sortCandidatesByScore();

    int sample_size_;
    int simulation_budget_;
    std::vector<MCTSNode*> candidates_;
};

} // namespace minizero::actor
