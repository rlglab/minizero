#pragma once

#include "base_env.h"
#include "configuration.h"
#include <algorithm>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace minizero::env::atari {

const int kAtariNumPlayer = 1;
const int kAtariActionSize = 18;
const std::vector<std::string> kAtariActionName = {
    "NOOP",          // 0
    "FIRE",          // 1
    "UP",            // 2
    "RIGHT",         // 3
    "LEFT",          // 4
    "DOWN",          // 5
    "UPRIGHT",       // 6
    "UPLEFT",        // 7
    "DOWNRIGHT",     // 8
    "DOWNLEFT",      // 9
    "UPFIRE",        // 10
    "RIGHTFIRE",     // 11
    "LEFTFIRE",      // 12
    "DOWNFIRE",      // 13
    "UPRIGHTFIRE",   // 14
    "UPLEFTFIRE",    // 15
    "DOWNRIGHTFIRE", // 16
    "DOWNLEFTFIRE",  // 17
};
const std::unordered_map<std::string, std::unordered_set<int>> kAtariActionMap = {
    {"ALE/MsPacman-v5", {0, 2, 3, 4, 5, 6, 7, 8, 9}},
    {"ALE/Breakout-v5", {0, 1, 3, 4}},
};

class AtariAction : public BaseAction {
public:
    AtariAction() : BaseAction() {}
    AtariAction(int action_id, Player player) : BaseAction(action_id, player) {}
    AtariAction(const std::vector<std::string>& action_string_args) {}

    inline Player nextPlayer() const override { return getNextPlayer(player_, kAtariNumPlayer); }
    inline std::string toConsoleString() const override { return kAtariActionName[action_id_]; }
};

class AtariEnv : public BaseEnv<AtariAction> {
public:
    AtariEnv() {}

    void reset() override
    {
        turn_ = Player::kPlayer1;
        actions_.clear();
        observation_history_.clear();
        observation_history_.resize(kFeatureHistorySize, std::vector<float>(3 * config::nn_input_channel_height * config::nn_input_channel_width, 0.0f));
        action_feature_history_.clear();
        action_feature_history_.resize(kFeatureHistorySize, std::vector<float>(config::nn_input_channel_height * config::nn_input_channel_width, 0.0f));
    }

    bool act(const AtariAction& action) override
    {
        assert(action.getPlayer() == Player::kPlayer1);
        assert(action.getActionID() >= 0 && action.getActionID() < kAtariActionSize);
        actions_.push_back(action);

        // maintain action feature history
        action_feature_history_.push_back(std::vector<float>(config::nn_input_channel_height * config::nn_input_channel_width, action.getActionID() * 1.0f / kAtariActionSize));
        action_feature_history_.pop_front();
        assert(static_cast<int>(action_feature_history_.size()) == kFeatureHistorySize);
        return true;
    }

    bool act(const std::vector<std::string>& action_string_args) override { return act(AtariAction(action_string_args)); }
    std::vector<AtariAction> getLegalActions() const override { return {}; }
    bool isLegalAction(const AtariAction& action) const override { return kAtariActionMap.at(name()).count(action.getActionID()); }
    bool isTerminal() const override { return false; }
    float getEvalScore(bool is_resign = false) const override { return 0.0f; }

    void setObservation(const std::vector<float>& observation) override
    {
        assert(static_cast<int>(observation.size()) == 3 * config::nn_input_channel_height * config::nn_input_channel_width);
        observation_history_.push_back(observation);
        observation_history_.pop_front();
        assert(static_cast<int>(observation_history_.size()) == kFeatureHistorySize);
        for (auto& f : observation_history_.back()) { f /= 255; }
    }

    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override
    {
        int feautre_plane_size = config::nn_input_channel_height * config::nn_input_channel_width;
        std::vector<float> features(kFeatureHistorySize * 4 * feautre_plane_size, 0.0f);
        for (size_t i = 0; i < observation_history_.size(); ++i) { // observation history, 3 for RGB
            std::copy(observation_history_[i].begin(), observation_history_[i].end(), features.begin() + i * 3 * feautre_plane_size);
        }
        for (size_t i = 0; i < action_feature_history_.size(); ++i) { // action history
            std::copy(action_feature_history_[i].begin(), action_feature_history_[i].end(), features.begin() + (i + 3 * observation_history_.size()) * feautre_plane_size);
        }
        return features;
    }

    std::vector<float> getActionFeatures(const AtariAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override
    {
        return std::vector<float>(config::nn_hidden_channel_height * config::nn_hidden_channel_width, action.getActionID() * 1.0f / kAtariActionSize);
    }

    std::string toString() const override { return ""; }
    inline std::string name() const override { return minizero::config::env_atari_name; }
    inline int getNumPlayer() const override { return kAtariNumPlayer; }

private:
    const int kFeatureHistorySize = 32;
    std::deque<std::vector<float>> observation_history_;
    std::deque<std::vector<float>> action_feature_history_;
};

class AtariEnvLoader : public BaseEnvLoader<AtariAction, AtariEnv> {
public:
    inline int getPolicySize() const override { return kAtariActionSize; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; }
    inline std::string getEnvName() const override { return minizero::config::env_atari_name; }
};

} // namespace minizero::env::atari
