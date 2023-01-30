#pragma once

#include "alphazero_network.h"
#include "muzero_network.h"
#include "zero_actor.h"

namespace minizero::actor {

class GumbelZeroActor : public ZeroActor {
public:
    GumbelZeroActor(long long tree_node_size) : ZeroActor(tree_node_size)
    {
    }

    virtual void afterNNEvaluation(const std::shared_ptr<network::NetworkOutput>& network_output) override;
    virtual std::string getActionComment() const override;

protected:
    virtual MCTSNode* decideActionNode() override;
    virtual std::vector<MCTSNode*> selection() override;
    virtual void sequentialHalving();
    virtual void sortCandidatesByScore();

    int sample_size_;
    int simulation_budget_;
    std::vector<MCTSNode*> candidates_;
};

} // namespace minizero::actor
