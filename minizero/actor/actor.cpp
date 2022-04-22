#include "actor.h"
#include "configuration.h"
#include "gumbel_zero_actor.h"
#include "random.h"
#include "time_system.h"
#include "zero_actor.h"

namespace minizero::actor {

void Actor::reset()
{
    env_.reset();
    action_comments_.clear();
    resetSearch();
}

void Actor::resetSearch()
{
    mcts_.reset();
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

void Actor::displayBoard(const MCTSNode* selected_node)
{
    const Action& action = selected_node->getAction();
    std::cerr << env_.toString();
    std::cerr << TimeSystem::getTimeString("[Y/m/d H:i:s.f] ")
              << "move number: " << env_.getActionHistory().size()
              << ", action: " << action.toConsoleString()
              << " (" << action.getActionID() << ")"
              << ", player: " << env::playerToChar(action.getPlayer()) << std::endl;
    std::cerr << "  root node info: " << mcts_.getRootNode()->toString() << std::endl;
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

void Actor::addNoiseToNodeChildren(MCTSNode* node)
{
    assert(node && node->getNumChildren() > 0);
    if (config::actor_use_dirichlet_noise) {
        const float epsilon = config::actor_dirichlet_noise_epsilon;
        std::vector<float> dirichlet_noise = utils::Random::randDirichlet(config::actor_dirichlet_noise_alpha, node->getNumChildren());
        MCTSNode* child = node->getFirstChild();
        for (int i = 0; i < node->getNumChildren(); ++i, ++child) {
            child->setPolicyNoise(dirichlet_noise[i]);
            child->setPolicy((1 - epsilon) * child->getPolicy() + epsilon * dirichlet_noise[i]);
        }
    } else if (config::actor_use_gumbel_noise) {
        std::vector<float> gumbel_noise = utils::Random::randGumbel(node->getNumChildren());
        MCTSNode* child = node->getFirstChild();
        for (int i = 0; i < node->getNumChildren(); ++i, ++child) {
            child->setPolicyNoise(gumbel_noise[i]);
            child->setPolicyLogit(child->getPolicyLogit() + gumbel_noise[i]);
        }
    }
}

std::shared_ptr<Actor> createActor(long long tree_node_size, const std::shared_ptr<network::Network>& network)
{
    if (config::actor_use_gumbel_noise) {
        auto actor = std::make_shared<GumbelZeroActor>(tree_node_size);
        actor->setNetwork(network);
        actor->reset();
        return actor;
    } else {
        auto actor = std::make_shared<ZeroActor>(tree_node_size);
        actor->setNetwork(network);
        actor->reset();
        return actor;
    }

    assert(false);
    return nullptr;
}

} // namespace minizero::actor