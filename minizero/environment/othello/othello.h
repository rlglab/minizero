#pragma once

#include "base_env.h"
#include "sgf_loader.h"
#include <algorithm>
#include <bitset>
#include <vector>

namespace minizero::env::othello {
using namespace minizero::utils;
const std::string kOthelloName = "othello";
const int kOthelloNumPlayer = 2;
const int kOthelloBoardSize = 8;
typedef std::bitset<kOthelloBoardSize * kOthelloBoardSize> OthelloBitboard;

class OthelloAction : public BaseAction {
public:
    OthelloAction() : BaseAction() {}
    OthelloAction(int action_id, Player player) : BaseAction(action_id, player) {}
    OthelloAction(const std::vector<std::string>& action_string_args);

    inline Player nextPlayer() const override { return getNextPlayer(player_, kOthelloNumPlayer); }
    inline std::string toConsoleString() const override { return minizero::utils::SGFLoader::actionIDToBoardCoordinateString(getActionID(), kOthelloBoardSize); }
};

class OthelloEnv : public BaseEnv<OthelloAction> {
public:
    OthelloEnv() { reset(); }

    void reset() override;
    bool act(const OthelloAction& action) override;
    bool act(const std::vector<std::string>& action_string_args) override;
    std::vector<OthelloAction> getLegalActions() const override;
    bool isLegalAction(const OthelloAction& action) const override;
    bool isTerminal() const override;
    float getEvalScore(bool is_resign = false) const override;
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const OthelloAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::string toString() const override;
    inline std::string name() const override { return kOthelloName; }
    inline bool isPassAction(const OthelloAction& action) const { return (action.getActionID() == kOthelloBoardSize * kOthelloBoardSize); }

private:
    Player eval() const;
    std::vector<Player> board_;
    OthelloBitboard black_board;
    OthelloBitboard white_board;
    OthelloBitboard one_board;
    OthelloBitboard black_legal_board; //black 可以落子的點
    OthelloBitboard white_legal_board; //white 可以落子的點
    OthelloBitboard mask[8];           //8 directions
    OthelloBitboard getCanPutPoint(
        int direction,
        OthelloBitboard mask,
        OthelloBitboard moves,
        OthelloBitboard empty_board,
        OthelloBitboard opponent_board,
        OthelloBitboard player_board);

    OthelloBitboard getFlipPoint(
        int direction,
        OthelloBitboard mask,
        OthelloBitboard moves,
        OthelloBitboard placed_pos,
        OthelloBitboard opponent_board,
        OthelloBitboard player_board);
    OthelloBitboard getCandidateAlongDirectionBoard(int direction, OthelloBitboard candidate);
    bool black_legal_pass;
    bool white_legal_pass;
};

class OthelloEnvLoader : public BaseEnvLoader<OthelloAction, OthelloEnv> {
public:
    inline int getPolicySize() const override { return kOthelloBoardSize * kOthelloBoardSize + 1; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return getPositionByRotating(rotation, position, kOthelloBoardSize); }
    inline std::string getEnvName() const override { return kOthelloName; }
};
} // namespace minizero::env::othello