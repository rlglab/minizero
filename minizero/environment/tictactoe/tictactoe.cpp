#include "tictactoe.h"
#include "sgf_loader.h"
#include <algorithm>
#include <string>

namespace minizero::env::tictactoe {

using namespace minizero::utils;

TicTacToeAction::TicTacToeAction(const std::vector<std::string>& action_string_args)
{
    assert(action_string_args.size() == 2);
    assert(action_string_args[0].size() == 1 && (charToPlayer(action_string_args[0][0]) == Player::kPlayer1 || charToPlayer(action_string_args[0][0]) == Player::kPlayer2));
    player_ = charToPlayer(action_string_args[0][0]);
    action_id_ = SGFLoader::boardCoordinateStringToActionID(action_string_args[1], kTicTacToeBoardSize);
}

void TicTacToeEnv::reset()
{
    turn_ = Player::kPlayer1;
    actions_.clear();
    board_.resize(kTicTacToeBoardSize * kTicTacToeBoardSize);
    fill(board_.begin(), board_.end(), Player::kPlayerNone);
}

bool TicTacToeEnv::act(const TicTacToeAction& action)
{
    if (!isLegalAction(action)) { return false; }
    actions_.push_back(action);
    board_[action.getActionID()] = action.getPlayer();
    turn_ = action.nextPlayer();
    return true;
}

bool TicTacToeEnv::act(const std::vector<std::string>& action_string_args)
{
    return act(TicTacToeAction(action_string_args));
}

std::vector<TicTacToeAction> TicTacToeEnv::getLegalActions() const
{
    std::vector<TicTacToeAction> actions;
    for (int pos = 0; pos < kTicTacToeBoardSize * kTicTacToeBoardSize; ++pos) {
        TicTacToeAction action(pos, turn_);
        if (!isLegalAction(action)) { continue; }
        actions.push_back(action);
    }
    return actions;
}

bool TicTacToeEnv::isLegalAction(const TicTacToeAction& action) const
{
    assert(action.getActionID() >= 0 && action.getActionID() < kTicTacToeBoardSize * kTicTacToeBoardSize);
    assert(action.getPlayer() == Player::kPlayer1 || action.getPlayer() == Player::kPlayer2);
    return (board_[action.getActionID()] == Player::kPlayerNone);
}

bool TicTacToeEnv::isTerminal() const
{
    // terminal: any player wins or board is filled
    return (eval() != Player::kPlayerNone || std::find(board_.begin(), board_.end(), Player::kPlayerNone) == board_.end());
}

float TicTacToeEnv::getEvalScore(bool is_resign /*= false*/) const
{
    Player result = (is_resign ? getNextPlayer(turn_, kTicTacToeNumPlayer) : eval());
    switch (result) {
        case Player::kPlayer1: return 1.0f;
        case Player::kPlayer2: return -1.0f;
        default: return 0.0f;
    }
}

std::vector<float> TicTacToeEnv::getFeatures(utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    /* 4 channels:
        0~1. own/opponent position
        2. Nought turn
        3. Cross turn
    */
    std::vector<float> vFeatures;
    for (int channel = 0; channel < 4; ++channel) {
        for (int pos = 0; pos < kTicTacToeBoardSize * kTicTacToeBoardSize; ++pos) {
            int rotation_pos = getPositionByRotating(utils::reversed_rotation[static_cast<int>(rotation)], pos, kTicTacToeBoardSize);
            if (channel == 0) {
                vFeatures.push_back((board_[rotation_pos] == turn_ ? 1.0f : 0.0f));
            } else if (channel == 1) {
                vFeatures.push_back((board_[rotation_pos] == getNextPlayer(turn_, kTicTacToeNumPlayer) ? 1.0f : 0.0f));
            } else if (channel == 2) {
                vFeatures.push_back((turn_ == Player::kPlayer1 ? 1.0f : 0.0f));
            } else if (channel == 3) {
                vFeatures.push_back((turn_ == Player::kPlayer2 ? 1.0f : 0.0f));
            }
        }
    }
    return vFeatures;
}

std::vector<float> TicTacToeEnv::getActionFeatures(const TicTacToeAction& action, utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    std::vector<float> action_features(kTicTacToeBoardSize * kTicTacToeBoardSize, 0.0f);
    action_features[getPositionByRotating(rotation, action.getActionID(), kTicTacToeBoardSize)] = 1.0f;
    return action_features;
}

std::string TicTacToeEnv::toString() const
{
    std::ostringstream oss;
    oss << "   A  B  C" << std::endl;
    for (int row = kTicTacToeBoardSize - 1; row >= 0; --row) {
        oss << row + 1 << " ";
        for (int col = 0; col < kTicTacToeBoardSize; ++col) {
            if (board_[row * kTicTacToeBoardSize + col] == Player::kPlayerNone) {
                oss << " . ";
            } else if (board_[row * kTicTacToeBoardSize + col] == Player::kPlayer1) {
                oss << " O ";
            } else {
                oss << " X ";
            }
        }
        oss << " " << row + 1 << std::endl;
    }
    oss << "   A  B  C" << std::endl;
    return oss.str();
}

Player TicTacToeEnv::eval() const
{
    int c;
    for (int i = 0; i < kTicTacToeBoardSize; ++i) {
        // rows
        c = 3;
        for (int j = 0; j < kTicTacToeBoardSize; ++j) { c &= static_cast<int>(board_[i * kTicTacToeBoardSize + j]); }
        if (c != static_cast<int>(Player::kPlayerNone)) { return static_cast<Player>(c); }

        // columns
        c = 3;
        for (int j = 0; j < kTicTacToeBoardSize; ++j) { c &= static_cast<int>(board_[j * kTicTacToeBoardSize + i]); }
        if (c != static_cast<int>(Player::kPlayerNone)) { return static_cast<Player>(c); }
    }

    // diagonal (left-up to right-down)
    c = 3;
    for (int i = 0; i < kTicTacToeBoardSize; ++i) { c &= static_cast<int>(board_[i * kTicTacToeBoardSize + i]); }
    if (c != static_cast<int>(Player::kPlayerNone)) { return static_cast<Player>(c); }

    // diagonal (right-up to left-down)
    c = 3;
    for (int i = 0; i < kTicTacToeBoardSize; ++i) { c &= static_cast<int>(board_[i * kTicTacToeBoardSize + (kTicTacToeBoardSize - 1 - i)]); }
    if (c != static_cast<int>(Player::kPlayerNone)) { return static_cast<Player>(c); }

    return Player::kPlayerNone;
}

std::vector<float> TicTacToeEnvLoader::getActionFeatures(const int pos, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    assert(pos < static_cast<int>(action_pairs_.size()));
    const TicTacToeAction& action = action_pairs_[pos].first;
    std::vector<float> action_features(kTicTacToeBoardSize * kTicTacToeBoardSize, 0.0f);
    action_features[getPositionByRotating(rotation, action.getActionID(), kTicTacToeBoardSize)] = 1.0f;
    return action_features;
}

} // namespace minizero::env::tictactoe
