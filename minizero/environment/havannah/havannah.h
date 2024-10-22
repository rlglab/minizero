#pragma once

#include "base_env.h"
#include "configuration.h"
#include <bitset>
#include <string>
#include <utility>
#include <vector>

namespace minizero::env::havannah {

const std::string kHavannahName = "havannah";
const int kHavannahNumPlayer = 2;
const int kMaxHavannahBoardSize = 19;
const int kMinHavannahBoardSize = 4;
const int kMaxExtendedHavannahBoardSize = kMaxHavannahBoardSize * 2 - 1;

typedef std::bitset<kMaxExtendedHavannahBoardSize * kMaxExtendedHavannahBoardSize> HavannahBitboard;

class HavannahAction : public BaseBoardAction<kHavannahNumPlayer> {
public:
    HavannahAction() : BaseBoardAction<kHavannahNumPlayer>() {}
    HavannahAction(int action_id, Player player) : BaseBoardAction<kHavannahNumPlayer>(action_id, player) {}
    HavannahAction(const std::vector<std::string>& action_string_args, int board_size = minizero::config::env_board_size)
    {
        assert(action_string_args.size() == 2);
        assert(action_string_args[0].size() == 1);
        player_ = charToPlayer(action_string_args[0][0]);
        assert(static_cast<int>(player_) > 0 && static_cast<int>(player_) <= kHavannahNumPlayer); // assume kPlayer1 == 1, kPlayer2 == 2, ...
        action_id_ = minizero::utils::SGFLoader::boardCoordinateStringToActionID(action_string_args[1], toExtendedBoardSize(board_size));
    }

    inline std::string toConsoleString() const override { return minizero::utils::SGFLoader::actionIDToBoardCoordinateString(getActionID(), toExtendedBoardSize(minizero::config::env_board_size)); }
    inline int toExtendedBoardSize(int edge_board_size) const { return edge_board_size * 2 - 1; }
};

class HavannahEnv : public BaseBoardEnv<HavannahAction> {
public:
    HavannahEnv()
    {
        assert(getBoardSize() <= kMaxHavannahBoardSize && getBoardSize() >= kMinHavannahBoardSize);
        extended_board_size_ = getBoardSize() * 2 - 1;
        initialize();
        reset();
    }

    HavannahEnv(const HavannahEnv& env) { *this = env; }
    HavannahEnv& operator=(const HavannahEnv& env);

    void initialize();
    void reset() override;
    bool act(const HavannahAction& action) override;
    bool act(const std::vector<std::string>& action_string_args) override { return act(HavannahAction(action_string_args)); }
    std::vector<HavannahAction> getLegalActions() const override;
    bool isLegalAction(const HavannahAction& action) const override;
    bool isTerminal() const override;
    float getReward() const override { return 0.0f; }
    float getEvalScore(bool is_resign = false) const override;
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const HavannahAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline int getNumInputChannels() const override { return 20; }
    inline int getInputChannelHeight() const override { return extended_board_size_; }
    inline int getInputChannelWidth() const override { return extended_board_size_; }
    inline int getHiddenChannelHeight() const override { return extended_board_size_; }
    inline int getHiddenChannelWidth() const override { return extended_board_size_; }
    inline int getPolicySize() const override { return extended_board_size_ * extended_board_size_; }
    std::string toString() const override;
    inline std::string name() const override { return kHavannahName + std::to_string(getBoardSize()) + "x" + std::to_string(getBoardSize()); }
    inline int getNumPlayer() const override { return kHavannahNumPlayer; }

    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; };
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return action_id; };

private:
    class HavannahPath {
    public:
        HavannahPath(int index) : index_(index) { reset(); }

        void reset()
        {
            player_ = Player::kPlayerNone;
            cells_.reset();
        }

        inline void setPlayer(Player player) { player_ = player; }
        inline void addCell(int pos) { cells_.set(pos); }
        inline int getIndex() const { return index_; }
        inline Player getPlayer() const { return player_; }
        inline HavannahBitboard getCells() const { return cells_; }
        inline int getNumCells() const { return cells_.count(); }

        inline void combineWithPath(HavannahPath* path)
        {
            assert(path);
            cells_ |= path->getCells();
        }

    private:
        int index_;
        Player player_;
        HavannahBitboard cells_;
    };

    class HavannahCell {
    public:
        HavannahCell() {}
        HavannahCell(int pos, int extended_board_size) : position_(pos), coordinate_{pos / extended_board_size, pos % extended_board_size} { reset(); }

        void reset()
        {
            player_ = Player::kPlayerNone;
            path_ = nullptr;
        }

        inline void setPlayer(Player player) { player_ = player; }
        inline void setPath(HavannahPath* path) { path_ = path; }

        inline int getPosition() const { return position_; }
        inline const std::pair<int, int>& getCoordinate() const { return coordinate_; }
        inline Player getPlayer() const { return player_; }
        inline HavannahPath* getPath() const { return path_; }

    private:
        int position_;
        std::pair<int, int> coordinate_;
        Player player_;
        HavannahPath* path_;
    };

private:
    inline bool isSwappable() const { return (config::env_havannah_use_swap_rule && actions_.size() == 1); }
    Player updateWinner(const HavannahAction& action);
    void calculateNeighbors();
    bool isValidCell(const HavannahCell& cell) const;
    bool isValidCoor(int i, int j) const;
    std::string getCoordinateString() const;

    HavannahPath* newPath();
    HavannahPath* combinePaths(HavannahPath* path1, HavannahPath* path2);
    void removePath(HavannahPath* path);
    bool isCycle(const HavannahPath* path, const HavannahAction& last_action) const;
    int computeOwnNeighbors(int pos, Player player) const;
    bool detectHole(const HavannahPath* path) const;

    int extended_board_size_;
    Player winner_;
    std::vector<HavannahCell> cells_;
    std::vector<HavannahPath> paths_;
    std::vector<std::array<int, 7>> neighbors_;

    GamePair<HavannahBitboard> bitboard_pair_;
    std::vector<GamePair<HavannahBitboard>> bitboard_history_;

    HavannahBitboard board_mask_bitboard_;
    HavannahBitboard free_path_id_bitboard_;
    HavannahBitboard corner_bitboard_;
    std::vector<HavannahBitboard> border_bitboards_;
};

class HavannahEnvLoader : public BaseBoardEnvLoader<HavannahAction, HavannahEnv> {
public:
    std::vector<float> getActionFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline std::vector<float> getValue(const int pos) const { return {getReturn()}; }
    inline std::string name() const override { return kHavannahName + std::to_string(getBoardSize()) + "x" + std::to_string(getBoardSize()); }
    inline int getPolicySize() const override { return (getBoardSize() * 2 - 1) * (getBoardSize() * 2 - 1); }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; };
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return action_id; };
};

} // namespace minizero::env::havannah
