#pragma once

#include "base_env.h"
#include <bitset>
#include <boost/dynamic_bitset.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace minizero::env::amazons {

const std::string kAmazonsName = "amazons";
const int kAmazonsNumPlayer = 2;
const int kMaxAmazonsBoardSize = 10;
const int kMinAmazonsBoardSize = 5;

extern std::unordered_map<int, int> kAmazonsPolicySize;
extern std::unordered_map<int, std::vector<int>> kAmazonsActionIdSplit;
extern std::unordered_map<int, std::vector<int>> kAmazonsActionIdToCoord;

typedef std::bitset<kMaxAmazonsBoardSize * kMaxAmazonsBoardSize> AmazonsBitboard;

void initialize();
std::array<int, 9> getDirectionMoveLength(const int board_size, const int x, const int y);

class AmazonsBoard {
public:
    AmazonsBoard() { reset(); }

    inline void reset() { black_ = white_ = arrow_ = AmazonsBitboard(); }
    inline void setPlayer(Player p, int pos) { (p == Player::kPlayer1 ? black_ : white_).set(pos); }
    inline void resetPlayer(Player p, int pos) { (p == Player::kPlayer1 ? black_ : white_).reset(pos); }
    inline void setArrow(int pos) { arrow_.set(pos); }
    inline Player getPlayer(int pos) const { return (black_.test(pos) ? Player::kPlayer1 : (white_.test(pos) ? Player::kPlayer2 : Player::kPlayerNone)); }
    inline const AmazonsBitboard& get(Player p) const { return (p == Player::kPlayer1 ? black_ : white_); }
    inline bool isArrow(int pos) const { return arrow_.test(pos); }
    inline bool isEmpty(int pos) const { return !(black_.test(pos) || white_.test(pos) || arrow_.test(pos)); }

private:
    AmazonsBitboard black_;
    AmazonsBitboard white_;
    AmazonsBitboard arrow_;
};

class AmazonsAction : public BaseBoardAction<kAmazonsNumPlayer> {
public:
    AmazonsAction() : BaseBoardAction() {}
    AmazonsAction(int action_id, Player player) : BaseBoardAction(action_id, player) {}
    AmazonsAction(const std::vector<std::string>& action_string_args);
    inline Player nextPlayer() const { throw std::runtime_error{"MuZero cannot call this method."}; }
    inline Player nextPlayer(int move_id) const { return (move_id > 0 && move_id % 2 == 1 ? getPlayer() : BaseBoardAction::nextPlayer()); }
    std::string toConsoleString() const override;
};

class AmazonsEnv : public BaseBoardEnv<AmazonsAction> {
public:
    AmazonsEnv() { reset(); }

    void reset() override;
    bool act(const AmazonsAction& action) override;
    bool act(const std::vector<std::string>& action_string_args) override { return act(AmazonsAction(action_string_args)); }
    std::vector<AmazonsAction> getLegalActions() const override;
    bool isLegalAction(const AmazonsAction& action) const override;
    bool isTerminal() const override { return winner_ != Player::kPlayerNone; }
    float getReward() const override { return 0.0f; }
    float getEvalScore(bool is_resign = false) const override;
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const AmazonsAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline int getNumInputChannels() const override { return 28; }
    inline int getPolicySize() const override { return kAmazonsPolicySize[getBoardSize()]; } // board_size: policy_size = {5: 345, 6: 616, 7: 1001, 8: 1520, 9: 2193, 10: 3040}
    std::string toString() const override;
    inline std::string name() const override { return kAmazonsName + "_" + std::to_string(getBoardSize()) + "x" + std::to_string(getBoardSize()); }
    inline int getNumPlayer() const override { return kAmazonsNumPlayer; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; }
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return action_id; }

private:
    inline int xyToPosition(const int x, const int y) const { return y * getBoardSize() + x; }
    inline int actionIdToCoord(const int action_id) const { return kAmazonsActionIdToCoord[board_size_][action_id]; }
    inline int getStartPosition(int action_id) const { return xyToPosition((actionIdToCoord(action_id) >> 18) & 0b111111, (actionIdToCoord(action_id) >> 12) & 0b111111); }
    inline int getEndPosition(int action_id) const { return xyToPosition((actionIdToCoord(action_id) >> 6) & 0b111111, actionIdToCoord(action_id) & 0b111111); }
    void updateLegalAction();
    std::string getCoordinateString() const;

    Player winner_;
    AmazonsBoard bitboard_;
    boost::dynamic_bitset<> actions_mask_;
    std::vector<AmazonsBoard> bitboard_history_;
};

class AmazonsEnvLoader : public BaseBoardEnvLoader<AmazonsAction, AmazonsEnv> {
public:
    std::vector<float> getActionFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline std::vector<float> getValue(const int pos) const { return {getReturn()}; }
    inline std::string name() const override { return kAmazonsName + "_" + std::to_string(getBoardSize()) + "x" + std::to_string(getBoardSize()); }
    inline int getPolicySize() const override { return kAmazonsPolicySize[getBoardSize()]; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; }
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return action_id; }
};

} // namespace minizero::env::amazons
