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
#include <utility>
#include <vector>

namespace minizero::env::atari {

const std::string kAtariName = "atari";
const int kAtariNumPlayer = 1;
const int kAtariActionSize = 18;
const int kAtariFrameSkip = 4;
const int kAtariResolution = 96;
const int kAtariFeatureHistorySize = 8;
const int kAtariMaxNumFramesPerEpisode = 108000;
const float kAtariRepeatActionProbability = 0.25f;

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
    bool isTerminal() const override { return ((getActionHistory().size() * kAtariFrameSkip) >= kAtariMaxNumFramesPerEpisode) || ale_.game_over(false); }
    float getReward() const override { return reward_; }
    float getEvalScore(bool is_resign = false) const override { return total_reward_; }
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const AtariAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::string toString() const override { return ""; }
    inline std::string name() const override { return kAtariName + "_" + minizero::config::env_atari_name; }
    inline int getNumPlayer() const override { return kAtariNumPlayer; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; }
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return action_id; }

    // atari environment states & settings
    inline int getSeed() const { return seed_; }
    inline int getLives() const { return lives_; }
    inline int getFrameNumber() const { return ale_.getFrameNumber(); }
    inline int getEpisodeFrameNumber() const { return ale_.getEpisodeFrameNumber(); }

private:
    std::vector<float> getObservation(bool scale_01 = true) const;
    std::string getObservationString() const;

    int seed_;
    int lives_;
    float reward_;
    float total_reward_;
    ale::ALEInterface ale_;
    std::unordered_set<int> minimal_action_set_;
    std::deque<std::vector<float>> feature_history_;
    std::deque<std::vector<float>> action_feature_history_;
};

class AtariEnvLoader : public BaseEnvLoader<AtariAction, AtariEnv> {
public:
    void reset() override;
    bool loadFromString(const std::string& content) override;
    void loadFromEnvironment(const AtariEnv& env, const std::vector<std::vector<std::pair<std::string, std::string>>>& action_info_history = {}) override;
    std::vector<float> getFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getValue(const int pos) const override { return toDiscreteValue(pos < static_cast<int>(action_pairs_.size()) ? utils::transformValue(calculateNStepValue(pos)) : 0.0f); }
    inline std::vector<float> getReward(const int pos) const override { return toDiscreteValue(pos < static_cast<int>(action_pairs_.size()) ? utils::transformValue(BaseEnvLoader::getReward(pos)[0]) : 0.0f); }
    float getPriority(const int pos) const override { return fabs(calculateNStepValue(pos) - BaseEnvLoader::getValue(pos)[0]); }

    inline std::string name() const override { return kAtariName + "_" + minizero::config::env_atari_name; }
    inline int getPolicySize() const override { return kAtariActionSize; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; }
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return action_id; }

private:
    void addObservations(const std::string& compressed_obs);
    std::vector<float> getFeaturesByReplay(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const;
    float calculateNStepValue(const int pos) const;
    std::vector<float> toDiscreteValue(float value) const;

    std::vector<std::string> observations_;
};

} // namespace minizero::env::atari
