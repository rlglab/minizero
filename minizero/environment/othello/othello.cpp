#include "othello.h"
#include "random.h"
#include "sgf_loader.h"
#include <algorithm>
#include <bitset>
#include <iostream>
#include <string>
#include <vector>

namespace minizero::env::othello {
using namespace minizero::utils;

void OthelloEnv::reset()
{
    turn_ = Player::kPlayer1;
    actions_.clear();
    legal_pass_.set(false, false);
    board_.reset();
    legal_board_.reset();
    one_board_.set(); // set to 1
    // initial pieces for othello
    int init_place = board_size_ * (board_size_ / 2 - (1 - board_size_ % 2)) + (board_size_ / 2 - 1); // initial black position ex:27 when board_size_=8
    board_.get(getNextPlayer(turn_, kOthelloNumPlayer)).set(init_place + 1, 1);
    board_.get(getNextPlayer(turn_, kOthelloNumPlayer)).set(init_place + board_size_, 1);
    board_.get(turn_).set(init_place, 1);
    board_.get(turn_).set(init_place + board_size_ + 1, 1);
    // initial legal board for white and black
    legal_board_.get(getNextPlayer(turn_, kOthelloNumPlayer)).set(init_place - 1, 1);
    legal_board_.get(getNextPlayer(turn_, kOthelloNumPlayer)).set(init_place - board_size_, 1);
    legal_board_.get(getNextPlayer(turn_, kOthelloNumPlayer)).set(init_place + board_size_ + 2, 1);
    legal_board_.get(getNextPlayer(turn_, kOthelloNumPlayer)).set(init_place + 2 * board_size_ + 1, 1);
    legal_board_.get(turn_).set(init_place + 2, 1);
    legal_board_.get(turn_).set(init_place - board_size_ + 1, 1);
    legal_board_.get(turn_).set(init_place + board_size_ - 1, 1);
    legal_board_.get(turn_).set(init_place + 2 * board_size_, 1);
    // initial direction step size
    dir_step_[0] = board_size_;  // up
    dir_step_[1] = -board_size_; // down
    dir_step_[2] = -1;           // left
    dir_step_[3] = 1;            // right
    dir_step_[4] = board_size_ - 1;
    dir_step_[5] = board_size_ + 1;
    dir_step_[6] = -board_size_ + 1;
    dir_step_[7] = -board_size_ - 1;
    // initial mask
    for (int i = 0; i < board_size_; i++) {
        for (int j = 0; j < board_size_; j++) {
            (i == 0 || i == board_size_ - 1) ? mask_[0].set(i * board_size_ + j, 0) : mask_[0].set(i * board_size_ + j, 1);
            (j == 0 || j == board_size_ - 1) ? mask_[2].set(i * board_size_ + j, 0) : mask_[2].set(i * board_size_ + j, 1);
            ((i == 0 || i == board_size_ - 1) || (j == 0 || j == board_size_ - 1)) ? mask_[4].set(i * board_size_ + j, 0) : mask_[4].set(i * board_size_ + j, 1);
        }
    }
    mask_[1] = mask_[0]; // vertical mask
    mask_[3] = mask_[2]; // horizontal mask
    mask_[5] = mask_[4]; // allside mask
    mask_[6] = mask_[4];
    mask_[7] = mask_[4];
}

// return the bitset that candidate shift toward the direction
OthelloBitboard OthelloEnv::getCandidateAlongDirectionBoard(int direction, OthelloBitboard candidate)
{
    return (direction > 0) ? (candidate << direction) : (candidate >> abs(direction));
}

// return the pieces that should be flip after the action
OthelloBitboard OthelloEnv::getFlipPoint(
    int direction, OthelloBitboard mask, OthelloBitboard placed_pos, OthelloBitboard opponent_board, OthelloBitboard player_board)
{
    OthelloBitboard candidate;
    OthelloBitboard tmp_flip;
    OthelloBitboard moves;
    moves.reset();
    tmp_flip.reset();
    candidate = opponent_board & (getCandidateAlongDirectionBoard(direction, placed_pos)) & mask;
    while (candidate != 0) {
        tmp_flip |= candidate;
        candidate = getCandidateAlongDirectionBoard(direction, candidate);
        moves = player_board & candidate;
        candidate = opponent_board & (candidate)&mask;
    }
    // if move is not empty, the direction reach player's pieces which is legal to flip
    if (moves.none()) { tmp_flip.reset(); }
    return tmp_flip;
}

// return the candidate that can put the piece
OthelloBitboard OthelloEnv::getCanPutPoint(
    int direction, OthelloBitboard mask, OthelloBitboard empty_board, OthelloBitboard opponent_board, OthelloBitboard player_board)
{
    OthelloBitboard candidate;
    OthelloBitboard moves;
    moves.reset();
    candidate = opponent_board & getCandidateAlongDirectionBoard(direction, player_board) & mask;
    while (candidate != 0) {
        moves |= empty_board & (getCandidateAlongDirectionBoard(direction, candidate));
        candidate = opponent_board & (getCandidateAlongDirectionBoard(direction, candidate)) & mask;
    }
    return moves;
}

// set the piece and flip the relevent pieces, then update the candidate board for black and white
bool OthelloEnv::act(const OthelloAction& action)
{
    OthelloBitboard empty_board;
    OthelloBitboard placed_pos; // the position that action placed
    OthelloBitboard flip;       // pieces ready to flip

    if (!isLegalAction(action)) { return false; }
    actions_.push_back(action);
    turn_ = action.nextPlayer();
    if (isPassAction(action)) { return true; }

    Player player = action.getPlayer();
    board_.get(player).set(action.getActionID(), 1);
    int ID = action.getActionID();
    placed_pos.reset();
    placed_pos.set(ID, 1); // the position that action placed
    flip.reset();

    // find the pieces that can be flipped
    for (int i = 0; i < 8; i++) {
        flip |= getFlipPoint(dir_step_[i], mask_[i], placed_pos, board_.get(getNextPlayer(player, kOthelloNumPlayer)), board_.get(player));
    }

    board_.get(player) |= flip;
    board_.get(getNextPlayer(player, kOthelloNumPlayer)) &= ~flip;

    // update legal action bitboard
    empty_board = (one_board_ ^ (board_.get(Player::kPlayer1) | board_.get(Player::kPlayer2))); // places with no pieces
    legal_board_.get(Player::kPlayer1).reset();                                                 // store the candidate of the legal bitboard
    legal_board_.get(Player::kPlayer2).reset();                                                 // store the candidate of the legal bitboard
    for (int i = 0; i < 8; i++) {
        legal_board_.get(player) |= getCanPutPoint(dir_step_[i], mask_[i], empty_board, board_.get(getNextPlayer(player, kOthelloNumPlayer)), board_.get(player));
        legal_board_.get(getNextPlayer(player, kOthelloNumPlayer)) |= getCanPutPoint(dir_step_[i], mask_[i], empty_board, board_.get(player), board_.get(getNextPlayer(player, kOthelloNumPlayer)));
    } // generate the legal bitboard
    legal_pass_.get(Player::kPlayer1) = legal_board_.get(Player::kPlayer1).none();
    legal_pass_.get(Player::kPlayer2) = legal_board_.get(Player::kPlayer2).none();
    return true;
}

bool OthelloEnv::act(const std::vector<std::string>& action_string_args)
{
    return act(OthelloAction(action_string_args, board_size_));
}

std::string OthelloEnv::toString() const
{
    std::ostringstream oss;
    oss << " " << getCoordinateString() << std::endl;
    for (int row = board_size_ - 1; row >= 0; --row) {
        oss << (row >= 9 ? "" : " ") << row + 1 << " ";
        for (int col = 0; col < board_size_; ++col) {
            if (board_.get(Player::kPlayer1)[row * board_size_ + col] == 1) {
                oss << " O ";
            } else if (board_.get(Player::kPlayer2)[row * board_size_ + col] == 1) {
                oss << " X ";
            } else {
                oss << " . ";
            }
        }
        oss << (row >= 9 ? "" : " ") << row + 1 << std::endl;
    }
    oss << " " << getCoordinateString() << std::endl;
    return oss.str();
}

std::string OthelloEnv::getCoordinateString() const
{
    std::ostringstream oss;
    oss << "  ";
    for (int i = 0; i < board_size_; ++i) {
        char c = 'A' + i + ('A' + i >= 'I' ? 1 : 0);
        oss << " " + std::string(1, c) + " ";
    }
    oss << "   ";
    return oss.str();
}

std::vector<OthelloAction> OthelloEnv::getLegalActions() const
{
    std::vector<OthelloAction> actions;
    actions.clear();
    for (int pos = 0; pos <= board_size_ * board_size_; ++pos) {
        OthelloAction action(pos, turn_);
        if (!isLegalAction(action)) {
            continue;
        }
        actions.push_back(action);
    }
    return actions;
}

// if actionID is board_size_*board_size_, then it is pass
bool OthelloEnv::isLegalAction(const OthelloAction& action) const
{
    if (isPassAction(action)) {
        return legal_pass_.get(action.getPlayer());
    }
    return legal_board_.get(action.getPlayer())[action.getActionID()];
}
// both act pass then terminate
bool OthelloEnv::isTerminal() const
{
    if (actions_.size() >= 2 &&
        isPassAction(actions_.back()) &&
        isPassAction(actions_[actions_.size() - 2])) { return true; }
    return false;
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

Player OthelloEnv::eval() const
{
    int totalPlayer1 = board_.get(Player::kPlayer1).count();
    int totalPlayer2 = board_.get(Player::kPlayer2).count();
    if (legal_board_.get(Player::kPlayer1).none() && legal_board_.get(Player::kPlayer2).none()) {
        if (totalPlayer1 > totalPlayer2) { // player 1 win
            return Player::kPlayer1;
        } else if (totalPlayer1 < totalPlayer2) {
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
    std::vector<float> vFeatures;
    for (int channel = 0; channel < 4; ++channel) {
        for (int pos = 0; pos < board_size_ * board_size_; ++pos) {
            int rotation_pos = getPositionByRotating(utils::reversed_rotation[static_cast<int>(rotation)], pos, board_size_);
            if (channel == 0) {
                vFeatures.push_back((board_.get(turn_)[rotation_pos] == 1 ? 1.0f : 0.0f));
            } else if (channel == 1) {
                vFeatures.push_back((board_.get(getNextPlayer(turn_, kOthelloNumPlayer))[rotation_pos] == 1 ? 1.0f : 0.0f));
            } else if (channel == 2) {
                vFeatures.push_back((turn_ == Player::kPlayer1 ? 1.0f : 0.0f));
            } else if (channel == 3) {
                vFeatures.push_back((turn_ == Player::kPlayer2 ? 1.0f : 0.0f));
            }
        }
    }
    return vFeatures;
}

std::vector<float> OthelloEnv::getActionFeatures(const OthelloAction& action, utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    std::vector<float> action_features(board_size_ * board_size_, 0.0f);
    if (!isPassAction(action)) { action_features[getPositionByRotating(rotation, action.getActionID(), board_size_)] = 1.0f; }
    return action_features;
}

std::vector<float> OthelloEnvLoader::getActionFeatures(const int pos, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    const OthelloAction& action = action_pairs_[pos].first;
    std::vector<float> action_features(getBoardSize() * getBoardSize(), 0.0f);
    if (pos < static_cast<int>(action_pairs_.size())) {
        if (!isPassAction(action)) { action_features[getPositionByRotating(rotation, action.getActionID(), getBoardSize())] = 1.0f; }
    } else {
        int action_id = utils::Random::randInt() % (action_features.size() + 1);
        if (action_id < static_cast<int>(action_pairs_.size())) { action_features[action_id] = 1.0f; }
    }
    return action_features;
}

} // namespace minizero::env::othello
