#pragma once

#include "base_env.h"
#include "configuration.h"
#include <string>
#include <vector>

namespace minizero::env::hex {

const std::string kHexName = "hex";
const int kHexNumPlayer = 2;
const int kMaxHexBoardSize = 19;

typedef BaseBoardAction<kHexNumPlayer> HexAction;

enum class Flag {
    BLACK_LEFT = 0x1,
    BLACK_RIGHT = 0x2,
    WHITE_BOTTOM = 0x4,
    WHITE_TOP = 0x10
};
inline Flag operator|(Flag a, Flag b)
{
    return static_cast<Flag>(static_cast<uint64_t>(a) | static_cast<uint64_t>(b));
}
inline Flag operator&(Flag a, Flag b)
{
    return static_cast<Flag>(static_cast<uint64_t>(a) & static_cast<uint64_t>(b));
}
struct Cell {
    Player player{};
    Flag flags;
};

class HexEnv : public BaseBoardEnv<HexAction> {
public:
    HexEnv()
    {
        assert(getBoardSize() <= kMaxHexBoardSize);
        reset();
    }

    void reset() override;
    bool act(const HexAction& action) override;
    bool act(const std::vector<std::string>& action_string_args) override;
    std::vector<HexAction> getLegalActions() const override;
    bool isLegalAction(const HexAction& action) const override;
    bool isTerminal() const override;
    float getReward() const override { return 0.0f; }
    float getEvalScore(bool is_resign = false) const override;
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const HexAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::string toString() const override;
    std::string toStringDebug() const;
    inline std::string name() const override { return kHexName + "_" + std::to_string(getBoardSize()) + "x" + std::to_string(getBoardSize()); }
    inline int getNumPlayer() const override { return kHexNumPlayer; }
    inline Player getWinner() const { return winner_; }
    inline const std::vector<Cell>& getBoard() const { return board_; }
    std::vector<int> getWinningStonesPosition() const;

private:
    Player updateWinner(int actionID);

    Player winner_;
    std::vector<Cell> board_;
};

class HexEnvLoader : public BaseBoardEnvLoader<HexAction, HexEnv> {
public:
    std::vector<float> getActionFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline std::vector<float> getValue(const int pos) const { return {getReturn()}; }
    inline std::string name() const override { return kHexName + "_" + std::to_string(getBoardSize()) + "x" + std::to_string(getBoardSize()); }
    inline int getPolicySize() const override { return getBoardSize() * getBoardSize(); }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; }
};

} // namespace minizero::env::hex
