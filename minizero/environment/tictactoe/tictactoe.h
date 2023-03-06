#pragma once

#include "base_env.h"
#include "sgf_loader.h"
#include <string>
#include <vector>

namespace minizero::env::tictactoe {

const std::string kTicTacToeName = "tictactoe";
const int kTicTacToeNumPlayer = 2;
const int kTicTacToeBoardSize = 3;

class TicTacToeAction : public BaseAction {
public:
    TicTacToeAction() : BaseAction() {}
    TicTacToeAction(int action_id, Player player) : BaseAction(action_id, player) {}
    TicTacToeAction(const std::vector<std::string>& action_string_args);

    inline Player nextPlayer() const override { return getNextPlayer(player_, kTicTacToeNumPlayer); }
    inline std::string toConsoleString() const override { return minizero::utils::SGFLoader::actionIDToBoardCoordinateString(getActionID(), kTicTacToeBoardSize); }
};

class TicTacToeEnv : public BaseEnv<TicTacToeAction> {
public:
    TicTacToeEnv() { reset(); }

    void reset() override;
    bool act(const TicTacToeAction& action) override;
    bool act(const std::vector<std::string>& action_string_args) override;
    std::vector<TicTacToeAction> getLegalActions() const override;
    bool isLegalAction(const TicTacToeAction& action) const override;
    bool isTerminal() const override;
    float getReward() const override { return 0.0f; }
    float getEvalScore(bool is_resign = false) const override;
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const TicTacToeAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::string toString() const override;
    inline std::string name() const override { return kTicTacToeName; }
    inline int getNumPlayer() const override { return kTicTacToeNumPlayer; }

private:
    Player eval() const;

    std::vector<Player> board_;
};

class TicTacToeEnvLoader : public BaseEnvLoader<TicTacToeAction, TicTacToeEnv> {
public:
    inline std::string name() const override { return kTicTacToeName; }
    inline int getPolicySize() const override { return kTicTacToeBoardSize * kTicTacToeBoardSize; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return getPositionByRotating(rotation, position, kTicTacToeBoardSize); }
};

} // namespace minizero::env::tictactoe
