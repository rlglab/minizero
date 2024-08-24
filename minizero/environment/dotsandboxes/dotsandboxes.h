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

    inline Player nextPlayer(int move_id) const
    {
        throw std::runtime_error{"MuZero does not support this game"};
    }

    inline std::string toConsoleString() const override
    {
        return getCoordString();
    }

private:
    inline int coordToActionID(const std::string& coord, int board_size) const
    {
        auto parse = [](std::string& buf, int size) {
            int width = size;
            int x = std::toupper(buf[0]) - 'A' + (std::toupper(buf[0]) > 'I' ? -1 : 0);
            int y = atoi(buf.substr(1).c_str()) - 1;
            if (x >= size || y >= size) {
                return -1;
            }
            return x + width * y;
        };
        std::string tmp = coord;
        std::transform(tmp.begin(), tmp.end(), tmp.begin(), ::toupper);

        int size = coord.size();
        int dots_size = board_size + 1;
        std::string frombuf, destbuf;
        if (size == 4) {
            // example, A1A2
            frombuf = tmp.substr(0, 2);
            destbuf = tmp.substr(2);
        } else if (size == 5) {
            // example, A9A10 or A10A9
            int mid = std::isdigit(tmp[2]) ? 3 : 2;
            frombuf = tmp.substr(0, mid);
            destbuf = tmp.substr(mid);
        } else if (size == 6) {
            // example, A10A11
            frombuf = tmp.substr(0, 3);
            destbuf = tmp.substr(3);
        } else {
            // invalid
            return -1;
        }

        int frompos = parse(frombuf, dots_size);
        int destpos = parse(destbuf, dots_size);
        if (frompos == -1 || destpos == -1) {
            return -1;
        }
        if (frompos > destpos) {
            // make sure the frompos is lower
            std::swap(frompos, destpos);
        }
        int diff = destpos - frompos;
        if (diff != 1 && diff != dots_size) {
            // diff = 1, means the line is horizontal
            // diff = dots_size, means the line is vertical
            // otherwise, the line is invalid
            return -1;
        }

        // shift coordinate
        int level = frompos / dots_size;
        frompos -= (level * dots_size);
        destpos -= (level * dots_size);
        return (diff == 1 ? frompos : destpos - 1) + level * (2 * dots_size - 1);
    }

    inline std::string getCoordString(int board_size = minizero::config::env_board_size) const
    {
        auto parse = [](int pos, int size) {
            int x = pos % size;
            int y = pos / size;
            std::ostringstream oss;
            oss << static_cast<char>(x + 'A' + (x >= 8)) << y + 1;
            return oss.str();
        };
        std::ostringstream oss;
        int dots_size = board_size + 1;
        int x2 = action_id_ % (2 * board_size + 1);
        int y2 = action_id_ / (2 * board_size + 1);
        int shift = y2 * dots_size;
        if (x2 < board_size) {
            int frompos = x2 + shift;
            int destpot = x2 + 1 + shift;
            oss << parse(frompos, dots_size) << parse(destpot, dots_size);
        } else {
            int frompos = x2 - board_size + shift;
            int destpot = x2 + 1 + shift;
            oss << parse(frompos, dots_size) << parse(destpot, dots_size);
        }
        return oss.str();
    }
};

class DotsAndBoxesEnv : public BaseBoardEnv<DotsAndBoxesAction> {
public:
    DotsAndBoxesEnv()
    {
        assert(getBoardSize() <= kMaxDotsAndBoxesBoardSize);
        reset();
    }

    void reset() override;
    bool act(const DotsAndBoxesAction& action) override;
    bool act(const std::vector<std::string>& action_string_args) override;
    std::vector<DotsAndBoxesAction> getLegalActions() const override;
    bool isLegalPlayer(const Player) const;
    bool isLegalAction(const DotsAndBoxesAction& action) const override;
    bool isTerminal() const override;
    float getReward() const override { return 0.0f; }
    float getEvalScore(bool is_resign = false) const override;
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const DotsAndBoxesAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline int getPolicySize() const override { return getNumGirdLines(); }
    inline int getNumActionFeatureChannels() const override { return 1; }
    std::string toString() const override;
    std::string toStringDebug() const;
    inline std::string name() const override { return kDotsAndBoxesName; }
    inline int getNumPlayer() const override { return kDotsAndBoxesNumPlayer; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; };
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return action_id; };
    inline int getNumInputChannels() const override { return 90; }
    inline int getInputChannelHeight() const override { return full_board_size_height_; }
    inline int getInputChannelWidth() const override { return full_board_size_width_; }
    inline int getHiddenChannelHeight() const override { return full_board_size_height_; }
    inline int getHiddenChannelWidth() const override { return full_board_size_width_; }

private:
    using Board = std::vector<Grid>;

    bool isOnBoard(const int x, const int y) const;
    bool shouldMark(const int pos) const;
    int getNumGirdLines() const;

    int xyToPosition(const int x, const int y) const;
    int xyToLineIdx(const int x, const int y) const;
    int posToLineIdx(int pos) const;
    int lineIdxToPos(int idx) const;
    std::vector<int> getAdjacentLinesPos(int pos) const;
    std::vector<std::array<int, 2>> direction_;
    std::vector<std::shared_ptr<Board>> board_history_;
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
    inline std::string name() const override { return kDotsAndBoxesName; }
    inline int getPolicySize() const override { return getNumGirdLines(); }
    inline int getNumActionFeatureChannels() const { return 1; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; };
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return getRotatePosition(action_id, rotation); };

private:
    int getNumGirdLines() const;
};

} // namespace minizero::env::dotsandboxes
