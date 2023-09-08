#pragma once

#include "base_env.h"
#include "configuration.h"
#include <algorithm>
#include <bitset>
#include <string>
#include <unordered_map>
#include <vector>

namespace minizero::env::othello {
using namespace minizero::utils;
const std::string kOthelloName = "othello";
const int kOthelloNumPlayer = 2;
const int kMaxOthelloBoardSize = 16;
typedef std::bitset<kMaxOthelloBoardSize * kMaxOthelloBoardSize> OthelloBitboard;

typedef BaseBoardAction<kOthelloNumPlayer> OthelloAction;

class OthelloEnv : public BaseBoardEnv<OthelloAction> {
public:
    OthelloEnv()
    {
        assert(getBoardSize() <= kMaxOthelloBoardSize);
        reset();
    }

    void reset() override;
    bool act(const OthelloAction& action) override;
    bool act(const std::vector<std::string>& action_string_args) override;
    std::vector<OthelloAction> getLegalActions() const override;
    bool isLegalAction(const OthelloAction& action) const override;
    bool isTerminal() const override;
    float getReward() const override { return 0.0f; }
    float getEvalScore(bool is_resign = false) const override;
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const OthelloAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline int getNumInputChannels() const override { return 4; }
    inline int getPolicySize() const override { return getBoardSize() * getBoardSize() + 1; }
    std::string toString() const override;
    inline std::string name() const override { return kOthelloName + "_" + std::to_string(getBoardSize()) + "x" + std::to_string(getBoardSize()); }
    inline int getNumPlayer() const override { return kOthelloNumPlayer; }
    inline bool isPassAction(const OthelloAction& action) const { return (action.getActionID() == getBoardSize() * getBoardSize()); }

    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return utils::getPositionByRotating(rotation, position, getBoardSize()); };
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return getRotatePosition(action_id, rotation); };

private:
    Player eval() const;
    OthelloBitboard getCanPutPoint(
        int direction,
        OthelloBitboard mask,
        OthelloBitboard empty_board,
        OthelloBitboard opponent_board,
        OthelloBitboard player_board);
    OthelloBitboard getFlipPoint(
        int direction,
        OthelloBitboard mask,
        OthelloBitboard placed_pos,
        OthelloBitboard opponent_board,
        OthelloBitboard player_board);
    OthelloBitboard getCandidateAlongDirectionBoard(int direction, OthelloBitboard candidate);
    std::string getCoordinateString() const;

    int dir_step_[8]; // 8 directions
    OthelloBitboard one_board_;
    OthelloBitboard mask_[8];               // 8 directions
    GamePair<bool> legal_pass_;             // store black/white legal pass
    GamePair<OthelloBitboard> legal_board_; // store black/white legal board
    GamePair<OthelloBitboard> board_;       // store black/white board
};

class OthelloEnvLoader : public BaseBoardEnvLoader<OthelloAction, OthelloEnv> {
public:
    std::vector<float> getActionFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline bool isPassAction(const OthelloAction& action) const { return (action.getActionID() == getBoardSize() * getBoardSize()); }
    inline std::vector<float> getValue(const int pos) const { return {getReturn()}; }
    inline std::string name() const override { return kOthelloName + "_" + std::to_string(getBoardSize()) + "x" + std::to_string(getBoardSize()); }
    inline int getPolicySize() const override { return getBoardSize() * getBoardSize() + 1; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return utils::getPositionByRotating(rotation, position, getBoardSize()); };
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return getRotatePosition(action_id, rotation); };
};

} // namespace minizero::env::othello
