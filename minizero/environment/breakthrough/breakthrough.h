#pragma once

#include "base_env.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace minizero::env::breakthrough {

typedef std::uint64_t BreakthroughBitBoard;

const std::string kBreakthroughName = "breakthrough";
const int kBreakthroughMaxBoardSize = 8; // fit 64bits int
const int kBreakthroughMinBoardSize = 5;
const int kBreakthroughNumPlayer = 2;

extern std::vector<int> kBreakthroughPolicySize;
extern std::vector<std::string> kBreakthroughIdxToStr;
extern std::vector<int> kBreakthroughIdxToFromIdx;
extern std::vector<int> kBreakthroughIdxToDestIdx;
extern std::unordered_map<std::string, int> kBreakthroughStrToIdx;

void initialize();

inline int getMoveIndex(const std::string move)
{
    auto it = kBreakthroughStrToIdx.find(move);
    if (it == kBreakthroughStrToIdx.end()) { return -1; }
    return it->second;
}

class BreakthroughAction : public BaseBoardAction<kBreakthroughNumPlayer> {
public:
    BreakthroughAction() : BaseBoardAction<kBreakthroughNumPlayer>() {}
    BreakthroughAction(int action_id, Player player) : BaseBoardAction<kBreakthroughNumPlayer>(action_id, player) {}
    BreakthroughAction(const std::vector<std::string>& action_string_args, int board_size = minizero::config::env_board_size)
    {
        assert(action_string_args.size() == 2);
        assert(action_string_args[0].size() == 1);
        player_ = charToPlayer(action_string_args[0][0]);
        assert(static_cast<int>(player_) > 0 && static_cast<int>(player_) <= kBreakthroughNumPlayer); // assume kPlayer1 == 1, kPlayer2 == 2, ...
        action_id_ = getMoveIndex(action_string_args[1]);
    }

    std::string toConsoleString() const override { return kBreakthroughIdxToStr[getActionID()]; }
    int getFromID(int board_size = minizero::config::env_board_size) const;
    int getDestID(int board_size = minizero::config::env_board_size) const;
};

class BreakthroughEnv : public BaseBoardEnv<BreakthroughAction> {
public:
    BreakthroughEnv() { reset(); }
    void reset() override;
    bool act(const BreakthroughAction& action) override;
    bool act(const std::vector<std::string>& action_string_args) override;
    std::vector<BreakthroughAction> getLegalActions() const override;
    bool isLegalAction(const BreakthroughAction& action) const override;
    bool isTerminal() const override;
    float getReward() const override { return 0.0f; }
    float getEvalScore(bool is_resign = false) const override;
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const BreakthroughAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline int getNumInputChannels() const override { return 20; }
    inline int getPolicySize() const override { return kBreakthroughPolicySize[board_size_]; }
    std::string toString() const override;
    inline std::string name() const override { return kBreakthroughName + "_" + std::to_string(getBoardSize()) + "x" + std::to_string(getBoardSize()); }
    inline int getNumPlayer() const override { return kBreakthroughNumPlayer; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; }
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return action_id; }

private:
    BreakthroughBitBoard bitboard_rank1_;
    BreakthroughBitBoard bitboard_rank2_;
    BreakthroughBitBoard bitboard_reverse_rank2_;
    BreakthroughBitBoard bitboard_reverse_rank1_;

    bool isThreatPosition(Player color, int position) const;
    BreakthroughBitBoard getThreatSpace(Player color) const;
    Player getPlayerAtBoardPos(int pos) const;

    GamePair<BreakthroughBitBoard> bitboard_;
    std::vector<GamePair<BreakthroughBitBoard>> bitboard_history_;
};

class BreakthroughEnvLoader : public BaseBoardEnvLoader<BreakthroughAction, BreakthroughEnv> {
public:
    std::vector<float> getActionFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline std::vector<float> getValue(const int pos) const { return {getReturn()}; }
    inline std::string name() const override { return kBreakthroughName + "_" + std::to_string(getBoardSize()) + "x" + std::to_string(getBoardSize()); }
    inline int getPolicySize() const override { return kBreakthroughPolicySize[board_size_]; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; }
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return action_id; }
};

} // namespace minizero::env::breakthrough
