#pragma once

#include "base_env.h"
#include "configuration.h"
#include "sgf_loader.h"
#include <algorithm>
#include <bitset>
#include <string>
#include <vector>

namespace minizero::env::othello {
using namespace minizero::utils;
const std::string kOthelloName = "othello";
const int kOthelloNumPlayer = 2;
const int kMaxOthelloBoardSize = 16;
typedef std::bitset<kMaxOthelloBoardSize * kMaxOthelloBoardSize> OthelloBitboard;

class OthelloAction : public BaseAction {
public:
    OthelloAction() : BaseAction() {}
    OthelloAction(int action_id, Player player) : BaseAction(action_id, player) {}
    OthelloAction(const std::vector<std::string>& action_string_args, int board_size);

    inline Player nextPlayer() const override { return getNextPlayer(player_, kOthelloNumPlayer); }
    inline std::string toConsoleString() const override { return minizero::utils::SGFLoader::actionIDToBoardCoordinateString(getActionID(), minizero::config::env_board_size); }
};

class OthelloEnv : public BaseEnv<OthelloAction> {
public:
    OthelloEnv()
        : board_size_(minizero::config::env_board_size)
    {
        assert(board_size_ > 0 && board_size_ <= kMaxOthelloBoardSize);
        reset();
    }

    void reset() override;
    bool act(const OthelloAction& action) override;
    bool act(const std::vector<std::string>& action_string_args) override;
    std::vector<OthelloAction> getLegalActions() const override;
    bool isLegalAction(const OthelloAction& action) const override;
    bool isTerminal() const override;
    float getEvalScore(bool is_resign = false) const override;
    void setObservation(const std::vector<float>& observation) override {}
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const OthelloAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::string toString() const override;
    inline std::string name() const override { return kOthelloName; }
    inline int getNumPlayer() const override { return kOthelloNumPlayer; }
    inline bool isPassAction(const OthelloAction& action) const { return (action.getActionID() == board_size_ * board_size_); }
    inline int getBoardSize() const { return board_size_; }

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

    int board_size_;
    int dir_step_[8]; // 8 directions
    OthelloBitboard one_board_;
    OthelloBitboard mask_[8];               // 8 directions
    GamePair<bool> legal_pass_;             // store black/white legal pass
    GamePair<OthelloBitboard> legal_board_; // store black/white legal board
    GamePair<OthelloBitboard> board_;       // store black/white board
};

class OthelloEnvLoader : public BaseEnvLoader<OthelloAction, OthelloEnv> {
public:
    void loadFromEnvironment(const OthelloEnv& env, const std::vector<std::string> action_distributions = {})
    {
        BaseEnvLoader<OthelloAction, OthelloEnv>::loadFromEnvironment(env, action_distributions);
        addTag("SZ", std::to_string(env.getBoardSize()));
    }
    inline int getBoardSize() const { return std::stoi(getTag("SZ")); }
    inline int getPolicySize() const override { return getBoardSize() * getBoardSize() + 1; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return getPositionByRotating(rotation, position, getBoardSize()); }
    inline std::string getEnvName() const override { return kOthelloName; }
};

} // namespace minizero::env::othello
