#pragma once

#include "base_env.h"
#include <string>
#include <vector>

namespace minizero::env::tictactoe {

const std::string kTicTacToeName = "tictactoe";
const int kTicTacToeNumPlayer = 2;
const int kTicTacToeBoardSize = 3;

typedef BaseBoardAction<kTicTacToeNumPlayer> TicTacToeAction;

class TicTacToeEnv : public BaseBoardEnv<TicTacToeAction> {
public:
    TicTacToeEnv() : BaseBoardEnv<TicTacToeAction>(kTicTacToeBoardSize) { reset(); }

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
    inline int getNumInputChannels() const override { return 4; }
    inline int getPolicySize() const override { return getBoardSize() * getBoardSize(); }
    std::string toString() const override;
    inline std::string name() const override { return kTicTacToeName; }
    inline int getNumPlayer() const override { return kTicTacToeNumPlayer; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return utils::getPositionByRotating(rotation, position, getBoardSize()); };
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return getRotatePosition(action_id, rotation); };

private:
    Player eval() const;

    std::vector<Player> board_;
};

class TicTacToeEnvLoader : public BaseBoardEnvLoader<TicTacToeAction, TicTacToeEnv> {
public:
    std::vector<float> getActionFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline std::vector<float> getValue(const int pos) const { return {getReturn()}; }
    inline std::string name() const override { return kTicTacToeName; }
    inline int getPolicySize() const override { return getBoardSize() * getBoardSize(); }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return utils::getPositionByRotating(rotation, position, getBoardSize()); };
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return getRotatePosition(action_id, rotation); };
};

} // namespace minizero::env::tictactoe
