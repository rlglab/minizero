#include "actor.h"
#include "alphazero_actor.h"
#include "configuration.h"
#include "gumbel_alphazero_actor.h"
#include "muzero_actor.h"
#include "random.h"
#include "time_system.h"

namespace minizero::actor {

void Actor::reset()
{
    env_.reset();
    action_comments_.clear();
    resetSearch();
}

void Actor::resetSearch()
{
    tree_.reset();
    evaluation_jobs_ = {{}, -1};
}

bool Actor::act(const Action& action, const std::string action_comment /*= ""*/)
{
    bool can_act = env_.act(action);
    if (can_act) {
        action_comments_.resize(env_.getActionHistory().size());
        action_comments_[action_comments_.size() - 1] = action_comment;
    }
    return can_act;
}

bool Actor::act(const std::vector<std::string>& action_string_args, const std::string action_comment /*= ""*/)
{
    bool can_act = env_.act(action_string_args);
    if (can_act) {
        action_comments_.resize(env_.getActionHistory().size());
        action_comments_[action_comments_.size() - 1] = action_comment;
    }
    return can_act;
}

void Actor::displayBoard(const MCTSTreeNode* selected_node)
{
    const Action& action = selected_node->getAction();
    std::cerr << env_.toString();
    std::cerr << TimeSystem::getTimeString("[Y/m/d H:i:s.f] ")
              << "move number: " << env_.getActionHistory().size()
              << ", action: " << action.toConsoleString()
              << " (" << action.getActionID() << ")"
              << ", player: " << env::playerToChar(action.getPlayer()) << std::endl;
    std::cerr << "  root node info: " << tree_.getRootNode()->toString() << std::endl;
    std::cerr << "action node info: " << selected_node->toString() << std::endl
              << std::endl;
}

std::string Actor::getRecord() const
{
    EnvironmentLoader env_loader;
    env_loader.loadFromEnvironment(env_, action_comments_);
    env_loader.addTag("EV", config::nn_file_name.substr(config::nn_file_name.find_last_of('/') + 1));

    // if the game is not ended, then treat the game as a resign game, where the next player is the lose side
    if (!isTerminal()) { env_loader.addTag("RE", std::to_string(env_.getEvalScore(true))); }
    return "SelfPlay " + std::to_string(env_loader.getActionPairs().size()) + " " + env_loader.toString();
}

void Actor::addNoiseToNodeChildren(MCTSTreeNode* node)
{
    assert(node && node->getNumChildren() > 0);
    if (config::actor_use_dirichlet_noise) {
        const float epsilon = config::actor_dirichlet_noise_epsilon;
        std::vector<float> dirichlet_noise = utils::Random::randDirichlet(config::actor_dirichlet_noise_alpha, node->getNumChildren());
        MCTSTreeNode* child = node->getFirstChild();
        for (int i = 0; i < node->getNumChildren(); ++i, ++child) {
            child->setPolicyNoise(dirichlet_noise[i]);
            child->setPolicy((1 - epsilon) * child->getPolicy() + epsilon * dirichlet_noise[i]);
        }
    } else if (config::actor_use_gumbel_noise) {
        std::vector<float> gumbel_noise = utils::Random::randGumbel(node->getNumChildren());
        MCTSTreeNode* child = node->getFirstChild();
        for (int i = 0; i < node->getNumChildren(); ++i, ++child) {
            child->setPolicyNoise(gumbel_noise[i]);
            child->setPolicyLogit(child->getPolicyLogit() + gumbel_noise[i]);
        }
    }
}

std::shared_ptr<Actor> createActor(long long tree_node_size, const std::string& network_type_name)
{
    if (network_type_name == "alphazero") {
        if (!config::actor_use_gumbel_noise) {
            return std::make_shared<AlphaZeroActor>(tree_node_size);
        } else {
            return std::make_shared<GumbelAlphaZeroActor>(tree_node_size);
        }
    } else if (network_type_name == "muzero") {
        return std::make_shared<MuZeroActor>(tree_node_size);
    }

    assert(false);
    return nullptr;
}

} // namespace minizero::actor