#pragma once

#include "base_env.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace minizero::env::linesofaction {

typedef std::uint64_t LinesOfActionHashKey;
typedef std::uint64_t LinesOfActionBitBoard;

extern std::unordered_map<std::string, int> kLinesOfActionStrToIdx;
extern std::vector<int> kLinesOfActionIdxToFromIdx;
extern std::vector<int> kLinesOfActionIdxToDestIdx;
extern std::vector<std::string> kLinesOfActionIdxToStr;
extern std::vector<std::string> kLinesOfActionSquareToStr;

const std::string kLinesOfActionName = "linesofaction";
const int kLinesOfActionNumPlayer = 2;
const int kLinesOfActionBoardSize = 8; // fit 64bits int

void initialize();

inline int getMoveIndex(const std::string move)
{
    auto it = kLinesOfActionStrToIdx.find(move);
    if (it == kLinesOfActionStrToIdx.end()) { return -1; }
    return it->second;
}

class LinesOfActionAction : public BaseBoardAction<kLinesOfActionNumPlayer> {
public:
    LinesOfActionAction() : BaseBoardAction<kLinesOfActionNumPlayer>() {}
    LinesOfActionAction(int action_id, Player player) : BaseBoardAction<kLinesOfActionNumPlayer>(action_id, player) {}
    LinesOfActionAction(const std::vector<std::string>& action_string_args, int board_size = minizero::config::env_board_size)
    {
        assert(action_string_args.size() == 2);
        assert(action_string_args[0].size() == 1);
        player_ = charToPlayer(action_string_args[0][0]);
        assert(static_cast<int>(player_) > 0 && static_cast<int>(player_) <= kLinesOfActionNumPlayer); // assume kPlayer1 == 1, kPlayer2 == 2, ...
        action_id_ = getMoveIndex(action_string_args[1]);
    }

    std::string toConsoleString() const override { return kLinesOfActionIdxToStr[action_id_]; }
    inline int getFromID() const { return kLinesOfActionIdxToFromIdx[action_id_]; }
    inline int getDestID() const { return kLinesOfActionIdxToDestIdx[action_id_]; }
};

class LinesOfActionEnv : public BaseBoardEnv<LinesOfActionAction> {
public:
    LinesOfActionEnv() : BaseBoardEnv<LinesOfActionAction>(kLinesOfActionBoardSize) { reset(); }

    void reset() override;
    bool act(const LinesOfActionAction& action) override;
    bool act(const std::vector<std::string>& action_string_args) override;
    std::vector<LinesOfActionAction> getLegalActions() const override;
    bool isLegalAction(const LinesOfActionAction& action) const override;
    bool isTerminal() const override;
    float getReward() const override { return 0.0f; }
    float getEvalScore(bool is_resign = false) const override;
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const LinesOfActionAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline int getNumInputChannels() const override { return 22; }
    inline int getPolicySize() const override { return kLinesOfActionIdxToStr.size(); }
    std::string toString() const override;
    inline std::string name() const override { return kLinesOfActionName; }
    inline int getNumPlayer() const override { return kLinesOfActionNumPlayer; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; }
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return action_id; }
    LinesOfActionHashKey computeHashKey() const;
    LinesOfActionHashKey computeHashKey(const GamePair<LinesOfActionBitBoard>& bitboard, Player turn) const;

private:
    bool isLegalActionInternal(const LinesOfActionAction& action, bool forbid_circular) const;
    int getNumPiecesOnLine(int pos, int k) const;
    int getNumPiecesOnLine(int x, int y, int dk, int dy) const;
    bool isOnBoard(int x, int y) const;
    bool searchConnection(Player p) const;
    Player whoConnectAll(bool& end) const;
    bool isCycleAction(const LinesOfActionAction& action) const;
    Player getPlayerAtBoardPos(int pos) const;

    GamePair<LinesOfActionBitBoard> bitboard_;
    LinesOfActionHashKey hash_key_;

    std::vector<GamePair<LinesOfActionBitBoard>> bitboard_history_;
    std::vector<std::array<int, 2>> direction_;
    std::vector<LinesOfActionHashKey> hashkey_history_;
};

class LinesOfActionEnvLoader : public BaseBoardEnvLoader<LinesOfActionAction, LinesOfActionEnv> {
public:
    std::vector<float> getActionFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline std::vector<float> getValue(const int pos) const { return {getReturn()}; }
    inline std::string name() const override { return kLinesOfActionName; }
    inline int getPolicySize() const override { return kLinesOfActionIdxToStr.size(); }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; }
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return action_id; }
};

} // namespace minizero::env::linesofaction
