#include "killallgo_actor.h"
#include "killallgo_configuration.h"
#include "random.h"
#include "time_system.h"

namespace solver {

using namespace minizero;
using namespace network;

void KillallGoMCTSSearchData::clear()
{
    selected_node_ = nullptr;
    node_path_.clear();
}

void KillallGoActor::resetSearch()
{
    BaseActor::resetSearch();
    mcts_.reset();
    mcts_search_data_.node_path_.clear();
}

Action KillallGoActor::think(bool with_play /*= false*/, bool display_board /*= false*/)
{
    resetSearch();
    while (!isSearchDone()) { step(); }
    if (with_play) { act(getSearchAction()); }
    if (display_board) { displayBoard(); }
    return getSearchAction();
}

void KillallGoActor::beforeNNEvaluation()
{
    mcts_search_data_.node_path_ = mcts_.select();
    Environment env_transition = getEnvironmentTransition(mcts_search_data_.node_path_);
    nn_evaluation_batch_id_ = pcn_network_->pushBack(env_transition.getFeatures());
}

void KillallGoActor::afterNNEvaluation(const std::shared_ptr<NetworkOutput>& network_output)
{
    const std::vector<KillAllGoMCTSNode*>& node_path = mcts_search_data_.node_path_;
    KillAllGoMCTSNode* leaf_node = node_path.back();
    Environment env_transition = getEnvironmentTransition(node_path);
    if (!env_transition.isTerminal()) {
        std::shared_ptr<ProofCostNetworkOutput> pcn_output = std::static_pointer_cast<ProofCostNetworkOutput>(network_output);
        mcts_.expand(leaf_node, calculateActionPolicy(env_transition, pcn_output));
        mcts_.backup(node_path, pcn_output->value_n_);
    } else {
        float value = (env_transition.getEvalScore() == 1 ? nn_value_size - 1 : 0);
        mcts_.backup(node_path, value);
    }
    if (leaf_node == mcts_.getRootNode()) { addNoiseToNodeChildren(leaf_node); }
    if (isSearchDone()) { mcts_search_data_.selected_node_ = decideActionNode(); }
}

void KillallGoActor::displayBoard() const
{
    assert(mcts_search_data_.selected_node_);
    const Action action = getSearchAction();
    std::cerr << env_.toString();
    std::cerr << TimeSystem::getTimeString("[Y/m/d H:i:s.f] ")
              << "move number: " << env_.getActionHistory().size()
              << ", action: " << action.toConsoleString()
              << " (" << action.getActionID() << ")"
              << ", player: " << env::playerToChar(action.getPlayer()) << std::endl;
    std::cerr << "  root node info: " << mcts_.getRootNode()->toString() << std::endl;
    std::cerr << "action node info: " << mcts_search_data_.selected_node_->toString() << std::endl
              << std::endl;
}

void KillallGoActor::step()
{
    beforeNNEvaluation();
    afterNNEvaluation(pcn_network_->forward()[getNNEvaluationBatchIndex()]);
}

KillAllGoMCTSNode* KillallGoActor::decideActionNode()
{
    if (config::actor_select_action_by_count) {
        return mcts_.selectChildByMaxCount(mcts_.getRootNode());
    } else if (config::actor_select_action_by_softmax_count) {
        return mcts_.selectChildBySoftmaxCount(mcts_.getRootNode(), config::actor_select_action_softmax_temperature);
    }

    assert(false);
    return nullptr;
}

void KillallGoActor::addNoiseToNodeChildren(KillAllGoMCTSNode* node)
{
    assert(node && node->getNumChildren() > 0);
    if (config::actor_use_dirichlet_noise) {
        const float epsilon = config::actor_dirichlet_noise_epsilon;
        std::vector<float> dirichlet_noise = utils::Random::randDirichlet(config::actor_dirichlet_noise_alpha, node->getNumChildren());
        KillAllGoMCTSNode* child = node->getFirstChild();
        for (int i = 0; i < node->getNumChildren(); ++i, ++child) {
            child->setPolicyNoise(dirichlet_noise[i]);
            child->setPolicy((1 - epsilon) * child->getPolicy() + epsilon * dirichlet_noise[i]);
        }
    } else if (config::actor_use_gumbel_noise) {
        std::vector<float> gumbel_noise = utils::Random::randGumbel(node->getNumChildren());
        KillAllGoMCTSNode* child = node->getFirstChild();
        for (int i = 0; i < node->getNumChildren(); ++i, ++child) {
            child->setPolicyNoise(gumbel_noise[i]);
            child->setPolicyLogit(child->getPolicyLogit() + gumbel_noise[i]);
        }
    }
}

std::vector<KillAllGoMCTS::ActionCandidate> KillallGoActor::calculateActionPolicy(const Environment& env_transition, const std::shared_ptr<ProofCostNetworkOutput>& pcn_output)
{
    std::vector<KillAllGoMCTS::ActionCandidate> action_candidates;
    for (size_t action_id = 0; action_id < pcn_output->policy_.size(); ++action_id) {
        Action action(action_id, env_transition.getTurn());
        if (!env_transition.isLegalAction(action)) { continue; }
        action_candidates.push_back(KillAllGoMCTS::ActionCandidate(action, pcn_output->policy_[action_id], pcn_output->policy_logits_[action_id]));
    }
    sort(action_candidates.begin(), action_candidates.end(), [](const KillAllGoMCTS::ActionCandidate& lhs, const KillAllGoMCTS::ActionCandidate& rhs) {
        return lhs.policy_ > rhs.policy_;
    });
    return action_candidates;
}

Environment KillallGoActor::getEnvironmentTransition(const std::vector<KillAllGoMCTSNode*>& node_path)
{
    Environment env = env_;
    for (size_t i = 1; i < node_path.size(); ++i) { env.act(node_path[i]->getAction()); }
    return env;
}

} // namespace solver
