#include "hex.h"
#include "color_message.h"
#include "random.h"
#include "sgf_loader.h"
#include <iostream>
#include <string>
#include <vector>

namespace minizero::env::hex {

using namespace minizero::utils;

void HexEnv::reset()
{
    winner_ = Player::kPlayerNone;
    turn_ = Player::kPlayer1;
    actions_.clear();
    board_.resize(board_size_ * board_size_);
    fill(board_.begin(), board_.end(), Cell{Player::kPlayerNone, (Flag)0});
}

bool HexEnv::act(const HexAction& action)
{
    if (!isLegalAction(action)) { return false; }
    actions_.push_back(action);

    int action_id = action.getActionID();

    if (config::env_hex_use_swap_rule) {
        // Check if it's the second move and the chosen action is same as the first action
        if (actions_.size() == 1 && action_id == actions_[0].getActionID()) {
            // Player 2 has chosen to swap

            // Reflect the first move's position over the other diagonal
            int original_row = actions_[0].getActionID() / board_size_;
            int original_col = actions_[0].getActionID() % board_size_;

            int reflected_row = board_size_ - 1 - original_col;
            int reflected_col = board_size_ - 1 - original_row;
            int reflected_id = reflected_row * board_size_ + reflected_col;

            // Clear original move
            board_[actions_[0].getActionID()].player = Player::kPlayerNone;
            board_[actions_[0].getActionID()].flags = Flag::NONE;

            action_id = reflected_id;
        }
    }

    Cell* cc{&board_[action_id]};
    cc->player = action.getPlayer();
    if (cc->player == Player::kPlayer1) {
        if (action_id % board_size_ == 0)
            cc->flags = Flag::EDGE1_CONNECTION;
        if (action_id % board_size_ == board_size_ - 1)
            cc->flags = Flag::EDGE2_CONNECTION;
    } else {
        if (action_id < board_size_)
            cc->flags = Flag::EDGE1_CONNECTION;
        if (action_id >= board_size_ * board_size_ - board_size_)
            cc->flags = Flag::EDGE2_CONNECTION;
    }

    winner_ = updateWinner(action_id);
    turn_ = action.nextPlayer();

    return true;
}

bool HexEnv::act(const std::vector<std::string>& action_string_args)
{
    return act(HexAction(action_string_args));
}

std::vector<HexAction> HexEnv::getLegalActions() const
{
    std::vector<HexAction> actions;
    for (int pos = 0; pos < board_size_ * board_size_; ++pos) {
        HexAction action(pos, turn_);
        if (!isLegalAction(action)) { continue; }
        actions.push_back(action);
    }
    return actions;
}

bool HexEnv::isLegalAction(const HexAction& action) const
{
    int action_id = action.getActionID();
    Player player = action.getPlayer();

    assert(action_id >= 0 && action_id < board_size_ * board_size_);
    assert(player == Player::kPlayer1 || player == Player::kPlayer2);

    // Check if the move is on an empty cell and it's the player's turn
    // return player == turn_ && board_[actionID].player == Player::kPlayerNone;
    return player == turn_ &&
           ((config::env_hex_use_swap_rule && actions_.size() == 1) // swap rule
            || (board_[action_id].player == Player::kPlayerNone));  // non-swap rule
}

bool HexEnv::isTerminal() const
{
    return winner_ != Player::kPlayerNone;
}

float HexEnv::getEvalScore(bool is_resign /* = false */) const
{
    if (is_resign) {
        return turn_ == Player::kPlayer1 ? -1. : 1.;
    }
    switch (winner_) {
        case Player::kPlayer1: return 1.0f;
        case Player::kPlayer2: return -1.0f;
        default: return 0.0f;
    }
}

std::vector<float> HexEnv::getFeatures(utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    /* 4 channels:
        0~1. own/opponent position
        2. Black's turn
        3. White's turn
    */
    std::vector<float> vFeatures;
    for (int channel = 0; channel < 4; ++channel) {
        for (int pos = 0; pos < board_size_ * board_size_; ++pos) {
            int rotation_pos = pos;
            if (channel == 0) {
                vFeatures.push_back((board_[rotation_pos].player == turn_ ? 1.0f : 0.0f));
            } else if (channel == 1) {
                vFeatures.push_back((board_[rotation_pos].player == getNextPlayer(turn_, kHexNumPlayer) ? 1.0f : 0.0f));
            } else if (channel == 2) {
                vFeatures.push_back((turn_ == Player::kPlayer1 ? 1.0f : 0.0f));
            } else if (channel == 3) {
                vFeatures.push_back((turn_ == Player::kPlayer2 ? 1.0f : 0.0f));
            }
        }
    }
    return vFeatures;
}

std::vector<float> HexEnv::getActionFeatures(const HexAction& action, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    std::vector<float> action_features(board_size_ * board_size_, 0.0f);
    action_features[action.getActionID()] = 1.0f;
    return action_features;
}

std::string HexEnv::toString() const
{
    // Declare the two new color variables
    auto color_player_1 = minizero::utils::TextColor::kRed;
    auto color_player_2 = minizero::utils::TextColor::kBlue;

    std::vector<char> rr{};

    // Printing the alphabets on top
    rr.push_back(' ');
    rr.push_back(' ');
    rr.push_back(' ');
    for (size_t ii = 0; ii < static_cast<size_t>(board_size_); ii++) {
        std::string colored{minizero::utils::getColorText(
            " " + std::string(1, ii + 97 + (ii > 7 ? 1 : 0)), minizero::utils::TextType::kBold, minizero::utils::TextColor::kWhite,
            color_player_2)};
        rr.insert(rr.end(), colored.begin(), colored.end());
    }
    rr.push_back('\n');

    for (size_t ii = board_size_; ii-- > 0;) {
        // Adding leading spaces
        rr.push_back(' ');
        for (size_t jj = 0; jj < board_size_ - ii - 1; jj++) {
            rr.push_back(' ');
        }

        // Printing row number
        std::string row_num = std::to_string(ii + 1);
        std::string colored_row_num{minizero::utils::getColorText(
            row_num, minizero::utils::TextType::kBold, minizero::utils::TextColor::kWhite,
            color_player_1)};
        rr.insert(rr.end(), colored_row_num.begin(), colored_row_num.end());

        // Adding extra space to compensate for larger row numbers
        if (ii < 9) rr.push_back(' ');

        rr.push_back('\\');

        // Printing board cells
        for (size_t jj = 0; jj < static_cast<size_t>(board_size_); jj++) {
            Cell cc{board_[jj + static_cast<size_t>(board_size_) * ii]};
            if (cc.player == Player::kPlayer1) {
                std::string colored{minizero::utils::getColorText(
                    "B ", minizero::utils::TextType::kBold, minizero::utils::TextColor::kWhite,
                    color_player_1)};
                rr.insert(rr.end(), colored.begin(), colored.end());
            } else if (cc.player == Player::kPlayer2) {
                std::string colored{minizero::utils::getColorText(
                    "W ", minizero::utils::TextType::kBold, minizero::utils::TextColor::kWhite,
                    color_player_2)};
                rr.insert(rr.end(), colored.begin(), colored.end());
            } else {
                rr.push_back('[');
                rr.push_back(']');
            }
        }

        rr.push_back('\\');
        rr.insert(rr.end(), colored_row_num.begin(), colored_row_num.end());

        // Adding extra space to compensate for larger row numbers
        if (ii < 9) rr.push_back(' ');

        rr.push_back('\n');
    }

    // Adding leading spaces
    rr.push_back(' ');
    for (size_t jj = 0; jj < static_cast<size_t>(board_size_); jj++) {
        rr.push_back(' ');
    }

    // Printing the alphabets on the bottom
    rr.push_back(' ');
    for (size_t ii = 0; ii < static_cast<size_t>(board_size_); ii++) {
        std::string colored{minizero::utils::getColorText(
            " " + std::string(1, ii + 97 + (ii > 7 ? 1 : 0)), minizero::utils::TextType::kBold, minizero::utils::TextColor::kWhite,
            color_player_2)};
        rr.insert(rr.end(), colored.begin(), colored.end());
    }
    rr.push_back('\n');

    std::string ss(rr.begin(), rr.end());
    return ss;
}

std::string HexEnv::toStringDebug() const
{
    std::vector<char> rr{};

    // Print the first line (the alphabet characters).
    for (size_t ii = 0; ii < static_cast<size_t>(board_size_); ii++) {
        rr.push_back(' ');
        rr.push_back(' ');
        rr.push_back(ii + 97 + (ii > 7 ? 1 : 0));
    }
    rr.push_back('\n');

    for (size_t ii = 0; ii < static_cast<size_t>(board_size_); ii++) {
        // Indentation.
        for (size_t jj = 0; jj < ii; jj++) {
            rr.push_back(' ');
        }

        // Row number at the start.
        rr.push_back(static_cast<char>(ii + 1 + '0'));
        rr.push_back('\\');
        rr.push_back(' ');

        for (size_t jj = 0; jj < static_cast<size_t>(board_size_); jj++) {
            Cell cc{board_[jj + static_cast<size_t>(board_size_) * (static_cast<size_t>(board_size_) - ii - 1)]};
            if (cc.player == Player::kPlayer1) {
                rr.push_back('B');
            } else if (cc.player == Player::kPlayer2) {
                rr.push_back('W');
            } else {
                rr.push_back('.');
            }
            rr.push_back(' ');
            rr.push_back(' ');
        }

        // Row number at the end.
        rr.push_back('\\');
        rr.push_back(static_cast<char>(ii + 1 + '0'));

        rr.push_back('\n');
    }

    // Print the last line (the alphabet characters).
    for (size_t ii = 0; ii < static_cast<size_t>(board_size_); ii++) {
        rr.push_back(' ');
        rr.push_back(' ');
        rr.push_back(ii + 97 + (ii > 8 ? 1 : 0));
    }
    rr.push_back('\n');

    std::string ss(rr.begin(), rr.end());
    return ss;
}

std::vector<int> HexEnv::getWinningStonesPosition() const
{
    if (winner_ == Player::kPlayerNone) { return {}; }
    std::vector<int> winning_stones{};
    for (size_t ii = 0; ii < board_size_ * board_size_; ii++) {
        if ((winner_ == Player::kPlayer1 && (static_cast<int>(board_[ii].flags & Flag::EDGE1_CONNECTION) > 0) &&
             (static_cast<int>(board_[ii].flags & Flag::EDGE2_CONNECTION) > 0)) ||
            (winner_ == Player::kPlayer2 && (static_cast<int>(board_[ii].flags & Flag::EDGE1_CONNECTION) > 0) &&
             (static_cast<int>(board_[ii].flags & Flag::EDGE2_CONNECTION) > 0))) {
            winning_stones.push_back(ii);
        }
    }
    return winning_stones;
}

Player HexEnv::updateWinner(int action_id)
{
    // struct Coord{int x{}; int y{};};
    /* neighboorActionIds
      4 5
      |/
    2-C-3
     /|
    0 1
    */

    // Get neighbor cells.
    int neighboor_action_id_offsets[6] = {
        -1 - board_size_, 0 - board_size_,
        -1 - 0 * board_size_, 1 + 0 * board_size_,
        0 + board_size_, 1 + board_size_};
    std::vector<int> neighboor_cells_actions{};
    for (size_t ii = 0; ii < 6; ii++) {
        // Outside right/left walls?
        int xx{action_id % board_size_};
        if (xx == 0 && (ii == 0 || ii == 2)) continue;
        if (xx == board_size_ - 1 && (ii == 3 || ii == 5)) continue;

        // Outside top/bottom walls?
        int neighboorActionId = action_id + neighboor_action_id_offsets[ii];
        if (neighboorActionId < 0 || neighboorActionId >= board_size_ * board_size_) continue;

        //
        neighboor_cells_actions.push_back(neighboorActionId);
    }

    // Update from surrounding cells.
    Cell* my_cell = &board_[action_id];
    for (size_t ii = 0; ii < neighboor_cells_actions.size(); ii++) {
        Cell* neighboor{&board_[neighboor_cells_actions[ii]]};
        if (my_cell->player == neighboor->player) {
            my_cell->flags = my_cell->flags | neighboor->flags;
        }
    }

    // Update surrounding cells.
    for (size_t ii = 0; ii < neighboor_cells_actions.size(); ii++) {
        int neighboor_cells_action{neighboor_cells_actions[ii]};
        Cell* neighboor{&board_[neighboor_cells_action]};
        if (my_cell->player == neighboor->player && neighboor->flags != my_cell->flags) {
            HexEnv::updateWinner(neighboor_cells_action);
        }
    }

    // Check victory.
    if (static_cast<int>(my_cell->flags & Flag::EDGE1_CONNECTION) > 0 && static_cast<int>(my_cell->flags & Flag::EDGE2_CONNECTION) > 0) {
        return my_cell->player;
    }

    return Player::kPlayerNone;
}

std::vector<float> HexEnvLoader::getActionFeatures(const int pos, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    const HexAction& action = action_pairs_[pos].first;
    std::vector<float> action_features(getBoardSize() * getBoardSize(), 0.0f);
    int action_id = ((pos < static_cast<int>(action_pairs_.size())) ? action.getActionID() : utils::Random::randInt() % action_features.size());
    action_features[action_id] = 1.0f;
    return action_features;
}

} // namespace minizero::env::hex
