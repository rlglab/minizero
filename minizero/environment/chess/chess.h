#pragma once

#include "base_env.h"
#include "sgf_loader.h"
#include "bitboard.h"
#include <iostream>
#include <stdlib.h>
#include <random>
#include <unordered_set>

// Board format
// 0 is a1, 8 is a2, 63 is h8.
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
    empty = 0,
    pawn = 1,
    knight = 3,
    bishop = 4,
    rook = 5,
    queen = 9,
    king = 10
};

static const std::pair<int, int> kKingMoves[] = {
    {-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}};

static const std::pair<int, int> kRookDirections[] = {
    {1, 0}, {-1, 0}, {0, 1}, {0, -1}};

static const std::pair<int, int> kBishopDirections[] = {
    {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};


class ChessAction : public BaseAction {
public:
    ChessAction() : BaseAction() {}
    ChessAction(int action_id, Player player) : BaseAction(action_id, player) {}
    ChessAction(const std::vector<std::string>& action_string_args);
    ChessAction(Player player, int from, int to, Pieces move_p, Pieces taken_p, Pieces promote_p, int id);
    inline Player nextPlayer() const override { return getNextPlayer(player_, kChessNumPlayer); }
    inline std::string toConsoleString() const override { return ""; };
    int convertToID();

    void showActionInfo() const{
        std::cout << "from " << from_ << "(" << char('a' + from_ % 8) << from_ / 8 + 1 << ") -> ";
        std::cout << "to " << to_ << "(" << char('a' + to_ % 8) << to_ / 8 + 1 << ")"<<std::endl;
        std::cout << "id: " << (getActionID() - 1) % 73 + 1 << std::endl;
        if(move_piece_ == Pieces::pawn){
            std::cout << "move[pawn] ";
        }else if(move_piece_ == Pieces::knight){
            std::cout << "move[knight] ";
        }else if(move_piece_ == Pieces::bishop){
            std::cout << "move[bishop] ";
        }else if(move_piece_ == Pieces::rook){
            std::cout << "move[rook] ";
        }else if(move_piece_ == Pieces::queen){
            std::cout << "move[queen] ";
        }else if(move_piece_ == Pieces::king){
            std::cout << "move[king] ";
        }
        if(taken_piece_ == Pieces::pawn){
            std::cout << "take[pawn] ";
        }else if(taken_piece_ == Pieces::knight){
            std::cout << "take[knight] ";
        }else if(taken_piece_ == Pieces::bishop){
            std::cout << "take[bishop] ";
        }else if(taken_piece_ == Pieces::rook){
            std::cout << "take[rook] ";
        }else if(taken_piece_ == Pieces::queen){
            std::cout << "take[queen] ";
        }else if(taken_piece_ == Pieces::empty){
            std::cout << "take[empty] ";
        }

        if(promotion_piece_ == Pieces::knight){
            std::cout << "promote[knight]\n";
        }else if(promotion_piece_ == Pieces::bishop){
            std::cout << "promote[bishop]\n";
        }else if(promotion_piece_ == Pieces::rook){
            std::cout << "promote[rook]\n";
        }else if(promotion_piece_ == Pieces::queen){
            std::cout << "promote[queen]\n";
        }else if(promotion_piece_ == Pieces::empty){
            std::cout << "promote[empty]\n";
        }
    }
    void setFrom(int from){ from_ = from; }
    void setTo(int to){ to_ = to; }
    
    void setMovedPiece(Pieces moved){ move_piece_ = moved; }
    void setTakenPiece(Pieces taken){ taken_piece_ = taken; }
    void setPromotedPiece(Pieces promote){ promotion_piece_ = promote; }
    
    void set00(bool shortcastle){ shortcastle_ = shortcastle; }
    void set000(bool longcastle){ longcastle_ = longcastle; }
    void setEnPassant(bool enpassant){ enpassant_ = enpassant;}
    
    bool is00() const { return shortcastle_;}
    bool is000() const { return longcastle_;}
    bool isEnPassant() const { return enpassant_;}

    int getFrom() const { return from_;}
    int getTo() const { return to_;}
    
    Pieces getMovedPiece() const { return move_piece_; }
    Pieces getTakenPiece() const { return taken_piece_; }
    Pieces getPromotedPiece() const { return promotion_piece_; }
    
private:
    Pieces move_piece_;         // King, Queen, Rook, Bishop, Knight, pawn
    Pieces taken_piece_;        // King, Queen, Rook, Bishop, Knight, pawn
    Pieces promotion_piece_;    // Queen, Rook, Bishop, Knight
    int from_;      // 0-63
    int to_;        // 0-63
    int move_id_;   // 8x8x73
    bool shortcastle_;
    bool longcastle_;
    bool enpassant_;
};

#define NUM_OF_PIECES 12    // 6 for each
#define NUM_OF_SQUARE 64    // a1b1...h1 -> a2b2...h2 -> ... -> a8b8...h8

#define PAWN 0
#define KNIGHT 1
#define BISHOP 2
#define ROOK 3
#define QUEEN 4
#define KING 5

class Hash{
public:
    Hash(){

        std::mt19937_64 generator;
        generator.seed(0);

        for(int i = 0; i < NUM_OF_PIECES; i++)
            for(int j = 0; j < NUM_OF_SQUARE; j++)
                key_[i][j] = generator();

        reset();
    }
    void reset(){

        hash_table_1.clear();
        hash_table_2.clear();

        hash_ = key_[ROOK][0] ^ key_[KNIGHT][1] ^ key_[BISHOP][2] ^ key_[QUEEN][3];
        hash_ ^= key_[ROOK][7] ^ key_[KNIGHT][6] ^ key_[BISHOP][5] ^ key_[KING][4];
        for(int i = 8; i < 16; i++)
            hash_ ^= key_[PAWN][i];

        for(int i = 48; i < 56; i++)
            hash_ ^= key_[PAWN + 6][i];
        hash_ ^= key_[ROOK + 6][56] ^ key_[KNIGHT + 6][57] ^ key_[BISHOP + 6][58] ^ key_[QUEEN + 6][59];
        hash_ ^= key_[ROOK + 6][63] ^ key_[KNIGHT + 6][62] ^ key_[BISHOP + 6][61] ^ key_[KING + 6][60];
        
        hash_table_1.insert(hash_);
    }

    int positionToRow(int position) const {
        return position / 8;
    }
    int positionToCol(int position) const {
        return position % 8;
    }
    int rowColToPosition(int row, int col) const { 
        return row * 8 + col;
    }
    
    int storeTable(std::uint64_t newhash){
        if(hash_table_1.count(newhash) == 0){
            hash_table_1.insert(newhash);
            return 1;
        }else if(hash_table_2.count(newhash) == 0){
            hash_table_2.insert(newhash);
            return 2;
        }
        return 3;
    }

    std::uint64_t applyMove(const ChessAction& action){
        int from = action.getFrom();
        int to = action.getTo();
        
        Pieces move = action.getMovedPiece();
        Pieces take = action.getTakenPiece();
        Pieces promote = action.getPromotedPiece();
        
        bool is_00 = action.is00();
        bool is_000 = action.is000();
        bool enpassant = action.isEnPassant();
        
        int turn_id = action.getPlayer() == Player::kPlayer1 ? 0 : 6;
        
        if(is_00){
            hash_ ^= key_[KING + turn_id][from] ^ key_[KING + turn_id][to];
            hash_ ^= key_[ROOK + turn_id][to + 1] ^ key_[ROOK + turn_id][to - 1];
            return hash_;
        }

        if(is_000){
            hash_ ^= key_[KING + turn_id][from] ^ key_[KING + turn_id][to];
            hash_ ^= key_[ROOK + turn_id][to - 2] ^ key_[ROOK + turn_id][to + 1];
            return hash_;
        }

        if(enpassant){
            hash_ ^= key_[PAWN + turn_id][from] ^ key_[PAWN + turn_id][to];
            hash_ ^= key_[PAWN + 6 - turn_id][rowColToPosition(positionToRow(from), positionToCol(to))];
            return hash_;
        }

        if(move == Pieces::pawn && promote == Pieces::empty){
            hash_ ^= key_[PAWN + turn_id][from] ^ key_[PAWN + turn_id][to];
        }else if(move == Pieces::knight){
            hash_ ^= key_[KNIGHT + turn_id][from] ^ key_[KNIGHT + turn_id][to];
        }else if(move == Pieces::bishop){
            hash_ ^= key_[BISHOP + turn_id][from] ^ key_[BISHOP + turn_id][to];
        }else if(move == Pieces::rook){
            hash_ ^= key_[ROOK + turn_id][from] ^ key_[ROOK + turn_id][to];
        }else if(move == Pieces::queen){
            hash_ ^= key_[QUEEN + turn_id][from] ^ key_[QUEEN + turn_id][to];
        }else if(move == Pieces::king){
            hash_ ^= key_[KING + turn_id][from] ^ key_[KING + turn_id][to];
        }

        if(take == Pieces::pawn){
            hash_ ^= key_[PAWN + 6 - turn_id][to];
        }else if(take == Pieces::knight){
            hash_ ^= key_[KNIGHT + 6 - turn_id][to];
        }else if(take == Pieces::bishop){
            hash_ ^= key_[BISHOP + 6 - turn_id][to];
        }else if(take == Pieces::rook){
            hash_ ^= key_[ROOK + 6 - turn_id][to];
        }else if(take == Pieces::queen){
            hash_ ^= key_[QUEEN + 6 - turn_id][to];
        }

        if(promote == Pieces::knight){
            if(turn_id == 0) 
                hash_ ^= key_[PAWN + turn_id][from] ^ key_[KNIGHT + turn_id][to];
            else
                hash_ ^= key_[PAWN + turn_id][from] ^ key_[KNIGHT + turn_id][to];
        }else if(promote == Pieces::bishop){
            if(turn_id == 0) 
                hash_ ^= key_[PAWN + turn_id][from] ^ key_[BISHOP + turn_id][to];
            else
                hash_ ^= key_[PAWN + turn_id][from] ^ key_[BISHOP + turn_id][to];
        }else if(promote == Pieces::rook){
            if(turn_id == 0) 
                hash_ ^= key_[PAWN + turn_id][from] ^ key_[ROOK + turn_id][to];
            else
                hash_ ^= key_[PAWN + turn_id][from] ^ key_[ROOK + turn_id][to];
        }else if(promote == Pieces::queen){
            if(turn_id == 0) 
                hash_ ^= key_[PAWN + turn_id][from] ^ key_[QUEEN + turn_id][to];
            else
                hash_ ^= key_[PAWN + turn_id][from] ^ key_[QUEEN + turn_id][to];
        }
        
        return hash_;
    }
private:
    std::uint64_t hash_;
    std::uint64_t key_[NUM_OF_PIECES][NUM_OF_SQUARE];
    std::unordered_set<std::uint64_t> hash_table_1;
    std::unordered_set<std::uint64_t> hash_table_2;
};

static const std::pair<int, int> kActionIdDirections56[] = {
    {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1},
    {0, 2}, {2, 2}, {2, 0}, {2, -2}, {0, -2}, {-2, -2}, {-2, 0}, {-2, 2},
    {0, 3}, {3, 3}, {3, 0}, {3, -3}, {0, -3}, {-3, -3}, {-3, 0}, {-3, 3},
    {0, 4}, {4, 4}, {4, 0}, {4, -4}, {0, -4}, {-4, -4}, {-4, 0}, {-4, 4},
    {0, 5}, {5, 5}, {5, 0}, {5, -5}, {0, -5}, {-5, -5}, {-5, 0}, {-5, 5},
    {0, 6}, {6, 6}, {6, 0}, {6, -6}, {0, -6}, {-6, -6}, {-6, 0}, {-6, 6},
    {0, 7}, {7, 7}, {7, 0}, {7, -7}, {0, -7}, {-7, -7}, {-7, 0}, {-7, 7},
    {0, 8}, {8, 8}, {8, 0}, {8, -8}, {0, -8}, {-8, -8}, {-8, 0}, {-8, 8}};

static const std::pair<int, int> kActionIdDirections64[] = {
    {1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};

static const std::pair<int, Pieces> kActionIdDirections73[] = {
    {-1, Pieces::rook}, {-1, Pieces::bishop}, {-1, Pieces::knight},
    { 0, Pieces::rook}, { 0, Pieces::bishop}, { 0, Pieces::knight},
    { 1, Pieces::rook}, { 1, Pieces::bishop}, { 1, Pieces::knight}};

class ChessEnv : public BaseEnv<ChessAction> {
public:
    ChessEnv() { reset(); }
    void reset() override
    {
        // TODO
        turn_ = Player::kPlayer1;
        actions_.clear();
                      // x  p   x  n  b  r   x   x   x  q  k
        int remain[5] = {8, 2, 2, 2, 1};
        setRemain(remain, remain);
        
        board_.resize(kChessBoardSize * kChessBoardSize);
        
        for(int i = 0; i < 16; i++){
            board_[i].first = Player::kPlayer1;
            if(i >= 8)
                board_[i].second = Pieces::pawn;
            else if(i == 0 || i == 7)
                board_[i].second = Pieces::rook;
            else if(i == 1 || i == 6)
                board_[i].second = Pieces::knight;
            else if(i == 2 || i == 5)
                board_[i].second = Pieces::bishop;
            else if(i == 3)
                board_[i].second = Pieces::queen;
            else if(i == 4)
                board_[i].second = Pieces::king;
        }
        for(int i=16; i<48; i++){
            board_[i].first = Player::kPlayerNone;
            board_[i].second = Pieces::empty;
        }
        for(int i = 48; i < 64; i++){
            board_[i].first = Player::kPlayer2;
            if (i < 56)
                board_[i].second = Pieces::pawn;
            else if(i == 56 || i == 63)
                board_[i].second = Pieces::rook;
            else if(i == 57 || i == 62)
                board_[i].second = Pieces::knight;
            else if(i == 58 || i == 61)
                board_[i].second = Pieces::bishop;
            else if(i == 59)
                board_[i].second = Pieces::queen;
            else if(i == 60)
                board_[i].second = Pieces::king;
        }

        setInt(0, 0, 0, 4, 60);
        setBoolean(false, false, false, false, false, false, false, false, false, false);
        setBitboard(0x8100000081000081, 0x0000810081810000, 0x0081000000008100, 0x4242424242424242, 0x0303030303030303, 0xC0C0C0C0C0C0C0C0);
        clearBitboardHistory();
        board_hash_.reset();
    }
    
    void setBoard(std::vector<std::pair<Player, Pieces>> board){
        board_.resize(kChessBoardSize * kChessBoardSize);
        for(int i = 0; i < 64; i++){
            board_[i].first = board[i].first;
            board_[i].second = board[i].second;
        }
    }

    void setBitboard(std::uint64_t rooks, std::uint64_t bishops, std::uint64_t knights, std::uint64_t pawns, std::uint64_t ply1, std::uint64_t ply2){
        rooks_.setboard(rooks);
        bishops_.setboard(bishops);
        knights_.setboard(knights);
        pawns_.setboard(pawns);
        ply1_pieces_.setboard(ply1);
        ply2_pieces_.setboard(ply2);
    }

    void setRemain(int ply1[], int ply2[]){
        for(int i = 0; i < 5; i++){
            ply1_remain_[i] = ply1[i];
            ply2_remain_[i] = ply2[i];
        }
    }

    void setBoolean(bool ply1_k, bool ply1_kr, bool ply1_qr, bool ply1_check, bool ply1_insuff,
                    bool ply2_k, bool ply2_kr, bool ply2_qr, bool ply2_check, bool ply2_insuff){
        ply1_k_moved_ = ply1_k;
        ply1_kr_moved_ = ply1_kr;
        ply1_qr_moved_ = ply1_qr;
        ply1_ischecked_ = ply1_check;
        ply1_insuffi_ = ply1_insuff;
        
        ply2_k_moved_ = ply2_k;
        ply2_kr_moved_ = ply2_kr;
        ply2_qr_moved_ = ply2_qr;
        ply2_ischecked_ = ply2_check;
        ply2_insuffi_ = ply2_insuff;
    }

    void setInt(int fifty1, int fifty2, int three_rep, int ply1_k_pos, int ply2_k_pos){
        fifty_steps_1_ = fifty1;
        fifty_steps_2_ = fifty2;
        repetitions_ = three_rep;
        ply1_king_pos_ = ply1_k_pos;
        ply2_king_pos_ = ply2_k_pos;
    }
    void clearBitboardHistory(){
        pawns_history_.clear();
        knights_history_.clear();
        bishops_history_.clear();
        rooks_history_.clear();
        queens_history_.clear();
        king_history_.clear();
    }
    bool rowOutOfBoard(int row) const { return row < 0 || row >= 8; }
    bool colOutOfBoard(int col) const { return col < 0 || col >= 8; }
    bool outOfBoard(int square) const { return square < 0 || square >= 64; }
    bool outOfBoard(int row, int col) const { return rowOutOfBoard(row) || colOutOfBoard(col); }
    int positionToRow(int position) const {
        assert(!outOfBoard(position));
        return position / 8;
    }
    int positionToCol(int position) const {
        assert(!outOfBoard(position));
        return position % 8;
    }
    int rowColToPosition(int row, int col) const { return row * 8 + col; }
    int toBitBoardSquare(int row, int col) const { return row + 8 * col; }
    int toBitBoardSquare(int position) const { return toBitBoardSquare(positionToRow(position), positionToCol(position)); }
    
    bool squareIsAttack(Player ply, int position, bool check_kings_attack) const ;
    bool PlyIsCheck(Player ply) const {
        assert(ply != Player::kPlayerNone);
        if(ply == Player::kPlayer1)
            return squareIsAttack(ply, ply1_king_pos_, false);
        else
            return squareIsAttack(ply, ply2_king_pos_, false);
    }

    void checkRemain() const {
        /*if(turn_ == Player::kPlayer2){
            std::cout << "after white's move\n";
            std::cout << "white (pawn, knight, bishop, rook, queen): " << 
                ply1_remain_[PAWN] << ", " << ply1_remain_[KNIGHT]<< ", " << ply1_remain_[BISHOP] << ", " <<
                ply1_remain_[ROOK]<< ", " << ply1_remain_[QUEEN] << std::endl;
            std::cout << "black (pawn, knight, bishop, rook, queen): " << 
                ply2_remain_[PAWN] << ", " << ply2_remain_[KNIGHT]<< ", " << ply2_remain_[BISHOP] << ", " <<
                ply2_remain_[ROOK]<< ", " << ply2_remain_[QUEEN] << std::endl;
        }else{
            std::cout << "after black's move\n";
            std::cout << "white (pawn, knight, bishop, rook, queen): " << 
                ply1_remain_[PAWN] << ", " << ply1_remain_[KNIGHT]<< ", " << ply1_remain_[BISHOP] << ", " <<
                ply1_remain_[ROOK]<< ", " << ply1_remain_[QUEEN] << std::endl;
            std::cout << "black (pawn, knight, bishop, rook, queen): " << 
                ply2_remain_[PAWN] << ", " << ply2_remain_[KNIGHT]<< ", " << ply2_remain_[BISHOP] << ", " <<
                ply2_remain_[ROOK]<< ", " << ply2_remain_[QUEEN] << std::endl;
        }*/
        for(int i = 0; i < 5; i++){
            assert(ply1_remain_[i] >= 0);
            assert(ply2_remain_[i] >= 0);
        }
    }
    void checkBitboard() const {
        /*if(pawns_.countPawns() != (ply1_remain_[PAWN] + ply2_remain_[PAWN])){
            std::cout << "pawns.count: " << pawns_.countPawns() <<std::endl;
            std::cout << "remain pawns (w/b): " << ply1_remain_[PAWN] << ", " << ply2_remain_[PAWN] << std::endl;
            pawns_.showBitboard();
            std::cout << "\n-- Current Board --\n" << toString() << std::endl;
        }*/
        assert(pawns_.countPawns() == (ply1_remain_[PAWN] + ply2_remain_[PAWN]));
        assert(knights_.count() == (ply1_remain_[KNIGHT] + ply2_remain_[KNIGHT]));
        assert(bishops_.count() == (ply1_remain_[BISHOP] + ply1_remain_[QUEEN] + ply2_remain_[BISHOP] + ply2_remain_[QUEEN]));
        assert(rooks_.count() == (ply1_remain_[ROOK] + ply1_remain_[QUEEN] + ply2_remain_[ROOK] + ply2_remain_[QUEEN]));
    }
    void showRemain() const {
        std::cout << "White: ";
        for(int i = 0; i < 5 ; i ++){
            std::cout << ply1_remain_[i] << ' ';
        }
        std::cout << "\nBlack: ";
        for(int i = 0; i < 5 ; i ++){
            std::cout << ply2_remain_[i] << ' ';
        }
        std::cout << std::endl;
    }
    bool act(const ChessAction& action) override
    {
        int from = action.getFrom();
        int to = action.getTo();
        
        board_[to].first = turn_;
        board_[to].second = action.getPromotedPiece() == Pieces::empty ? board_[from].second : action.getPromotedPiece();
        board_[from].first = Player::kPlayerNone;
        board_[from].second = Pieces::empty;
        
        if(action.is00()){
            // std::cout << "shortcastle\n";
            board_[from + 1].first = turn_;
            board_[from + 1].second = Pieces::rook;
            board_[from + 3].first = Player::kPlayerNone;
            board_[from + 3].second = Pieces::empty;
        }else if(action.is000()){
            // std::cout << "longcastle\n";
            board_[from - 1].first = turn_;
            board_[from - 1].second = Pieces::rook;
            board_[from - 4].first = Player::kPlayerNone;
            board_[from - 4].second = Pieces::empty;
        }

        if(action.isEnPassant()){
            // std::cout << "enpassant\n";
            assert(action.getTakenPiece() == Pieces::pawn);
            board_[rowColToPosition(positionToRow(from), positionToCol(to))].first = Player::kPlayerNone;
            board_[rowColToPosition(positionToRow(from), positionToCol(to))].second = Pieces::empty; 
        }

        if(action.getTakenPiece() == Pieces::king){
            std::cout << "King is taken ~^^" << std::endl ;
            action.showActionInfo();
            assert(action.getTakenPiece() != Pieces::king);
        }

        if(action.getTakenPiece() == Pieces::empty && action.getMovedPiece() != Pieces::pawn){
            if(turn_ == Player::kPlayer1)
                fifty_steps_1_++;
            else 
                fifty_steps_2_++;
        }else{
            if(turn_ == Player::kPlayer1)
                fifty_steps_1_ = 0;
            else 
                fifty_steps_2_ = 0;
        }

        if(turn_ == Player::kPlayer1){
            // std::cout << "turn = white\n";
            if(!ply1_kr_moved_ && action.getMovedPiece() == Pieces::rook && from == 7)
                ply1_kr_moved_ = true;
            if(!ply1_qr_moved_ && action.getMovedPiece() == Pieces::rook && from == 0)
                ply1_qr_moved_ = true;
            if(action.getTakenPiece() == Pieces::rook){
                if(!ply2_kr_moved_ && to == 63)
                    ply2_kr_moved_ = true;
                if(!ply2_qr_moved_ && to == 56)
                    ply2_qr_moved_ = true;
            }
        }else{
            // std::cout << "turn = black\n";
            if(!ply2_kr_moved_ && action.getMovedPiece() == Pieces::rook && from == 63)
                ply2_kr_moved_ = true;
            if(!ply2_qr_moved_ && action.getMovedPiece() == Pieces::rook && from == 56)
                ply2_qr_moved_ = true;
            if(action.getTakenPiece() == Pieces::rook){
                if(!ply1_kr_moved_ && to == 7)
                    ply1_kr_moved_ = true;
                if(!ply1_qr_moved_ && to == 0)
                    ply1_qr_moved_ = true;
            }
        }

        if(turn_ == Player::kPlayer1 && action.getMovedPiece() == Pieces::king){
            ply1_k_moved_ = true;
            ply1_king_pos_ = to;
        }else if(turn_ == Player::kPlayer2 && action.getMovedPiece() == Pieces::king){
            ply2_k_moved_ = true;
            ply2_king_pos_ = to;
        }
        
        if(turn_ == Player::kPlayer1){
            ply1_pieces_.reset(toBitBoardSquare(from)); // to 0
            ply1_pieces_.set(toBitBoardSquare(to)); // to 1 (K)
            if(action.is00()){
                ply1_pieces_.set(toBitBoardSquare(from + 1)); // to 1 (R)
                ply1_pieces_.reset(toBitBoardSquare(from + 3)); // to 0
                rooks_.set(toBitBoardSquare(from + 1));
                rooks_.reset(toBitBoardSquare(from + 3));
            }else if(action.is000()){
                ply1_pieces_.set(toBitBoardSquare(from - 1));
                ply1_pieces_.reset(toBitBoardSquare(from - 4));
                rooks_.set(toBitBoardSquare(from - 1));
                rooks_.reset(toBitBoardSquare(from - 4));
            }
            if(action.isEnPassant()){
                pawns_.reset(toBitBoardSquare(4, positionToCol(to)));
                ply2_pieces_.reset(toBitBoardSquare(4, positionToCol(to)));
            }else if(action.getTakenPiece() != Pieces::empty)
                ply2_pieces_.reset(toBitBoardSquare(to));
            
            for(int i = 0; i < kChessBoardSize; i++)
                pawns_.reset(toBitBoardSquare(0, i));
        }else{
            ply2_pieces_.reset(toBitBoardSquare(from)); // to 0
            ply2_pieces_.set(toBitBoardSquare(to)); // to 1 (K)
            if(action.is00()){
                ply2_pieces_.set(toBitBoardSquare(from + 1)); // to 1 (R)
                ply2_pieces_.reset(toBitBoardSquare(from + 3)); // to 0
                rooks_.set(toBitBoardSquare(from + 1));
                rooks_.reset(toBitBoardSquare(from + 3));
            }else if(action.is000()){
                ply2_pieces_.set(toBitBoardSquare(from - 1));
                ply2_pieces_.reset(toBitBoardSquare(from - 4));
                rooks_.set(toBitBoardSquare(from - 1));
                rooks_.reset(toBitBoardSquare(from - 4));
            }
            
            if(action.isEnPassant()){
                pawns_.reset(toBitBoardSquare(3, positionToCol(to)));
                ply1_pieces_.reset(toBitBoardSquare(3, positionToCol(to)));
            }else if(action.getTakenPiece() != Pieces::empty)
                ply1_pieces_.reset(toBitBoardSquare(to));
            
            for(int i = 0; i < kChessBoardSize; i++)
                pawns_.reset(toBitBoardSquare(7, i));
        }

        if(action.getTakenPiece() == Pieces::pawn){
            if(action.isEnPassant())
                pawns_.reset(toBitBoardSquare(positionToRow(from), positionToCol(to)));
            else
                pawns_.reset(toBitBoardSquare(to));
            if(turn_ == Player::kPlayer2)
                ply1_remain_[PAWN] -= 1;
            else
                ply2_remain_[PAWN] -= 1;
        }else if(action.getTakenPiece() == Pieces::bishop || action.getTakenPiece() == Pieces::queen){
            bishops_.reset(toBitBoardSquare(to));
            if(action.getTakenPiece() == Pieces::bishop){
                if(turn_ == Player::kPlayer2)
                    ply1_remain_[BISHOP] -= 1;
                else
                    ply2_remain_[BISHOP] -= 1;
            }else{
                if(turn_ == Player::kPlayer2)
                    ply1_remain_[QUEEN] -= 1;
                else
                    ply2_remain_[QUEEN] -= 1;
            }
        }else if(action.getTakenPiece() == Pieces::knight){
            knights_.reset(toBitBoardSquare(to));
            if(turn_ == Player::kPlayer2)
                ply1_remain_[KNIGHT] -= 1;
            else
                ply2_remain_[KNIGHT] -= 1;
        }
        if(action.getTakenPiece() == Pieces::rook || action.getTakenPiece() == Pieces::queen){
            rooks_.reset(toBitBoardSquare(to));
            if(action.getTakenPiece() == Pieces::rook){
                if(turn_ == Player::kPlayer2)
                    ply1_remain_[ROOK] -= 1;
                else
                    ply2_remain_[ROOK] -= 1;
            }
        }

        if(action.getMovedPiece() == Pieces::pawn){
            pawns_.reset(toBitBoardSquare(from));
            if(action.getPromotedPiece() != Pieces::empty){
                if(turn_ == Player::kPlayer1)
                    ply1_remain_[PAWN] -= 1;
                else
                    ply2_remain_[PAWN] -= 1;
                    
            }
            if(action.getPromotedPiece() == Pieces::empty)
                pawns_.set(toBitBoardSquare(to));
            else if(action.getPromotedPiece() == Pieces::bishop || action.getPromotedPiece() == Pieces::queen){
                bishops_.set(toBitBoardSquare(to));
                if(action.getPromotedPiece() == Pieces::bishop){
                    if(turn_ == Player::kPlayer1)
                        ply1_remain_[BISHOP] += 1;
                    else
                        ply2_remain_[BISHOP] += 1;
                }
            }
            if (action.getPromotedPiece() == Pieces::rook || action.getPromotedPiece() == Pieces::queen){
                rooks_.set(toBitBoardSquare(to));
                if(action.getPromotedPiece() == Pieces::rook){
                    if(turn_ == Player::kPlayer1)
                        ply1_remain_[ROOK] += 1;
                    else
                        ply2_remain_[ROOK] += 1;
                }else{
                    if(turn_ == Player::kPlayer1)
                        ply1_remain_[QUEEN] += 1;
                    else
                        ply2_remain_[QUEEN] += 1;
                }
            }
            if(action.getPromotedPiece() == Pieces::knight){
                knights_.set(toBitBoardSquare(to));
                if(turn_ == Player::kPlayer1)
                    ply1_remain_[KNIGHT] += 1;
                else
                    ply2_remain_[KNIGHT] += 1;
            }
            if(positionToRow(to) - positionToRow(from) == 2 && turn_ == Player::kPlayer1)
                pawns_.set(toBitBoardSquare(7, positionToCol(to))); // ply2 can enpassant
            else if(positionToRow(to) - positionToRow(from) == -2 && turn_ == Player::kPlayer2){
                pawns_.set(toBitBoardSquare(0, positionToCol(to)));
            }

        }else if(action.getMovedPiece() == Pieces::bishop || action.getMovedPiece() == Pieces::queen){
            bishops_.reset(toBitBoardSquare(from));
            bishops_.set(toBitBoardSquare(to));
        }else if(action.getMovedPiece() == Pieces::knight){
            knights_.reset(toBitBoardSquare(from));
            knights_.set(toBitBoardSquare(to));
        }
        if(action.getMovedPiece() == Pieces::rook || action.getMovedPiece() == Pieces::queen){
            rooks_.reset(toBitBoardSquare(from));
            rooks_.set(toBitBoardSquare(to));
        }

        /*for(int i = 0; i < 5 ; i ++){
            if(ply1_remain_[i] < 0){
                // std::cout << toString() << std::endl;
                action.showActionInfo();
                std::cout << "ply1_remain[" << i << "] < 0\n";
                assert(ply1_remain_[i] >= 0);
            }
            if(ply2_remain_[i] < 0){
                std::cout << toString() << std::endl;
                action.showActionInfo();
                std::cout << "ply2_remain[" << i << "] < 0\n";
                assert(ply2_remain_[i] >= 0);
            }
        }*/
        checkRemain();

        if(ply1_remain_[PAWN] > 0 || ply1_remain_[ROOK] > 0 || ply1_remain_[QUEEN] > 0 || 
           ply1_remain_[BISHOP] >= 2 || ((ply1_remain_[BISHOP] > 0 && ply1_remain_[KNIGHT]  > 0)) ||
           ply1_remain_[KNIGHT] >= 2){
            ply1_insuffi_ = false;
        }else{
            ply1_insuffi_ = true;
        }

        if(ply2_remain_[PAWN] > 0 || ply2_remain_[ROOK] > 0 || ply2_remain_[QUEEN] > 0 || 
           ply2_remain_[BISHOP] >= 2 || ((ply2_remain_[BISHOP] > 0 && ply2_remain_[KNIGHT]  > 0)) ||
           ply2_remain_[KNIGHT] >= 2){
            ply2_insuffi_ = false;
        }else{
            ply2_insuffi_ = true;
        }

        if(PlyIsCheck(Player::kPlayer1)){
            if(turn_ == Player::kPlayer1)
                return false;
            ply1_ischecked_ = true;
            // std::cout << "------white is check------\n";
            // action.showActionInfo();
            // std::cout << "--------------------------\n";
        }else{
            ply1_ischecked_ = false;
        }
        
        if(PlyIsCheck(Player::kPlayer2)){
            if(turn_ == Player::kPlayer2)
                return false;
            ply2_ischecked_ = true;
        }else{
            ply2_ischecked_ = false;
        }

        int repeat = board_hash_.storeTable(board_hash_.applyMove(action));
        if(repetitions_ < repeat){
            repetitions_ = repeat;
        }

        checkBitboard();
        
        if(turn_ == Player::kPlayer1){
            pawns_history_.push_back(ply1_pieces_ & pawns_);
            knights_history_.push_back(ply1_pieces_ & knights_);
            bishops_history_.push_back((ply1_pieces_ & bishops_) - (bishops_ & rooks_));
            rooks_history_.push_back((ply1_pieces_ & rooks_) - (bishops_ & rooks_));
            queens_history_.push_back(ply1_pieces_ & (bishops_ & rooks_));
            king_history_.push_back(ply1_king_pos_);
        }else{
            pawns_history_.push_back(ply2_pieces_ & pawns_);
            knights_history_.push_back(ply2_pieces_ & knights_);
            bishops_history_.push_back((ply2_pieces_ & bishops_) - (bishops_ & rooks_));
            rooks_history_.push_back((ply2_pieces_ & rooks_) - (bishops_ & rooks_));
            queens_history_.push_back(ply2_pieces_ & (bishops_ & rooks_));
            king_history_.push_back(ply2_king_pos_);
        }
        
        actions_.push_back(action);
        turn_ = action.nextPlayer();

        // std::cout << ToString();

        for(int i = 0; i < 64; i++){
            if(board_[i].second == Pieces::empty)
                assert(board_[i].first == Player::kPlayerNone);
            if(board_[i].first == Player::kPlayerNone)
                assert(board_[i].second == Pieces::empty);
        }
        return true;
    }

    bool act(const std::vector<std::string>& action_string_args) override
    {
        // TODO
        assert(action_string_args.size() == 2);
        assert(action_string_args[0].size() == 1 && (charToPlayer(action_string_args[0][0]) == Player::kPlayer1 || charToPlayer(action_string_args[0][0]) == Player::kPlayer2));
        turn_ = charToPlayer(action_string_args[0][0]);
        std::string move_str = action_string_args[1];
        int i;
        for(i = 0; i < 1858; i++){
            if(move_str == kAllMoves[i])
                break;
        }
        if(i == 1858)
            return false;
        Pieces move, taken, promote = Pieces::empty;
        // promote: q n b r
        
        if(move_str.size() == 5){
            if(move_str[4] == 'q')
                promote = Pieces::queen;
            else if(move_str[4] == 'r')
                promote = Pieces::rook;
            else if(move_str[4] == 'b')
                promote = Pieces::bishop;
            else if(move_str[4] == 'n')
                promote = Pieces::knight;
        }else{
            promote = Pieces::empty;
        }
        
        int from_col = move_str[0] - 'a';
        int from_row = move_str[1] - '1';
        int to_col = move_str[2] - 'a';
        int to_row = move_str[3] - '1';

        int from = rowColToPosition(from_row, from_col);
        int to = rowColToPosition(to_row, to_col);

        move = board_[from].second;
        taken = board_[to].second;

        // std::cout << turn_ << from << to << move << taken << promote << endl;
        if(move == Pieces::pawn && ((turn_ == Player::kPlayer1 && from_row == 6) || (turn_ == Player::kPlayer2 && from_row == 1)))
            promote = Pieces::queen;

        ChessAction action(turn_, from, to, move, taken, promote, -1);

        if(move == Pieces::king){
            if((turn_ == Player::kPlayer1 && move_str == "e1g1") || (turn_ == Player::kPlayer2 && move_str == "e8g8")){
                action.set00(true);
            }else if((turn_ == Player::kPlayer1 && move_str == "e1c1") || (turn_ == Player::kPlayer2 && move_str == "e8c8")){
                action.set000(true);
            }
        }else if(move == Pieces::pawn){
            if(turn_ == Player::kPlayer1){
                if(to_row <= from_row)
                    return false;
                if(from_col != to_col && taken == Pieces::empty){
                    
                    std::cout << std::endl;
                    if(from_row == 4 && board_[rowColToPosition(from_row, to_col)].first == Player::kPlayer2 && board_[rowColToPosition(from_row, to_col)].second == Pieces::pawn){
                        taken = Pieces::pawn;
                        action.setEnPassant(true);
                    }else{
                        return false;
                    }
                }
            }else{
                if(to_row >= from_row)
                    return false;
                if(from_col != to_col && taken == Pieces::empty){
                    if(from_row == 3 && board_[rowColToPosition(from_row, to_col)].first == Player::kPlayer1 && board_[rowColToPosition(from_row, to_col)].second == Pieces::pawn){
                        taken = Pieces::pawn;
                        action.setEnPassant(true);
                    }else{
                        return false;
                    }
                }
            }
        }
        if (!isLegalAction(action)) { return false; }
        return act(action);
    }
    

    std::vector<ChessAction> getLegalActions() const override
    {
        // TODO
        std::vector<ChessAction> actions;
        for(int pos = 0; pos < kChessBoardSize * kChessBoardSize; pos++){
            for(int id = 1; id <=73; id++){

                int from = pos, to = -1;
                int row = positionToRow(from), col = positionToCol(from);
                bool is_00 = false, is_000 = false, isenpassant = false;

                Pieces move = board_[from].second;
                Pieces take = Pieces::empty; 
                Pieces promote = Pieces::empty;

                int action_id = pos * 73 + id;
                
                if(id <= 56){
                    if(move == Pieces::knight)
                        continue;
                    
                    int new_row = row + kActionIdDirections56[id - 1].second;
                    int new_col = col + kActionIdDirections56[id - 1].first;
                    if(outOfBoard(new_row, new_col))
                        continue;

                    to = rowColToPosition(new_row, new_col);
                    take = board_[to].second;

                    if(move == Pieces::king){
                        if(turn_ == Player::kPlayer1 && from == 4){
                            if(to == 6){
                                is_00 = true;
                            }else if(to == 2){
                                is_000 = true;
                            }
                        }else if(turn_ == Player::kPlayer2 && from == 60){
                            if(to == 62){
                                is_00 = true;
                            }else if(to == 58){
                                is_000 = true;
                            }
                        }
                    }else if(move == Pieces::pawn && take == Pieces::empty){
                        if(kActionIdDirections56[id - 1].first == 1 || kActionIdDirections56[id - 1].first == -1){
                            if(turn_ == Player::kPlayer1 && kActionIdDirections56[id - 1].second != 1)
                                continue;
                            else if(turn_ == Player::kPlayer2 && kActionIdDirections56[id - 1].second != -1)
                                continue;
                            if((turn_ == Player::kPlayer1 && row == 4) || (turn_ == Player::kPlayer2 && row == 3)){
                                if(board_[from + kActionIdDirections56[id - 1].first].second == Pieces::pawn){
                                    if(board_[from + kActionIdDirections56[id - 1].first].first != turn_){
                                        take = Pieces::pawn;
                                        isenpassant = true;
                                    }else{
                                        continue;
                                    }
                                }
                            }    
                        }
                    }
                    if(move == Pieces::pawn && ((turn_ == Player::kPlayer1 && row == 6) || (turn_ == Player::kPlayer2 && row == 1)))
                        promote = Pieces::queen;

                }else if(id <= 64){
                    if(move != Pieces::knight)
                        continue;
                    int direction = id - 57;
                    int new_row = row + kActionIdDirections64[direction].second;
                    int new_col = col + kActionIdDirections64[direction].first;
                    if(outOfBoard(new_row, new_col))
                        continue;
                    to = rowColToPosition(new_row, new_col);
                    take = board_[to].second;
                }else{
                    if(move != Pieces::pawn)
                        continue;

                    //       l  f  r           l  f  r           l  f  r
                    // rook: 0, 3, 6   bishop: 1, 4, 7   knight: 2, 5, 8
                    
                    if ((turn_ == Player::kPlayer2 && row != 1) || (turn_ == Player::kPlayer1 && row != 6))
                        continue;
                    
                    int new_row = (turn_ == Player::kPlayer2) ? 0 : 7;
                    int new_col = col + kActionIdDirections73[id - 65].first;

                    if(outOfBoard(new_row, new_col))
                        continue;

                    to = rowColToPosition(new_row, new_col);
                    take = board_[to].second;
                    
                    promote = kActionIdDirections73[id - 65].second;
                }


                ChessAction action(turn_, from, to, move, take, promote, action_id);
                
                action.setEnPassant(isenpassant);
                action.set00(is_00);
                action.set000(is_000);

                if(isLegalAction(action)){
                    actions.push_back(action);
                }
            }
        }   
        return actions;
    }
    
    bool isLegalAction(const ChessAction& action) const override
    {
        int from = action.getFrom();
        int to = action.getTo();

        if(board_[from].first != turn_)
            return false;
        if(outOfBoard(from) || outOfBoard(to))
            return false;

        int action_id = action.getActionID();
        int id = (action_id - 1) % 73 + 1; // 1-73
        int row = positionToRow(from);
        int col = positionToCol(from);
        
        if(id <= 56){
            // std::cout << "direction: " << kActionIdDirections56[id - 1].first << ", " << kActionIdDirections56[id - 1].second << std::endl;
            if(outOfBoard(row + kActionIdDirections56[id - 1].second, col + kActionIdDirections56[id - 1].first))
                return false;
        }else if(id <= 64){
            // std::cout << "direction: " << kActionIdDirections64[id - 57].first << ", " << kActionIdDirections64[id - 57].second << std::endl;
            if(outOfBoard(row + kActionIdDirections64[id - 57].second, col + kActionIdDirections64[id - 57].first))
                return false;
        }else{
            if(outOfBoard(row, col + kActionIdDirections73[id - 65].first))
                return false;
        }
        Pieces move = action.getMovedPiece();
        if(move == Pieces::empty)
            return false;

        Pieces take = action.getTakenPiece();
        if(take != Pieces::empty){
            if(turn_ == board_[to].first)
                return false;
        }

        Pieces promote = action.getPromotedPiece();

        bool is_00 = action.is00();
        bool is_000 = action.is000();
        bool isenpassant = action.isEnPassant();
        
        if((is_00 || is_000) && move != Pieces::king)
            return false;
        
        if(isenpassant && move != Pieces::pawn)
            return false;
        
        if(move != Pieces::pawn && (promote != Pieces::empty || id > 64))
            return false;
        
        if(move != Pieces::knight && (id <= 64 && id > 56))
            return false;

        if(move == Pieces::pawn){
            if(id <= 8){
                if(id == 3 || id == 7)
                    return false;
                // wrong moving direction
                if(turn_ == Player::kPlayer1 && kActionIdDirections56[id - 1].second < 0)
                    return false;
                if(turn_ == Player::kPlayer2 && kActionIdDirections56[id - 1].second > 0)
                    return false;
                
                if(kActionIdDirections56[id - 1].first == 0){   // move forward
                    if(take != Pieces::empty)
                        return false;
                }else{  // take
                    if(isenpassant){
                        // std::cout << "enpassant\n";
                        // pawns_(0, to.col): player1 can enpassant
                        // pawns_(7, to.col): player2 can enpassant
                        if(turn_ == Player::kPlayer1 && !pawns_.get(toBitBoardSquare(0, positionToCol(to))))
                            return false;
                        if(turn_ == Player::kPlayer2 && !pawns_.get(toBitBoardSquare(7, positionToCol(to))))
                            return false;

                        if(board_[to].first != Player::kPlayerNone || board_[to].second != Pieces::empty)
                            return false;
                        if(board_[rowColToPosition(row, positionToCol(to))].first == turn_ || board_[rowColToPosition(row, positionToCol(to))].second != Pieces::pawn)
                            return false;

                    }else if(board_[to].first == turn_ || board_[to].second == Pieces::empty)
                        return false;
                }
            }else if(id == 9 || id == 13){ // 8: {0, 2}  12: {0, -2}
                if(turn_ == Player::kPlayer1){
                    if(row != 1 || id != 9)
                        return false;
                    assert(to == (from + 16));
                    if(board_[from + 8].second != Pieces::empty || board_[to].second != Pieces::empty)
                        return false;
                }else{
                    if(row != 6 || id != 13)
                        return false;
                    assert(to == (from - 16));
                    if(board_[from - 8].second != Pieces::empty || board_[to].second != Pieces::empty)
                        return false;
                }
            }else if(id >= 65){ // underpromotion
                
                if(turn_ == Player::kPlayer1 && row != 6)
                    return false;
                if(turn_ == Player::kPlayer2 && row != 1)
                    return false;
                
                int direction = kActionIdDirections73[id - 65].first;
                if(promote != kActionIdDirections73[id - 65].second){
                    std::cout << "assertion failed !\n";
                    if(promote == Pieces::knight)
                        std::cout << "id: " << id << ", promote: knight\n";
                    else if(promote == Pieces::rook)
                        std::cout << "id: " << id << ", promote: rook\n";
                    if(promote == Pieces::bishop)
                        std::cout << "id: " << id << ", promote: bishop\n";
                }
                assert(promote == kActionIdDirections73[id - 65].second);

                if(direction == 0 && (take != Pieces::empty || board_[to].first != Player::kPlayerNone || board_[to].second != Pieces::empty))
                    return false;

            }else{
                return false;
            }
        }else if(move == Pieces::knight){
            if(board_[to].first == turn_)
                return false;
        }else if(move == Pieces::bishop || move == Pieces::rook || move == Pieces::queen){
            if(id > 56)
                return false;
            if(move == Pieces::rook && id % 2 == 0) // diagonal
                return false; 
            if(move == Pieces::bishop && id % 2 == 1) // straight
                return false;

            if(board_[to].first == turn_)
                return false;

            int dir_row = kActionIdDirections56[id - 1].second;
            int dir_col = kActionIdDirections56[id - 1].first;
            int num_of_square = (dir_row == 0) ? dir_col : dir_row;
            
            if(dir_row < 0)
                dir_row = -1;
            else if(dir_row > 0)
                dir_row = 1;
            
            if(dir_col < 0)
                dir_col = -1;
            else if(dir_col > 0)
                dir_col = 1;
            
            num_of_square = (num_of_square < 0) ? -1 * num_of_square : num_of_square;

            for(int i = 1; i < num_of_square; i++){
                if(board_[rowColToPosition(row + dir_row * i, col + dir_col * i)].second != Pieces::empty)
                    return false;
            }
        }else if(move == Pieces::king){
            if(is_00){
                if(turn_ == Player::kPlayer1){
                    if(ply1_king_pos_ != 4)
                        return false;
                    if(ply1_ischecked_ || ply1_k_moved_ || ply1_kr_moved_)
                        return false;
                    if(board_[5].second != Pieces::empty || board_[to].second != Pieces::empty)
                        return false;
                    if(squareIsAttack(turn_, 5, true) || squareIsAttack(turn_, to, true))
                        return false;
                }else{
                    if(ply2_king_pos_ != 60)
                        return false;
                    if(ply2_ischecked_ || ply2_k_moved_ || ply2_kr_moved_)
                        return false;
                    if(board_[61].second != Pieces::empty || board_[to].second != Pieces::empty)
                        return false;
                    if(squareIsAttack(turn_, 61, true) || squareIsAttack(turn_, to, true))
                        return false;
                }
            }else if(is_000){
                if(turn_ == Player::kPlayer1){
                    if(ply1_king_pos_ != 4)
                        return false;
                    if(ply1_ischecked_ || ply1_k_moved_ || ply1_qr_moved_)
                        return false;
                    if(board_[3].second != Pieces::empty || board_[to].second != Pieces::empty)
                        return false;
                    if(squareIsAttack(turn_, 3, true) || squareIsAttack(turn_, to, true))
                        return false;
                }else{
                    if(ply2_king_pos_ != 60)
                        return false;
                    if(ply2_ischecked_ || ply2_k_moved_ || ply2_qr_moved_)
                        return false;
                    if(board_[59].second != Pieces::empty || board_[to].second != Pieces::empty)
                        return false;
                    if(squareIsAttack(turn_, 59, true) || squareIsAttack(turn_, to, true))
                        return false;
                }
            }else{
                if(id > 8)
                    return false;
                if(squareIsAttack(turn_, to, true))
                    return false;
                if(board_[to].first == turn_)
                    return false;
                /*if(from == 11 && to == 18 && board_[25].second == Pieces::pawn){
                    std::cout << "====================================\n";
                    action.showActionInfo();
                    std::cout << "====================================\n";
                }*/
                    
            }
        }
        // if(move == Pieces::king && from == 11 && to == 18 && board_[25].second == Pieces::pawn && board_[25].first != turn_)system("read -p 'Press Enter to continue...' var");
        ChessEnv AfterAction(*this);
        AfterAction.act(action);
        if(move == Pieces::king && AfterAction.PlyIsCheck(this->turn_)){
            // std::cout << "-- illegal action [king's checked] --\n";
            // action.showActionInfo();
            // std::cout << "-------------------------------------\n";
            return false;
        }
        // if(move == Pieces::king && from == 11 && to == 18 && board_[25].second == Pieces::pawn && board_[25].first != turn_)system("read -p 'Press Enter to continue...' var");
        // if(move == Pieces::king && from == 11 && to == 18 && board_[25].second == Pieces::pawn && board_[25].first != turn_)assert(move != Pieces::king);
        if(move != Pieces::king && AfterAction.PlyIsCheck(this->turn_)){
            // std::cout << "-- illegal action [king's checked] --\n";
            // action.showActionInfo();
            // std::cout << "-------------------------------------\n";
            return false;
        }

        return true;
    }
    
    bool isTerminal() const override
    {
        Player who = eval();
        if(who != Player::kPlayerNone) return true;
        else{           
            if(fifty_steps_2_ >= 50 || fifty_steps_1_ >= 50 || repetitions_ >= 3 || (ply1_insuffi_ && ply2_insuffi_)){
                // std::cout << "result: .5-.5\n";
                // std::cout << "fifty-step(white/black): " << fifty_steps_1_ << '/' << fifty_steps_2_ << std::endl;
                // std::cout << "three-repe: " << repetitions_ << std::endl;
                // showRemain();
                // std::cout << std::endl;
                // std::cout << "insuffi. (w/b): ";
                // if(ply1_insuffi_) std::cout << "true/";
                // else std::cout << "false/";
                // if(ply2_insuffi_) std::cout << "true";
                // else std::cout << "false";
                // std::cout << std::endl;
                return true;
            }else{
                std::vector<ChessAction> actions = getLegalActions();
                if(actions.size() == 0) {
                    // std::cout << "result: .5-.5\n";
                    // std::cout << "stalemate\n";
                    return true;
                }
            }
        }
        return false;
    }
    
    Player eval() const
    {
        // TODO
        if(turn_ == Player::kPlayer1 && ply1_ischecked_){
            std::vector<ChessAction> actions = getLegalActions();
            if(actions.size() == 0) {
                // std::cout << "result: 0-1\n";
                return Player::kPlayer2;
            }
        }
        if(turn_ == Player::kPlayer2 &&ply2_ischecked_){
            std::vector<ChessAction> actions = getLegalActions();
            if(actions.size() == 0){
                // std::cout << "result: 1-0\n";
                return Player::kPlayer1;
            }
        }
        return Player::kPlayerNone;
    }
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

    void setObservation(const std::vector<float>& observation) override {}

    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override
    {
        std::vector<float> vFeatures;
        for (int channel = 0; channel < 120; ++channel) {
            int ind = pawns_history_.size() - 1 - 2 * (channel / 14);
            for (int pos = 0; pos < kChessBoardSize * kChessBoardSize; ++pos) {
                if (channel < 112) {
                    // (turn_ pieces x6 + opponent pieces x6 + repetition x2) x8
                    if (((channel % 2 == 0) && ind < 0) || ((channel % 2 == 1) && ind < 1)){
                        vFeatures.push_back(0.0f);
                    }else{
                        int channel_id = channel % 14;
                        if(channel_id == 0)         // our-pawns
                            vFeatures.push_back(pawns_history_[ind].get(toBitBoardSquare(pos)) ? 1.0f : 0.0f);
                        else if(channel_id == 1)    // opp-pawns        
                            vFeatures.push_back(pawns_history_[ind - 1].get(toBitBoardSquare(pos)) ? 1.0f : 0.0f);
                        else if(channel_id == 2)    // our-knights        
                            vFeatures.push_back(knights_history_[ind].get(toBitBoardSquare(pos)) ? 1.0f : 0.0f);
                        else if(channel_id == 3)    // opp-knights         
                            vFeatures.push_back(knights_history_[ind - 1].get(toBitBoardSquare(pos)) ? 1.0f : 0.0f);
                        else if(channel_id == 4)    // our-bishops         
                            vFeatures.push_back(bishops_history_[ind].get(toBitBoardSquare(pos)) ? 1.0f : 0.0f);
                        else if(channel_id == 5)    // opp-bishops         
                            vFeatures.push_back(bishops_history_[ind - 1].get(toBitBoardSquare(pos)) ? 1.0f : 0.0f);
                        else if(channel_id == 6)    // our-rooks     
                            vFeatures.push_back(rooks_history_[ind].get(toBitBoardSquare(pos)) ? 1.0f : 0.0f);
                        else if(channel_id == 7)    // opp-rooks        
                            vFeatures.push_back(rooks_history_[ind - 1].get(toBitBoardSquare(pos)) ? 1.0f : 0.0f);
                        else if(channel_id == 8)    // our-queens        
                            vFeatures.push_back(queens_history_[ind].get(toBitBoardSquare(pos)) ? 1.0f : 0.0f);
                        else if(channel_id == 9)    // opp-queens         
                            vFeatures.push_back(queens_history_[ind - 1].get(toBitBoardSquare(pos)) ? 1.0f : 0.0f);
                        else if(channel_id == 10)   // our-king        
                            vFeatures.push_back(toBitBoardSquare(king_history_[ind]) == pos ? 1.0f : 0.0f);
                        else if(channel_id == 11)   // opp-king        
                            vFeatures.push_back(toBitBoardSquare(king_history_[ind - 1]) == pos ? 1.0f : 0.0f);
                        else { // TODO: repetition-1, repetition-2
                            if ((repetitions_ == 1 && channel_id == 12) || (repetitions_ == 2 && channel_id == 13))
                                vFeatures.push_back(0.0f);
                            else if (repetitions_ == 3 || (repetitions_ == 1 && channel_id == 13) || (repetitions_ == 2 && channel_id == 12))
                                vFeatures.push_back(1.0f);
                        }
                    }
                    
                } else if (channel == 112) {
                    // colour
                    vFeatures.push_back((turn_ == Player::kPlayer1) ? 1.0f : 0.0f);
                } else if (channel == 113) {
                    // colour
                    vFeatures.push_back((turn_ == Player::kPlayer2) ? 1.0f : 0.0f);
                } else if (channel == 114) {
                    // TODO: total move count /100
                    vFeatures.push_back((turn_ == Player::kPlayer1) ? (pawns_history_.size() + 1) / 200 : pawns_history_.size() / 200);
                } else if (channel == 115) {
                    // TODO: turn_ 0-0
                    vFeatures.push_back(actions_[actions_.size() - 1].is00() ? 1.0f : 0.0f);
                } else if (channel == 116) {
                    // TODO: turn_ 0-0-0
                    vFeatures.push_back(actions_[actions_.size() - 1].is000() ? 1.0f : 0.0f);
                } else if (channel == 117) {
                    // TODO: opponent 0-0
                    if (actions_.size() > 1)
                        vFeatures.push_back(actions_[actions_.size() - 2].is00() ? 1.0f : 0.0f);
                    else
                        vFeatures.push_back(0.0f);
                } else if (channel == 118) {
                    // TODO: opponent 0-0-0
                    if (actions_.size() > 1)
                        vFeatures.push_back(actions_[actions_.size() - 2].is000() ? 1.0f : 0.0f);
                    else
                        vFeatures.push_back(0.0f);
                } else if (channel == 119) {
                    // TODO: no progress count /50
                    vFeatures.push_back((turn_ == Player::kPlayer1) ? fifty_steps_1_ / 50 : fifty_steps_2_ / 50);
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
    

    std::string toString() const override
    {
        // TODO
        std::ostringstream oss;
        oss << "    a   b   c   d   e   f   g   h" << std::endl;
        for (int row = kChessBoardSize - 1; row >= 0; --row) {
            oss << row + 1 << " ";
            for (int col = 0; col < kChessBoardSize; ++col) {
                int id = row * kChessBoardSize + col;
                if (board_[id].first == Player::kPlayerNone) {
                    oss << " .. ";
                } else{

                    if (board_[id].first == Player::kPlayer1)
                        oss<< " w";
                    else if (board_[id].first == Player::kPlayer2)
                        oss<< " b";

                    switch (board_[id].second){
                        case Pieces::pawn:
                            oss << "p";
                            break;
                        case Pieces::knight:
                            oss << "N";
                            break;
                        case Pieces::bishop:
                            oss << "B";
                            break;
                        case Pieces::rook:
                            oss << "R";
                            break;
                        case Pieces::queen:
                            oss << "Q";
                            break;
                        case Pieces::king:
                            oss << "K";
                            break;
                        default:
                            oss << "-";
                            break;
                    }
                    oss << " ";
                }
            }
            oss << " " << row + 1 << std::endl;
        }
        oss << "    a   b   c   d   e   f   g   h" << std::endl;
        return oss.str(); 
    }

    inline std::string name() const override { return kChessName; }
private:
        
    int ply1_remain_[5];
    int ply2_remain_[5];

    int fifty_steps_1_;
    int fifty_steps_2_;
    int repetitions_;
    int total_numof_moves_;
    
    bool ply1_ischecked_;
    bool ply2_ischecked_;
    bool ply1_k_moved_;
    bool ply2_k_moved_;
    bool ply1_kr_moved_;
    bool ply2_kr_moved_;
    bool ply1_qr_moved_;
    bool ply2_qr_moved_;
    bool ply1_insuffi_;
    bool ply2_insuffi_;

    int ply1_king_pos_;
    int ply2_king_pos_;
    
    Bitboard ply1_pieces_;
    Bitboard ply2_pieces_;
    Bitboard rooks_; // R + Q
    Bitboard bishops_; // B + Q
    Bitboard knights_;
    Bitboard pawns_;
    
    Hash board_hash_;

    std::vector<std::pair<Player, Pieces>> board_;
    
    std::vector<Bitboard> pawns_history_;
    std::vector<Bitboard> knights_history_;
    std::vector<Bitboard> bishops_history_;
    std::vector<Bitboard> rooks_history_;
    std::vector<Bitboard> queens_history_;
    std::vector<int> king_history_;

};

class ChessEnvLoader : public BaseEnvLoader<ChessAction, ChessEnv> {
public:
    inline int getPolicySize() const override { return 4672; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; }
    inline std::string getEnvName() const override { return kChessName; }
};

}