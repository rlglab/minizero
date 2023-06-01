#pragma once

#include "base_env.h"
#include "random.h"
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace minizero::env {

template <class Action>
class StochasticEnv : public BaseEnv<Action> {
public:
    StochasticEnv() : BaseEnv<Action>(), seed_(0) {}
    virtual ~StochasticEnv() = default;

    void reset() override { reset(utils::Random::randInt()); }
    bool act(const Action& action) override { return act(action, true); }
    bool act(const std::vector<std::string>& action_string_args) override { return act(action_string_args, true); }

    virtual void reset(int seed) = 0;
    virtual bool act(const Action& action, bool with_chance = true) = 0;
    virtual bool act(const std::vector<std::string>& action_string_args, bool with_chance = true) = 0;
    virtual bool actChanceEvent(const Action& action) = 0;
    virtual std::vector<Action> getLegalChanceEvents() const = 0;
    virtual bool isLegalChanceEvent(const Action& action) const = 0;
    virtual int getChanceEventSize() const = 0;
    virtual float getChanceEventProbability(const Action& action) const = 0;

    inline int getSeed() const { return seed_; }

protected:
    int seed_;
    std::mt19937 random_;
};

template <class Action, class Env>
class StochasticEnvLoader : public BaseEnvLoader<Action, Env> {
public:
    StochasticEnvLoader() : BaseEnvLoader<Action, Env>() {}
    virtual ~StochasticEnvLoader() = default;

    void loadFromEnvironment(const Env& env, const std::vector<std::vector<std::pair<std::string, std::string>>>& action_info_history = {}) override
    {
        BaseEnvLoader<Action, Env>::loadFromEnvironment(env, action_info_history);
        BaseEnvLoader<Action, Env>::addTag("SD", std::to_string(env.getSeed()));
    }

    inline int getSeed() const { return std::stoi(BaseEnvLoader<Action, Env>::getTag("SD")); }

    std::vector<float> getFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override
    {
        // a slow but naive method which simply replays the game again to get features
        Env env;
        env.reset(getSeed());
        const auto& action_pairs_ = BaseEnvLoader<Action, Env>::action_pairs_;
        for (int i = 0; i < std::min(pos, static_cast<int>(action_pairs_.size())); ++i) { env.act(action_pairs_[i].first); }
        return env.getFeatures(rotation);
    }
};

} // namespace minizero::env
