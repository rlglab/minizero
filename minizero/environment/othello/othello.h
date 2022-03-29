#pragma once

#include "base_env.h"
#include "sgf_loader.h"
#include <vector>

namespace minizero::env::othello {

const std::string kOthelloName = "othello";
const int kOthelloNumPlayer = 2;
const int kOthelloBoardSize = 8;

class OthelloAction : public BaseAction {
public:
    OthelloAction() : BaseAction() {}
    OthelloAction(int action_id, Player player) : BaseAction(action_id, player) {}
    OthelloAction(const std::vector<std::string>& action_string_args)
    {
        // TODO
    }

    inline Player nextPlayer() const override { return getNextPlayer(player_, kOthelloNumPlayer); }
    inline std::string toConsoleString() const override { return minizero::utils::SGFLoader::actionIDToBoardCoordinateString(getActionID(), kOthelloBoardSize); }
};

class OthelloEnv : public BaseEnv<OthelloAction> {
public:
    OthelloEnv() {}

    void reset() override {}
    bool act(const OthelloAction& action) override { return true; }
    bool act(const std::vector<std::string>& action_string_args) override { return true; }
    std::vector<OthelloAction> getLegalActions() const override { return {}; }
    bool isLegalAction(const OthelloAction& action) const override { return true; }
    bool isTerminal() const override { return true; }
    float getEvalScore(bool is_resign = false) const override { return 0.0f; }
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override { return {}; }
    std::string toString() const override { return ""; }
    inline std::string name() const override { return kOthelloName; }
};

class OthelloEnvLoader : public BaseEnvLoader<OthelloAction, OthelloEnv> {
public:
    inline std::vector<float> getActionFeatures(int id, utils::Rotation rotation = utils::Rotation::kRotationNone) const override
    {
        assert(id < static_cast<int>(action_pairs_.size()));
        std::vector<float> action_features(kOthelloBoardSize * kOthelloBoardSize, 0.0f);
        action_features[getRotatePosition(action_pairs_[id].first.getActionID(), rotation)] = 1.0f;
        return action_features;
    }

    inline int getPolicySize() const override { return kOthelloBoardSize * kOthelloBoardSize; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return getPositionByRotating(rotation, position, kOthelloBoardSize); }
    inline std::string getEnvName() const override { return kOthelloName; }
};

} // namespace minizero::env::othello