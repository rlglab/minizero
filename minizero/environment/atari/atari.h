#pragma once

#include "base_env.h"
#include "configuration.h"

namespace minizero::env::atari {

const int kAtariNumPlayer = 1;
const int kAtariActionSize = 18;

class AtariAction : public BaseAction {
public:
    AtariAction() : BaseAction() {}
    AtariAction(int action_id, Player player) : BaseAction(action_id, player) {}
    AtariAction(const std::vector<std::string>& action_string_args) {}

    inline Player nextPlayer() const override { return getNextPlayer(player_, kAtariNumPlayer); }
    inline std::string toConsoleString() const override { return ""; }
};

class AtariEnv : public BaseEnv<AtariAction> {
public:
    AtariEnv() {}

    void reset() override
    {
        turn_ = Player::kPlayer1;
        actions_.clear();
    }
    bool act(const AtariAction& action) override
    {
        assert(action.getPlayer() == Player::kPlayer1);
        assert(action.getActionID() >= 0 && action.getActionID() < kAtariActionSize);
        actions_.push_back(action);
        return true;
    }
    bool act(const std::vector<std::string>& action_string_args) override { return act(AtariAction(action_string_args)); }
    std::vector<AtariAction> getLegalActions() const override { return {}; }
    bool isLegalAction(const AtariAction& action) const override { return true; }
    bool isTerminal() const override { return false; }
    float getEvalScore(bool is_resign = false) const override { return 0.0f; }
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override { return {}; }
    std::vector<float> getActionFeatures(const AtariAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override { return {}; }
    std::string toString() const override { return ""; }
    inline std::string name() const override { return minizero::config::env_atari_name; }
};

class AtariEnvLoader : public BaseEnvLoader<AtariAction, AtariEnv> {
public:
    inline int getPolicySize() const override { return kAtariActionSize; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; }
    inline std::string getEnvName() const override { return minizero::config::env_atari_name; }
};

} // namespace minizero::env::atari