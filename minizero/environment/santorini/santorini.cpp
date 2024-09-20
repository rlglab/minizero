#include "santorini.h"
#include "random.h"
#include "sgf_loader.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
namespace minizero::env::santorini {

using namespace minizero::utils;
using namespace std;

const int kDirection[8] = {
    -kSantoriniLetterBoxSize - 1,
    -kSantoriniLetterBoxSize,
    -kSantoriniLetterBoxSize + 1,
    -1,
    +1,
    kSantoriniLetterBoxSize - 1,
    kSantoriniLetterBoxSize,
    kSantoriniLetterBoxSize + 1,
};
const std::map<int, int> kPosistionDistanceToDirection = {
    {-kSantoriniBoardSize - 1, 0},
    {-kSantoriniBoardSize, 1},
    {-kSantoriniBoardSize + 1, 2},
    {-1, 3},
    {1, 4},
    {kSantoriniBoardSize - 1, 5},
    {kSantoriniBoardSize, 6},
    {kSantoriniBoardSize + 1, 7},
};

std::string SantoriniAction::getSquareString(int pos) const
{
    int x = pos % kSantoriniBoardSize;
    int y = pos / kSantoriniBoardSize;
    return string(1, static_cast<char>('A' + x)) + std::to_string(y + 1);
}

int SantoriniAction::encodePlaced(int x, int y) const
{
    // Transfer the (x, y) coordinate to combination index z.
    // The range of x and y are 0~24 (number of board square), z is 1~300.
    // So this function mapping (0~24, 0~24) to (1~300).
    // We assume x is greater than y and x is not equal to y.
    // In this condtion, y is equal to '[x * (49 - x) ] / 2'.
    // Then our solution is 'z = y - x = [x * ( 47 - x) / 2] + y'.
    if (x > y) { std::swap(x, y); }
    return (47 - x) * x / 2 + y;
}

std::pair<int, int> SantoriniAction::decodePlaced(int z) const
{
    // Transfer combination index z to the (x, y) coordinate; the invert function of encodePlaced().
    // Hence range of x, y, and z are as same as encodePlaced(): mapping (1~300) to (0~24, 0~24).
    // We assume x is greater than y and x is not equal to y.
    // Based on encodePlaced() result, we solve 'z = y - x' and '0 < y - x <= 24' functions.
    // The solution should be 'x <= (47 + sqrt(2401 - 8*z)/2)'. Because x is Integer, we do ceil operator.
    int x = std::ceil((47.0f - std::sqrt(2401 - 8 * z)) / 2);
    int y = z - encodePlaced(x, x) + x;
    return std::make_pair(x, y);
}

void SantoriniAction::parseAction(int action_id)
{
    if (action_id < 1600) {
        // from square: 25
        // move direction: 8
        // build direction: 8
        //
        // So we have 25 * 8 * 8 possible moves.
        int x = action_id / 64;
        int y = action_id % 64;
        from_ = positionToLetterBoxIdx(x);
        to_ = from_ + kDirection[y / 8];
        build_ = to_ + kDirection[y % 8];
    } else {
        // Place pieces at two square in the first move. All possible legal
        // moves should be equal to 2 combinations of 25.
        action_id -= 1600;
        auto [x, y] = decodePlaced(action_id + 1);
        from_ = positionToLetterBoxIdx(x);
        to_ = positionToLetterBoxIdx(y);
        build_ = -1;
    }
}

SantoriniAction::SantoriniAction(int action_id, Player player)
{
    player_ = player;
    action_id_ = action_id;
    parseAction(action_id_);
}

SantoriniAction::SantoriniAction(const std::vector<std::string>& action_string_args)
{
    assert(action_string_args.size() == 2);
    assert(action_string_args[0].size() == 1);
    player_ = charToPlayer(action_string_args[0][0]);
    assert(static_cast<int>(player_) > 0 && static_cast<int>(player_) <= kSantoriniNumPlayer); // assume kPlayer1 == 1, kPlayer2 == 2, ...

    std::string action_str = action_string_args[1];
    int from = (action_str[0] - 'A') + kSantoriniBoardSize * (action_str[1] - '1');
    int to = (action_str[2] - 'A') + kSantoriniBoardSize * (action_str[3] - '1');
    if (action_str.size() > 4) {
        int build = (action_str[4] - 'A') + kSantoriniBoardSize * (action_str[5] - '1');
        // equal to: action_id_ = from * 64 + move-direction * 8 + build-direction
        action_id_ = (from << 6) + (kPosistionDistanceToDirection.at(to - from) << 3) + kPosistionDistanceToDirection.at(build - to);
    } else {
        action_id_ = encodePlaced(from, to) + 1599;
    }
    parseAction(action_id_);
}

std::string SantoriniAction::toConsoleString() const
{
    int from = letterBoaxIdxToposition(from_);
    int to = letterBoaxIdxToposition(to_);
    if (build_ == -1) {
        return getSquareString(from) + getSquareString(to);
    }
    int build = letterBoaxIdxToposition(build_);
    return getSquareString(from) + getSquareString(to) + getSquareString(build);
}
void SantoriniEnv::reset()
{
    turn_ = Player::kPlayer1;
    actions_.clear();
    board_ = Board();
    history_.clear();
    history_.emplace_back(board_);
}

bool SantoriniEnv::act(const SantoriniAction& action)
{
    if (!isLegalAction(action)) { return false; }
    actions_.emplace_back(action);

    if (action.getBuild() == -1) {
        board_.setPlayer(turn_ == Player::kPlayer2, action.getFrom(), action.getTo());
    } else {
        board_.movePiece(action.getFrom(), action.getTo());
        board_.buildTower(action.getBuild());
    }
    turn_ = action.nextPlayer();
    history_.emplace_back(board_);
    while (history_.size() > kSantoriniHistoryLength) {
        for (int i = 0; i < kSantoriniHistoryLength; ++i) {
            history_[i] = history_[i + 1];
        }
        history_.pop_back();
    }
    return true;
}

bool SantoriniEnv::act(const std::vector<std::string>& action_string_args)
{
    return act(SantoriniAction(action_string_args));
}

std::vector<SantoriniAction> SantoriniEnv::getLegalActions() const
{
    std::vector<SantoriniAction> actions;
    for (int i = 0; i < kSantoriniPolicySize; ++i) {
        SantoriniAction action(i, turn_);
        if (!isLegalAction(action)) { continue; }
        actions.emplace_back(action);
    }
    return actions;
}

bool SantoriniEnv::isLegalAction(const SantoriniAction& action) const
{
    if (action.getActionID() < 0 || action.getActionID() >= kSantoriniPolicySize) { return false; }
    assert(action.getPlayer() == Player::kPlayer1 || action.getPlayer() == Player::kPlayer2);

    int p_id = (action.getPlayer() == Player::kPlayer2);
    auto [x, y] = board_.getPlayerIdx(p_id);

    if ((action.getActionID() >= 1600) ^ (x == 0 || y == 0)) {
        // Need to initialize the board.
        return false;
    }
    if (action.getActionID() >= 1600) {
        // Place the building on the board.
        if (action.getFrom() == action.getTo()) {
            // Can not place the two building on the same square.
            return false;
        }
        auto [ex, ey] = board_.getPlayerIdx(p_id ^ 1);
        if (ex == action.getFrom() || ex == action.getTo() || ey == action.getFrom() || ey == action.getTo()) {
            // Can not place the building on the piece.
            return false;
        } else {
            return true;
        }
    }

    Board tmp_board = board_;
    std::vector<std::pair<int, int>> move_list = tmp_board.getLegalMove(p_id);
    if (std::find(move_list.begin(), move_list.end(), std::make_pair(action.getFrom(), action.getTo())) == move_list.end()) {
        // Current move is not in the legal moves list.
        return false;
    }
    tmp_board.movePiece(action.getFrom(), action.getTo());
    std::vector<int> build_list = tmp_board.getLegalBuild(action.getTo());
    if (std::find(build_list.begin(), build_list.end(), action.getBuild()) == build_list.end()) {
        // Current building move is not in the legal moves list.
        return false;
    }
    return true;
}

bool SantoriniEnv::isTerminal() const
{
    return board_.isTerminal(turn_ == Player::kPlayer2);
}

float SantoriniEnv::getEvalScore(bool is_resign /* = false */) const
{
    if (is_resign) { // when resign, next player is winner
        if (getNextPlayer(turn_, kSantoriniNumPlayer) == Player::kPlayer1) {
            return 1.0f;
        } else {
            return -1.0f;
        }
    } else {
        if (isTerminal()) {
            if (board_.checkWin(0)) { // play1 win
                return 1.0f;
            } else if (board_.checkWin(1)) { // play2 win
                return -1.0f;
            } else { // no legal move, next player is winner
                if (getNextPlayer(turn_, kSantoriniNumPlayer) == Player::kPlayer1) {
                    return 1.0f;
                } else {
                    return -1.0f;
                }
            }
        } else {
            return 0.0f;
        }
    }
}

std::vector<float> SantoriniEnv::getFeatures(utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    /* 50 channels:
        0~47. own/opponent history for last 8 turns
            x+0. own piece
            x+1. opponent piece
            x+2~x+5. buildings
        48. black turn
        49. white turn
    */
    int spatial = board_size_ * board_size_;
    std::vector<float> features(getNumInputChannels() * spatial, 0.0f);
    int num_features_per_history = kSantoriniHistorySize * spatial;
    int h_idx = history_.size() - 1;

    // 0 ~ 47
    for (int i = 0; i < kSantoriniHistoryLength; ++i, --h_idx) {
        if (h_idx < 0) {
            break;
        }
        int own_player = turn_ == Player::kPlayer1 ? 0 : 1;
        int opp_player = turn_ == Player::kPlayer2 ? 0 : 1;

        const Board& h_board = history_[h_idx];
        auto [x, y] = h_board.getPlayerIdx(own_player);
        if (x && y) {
            x = letterBoaxIdxToposition(x);
            y = letterBoaxIdxToposition(y);
            features[i * num_features_per_history + x] = 1.0f;
            features[i * num_features_per_history + y] = 1.0f;
        }
        std::tie(x, y) = h_board.getPlayerIdx(opp_player);
        if (x && y) {
            x = letterBoaxIdxToposition(x);
            y = letterBoaxIdxToposition(y);
            features[i * num_features_per_history + spatial + x] = 1.0f;
            features[i * num_features_per_history + spatial + y] = 1.0f;
        }
        const std::array<Bitboard, 5>& planes = h_board.getPlanes();
        for (int j = 0; j < 4; ++j) {
            int base = i * num_features_per_history + (j + 2) * spatial;
            for (int idx : planes[j + 1].toList()) {
                features[base + letterBoaxIdxToposition(idx)] = 1.0f;
            }
        }
    }

    // 48 ~ 49
    for (int pos = 0; pos < spatial; ++pos) {
        features[48 * spatial + pos] = turn_ == Player::kPlayer1 ? 1.0f : 0.0f;
        features[49 * spatial + pos] = turn_ == Player::kPlayer2 ? 1.0f : 0.0f;
    }
    return features;
}

std::vector<float> SantoriniEnv::getActionFeatures(const SantoriniAction& action, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    throw std::runtime_error{"SantoriniEnv::getActionFeatures() is not implemented"};
}

std::string SantoriniEnv::toString() const
{
    return board_.toConsole();
}

std::vector<float> SantoriniEnvLoader::getActionFeatures(const int pos, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    throw std::runtime_error{"SantoriniEnvLoader::getActionFeatures() is not implemented"};
}

} // namespace minizero::env::santorini
