#pragma once

#include "base_env.h"
#include "configuration.h"
#include "random.h"
#include <ale_interface.hpp>
#include <algorithm>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace minizero::env::atari {

const std::string kAtariName = "atari";
const int kAtariNumPlayer = 1;
const int kAtariActionSize = 18;
const int kAtariFrameSkip = 4;
const int kAtariResolution = 96;
const int kAtariFeatureHistorySize = 32;
const float kAtariRepeatActionProbability = 0.0f;

std::string getAtariActionName(int action_id);

class AtariAction : public BaseAction {
public:
    AtariAction() : BaseAction() {}
    AtariAction(int action_id, Player player) : BaseAction(action_id, player) {}
    AtariAction(const std::vector<std::string>& action_string_args) {}

    inline Player nextPlayer() const override { return getNextPlayer(player_, kAtariNumPlayer); }
    inline std::string toConsoleString() const override { return getAtariActionName(action_id_); }
};

class AtariEnv : public BaseEnv<AtariAction> {
public:
    AtariEnv()
    {
        ale::Logger::setMode(ale::Logger::mode::Error);
        reset();
    }
    AtariEnv(const AtariEnv& env) { *this = env; }
    AtariEnv& operator=(const AtariEnv& env);

    void reset() override { reset(utils::Random::randInt()); }
    void reset(int seed);
    bool act(const AtariAction& action) override;
    bool act(const std::vector<std::string>& action_string_args) override { return act(AtariAction(action_string_args)); }
    std::vector<AtariAction> getLegalActions() const override;
    bool isLegalAction(const AtariAction& action) const override { return minimal_action_set_.count(action.getActionID()); }
    bool isTerminal() const override { return ale_.game_over(false); }
    float getReward() const override { return reward_; }
    float getEvalScore(bool is_resign = false) const override { return total_reward_; }
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const AtariAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::string toString() const override { return ""; }
    inline std::string name() const override { return kAtariName + "_" + minizero::config::env_atari_name; }
    inline int getNumPlayer() const override { return kAtariNumPlayer; }

    // atari environment states & settings
    inline int getSeed() const { return seed_; }
    inline int getLives() { return ale_.lives(); }
    inline int getFrameNumber() const { return ale_.getFrameNumber(); }
    inline int getEpisodeFrameNumber() const { return ale_.getEpisodeFrameNumber(); }

private:
    std::vector<float> getObservation(bool scale_01 = true) const;
    std::string getObservationString() const;

    int seed_;
    float reward_;
    float total_reward_;
    ale::ALEInterface ale_;
    std::unordered_set<int> minimal_action_set_;
    std::deque<std::vector<float>> feature_history_;
    std::deque<std::vector<float>> action_feature_history_;
};

class AtariEnvLoader : public BaseEnvLoader<AtariAction, AtariEnv> {
public:
    void loadFromEnvironment(const AtariEnv& env, const std::vector<std::unordered_map<std::string, std::string>>& action_info_history = {})
    {
        BaseEnvLoader<AtariAction, AtariEnv>::loadFromEnvironment(env, action_info_history);
        addTag("SD", std::to_string(env.getSeed()));
    }

    float getValue(const int pos) const override;
    float getPriority(const int pos) const override { return fabs(getValue(pos) - BaseEnvLoader::getValue(pos)); }

    inline std::string name() const override { return kAtariName + "_" + minizero::config::env_atari_name; }
    inline int getPolicySize() const override { return kAtariActionSize; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; }
};

} // namespace minizero::env::atari
