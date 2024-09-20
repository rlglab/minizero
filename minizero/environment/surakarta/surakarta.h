#pragma once

#include "base_env.h"
#include "configuration.h"
#include <bitset>
#include <iostream>
#include <string>
#include <vector>

namespace minizero::env::surakarta {

const std::string kSurakartaName = "surakarta";
const int kSurakartaNumPlayer = 2;
const int kSurakartaBoardSize = 6;
typedef std::uint64_t SurakartaHashKey;

typedef std::bitset<kSurakartaBoardSize * kSurakartaBoardSize> SurakartaBitboard;

class SurakartaAction : public BaseAction {
public:
    SurakartaAction() : BaseAction() {}
    SurakartaAction(int action_id, Player player) : BaseAction(action_id, player) {}
    SurakartaAction(const std::vector<std::string>& action_string_args) : BaseAction()
    {
        action_id_ = stringToActionID(action_string_args);
        player_ = charToPlayer(action_string_args[0][0]);
    }
    inline Player nextPlayer() const override { return getNextPlayer(getPlayer(), kSurakartaNumPlayer); }
    inline std::string toConsoleString() const { return actionIDtoString(action_id_); }
    inline int getFromPos() const { return getFromPos(action_id_); }
    inline int getDestPos() const { return getDestPos(action_id_); }

private:
    int board_size_ = minizero::config::env_board_size;

    inline int getFromPos(int action_id) const { return action_id / (board_size_ * board_size_); }
    inline int getDestPos(int action_id) const { return action_id % (board_size_ * board_size_); }
    int coordinateToID(int c1, int r1, int c2, int r2) const;
    int charToPos(char c) const;
    int stringToActionID(const std::vector<std::string>& action_string_args) const;
    std::string actionIDtoString(int action_id) const;
};

class SurakartaEnv : public BaseBoardEnv<SurakartaAction> {
public:
    SurakartaEnv()
    {
        assert(board_size_ == kSurakartaBoardSize);
        createTrajectories();
        reset();
    }

    void reset() override;
    bool act(const SurakartaAction& action) override;
    bool act(const std::vector<std::string>& action_string_args) override;
    std::vector<SurakartaAction> getLegalActions() const override;
    bool isLegalAction(const SurakartaAction& action) const override;
    bool isTerminal() const override;
    float getReward() const override { return 0.0f; }

    float getEvalScore(bool is_resign = false) const override;
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const SurakartaAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline int getNumInputChannels() const override { return 18; }
    inline int getNumActionFeatureChannels() const override { return 0; }
    inline int getInputChannelHeight() const override { return getBoardSize(); }
    inline int getInputChannelWidth() const override { return getBoardSize(); }
    inline int getHiddenChannelHeight() const override { return getBoardSize(); }
    inline int getHiddenChannelWidth() const override { return getBoardSize(); }
    inline int getPolicySize() const override { return getBoardSize() * getBoardSize() * getBoardSize() * getBoardSize(); }
    std::string toString() const override;
    inline std::string name() const override { return kSurakartaName; }
    inline int getNumPlayer() const override { return kSurakartaNumPlayer; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; }
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return action_id; }

    SurakartaHashKey computeHashKey() const;
    SurakartaHashKey computeHashKey(const GamePair<SurakartaBitboard>& bitboard, Player turn) const;

private:
    enum class Direction {
        kNone,
        kVertical,
        kHorizontal
    };
    struct Point {
        int x;
        int y;
        Direction dir;

        Point(int x, int y, Direction dir) : x(x), y(y), dir(dir) {}
        Point(int x, int y) : x(x), y(y), dir(Direction::kNone) {}
        bool operator==(const Point& other) const { return (x == other.x) && (y == other.y); }
        bool operator!=(const Point& other) const { return !(*this == other); }
    };

private:
    std::vector<int> findNeighbors(int x, int y, std::vector<Point> circuit, std::vector<int> colorline) const;
    int findStartTrajectoryIndex(Point target, std::vector<Point> trajectory, bool same_direction) const;
    bool runCircuit(int pos, int dest_pos, Player next_player, std::vector<Point> circuit, std::vector<int> colorline) const;
    bool testCircular(const SurakartaAction& action, bool forbid_circular) const;
    bool isCircularAction(const SurakartaAction& action) const;
    bool isLegalActionInternal(const SurakartaAction& action, bool forbid_circular) const;

    Player eval() const;
    std::string getCoordinateString() const;
    void createTrajectories();
    std::vector<Point> createSingleTrajectory(std::vector<int> line_coordinates_);
    Player getPlayerAtBoardPos(int position) const;

    std::vector<int> redline_coord_;
    std::vector<int> greenline_coord_;
    std::vector<Point> redline_trajectory_;
    std::vector<Point> greenline_trajectory_;
    GamePair<SurakartaBitboard> bitboard_;
    std::vector<GamePair<SurakartaBitboard>> bitboard_history_;

    int no_capture_plies_;
    SurakartaHashKey hash_key_;
    std::vector<SurakartaHashKey> hashkey_history_;
};

class SurakartaEnvLoader : public BaseBoardEnvLoader<SurakartaAction, SurakartaEnv> {
public:
    std::vector<float> getActionFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline std::vector<float> getValue(const int pos) const { return {getReturn()}; }
    inline std::string name() const override { return kSurakartaName; }
    inline int getPolicySize() const override { return getBoardSize() * getBoardSize() * getBoardSize() * getBoardSize(); }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; }
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return action_id; }
};

} // namespace minizero::env::surakarta
