#pragma once

#include "base_env.h"
#include <bitset>
#include <string>
#include <utility>
#include <vector>

namespace minizero::env::connect6 {

const std::string kConnect6Name = "connect6";
const int kConnect6NumPlayer = 2;
const int kConnect6NumWinConnectStone = 6;
const int kMaxConnect6BoardSize = 19;

typedef std::bitset<kMaxConnect6BoardSize * kMaxConnect6BoardSize> Connect6Bitboard;

class Connect6Action : public BaseBoardAction<kConnect6NumPlayer> {
public:
    Connect6Action() : BaseBoardAction() {}
    Connect6Action(int action_id, Player player) : BaseBoardAction(action_id, player) {}
    Connect6Action(const std::vector<std::string>& action_string_args) : BaseBoardAction(action_string_args) {}
    inline Player nextPlayer() const { throw std::runtime_error{"MuZero does not support this game"}; }
    inline Player nextPlayer(int move_id) const { return move_id > 0 && move_id % 2 == 0 ? getPlayer() : BaseBoardAction::nextPlayer(); }
};

class Connect6Env : public BaseBoardEnv<Connect6Action> {
public:
    Connect6Env()
    {
        assert(getBoardSize() <= kMaxConnect6BoardSize);
        reset();
    }

    void reset() override;
    bool act(const Connect6Action& action) override;
    bool act(const std::vector<std::string>& action_string_args) override;
    std::vector<Connect6Action> getLegalActions() const override;
    bool isLegalAction(const Connect6Action& action) const override;
    bool isTerminal() const override;
    float getReward() const override { return 0.0f; }
    float getEvalScore(bool is_resign = false) const override;
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const Connect6Action& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline int getNumInputChannels() const override { return 24; }
    inline int getPolicySize() const override { return getBoardSize() * getBoardSize(); }
    std::string toString() const override;
    inline std::string name() const override { return kConnect6Name; }
    inline int getNumPlayer() const override { return kConnect6NumPlayer; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return utils::getPositionByRotating(rotation, position, getBoardSize()); };
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return getRotatePosition(action_id, rotation); };

private:
    Connect6Bitboard scanThreadSpace(Player p, int target) const;

    Player updateWinner(const Connect6Action& action);
    int calculateNumberOfConnection(const Connect6Action& action, std::pair<int, int> direction);
    std::string getCoordinateString() const;
    Player getPlayerAtBoardPos(int position) const;

    Player winner_;
    GamePair<Connect6Bitboard> bitboard_;
    std::vector<GamePair<Connect6Bitboard>> bitboard_history_;
};

class Connect6EnvLoader : public BaseBoardEnvLoader<Connect6Action, Connect6Env> {
public:
    std::vector<float> getActionFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline std::vector<float> getValue(const int pos) const { return {getReturn()}; }
    inline std::string name() const override { return kConnect6Name; }
    inline int getPolicySize() const override { return getBoardSize() * getBoardSize(); }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return utils::getPositionByRotating(rotation, position, getBoardSize()); };
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return getRotatePosition(action_id, rotation); };
};

} // namespace minizero::env::connect6
