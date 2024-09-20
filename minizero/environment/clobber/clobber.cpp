#include "clobber.h"
#include "random.h"
#include "sgf_loader.h"
#include <algorithm>
#include <cctype>
#include <string>

namespace minizero::env::clobber {

using namespace minizero::utils;

int ClobberAction::charToPos(char c) const
{
    c = std::toupper(c);
    if ('A' <= c && c <= 'Z' && c != 'I') {
        return c - 'A' - (c > 'I' ? 1 : 0);
    }
    assert(false);
    return -1;
}

int ClobberAction::actionStringToID(const std::vector<std::string>& action_string_args) const
{
    // Action string includes a source/from position and a destination position.
    // E.g., a10b10, the source/from position is a10 and destination position is b10.
    std::string command_str = action_string_args.back();

    // The index of the destination position in command_str.
    // For example, for a10b10 is 3; for a2b2 is 2.
    int dest_pos_idx = command_str.find_first_not_of("0123456789", 1);

    int r1_r = std::stoi(command_str.substr(1, dest_pos_idx));
    int r2_r = std::stoi(command_str.substr(dest_pos_idx + 1, (command_str.size() - (dest_pos_idx + 1))));
    int r1 = r1_r - 1;
    int r2 = r2_r - 1;

    int c1 = charToPos(command_str[0]);
    int c2 = charToPos(command_str[dest_pos_idx]);
    if (r1 < 0 || r2 < 0 || c1 < 0 || c2 < 0) {
        return -1; // simply return illegal action error
    }
    if (r1 >= board_size_ || r2 >= board_size_ || c1 >= board_size_ || c2 >= board_size_) {
        return -1; // simply return illegal action error
    }
    return coordinateToID(c1, r1, c2, r2);
}

int ClobberAction::getFromPos(int action_id) const
{
    return action_id % (board_size_ * board_size_);
}

int ClobberAction::getDestPos(int action_id) const
{
    int spatial = board_size_ * board_size_;
    int dir = action_id / spatial;
    int pos = action_id % spatial;

    int row = pos / board_size_;
    int col = pos % board_size_;

    // direction represent:
    //     dir=0: upper side
    //     dir=1: down side
    //     dir=2: left side
    //     dir=3: right side
    if (dir == 0) {
        ++row;
    } else if (dir == 1) {
        --row;
    } else if (dir == 2) {
        --col;
    } else if (dir == 3) {
        ++col;
    }

    // Dest position is out of board area.
    // It is not a legal move.
    if (col >= board_size_ || col < 0 || row >= board_size_ || row < 0) {
        return -1;
    }

    return row * board_size_ + col;
}

std::string ClobberAction::actionIDtoString(int action_id) const
{
    int pos = getFromPos(action_id);
    int row = pos / board_size_;
    int col = pos % board_size_;

    int dest_pos = getDestPos(action_id);
    int dest_row = dest_pos / board_size_;
    int dest_col = dest_pos % board_size_;

    char col_c = 'a' + col + ('a' + col >= 'i' ? 1 : 0);
    char dest_col_c = 'a' + dest_col + ('a' + dest_col >= 'i' ? 1 : 0);

    return std::string(1, col_c) + std::to_string(row + 1) + std::string(1, dest_col_c) + std::to_string(dest_row + 1);
}

int ClobberAction::coordinateToID(int c1, int r1, int c2, int r2) const
{
    // direction represent:
    //     dir=0: upper side
    //     dir=1: down side
    //     dir=2: left side
    //     dir=3: right side
    int dir = 0;
    if ((c2 - c1 == 1) && (r2 == r1)) {
        dir = 3;
    } else if ((c2 - c1 == -1) && (r2 == r1)) {
        dir = 2;
    } else if ((c2 == c1) && (r2 - r1 == 1)) {
        dir = 0;
    } else if ((c2 == c1) && (r2 - r1 == -1)) {
        dir = 1;
    } else {
        return -1; // simply return illegal action error
    }
    int from_pos = r1 * board_size_ + c1;

    return (dir * (board_size_ * board_size_)) + from_pos;
}

void ClobberEnv::reset()
{
    turn_ = Player::kPlayer1;
    actions_.clear();
    bitboard_.reset();

    // Place the 1st player's pieces.
    for (int row = 0; row < board_size_; ++row) {
        int start_col = (row % 2 == 0) ? 1 : 0;
        for (int col = start_col; col < board_size_; col += 2) {
            bitboard_.get(Player::kPlayer1).set(row * board_size_ + col);
        }
    }

    // Place the 2nd player's pieces.
    for (int row = 0; row < board_size_; ++row) {
        int start_col = (row % 2 == 0) ? 0 : 1;
        for (int col = start_col; col < board_size_; col += 2) {
            bitboard_.get(Player::kPlayer2).set(row * board_size_ + col);
        }
    }
    bitboard_history_.clear();
    bitboard_history_.push_back(bitboard_);
}

bool ClobberEnv::act(const ClobberAction& action)
{
    if (!isLegalAction(action)) { return false; }
    int pos = action.getFromPos();
    int dest_pos = action.getDestPos();

    bitboard_.get(action.getPlayer()).reset(pos);
    bitboard_.get(action.nextPlayer()).reset(dest_pos);
    bitboard_.get(action.getPlayer()).set(dest_pos);

    actions_.push_back(action);
    bitboard_history_.push_back(bitboard_);

    turn_ = action.nextPlayer();
    return true;
}

bool ClobberEnv::act(const std::vector<std::string>& action_string_args)
{
    return act(ClobberAction(action_string_args));
}

std::vector<ClobberAction> ClobberEnv::getLegalActions() const
{
    std::vector<ClobberAction> actions;
    for (int pos = 0; pos < kNumDirections * board_size_ * board_size_; ++pos) {
        ClobberAction action(pos, turn_);
        if (!isLegalAction(action)) { continue; }
        actions.push_back(action);
    }
    return actions;
}

bool ClobberEnv::isLegalAction(const ClobberAction& action) const
{
    if (action.getPlayer() != getTurn()) { return false; }
    assert(action.getActionID() >= 0 && action.getActionID() < kNumDirections * board_size_ * board_size_);

    // The piece at the source/from position should be ours.
    int pos = action.getFromPos();
    if (getPlayerAtBoardPos(pos) != action.getPlayer()) { return false; }

    // The destination position is out of board.
    int dest_pos = action.getDestPos();
    if (dest_pos == -1) { return false; }

    // The piece at the destination position should be opp's.
    // Only accept capture move.
    if (getPlayerAtBoardPos(dest_pos) == action.nextPlayer()) { return true; }

    return false;
}

bool ClobberEnv::isTerminal() const
{
    return getLegalActions().empty();
}

float ClobberEnv::getEvalScore(bool is_resign /* = false */) const
{
    Player result = (is_resign ? getNextPlayer(turn_, kClobberNumPlayer) : eval());
    switch (result) {
        case Player::kPlayer1: return 1.0f;
        case Player::kPlayer2: return -1.0f;
        default: return 0.0f;
    }
}

std::vector<float> ClobberEnv::getFeatures(utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    /* 18 channels:
        0~15. own/opponent position for last 8 turns
        16. 1st player turn
        17. 2nd player turn
    */
    int past_moves = std::min(8, static_cast<int>(bitboard_history_.size()));
    int spatial = board_size_ * board_size_;
    std::vector<float> features(getNumInputChannels() * spatial, 0.f);
    int last_idx = bitboard_history_.size() - 1;

    // 0 ~ 15
    for (int c = 0; c < 2 * past_moves; c += 2) {
        const ClobberBitboard& own_bitboard = bitboard_history_[last_idx - (c / 2)].get(turn_);
        const ClobberBitboard& opponent_bitboard = bitboard_history_[last_idx - (c / 2)].get(getNextPlayer(turn_, kClobberNumPlayer));
        for (int pos = 0; pos < spatial; ++pos) {
            int rotation_pos = getRotatePosition(pos, utils::reversed_rotation[static_cast<int>(rotation)]);
            features[pos + c * spatial] = (own_bitboard.test(rotation_pos) ? 1.0f : 0.0f);
            features[pos + (c + 1) * spatial] = (opponent_bitboard.test(rotation_pos) ? 1.0f : 0.0f);
        }
    }

    // 16 ~ 17
    for (int pos = 0; pos < spatial; ++pos) {
        features[pos + 16 * spatial] = static_cast<float>(turn_ == Player::kPlayer1);
        features[pos + 17 * spatial] = static_cast<float>(turn_ == Player::kPlayer2);
    }
    return features;
}

std::vector<float> ClobberEnv::getActionFeatures(const ClobberAction& action, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    throw std::runtime_error{"ClobberEnv::getActionFeatures() is not implemented"};
}

std::string ClobberEnv::getCoordinateString() const
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

std::string ClobberEnv::toString() const
{
    std::ostringstream oss;
    oss << " " << getCoordinateString() << std::endl;
    for (int row = board_size_ - 1; row >= 0; --row) {
        oss << (row >= 9 ? "" : " ") << row + 1 << " ";
        for (int col = 0; col < board_size_; ++col) {
            Player color = getPlayerAtBoardPos(row * board_size_ + col);
            if (color == Player::kPlayer1) {
                oss << " O ";
            } else if (color == Player::kPlayer2) {
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

Player ClobberEnv::eval() const
{
    if (getLegalActions().empty()) { return getNextPlayer(turn_, kClobberNumPlayer); }
    return Player::kPlayerNone;
}

Player ClobberEnv::getPlayerAtBoardPos(int position) const
{
    if (bitboard_.get(Player::kPlayer1).test(position)) {
        return Player::kPlayer1;
    } else if (bitboard_.get(Player::kPlayer2).test(position)) {
        return Player::kPlayer2;
    }
    return Player::kPlayerNone;
}

std::vector<float> ClobberEnvLoader::getActionFeatures(const int pos, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    throw std::runtime_error{"ClobberEnvLoader::getActionFeatures() is not implemented"};
}

} // namespace minizero::env::clobber
