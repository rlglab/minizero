#include "base_actor.h"
#include "configuration.h"
#include <string>
#include <utility>

namespace minizero::actor {

void BaseActor::reset()
{
    env_.reset();
    action_info_history_.clear();
    resetSearch();
}

void BaseActor::resetSearch()
{
    nn_evaluation_batch_id_ = -1;
    if (!search_) { search_ = createSearch(); }
    search_->reset();
}

bool BaseActor::act(const Action& action)
{
    bool can_act = env_.act(action);
    if (can_act) {
        action_info_history_.resize(env_.getActionHistory().size());
        action_info_history_.back() = getActionInfo();
    }
    return can_act;
}

bool BaseActor::act(const std::vector<std::string>& action_string_args)
{
    bool can_act = env_.act(action_string_args);
    if (can_act) {
        action_info_history_.resize(env_.getActionHistory().size());
        action_info_history_.back() = getActionInfo();
    }
    return can_act;
}

std::string BaseActor::getRecord(const std::unordered_map<std::string, std::string>& tags /* = {} */) const
{
    EnvironmentLoader env_loader;
    env_loader.loadFromEnvironment(env_, action_info_history_);
    env_loader.addTag("EV", config::nn_file_name.substr(config::nn_file_name.find_last_of('/') + 1));

    // if the game is not ended, then treat the game as a resign game, where the next player is the lose side
    if (!isEnvTerminal()) {
        float result = env_.getEvalScore(true);
        std::ostringstream oss;
        oss << result;
        env_loader.addTag("RE", oss.str());
    }
    for (auto tag : tags) { env_loader.addTag(tag.first, tag.second); }
    return env_loader.toString();
}

std::vector<std::pair<std::string, std::string>> BaseActor::getActionInfo() const
{
    std::vector<std::pair<std::string, std::string>> action_info;
    action_info.push_back({"P", getMCTSPolicy()});
    action_info.push_back({"V", getMCTSValue()});
    action_info.push_back({"R", getEnvReward()});
    return action_info;
}

} // namespace minizero::actor
