#pragma once

#include "base_env.h"
#include "sgf_loader.h"

namespace minizero::env::hex {

const std::string kHexName = "hex";
const int kHexNumPlayer = 2;
const int kHexBoardSize = 11;

class HexAction : public BaseAction {
public:
    HexAction() : BaseAction() {}
    HexAction(int action_id, Player player) : BaseAction(action_id, player) {}
    HexAction(const std::vector<std::string>& action_string_args);

    inline Player nextPlayer() const override { return getNextPlayer(player_, kHexNumPlayer); }
    inline std::string toConsoleString() const override { return minizero::utils::SGFLoader::actionIDToBoardCoordinateString(getActionID(), kHexBoardSize); }
};

class HexEnv : public BaseEnv<HexAction> {
public:
    HexEnv() {}

    void reset() override;
    bool act(const HexAction& action) override;
    bool act(const std::vector<std::string>& action_string_args) override;
    std::vector<HexAction> getLegalActions() const override;
    bool isLegalAction(const HexAction& action) const override;
    bool isTerminal() const override;
    float getEvalScore(bool is_resign = false) const override;
    void setObservation(const std::vector<float>& observation) override {}
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const HexAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::string toString() const override;
    inline std::string name() const override { return kHexName; }
};

class HexEnvLoader : public BaseEnvLoader<HexAction, HexEnv> {
public:
    inline int getPolicySize() const override { return kHexBoardSize * kHexBoardSize; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return getPositionByRotating(rotation, position, kHexBoardSize); }
    inline std::string getEnvName() const override { return kHexName; }
};

} // namespace minizero::env::hex
