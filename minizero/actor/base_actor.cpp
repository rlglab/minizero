#include "base_actor.h"
#include "configuration.h"
#include "gumbel_zero_actor.h"
#include "zero_actor.h"

namespace minizero::actor {

void BaseActor::reset()
{
    env_.reset();
    action_comments_.clear();
    resetSearch();
}

void BaseActor::resetSearch()
{
    nn_evaluation_batch_id_ = -1;
}

bool BaseActor::act(const Action& action, const std::string action_comment /*= ""*/)
{
    bool can_act = env_.act(action);
    if (can_act) {
        action_comments_.resize(env_.getActionHistory().size());
        action_comments_[action_comments_.size() - 1] = action_comment;
    }
    return can_act;
}

bool BaseActor::act(const std::vector<std::string>& action_string_args, const std::string action_comment /*= ""*/)
{
    bool can_act = env_.act(action_string_args);
    if (can_act) {
        action_comments_.resize(env_.getActionHistory().size());
        action_comments_[action_comments_.size() - 1] = action_comment;
    }
    return can_act;
}

std::string BaseActor::getRecord() const
{
    EnvironmentLoader env_loader;
    env_loader.loadFromEnvironment(env_, action_comments_);
    env_loader.addTag("EV", config::nn_file_name.substr(config::nn_file_name.find_last_of('/') + 1));

    // if the game is not ended, then treat the game as a resign game, where the next player is the lose side
    if (!isEnvTerminal()) { env_loader.addTag("RE", std::to_string(env_.getEvalScore(true))); }
    return "SelfPlay " + std::to_string(env_loader.getActionPairs().size()) + " " + env_loader.toString();
}

std::shared_ptr<BaseActor> createActor(long long tree_node_size, const std::shared_ptr<network::Network>& network)
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