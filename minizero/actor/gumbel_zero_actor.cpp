#include "gumbel_zero_actor.h"
#include <cmath>

namespace minizero::actor {

using namespace minizero;
using namespace network;

void GumbelZeroActor::beforeNNEvaluation()
{
    // get selection path
    if (mcts_.getNumSimulation() == 0) {
        mcts_search_data_.node_path_ = mcts_.select();
    } else {
        assert(candidates_.size() > 0);
        sort(candidates_.begin(), candidates_.end(), [](const MCTSNode* lhs, const MCTSNode* rhs) {
            return (lhs->getCount() < rhs->getCount() || (lhs->getCount() == rhs->getCount() && lhs->getPolicyLogit() > rhs->getPolicyLogit()));
        });
        mcts_search_data_.node_path_ = mcts_.selectFromNode(candidates_[0]);
        mcts_search_data_.node_path_.insert(mcts_search_data_.node_path_.begin(), mcts_.getRootNode());
    }

    // before nn evaluation
    const std::vector<MCTSNode*>& node_path = mcts_search_data_.node_path_;
    if (alphazero_network_) {
        Environment env_transition = getEnvironmentTransition(node_path);
        nn_evaluation_batch_id_ = alphazero_network_->pushBack(env_transition.getFeatures());
    } else if (muzero_network_) {
        if (mcts_.getNumSimulation() == 0) { // initial inference for root node
            nn_evaluation_batch_id_ = muzero_network_->pushBackInitialData(env_.getFeatures());
        } else { // for non-root nodes
            MCTSNode* leaf_node = node_path.back();
            MCTSNode* parent_node = node_path[node_path.size() - 2];
            assert(node_path.size() >= 2 && parent_node && parent_node->getExtraDataIndex() != -1);
            const std::vector<float>& hidden_state = mcts_.getTreeExtraData().getExtraData(parent_node->getExtraDataIndex()).hidden_state_;
            nn_evaluation_batch_id_ = muzero_network_->pushBackRecurrentData(hidden_state, env_.getActionFeatures(leaf_node->getAction()));
        }
    } else {
        assert(false);
    }
}

void GumbelZeroActor::afterNNEvaluation(const std::shared_ptr<network::NetworkOutput>& network_output)
{
    ZeroActor::afterNNEvaluation(network_output);

    // sequential halving
    if (mcts_.getNumSimulation() == 1) {
        // collect candidates
        candidates_.clear();
        MCTSNode* child = mcts_.getRootNode()->getFirstChild();
        for (int i = 0; i < mcts_.getRootNode()->getNumChildren(); ++i, ++child) { candidates_.push_back(child); }
        sort(candidates_.begin(), candidates_.end(), [](const MCTSNode* lhs, const MCTSNode* rhs) { return lhs->getPolicyLogit() > rhs->getPolicyLogit(); });
        if (static_cast<int>(candidates_.size()) > config::actor_gumbel_sample_size) { candidates_.resize(config::actor_gumbel_sample_size); }
        sample_size_ = config::actor_gumbel_sample_size;
        simulation_budget_ = std::max(1.0, std::floor(config::actor_num_simulation / (std::log2(config::actor_gumbel_sample_size) * sample_size_)));
    } else {
        bool all_candidates_reach_budget = true;
        for (auto node : candidates_) {
            if (node->getCount() >= simulation_budget_) { continue; }
            all_candidates_reach_budget = false;
            break;
        }

        if (all_candidates_reach_budget) {
            int next_budget = std::floor(config::actor_num_simulation / (std::log2(config::actor_gumbel_sample_size) * sample_size_ / 2));
            if (next_budget > 0 && sample_size_ > 2) {
                sample_size_ /= 2;
                assert(sample_size_ > 0);
                sortCandidatesByScore();
                if (static_cast<int>(candidates_.size()) > sample_size_) { candidates_.resize(sample_size_); }
                simulation_budget_ = candidates_[0]->getCount() + next_budget;
            }
        }
    }
}

std::string GumbelZeroActor::getActionComment() const
{
    // calculate value for non-visisted nodes
    float pi_sum = 0.0f, q_sum = 0.0f;
    MCTSNode* child = mcts_.getRootNode()->getFirstChild();
    for (int i = 0; i < mcts_.getRootNode()->getNumChildren(); ++i, ++child) {
        if (child->getCount() == 0) { continue; }
        float value = (child->getAction().getPlayer() == env::Player::kPlayer1 ? child->getValue() : -child->getValue());
        pi_sum += child->getPolicy();
        q_sum += child->getPolicy() * value;
    }
    float value_pi = (mcts_.getRootNode()->getFirstChild()->getAction().getPlayer() == env::Player::kPlayer1 ? mcts_.getRootNode()->getValue() : -mcts_.getRootNode()->getValue());
    float non_visited_node_value = 1.0 / (1 + config::actor_num_simulation) * (value_pi + (config::actor_num_simulation / pi_sum) * q_sum);

    // calculate completed Q-values
    std::unordered_map<int, float> new_logits;
    float max_logit = -std::numeric_limits<float>::max();
    child = mcts_.getRootNode()->getFirstChild();
    for (int i = 0; i < mcts_.getRootNode()->getNumChildren(); ++i, ++child) {
        float value = (child->getCount() == 0 ? non_visited_node_value : (child->getAction().getPlayer() == env::Player::kPlayer1 ? child->getValue() : -child->getValue()));
        float logit_without_noise = child->getPolicyLogit() - child->getPolicyNoise();
        float score = logit_without_noise + (config::actor_gumbel_sigma_visit_c + 1) * config::actor_gumbel_sigma_scale_c * value;
        new_logits.insert({child->getAction().getActionID(), score});
        max_logit = fmax(max_logit, score);
    }

    // return normalized completed Q-values
    std::ostringstream oss;
    for (auto& logit : new_logits) {
        oss << (oss.str().empty() ? "" : ",")
            << logit.first << ":" << logit.second - max_logit;
    }
    return oss.str();
}

MCTSNode* GumbelZeroActor::decideActionNode()
{
    assert(candidates_.size() > 0);
    sortCandidatesByScore();
    return candidates_[0];
}

void GumbelZeroActor::sortCandidatesByScore()
{
    assert(!candidates_.empty());
    sort(candidates_.begin(), candidates_.end(), [](const MCTSNode* lhs, const MCTSNode* rhs) {
        float min_value = -std::numeric_limits<float>::max();
        float lhs_value = (lhs->getAction().getPlayer() == env::Player::kPlayer1 ? lhs->getMean() : -lhs->getMean());
        float lhs_score = lhs->getPolicyLogit() + (config::actor_gumbel_sigma_visit_c + 1) * config::actor_gumbel_sigma_scale_c * lhs_value;
        lhs_score = (lhs->getCount() > 0 ? lhs_score : min_value);
        float rhs_value = (rhs->getAction().getPlayer() == env::Player::kPlayer1 ? rhs->getMean() : -rhs->getMean());
        float rhs_score = rhs->getPolicyLogit() + (config::actor_gumbel_sigma_visit_c + 1) * config::actor_gumbel_sigma_scale_c * rhs_value;
        rhs_score = (rhs->getCount() > 0 ? rhs_score : min_value);
        return lhs_score > rhs_score;
    });
}

} // namespace minizero::actor
