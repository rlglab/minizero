#pragma once

#include "base_env.h"
#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace minizero::env::dotsandboxes {

enum class Grid {
    kGridNoPlayer = static_cast<int>(Player::kPlayerNone),
    kGridPlayer1 = static_cast<int>(Player::kPlayer1),
    kGridPlayer2 = static_cast<int>(Player::kPlayer2),
    kGridNoLine = static_cast<int>(Player::kPlayer2) + 1,
    kGridLine = static_cast<int>(Player::kPlayer2) + 2,
    kGridDots = static_cast<int>(Player::kPlayer2) + 3
};

const std::string kDotsAndBoxesName = "dotsandboxes";
const int kDotsAndBoxesNumPlayer = 2;
const int kMaxDotsAndBoxesBoardSize = 9;

class DotsAndBoxesAction : public BaseBoardAction<kDotsAndBoxesNumPlayer> {
public:
    DotsAndBoxesAction() : BaseBoardAction<kDotsAndBoxesNumPlayer>() {}
    DotsAndBoxesAction(int action_id, Player player) : BaseBoardAction<kDotsAndBoxesNumPlayer>(action_id, player) {}
    DotsAndBoxesAction(const std::vector<std::string>& action_string_args, int board_size = minizero::config::env_board_size) : BaseBoardAction<kDotsAndBoxesNumPlayer>(action_string_args, board_size)
    {
        assert(action_string_args.size() == 2);
        assert(action_string_args[0].size() == 1);
        player_ = charToPlayer(action_string_args[0][0]);
        assert(static_cast<int>(player_) > 0 && static_cast<int>(player_) <= kDotsAndBoxesNumPlayer); // assume kPlayer1 == 1, kPlayer2 == 2, ...
        action_id_ = coordToActionID(action_string_args[1], board_size);
    }

    inline Player nextPlayer() const { throw std::runtime_error{"MuZero cannot call this method."}; }
    inline std::string toConsoleString() const override { return getCoordString(); }

private:
    int coordToActionID(const std::string& coord, int board_size) const;
    std::string getCoordString(int board_size = minizero::config::env_board_size) const;
};

class DotsAndBoxesEnv : public BaseBoardEnv<DotsAndBoxesAction> {
public:
    DotsAndBoxesEnv() { reset(); }

    void reset() override;
    bool act(const DotsAndBoxesAction& action) override;
    bool act(const std::vector<std::string>& action_string_args) override;
    std::vector<DotsAndBoxesAction> getLegalActions() const override;
    bool isLegalAction(const DotsAndBoxesAction& action) const override;
    bool isTerminal() const override;
    float getReward() const override { return 0.0f; }
    float getEvalScore(bool is_resign = false) const override;
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const DotsAndBoxesAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline int getPolicySize() const override { return getNumGirdLines(); }
    inline int getNumActionFeatureChannels() const override { return 1; }
    std::string toString() const override;
    inline std::string name() const override { return kDotsAndBoxesName + "_" + std::to_string(getBoardSize()) + "x" + std::to_string(getBoardSize()); }
    inline int getNumPlayer() const override { return kDotsAndBoxesNumPlayer; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; };
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return action_id; };
    inline int getNumInputChannels() const override { return 90; }
    inline int getInputChannelHeight() const override { return full_board_size_height_; }
    inline int getInputChannelWidth() const override { return full_board_size_width_; }
    inline int getHiddenChannelHeight() const override { return full_board_size_height_; }
    inline int getHiddenChannelWidth() const override { return full_board_size_width_; }

private:
    typedef std::vector<Grid> Board;

    std::string toStringDebug() const;

    inline bool isOnBoard(const int x, const int y) const { return x >= 0 && x < full_board_size_width_ && y >= 0 && y < full_board_size_height_; }
    bool shouldMark(const int pos) const;
    inline int xyToPosition(const int x, const int y) const { return y * full_board_size_width_ + x; }
    inline int xyToLineIdx(const int x, const int y) const { return y * (board_size_width_ + 1) + x - (y + 1) / 2; }
    inline int posToLineIdx(int pos) const { return xyToLineIdx(pos % full_board_size_width_, pos / full_board_size_width_); }
    inline int lineIdxToPos(int idx) const { return 2 * idx + 1; } // the line index is action id.
    inline int getNumGirdLines() const { return board_size_width_ * (board_size_height_ + 1) + (board_size_width_ + 1) * board_size_height_; }
    std::vector<int> getAdjacentLinesPos(int pos) const;

    std::vector<std::array<int, 2>> direction_;
    std::vector<Board> board_history_;
    std::vector<Player> continue_history_;

    Board board_;
    bool current_player_continue_;

    int board_size_width_;
    int board_size_height_;
    int full_board_size_width_;
    int full_board_size_height_;
};

class DotsAndBoxesEnvLoader : public BaseBoardEnvLoader<DotsAndBoxesAction, DotsAndBoxesEnv> {
public:
    std::vector<float> getActionFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline std::vector<float> getValue(const int pos) const { return {getReturn()}; }
    inline std::string name() const override { return kDotsAndBoxesName + "_" + std::to_string(getBoardSize()) + "x" + std::to_string(getBoardSize()); }
    inline int getPolicySize() const override { return getNumGirdLines(); }
    inline int getNumActionFeatureChannels() const { return 1; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; };
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return action_id; };

private:
    inline int getNumGirdLines() const { return board_size_ * (board_size_ + 1) + (board_size_ + 1) * board_size_; }
};

} // namespace minizero::env::dotsandboxes
