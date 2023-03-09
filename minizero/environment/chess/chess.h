#pragma once

#include "base_env.h"
#include "bitboard.h"
#include "sgf_loader.h"
#include <bitset>
#include <iostream>
#include <random>
#include <stdlib.h>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

// Board format
//    ++++++++++++++++++++++++
// 8 + 56 57 58 59 60 61 62 63 +
// 7 + 48 49 50 51 52 53 54 55 +
// 6 + 40 41 42 43 44 45 46 47 +
// 5 + 32 33 34 35 36 37 38 39 +
// 4 + 24 25 26 27 28 29 30 31 +
// 3 + 16 17 18 19 20 21 22 23 +
// 2 + 08 09 10 11 12 13 14 15 +
// 1 + 00 01 02 03 04 05 06 07 +
//    ++++++++++++++++++++++++
//      a  b  c  d  e  f  g  h

namespace minizero::env::chess {

const std::string kChessName = "chess";
const int kChessNumPlayer = 2;
const int kChessBoardSize = 8;
enum class Pieces {
    empty,
    pawn,
    knight,
    bishop,
    rook,
    queen,
    king
};
void initialize();
static const std::pair<int, int> kKingMoves[] = {
    {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
static const std::pair<int, int> kRookDirections[] = {
    {1, 0}, {0, 1}, {-1, 0}, {0, -1}};
static const std::pair<int, int> kBishopDirections[] = {
    {1, 1}, {-1, 1}, {-1, -1}, {1, -1}};
static const std::pair<int, int> kActionIdDirections64[] = {
    {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}, {0, 2}, {2, 2}, {2, 0}, {2, -2}, {0, -2}, {-2, -2}, {-2, 0}, {-2, 2}, {0, 3}, {3, 3}, {3, 0}, {3, -3}, {0, -3}, {-3, -3}, {-3, 0}, {-3, 3}, {0, 4}, {4, 4}, {4, 0}, {4, -4}, {0, -4}, {-4, -4}, {-4, 0}, {-4, 4}, {0, 5}, {5, 5}, {5, 0}, {5, -5}, {0, -5}, {-5, -5}, {-5, 0}, {-5, 5}, {0, 6}, {6, 6}, {6, 0}, {6, -6}, {0, -6}, {-6, -6}, {-6, 0}, {-6, 6}, {0, 7}, {7, 7}, {7, 0}, {7, -7}, {0, -7}, {-7, -7}, {-7, 0}, {-7, 7}, {1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};

static const std::pair<int, Pieces> kActionIdDirections73[] = {
    {-1, Pieces::rook}, {-1, Pieces::bishop}, {-1, Pieces::knight}, {0, Pieces::rook}, {0, Pieces::bishop}, {0, Pieces::knight}, {1, Pieces::rook}, {1, Pieces::bishop}, {1, Pieces::knight}};

class ChessAction : public BaseAction {
public:
    ChessAction() : BaseAction() {}
    ChessAction(int action_id, Player player) : BaseAction(action_id, player) { pawnmove_ = false; }
    ChessAction(const std::vector<std::string>& action_string_args);
    std::string getActionString() const;
    void isPawnMove() { pawnmove_ = true; }
    inline Player nextPlayer() const override { return getNextPlayer(player_, kChessNumPlayer); }
    inline std::string toConsoleString() const override { return getActionString(); };

private:
    bool pawnmove_;
};

#define NUM_OF_PIECES 12
#define NUM_OF_SQUARE 64
#define PAWN 0
#define KNIGHT 1
#define BISHOP 2
#define ROOK 3
#define QUEEN 4
#define KING 5
#define ID00 11
#define ID000 15

class ChessEnv : public BaseEnv<ChessAction> {
public:
    ChessEnv()
    {
        initialize();
        reset();
    }
    void reset() override;
    void setInt();
    void setChessPair();
    void setHash();
    void setLegalAction();
    bool rowOutOfBoard(int row) const { return row < 0 || row >= 8; }
    bool colOutOfBoard(int col) const { return col < 0 || col >= 8; }
    bool outOfBoard(int square) const { return square < 0 || square >= 64; }
    bool outOfBoard(int row, int col) const { return rowOutOfBoard(row) || colOutOfBoard(col); }
    int positionToRow(int position) const { return position / 8; }
    int positionToCol(int position) const { return position % 8; }
    int rowColToPosition(int row, int col) const;
    int toBitBoardSquare(int row, int col) const { return row + 8 * col; }
    int toBitBoardSquare(int position) const { return toBitBoardSquare(positionToRow(position), positionToCol(position)); }
    bool squareIsAttacked(Player ply, int position, bool check_kings_attack) const;
    bool plyIsCheck(Player ply) const;
    int storeHashTable(std::uint64_t newhash);
    std::uint64_t updateHashValue(Player turn, int move_from, int move_to, Pieces moved, Pieces taken,
                                  Pieces promote_to, bool is00, bool is000, bool isenpassant);
    bool checkBitboard() const;
    void showRemain() const;
    void showMoveInfo(int from, int to, Pieces move, Pieces take, Pieces promote);
    void updateBoard(int from, int to, Pieces promote);
    void update50Steps(Pieces take, Pieces move);
    void setEnPassantFlag(int from, int to);
    bool canPlayer00(Player player, int next_square1, int next_square2);
    bool canPlayer000(Player player, int next_square1, int next_square2, int next_square3);
    bool whiteInsuffi();
    bool blackInsuffi();
    void handle00(bool is00, int from);
    void handle000(bool is000, int from);
    void handleEnPassant(int from, int to);
    void handlePawnMove(Pieces promote, int from, int to);
    void handleKnightMove(int from, int to);
    void handleBishopMove(int from, int to);
    void handleRookMove(int from, int to);
    void handleQueenMove(int from, int to);
    void handleKingMove(int to);
    void handleMyRookIsMoved(Player turn, Pieces move, int from, int krook_pos, int qrook_pos);
    void handleEnemyRookIsMoved(Player opponent, int to, int krook_pos, int qrook_pos);
    void handleNotRookCapture(Pieces take, int to);
    bool generateKingAttackedInfo(Player ply);
    void generateLegalActions();
    int getToFromID(int move_dir, int from);
    Pieces getPromoteFromID(int move_dir, int to, Pieces move);
    int getDirection(int dir) const;
    void fakeAct(int from, int to, Pieces move, Pieces take, Pieces promote, bool is00, bool is000, bool isenpassant);
    bool act(const ChessAction& action) override;
    bool act(const std::vector<std::string>& action_string_args) override;
    bool isntCheckMove(const ChessAction& action) const;
    bool isLegalAction(const ChessAction& action) const override;
    std::vector<ChessAction> getLegalActions() const override;
    bool noMoreLegalAction() const;
    bool isTerminal() const override;
    Player eval() const;
    float getReward() const override { return 0.0f; }
    float getEvalScore(bool is_resign = false) const override
    {
        // TODO
        Player eval_result = eval();
        switch (eval_result) {
            case Player::kPlayer1: return 1.0f;
            case Player::kPlayer2: return -1.0f;
            default: return 0.0f;
        }
    }
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override
    {
        std::vector<float> vFeatures;
        Player opponent = getNextPlayer(turn_, kChessNumPlayer);
        for (int channel = 0; channel < 120; ++channel) {
            for (int pos = 0; pos < kChessBoardSize * kChessBoardSize; ++pos) {
                Player player = channel % 2 == 0 ? turn_ : opponent;
                if (channel < 112) {
                    // (turn_ pieces x6 + opponent pieces x6 + repetition x2) x8
                    int ind = history_.size() - 1 - channel / 14;
                    if (ind < 0) {
                        vFeatures.push_back(0.0f);
                    } else {
                        int channel_id = channel % 14;
                        if (channel_id <= 1) { // pawns
                            vFeatures.push_back(history_.pawn_[ind].get(player).get(toBitBoardSquare(pos)) ? 1.0f : 0.0f);
                        } else if (channel_id <= 3) { // knights
                            vFeatures.push_back(history_.knight_[ind].get(player).get(toBitBoardSquare(pos)) ? 1.0f : 0.0f);
                        } else if (channel_id <= 5) { // bishops
                            vFeatures.push_back(history_.bishop_[ind].get(player).get(toBitBoardSquare(pos)) ? 1.0f : 0.0f);
                        } else if (channel_id <= 7) { // rooks
                            vFeatures.push_back(history_.rook_[ind].get(player).get(toBitBoardSquare(pos)) ? 1.0f : 0.0f);
                        } else if (channel_id <= 9) { // queens
                            vFeatures.push_back(history_.queen_[ind].get(player).get(toBitBoardSquare(pos)) ? 1.0f : 0.0f);
                        } else if (channel_id <= 11) { // king
                            vFeatures.push_back(toBitBoardSquare(history_.king_[ind].get(player)) == pos ? 1.0f : 0.0f);
                        } else { // TODO: repetition-1, repetition-2
                            if ((repetitions_ == 1 && channel_id == 12) || (repetitions_ == 2 && channel_id == 13))
                                vFeatures.push_back(0.0f);
                            else if (repetitions_ == 3 || (repetitions_ == 1 && channel_id == 13) || (repetitions_ == 2 && channel_id == 12))
                                vFeatures.push_back(1.0f);
                        }
                    }
                } else if (channel == 112) { // colour
                    vFeatures.push_back((turn_ == Player::kPlayer1) ? 1.0f : 0.0f);
                } else if (channel == 113) { // colour
                    vFeatures.push_back((turn_ == Player::kPlayer2) ? 1.0f : 0.0f);
                } else if (channel == 114) { // total move count /100
                    vFeatures.push_back(history_.size() / 100);
                } else if (channel <= 116) { // turn/opp 0-0
                    vFeatures.push_back(can00_.get(player) ? 1.0f : 0.0f);
                } else if (channel <= 118) { // turn/opp 0-0-0
                    vFeatures.push_back(can000_.get(player) ? 1.0f : 0.0f);
                } else {
                    // TODO: no progress count /50
                    vFeatures.push_back((fifty_steps_ % 2) ? (fifty_steps_ + 1) / 100 : fifty_steps_ / 100);
                }
            }
        }
        return vFeatures;
    }
    std::vector<float> getActionFeatures(const ChessAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override
    {
        // TODO
        return {};
    }
    std::string toString() const override;
    inline std::string name() const override { return kChessName; }
    inline int getNumPlayer() const override { return kChessNumPlayer; }

private:
    int ply1_remain_[5];
    int ply2_remain_[5];
    int fifty_steps_;
    int repetitions_;
    bool is_double_checked_;
    ChessPair<bool> ischecked_;
    ChessPair<bool> king_moved_;
    ChessPair<bool> krook_moved_;
    ChessPair<bool> qrook_moved_;
    ChessPair<bool> insuffi_;
    ChessPair<bool> can00_;
    ChessPair<bool> can000_;
    ChessPair<int> king_pos_;
    std::vector<std::pair<Player, Pieces>> board_;
    Pieces_Bitboard bitboard_;
    Pieces_History history_;
    std::uint64_t hash_value_;
    std::unordered_set<std::uint64_t> hash_table_1_;
    std::unordered_set<std::uint64_t> hash_table_2_;
    std::bitset<64> king_attacked_info_;
    std::bitset<4672> legal_actions_;
};
class ChessEnvLoader : public BaseEnvLoader<ChessAction, ChessEnv> {
public:
    std::vector<float> getActionFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline std::vector<float> getValue(const int pos) const { return {getReturn()}; }
    inline std::string name() const override { return kChessName; }
    inline int getPolicySize() const override { return 4672; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; }
};

} // namespace minizero::env::chess
