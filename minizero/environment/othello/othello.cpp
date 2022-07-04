#include "othello.h"
#include "sgf_loader.h"
#include <algorithm>
#include <bitset>
#include <iostream>
#include <string>
#include <vector>

namespace minizero::env::othello {
using namespace minizero::utils;

OthelloAction::OthelloAction(const std::vector<std::string>& action_string_args)
{
    assert(action_string_args.size() == 2);
    assert(action_string_args[0].size() == 1 && (charToPlayer(action_string_args[0][0]) == Player::kPlayer1 || charToPlayer(action_string_args[0][0]) == Player::kPlayer2));
    player_ = charToPlayer(action_string_args[0][0]);
    action_id_ = SGFLoader::boardCoordinateStringToActionID(action_string_args[1], kOthelloBoardSize);
}

void OthelloEnv::reset()
{
    turn_ = Player::kPlayer1;
    actions_.clear();
    white_legal_pass = false;
    black_legal_pass = false;
    black_board.reset();
    white_board.reset();
    black_legal_board.reset(); //reset to 0
    white_legal_board.reset(); //reset to 0
    one_board.set();           //set to 1
    //initial pieces for othello
    int init_place = kOthelloBoardSize * (kOthelloBoardSize / 2 - 1) + (kOthelloBoardSize / 2 - 1); //初始黑棋位置(27)
    black_board.set(init_place, 1);
    white_board.set(init_place + 1, 1);
    white_board.set(init_place + kOthelloBoardSize, 1);
    black_board.set(init_place + kOthelloBoardSize + 1, 1);
    //initial legal board for white and black
    white_legal_board.set(init_place - 1, 1);
    white_legal_board.set(init_place - kOthelloBoardSize, 1);
    white_legal_board.set(init_place + kOthelloBoardSize + 2, 1);
    white_legal_board.set(init_place + 2 * kOthelloBoardSize + 1, 1);
    black_legal_board.set(init_place + 2, 1);
    black_legal_board.set(init_place - kOthelloBoardSize + 1, 1);
    black_legal_board.set(init_place + kOthelloBoardSize - 1, 1);
    black_legal_board.set(init_place + 2 * kOthelloBoardSize, 1);
    std::string vert_synths;
    for (int i = 0; i < kOthelloBoardSize; i++) {
        for (int j = 0; j < kOthelloBoardSize; j++) {
            (i == 0 || i == kOthelloBoardSize - 1) ? vert_synths += "0" : vert_synths += "1";
        }
    }
    std::string hori_synths;
    for (int i = 0; i < kOthelloBoardSize; i++) {
        for (int j = 0; j < kOthelloBoardSize; j++) {
            (j == 0 || j == kOthelloBoardSize - 1) ? hori_synths += "0" : hori_synths += "1";
        }
    }
    std::string allside_synths;
    for (int i = 0; i < kOthelloBoardSize; i++) {
        for (int j = 0; j < kOthelloBoardSize; j++) {
            if (i == 0 || i == kOthelloBoardSize - 1) {
                allside_synths += "0";
                continue;
            } else if (j == 0 || j == kOthelloBoardSize - 1) {
                allside_synths += "0";
                continue;
            } else {
                allside_synths += "1";
            }
        }
    }
    mask[0] = OthelloBitboard(std::string(vert_synths));    //vert_up 上
    mask[1] = OthelloBitboard(std::string(vert_synths));    //vert_dn 下
    mask[2] = OthelloBitboard(std::string(hori_synths));    //hori_lf 左
    mask[3] = OthelloBitboard(std::string(hori_synths));    //hori_rt 右
    mask[4] = OthelloBitboard(std::string(allside_synths)); //allside 斜
    mask[5] = OthelloBitboard(std::string(allside_synths)); //allside 斜
    mask[6] = OthelloBitboard(std::string(allside_synths)); //allside 斜
    mask[7] = OthelloBitboard(std::string(allside_synths)); //allside 斜
}

//return the bitset that candidate shift toward the direction
OthelloBitboard OthelloEnv::getCandidateAlongDirectionBoard(int direction, OthelloBitboard candidate)
{
    return (direction > 0) ? candidate << direction : candidate >> abs(direction);
}

//return the pieces that should be flip after the action
OthelloBitboard OthelloEnv::getFlipPoint(
    int direction, OthelloBitboard mask, OthelloBitboard moves, OthelloBitboard placed_pos, OthelloBitboard opponent_board, OthelloBitboard player_board)
{
    OthelloBitboard candidate;
    OthelloBitboard tmp_flip;
    tmp_flip.reset();
    candidate = opponent_board & (getCandidateAlongDirectionBoard(direction, placed_pos)) & mask;
    while (candidate != 0) {
        tmp_flip |= candidate;
        candidate = getCandidateAlongDirectionBoard(direction, candidate);
        moves = player_board & candidate;
        candidate = opponent_board & (candidate)&mask;
    }
    if (!moves.none()) //if move isnot empty, reach player pieces, legal to flip
    {
        return tmp_flip;
    } else {
        tmp_flip.reset();
        return tmp_flip;
    }
}

//return the candidate that can put the piece
OthelloBitboard OthelloEnv::getCanPutPoint(
    int direction, OthelloBitboard mask, OthelloBitboard moves, OthelloBitboard empty_board, OthelloBitboard opponent_board, OthelloBitboard player_board)
{
    OthelloBitboard candidate;
    OthelloBitboard test_board;
    candidate = opponent_board & getCandidateAlongDirectionBoard(direction, player_board) & mask;
    while (candidate != 0) {
        moves |= empty_board & (getCandidateAlongDirectionBoard(direction, candidate));
        candidate = opponent_board & (getCandidateAlongDirectionBoard(direction, candidate)) & mask;
    }
    return moves;
}

//set the piece and flip the relevent pieces, then update the candidate board for black and white
bool OthelloEnv::act(const OthelloAction& action)
{
    OthelloBitboard candidate;
    OthelloBitboard empty_board;
    OthelloBitboard placed_pos;     //act 下棋的位置
    OthelloBitboard opponent_board; //對手
    OthelloBitboard player_board;   //誰下的
    OthelloBitboard moves;          //合法步
    OthelloBitboard moves_2;        //合法步
    OthelloBitboard flip;           //翻棋
    OthelloBitboard tmp_flip;

    if (!isLegalAction(action)) { return false; }
    actions_.push_back(action);
    turn_ = action.nextPlayer();
    if (isPassAction(action)) { return true; }
    if (action.getPlayer() == Player::kPlayer1) {
        black_board.set(action.getActionID(), 1);
    } else {
        white_board.set(action.getActionID(), 1);
    }
    int ID = action.getActionID();
    Player player = action.getPlayer();
    if (player == Player::kPlayer1) {
        placed_pos.reset();
        placed_pos.set(ID, 1);      //下棋的位置
        player_board = black_board; //誰下的
        opponent_board = white_board;
    } else {
        placed_pos.reset();
        placed_pos.set(ID, 1);
        player_board = white_board;
        opponent_board = black_board;
    }
    int dir_step[kOthelloBoardSize] = {kOthelloBoardSize, -kOthelloBoardSize, -1, 1, kOthelloBoardSize - 1, kOthelloBoardSize + 1, -kOthelloBoardSize + 1, -kOthelloBoardSize - 1}; //上下左右、左上、右上、右下、左下

    //do action flip the points
    tmp_flip.reset();
    flip.reset();
    moves.reset();
    for (int i = 0; i < kOthelloBoardSize; i++) {
        flip |= getFlipPoint(dir_step[i], mask[i], moves, placed_pos, opponent_board, player_board);
    } //do action flip the points

    while (flip._Find_first() != flip.size()) {
        if (player == Player::kPlayer1) {
            black_board.set(flip._Find_first(), 1);
            white_board.reset(flip._Find_first());
        } else {
            white_board.set(flip._Find_first(), 1);
            black_board.reset(flip._Find_first());
        }
        flip.set(flip._Find_first(), 0);
    }
    // generate the legal bitboard
    if (player == Player::kPlayer1) {
        player_board = black_board; //誰下的
        opponent_board = white_board;
    } else {
        player_board = white_board;
        opponent_board = black_board;
    }
    empty_board = (one_board ^ (black_board | white_board));
    moves.reset(); // store the candidate of the legal bitboard
    for (int i = 0; i < kOthelloBoardSize; i++) {
        moves |= getCanPutPoint(dir_step[i], mask[i], moves, empty_board, opponent_board, player_board);
    } // generate the legal bitboard

    moves_2.reset(); // store the candidate of the legal bitboard
    for (int i = 0; i < kOthelloBoardSize; i++) {
        moves_2 |= getCanPutPoint(dir_step[i], mask[i], moves_2, empty_board, player_board, opponent_board);
    } // generate the legal bitboard
    if (player == Player::kPlayer1) {
        black_legal_board = moves;
        white_legal_board = moves_2;
    } else {
        black_legal_board = moves_2;
        white_legal_board = moves;
    }
    if (black_legal_board.none()) {
        black_legal_pass = true;
    } else {
        black_legal_pass = false;
    }
    if (white_legal_board.none()) {
        white_legal_pass = true;
    } else {
        white_legal_pass = false;
    }
    return true;
}

bool OthelloEnv::act(const std::vector<std::string>& action_string_args)
{
    return act(OthelloAction(action_string_args));
}

std::string OthelloEnv::toString() const
{
    std::ostringstream oss;
    std::string black_to_string = black_board.to_string();
    std::string white_to_string = white_board.to_string();
    std::reverse(black_to_string.begin(), black_to_string.end());
    std::reverse(white_to_string.begin(), white_to_string.end());
    oss << "   A  B  C  D  E  F  G  H" << std::endl;
    for (int row = kOthelloBoardSize - 1; row >= 0; --row) {
        oss << row + 1 << " ";
        for (int col = 0; col < kOthelloBoardSize; ++col) {
            if (black_to_string[row * kOthelloBoardSize + col] == '1') {
                oss << " O ";
            } else if (white_to_string[row * kOthelloBoardSize + col] == '1') {
                oss << " X ";
            } else {
                oss << " . ";
            }
        }
        oss << " " << row + 1 << std::endl;
    }
    oss << "   A  B  C  D  E  F  G  H" << std::endl;
    return oss.str();
}

std::vector<OthelloAction> OthelloEnv::getLegalActions() const //改判斷
{
    std::vector<OthelloAction> actions;
    actions.clear();
    for (int pos = 0; pos <= kOthelloBoardSize * kOthelloBoardSize; ++pos) {
        OthelloAction action(pos, turn_);
        if (!isLegalAction(action)) {
            continue;
        }
        actions.push_back(action);
    }
    return actions;
}

//可以落子的bit
//mask預防跑到第二行
bool OthelloEnv::isLegalAction(const OthelloAction& action) const
{
    if (action.getPlayer() == Player::kPlayer1) {
        if (action.getActionID() == kOthelloBoardSize * kOthelloBoardSize) {
            return black_legal_pass;
        }
        return black_legal_board[action.getActionID()];
    } else {
        if (action.getActionID() == kOthelloBoardSize * kOthelloBoardSize) {
            return white_legal_pass;
        }
        return white_legal_board[action.getActionID()];
    }
}
//若只剩白棋在盤面上isterminal結束
bool OthelloEnv::isTerminal() const
{
    if (black_legal_board.none() && white_legal_board.none()) return true;
    return (eval() != Player::kPlayerNone || ((black_board | white_board).all()));
}

float OthelloEnv::getEvalScore(bool is_resign /*= false*/) const
{
    Player result = (is_resign ? getNextPlayer(turn_, kOthelloNumPlayer) : eval());
    switch (result) {
        case Player::kPlayer1: return 1.0f;
        case Player::kPlayer2: return -1.0f;
        default: return 0.0f;
    }
}

Player OthelloEnv::eval() const //bit count 比大小
{
    int TotalPlayer1 = black_board.count();
    int TotalPlayer2 = white_board.count();
    if (black_legal_board.none() && white_legal_board.none()) {
        if (TotalPlayer1 > TotalPlayer2) { return Player::kPlayer1; } //1獲勝
        else if (TotalPlayer1 < TotalPlayer2) {
            return Player::kPlayer2;
        } else {
            return Player::kPlayerNone;
        }
    } else {
        return Player::kPlayerNone;
    }
}
std::vector<float> OthelloEnv::getFeatures(utils::Rotation rotation) const
{
    std::vector<OthelloBitboard> feature_bitboard_list;
    feature_bitboard_list.clear();
    // if (turn_ == Player::kPlayer1) {
    //     feature_bitboard_list.push_back(black_board);
    //     feature_bitboard_list.push_back(white_board);
    // } else {
    //     feature_bitboard_list.push_back(white_board);
    //     feature_bitboard_list.push_back(black_board);
    // }
    feature_bitboard_list.push_back(black_board);
    feature_bitboard_list.push_back(white_board);
    if (turn_ == Player::kPlayer1) {
        feature_bitboard_list.push_back(one_board);
        feature_bitboard_list.push_back(~one_board);
    } else {
        feature_bitboard_list.push_back(~one_board);
        feature_bitboard_list.push_back(one_board);
    }
    std::vector<float> cat_string;
    cat_string.clear();
    for (long unsigned int j = 0; j < feature_bitboard_list.size(); j++) {
        std::string moves_to_string = feature_bitboard_list[j].to_string('0', '1');
        for (long unsigned int i = 0; i < moves_to_string.length(); i++) {
            float tmp = float(float(moves_to_string[i]) - '0');
            cat_string.push_back(tmp);
        }
    }
    return cat_string;
}
std::vector<float> OthelloEnv::getActionFeatures(const OthelloAction& action, utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    std::vector<float> action_features(kOthelloBoardSize * kOthelloBoardSize, 0.0f);
    if (!isPassAction(action)) {
        action_features[getPositionByRotating(rotation, action.getActionID(), kOthelloBoardSize)] = 1.0f;
    }
    return action_features;
}
} // namespace minizero::env::othello