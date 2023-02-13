#include "gomoku.h"
#include "color_message.h"
#include "sgf_loader.h"
#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>

namespace minizero::env::gomoku {

using namespace minizero::utils;

GomokuAction::GomokuAction(const std::vector<std::string>& action_string_args)
{
    assert(action_string_args.size() == 2);
    assert(action_string_args[0].size() == 1 && (charToPlayer(action_string_args[0][0]) == Player::kPlayer1 || charToPlayer(action_string_args[0][0]) == Player::kPlayer2));
    player_ = charToPlayer(action_string_args[0][0]);
    action_id_ = SGFLoader::boardCoordinateStringToActionID(action_string_args[1], minizero::config::env_board_size);
}

void GomokuEnv::reset()
{
    winner_ = Player::kPlayerNone;
    turn_ = Player::kPlayer1;
    actions_.clear();
    board_.resize(board_size_ * board_size_);
    fill(board_.begin(), board_.end(), Player::kPlayerNone);
}

bool GomokuEnv::act(const GomokuAction& action)
{
    if (!isLegalAction(action)) { return false; }
    actions_.push_back(action);
    board_[action.getActionID()] = action.getPlayer();
    turn_ = action.nextPlayer();
    winner_ = updateWinner(action);
    return true;
}

bool GomokuEnv::act(const std::vector<std::string>& action_string_args)
{
    return act(GomokuAction(action_string_args));
}

std::vector<GomokuAction> GomokuEnv::getLegalActions() const
{
    std::vector<GomokuAction> actions;
    for (int pos = 0; pos < board_size_ * board_size_; ++pos) {
        GomokuAction action(pos, turn_);
        if (!isLegalAction(action)) { continue; }
        actions.push_back(action);
    }
    return actions;
}

bool GomokuEnv::isLegalAction(const GomokuAction& action) const
{
    assert(action.getActionID() >= 0 && action.getActionID() < board_size_ * board_size_);
    assert(action.getPlayer() == Player::kPlayer1 || action.getPlayer() == Player::kPlayer2);
    return (board_[action.getActionID()] == Player::kPlayerNone);
}

bool GomokuEnv::isTerminal() const
{
    return (winner_ != Player::kPlayerNone || std::find(board_.begin(), board_.end(), Player::kPlayerNone) == board_.end());
}

float GomokuEnv::getEvalScore(bool is_resign /*= false*/) const
{
    Player eval = (is_resign ? getNextPlayer(turn_, board_size_) : winner_);
    switch (eval) {
        case Player::kPlayer1: return 1.0f;
        case Player::kPlayer2: return -1.0f;
        default: return 0.0f;
    }
}

std::vector<float> GomokuEnv::getFeatures(utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    /* 4 channels:
        0~1. own/opponent position
        2. Black's turn
        3. White's turn
    */
    std::vector<float> vFeatures;
    for (int channel = 0; channel < 4; ++channel) {
        for (int pos = 0; pos < board_size_ * board_size_; ++pos) {
            int rotation_pos = getPositionByRotating(utils::reversed_rotation[static_cast<int>(rotation)], pos, board_size_);
            if (channel == 0) {
                vFeatures.push_back((board_[rotation_pos] == turn_ ? 1.0f : 0.0f));
            } else if (channel == 1) {
                vFeatures.push_back((board_[rotation_pos] == getNextPlayer(turn_, board_size_) ? 1.0f : 0.0f));
            } else if (channel == 2) {
                vFeatures.push_back((turn_ == Player::kPlayer1 ? 1.0f : 0.0f));
            } else if (channel == 3) {
                vFeatures.push_back((turn_ == Player::kPlayer2 ? 1.0f : 0.0f));
            }
        }
    }
    return vFeatures;
}

std::vector<float> GomokuEnv::getActionFeatures(const GomokuAction& action, utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    std::vector<float> action_features(board_size_ * board_size_, 0.0f);
    action_features[getPositionByRotating(rotation, action.getActionID(), board_size_)] = 1.0f;
    return action_features;
}

std::string GomokuEnv::toString() const
{
    int last_move_pos = -1, last2_move_pos = -1;
    if (actions_.size() >= 1) { last_move_pos = actions_.back().getActionID(); }
    if (actions_.size() >= 2) { last2_move_pos = actions_[actions_.size() - 2].getActionID(); }

    std::unordered_map<Player, std::pair<std::string, TextColor>> player_to_text_color({{Player::kPlayerNone, {".", TextColor::kBlack}},
                                                                                        {Player::kPlayer1, {"O", TextColor::kBlack}},
                                                                                        {Player::kPlayer2, {"O", TextColor::kWhite}}});
    std::ostringstream oss;
    oss << getCoordinateString() << std::endl;
    for (int row = board_size_ - 1; row >= 0; --row) {
        oss << getColorText((row + 1 < 10 ? " " : "") + std::to_string(row + 1), TextType::kBold, TextColor::kBlack, TextColor::kYellow);
        for (int col = 0; col < board_size_; ++col) {
            int pos = row * board_size_ + col;
            const std::pair<std::string, TextColor> text_pair = player_to_text_color[board_[pos]];
            if (pos == last_move_pos) {
                oss << getColorText(">", TextType::kBold, TextColor::kRed, TextColor::kYellow);
                oss << getColorText(text_pair.first, TextType::kBold, text_pair.second, TextColor::kYellow);
            } else if (pos == last2_move_pos) {
                oss << getColorText(">", TextType::kNormal, TextColor::kRed, TextColor::kYellow);
                oss << getColorText(text_pair.first, TextType::kBold, text_pair.second, TextColor::kYellow);
            } else {
                oss << getColorText(" " + text_pair.first, TextType::kBold, text_pair.second, TextColor::kYellow);
            }
        }
        oss << getColorText(" " + std::to_string(row + 1) + (row + 1 < 10 ? " " : ""), TextType::kBold, TextColor::kBlack, TextColor::kYellow);
        oss << std::endl;
    }
    oss << getCoordinateString() << std::endl;
    return oss.str();
}

Player GomokuEnv::updateWinner(const GomokuAction& action)
{
    int pos = action.getActionID();
    if (calculateNumberOfConnection(pos, {1, 0}) + calculateNumberOfConnection(pos, {-1, 0}) - 1 >= 5) { return action.getPlayer(); }  // row
    if (calculateNumberOfConnection(pos, {0, 1}) + calculateNumberOfConnection(pos, {0, -1}) - 1 >= 5) { return action.getPlayer(); }  // column
    if (calculateNumberOfConnection(pos, {1, 1}) + calculateNumberOfConnection(pos, {-1, -1}) - 1 >= 5) { return action.getPlayer(); } // diagonal (right-up to left-down)
    if (calculateNumberOfConnection(pos, {1, -1}) + calculateNumberOfConnection(pos, {-1, 1}) - 1 >= 5) { return action.getPlayer(); } // diagonal (left-up to right-down)
    return Player::kPlayerNone;
}

int GomokuEnv::calculateNumberOfConnection(int start_pos, std::pair<int, int> direction)
{
    int count = 0;
    int x = start_pos % board_size_;
    int y = start_pos / board_size_;
    Player find_player = board_[start_pos];
    while (x >= 0 && x < board_size_ && y >= 0 && y < board_size_) {
        if (board_[y * board_size_ + x] != find_player) { break; }
        ++count;
        x += direction.first;
        y += direction.second;
    }
    return count;
}

std::string GomokuEnv::getCoordinateString() const
{
    std::ostringstream oss;
    oss << "  ";
    for (int i = 0; i < board_size_; ++i) {
        char c = 'A' + i + ('A' + i >= 'I' ? 1 : 0);
        oss << " " + std::string(1, c);
    }
    oss << "   ";
    return getColorText(oss.str(), TextType::kBold, TextColor::kBlack, TextColor::kYellow);
}

} // namespace minizero::env::gomoku
