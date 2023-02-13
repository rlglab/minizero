#include "hex.h"

#include "color_message.h"
#include "sgf_loader.h"
#include <iostream>
#include <string>
#include <vector>

namespace minizero::env::hex {

using namespace minizero::utils;

HexAction::HexAction(const std::vector<std::string>& action_string_args, int board_size /* = minizero::config::env_board_size */)
{
    // play b d5 => {"b", "d5"}
    if (action_string_args.size() != 2) {
        throw std::string{
            "Number of actions must be exactly two."} +
            " Length of argument was " + std::to_string(action_string_args.size());
    }
    size_t action_size{action_string_args[1].size()};
    if ((action_size < 2) || (action_size > 4)) {
        throw std::string{
            "Number of letters in action must be exactly 2. For example \"d5\"."} +
            " Length of argument was " + std::to_string(action_string_args[1].size());
    }
    action_id_ = SGFLoader::boardCoordinateStringToActionID(action_string_args[1], board_size);

    // Player.
    assert((action_string_args[0].size() == 1) && "First argument must be of size 1.");
    assert((charToPlayer(action_string_args[0].at(0)) != Player::kPlayerSize) && "Error in formatting of player");

    player_ = charToPlayer(action_string_args[0].at(0));
}

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

    int actionId = action.getActionID();
    Cell* cc{&board_[actionId]};
    cc->player = action.getPlayer();
    if (action.getPlayer() == Player::kPlayer1) {
        if (actionId % board_size_ == 0)
            cc->flags = Flag::BLACK_LEFT;
        if (actionId % board_size_ == board_size_ - 1)
            cc->flags = Flag::BLACK_RIGHT;
    } else {
        if (actionId < board_size_)
            cc->flags = Flag::WHITE_BOTTOM;
        if (actionId >= board_size_ * board_size_ - board_size_)
            cc->flags = Flag::WHITE_TOP;
    }
    winner_ = updateWinner(actionId);

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
    assert(action.getActionID() >= 0 && action.getActionID() < board_size_ * board_size_);
    assert(action.getPlayer() == Player::kPlayer1 || action.getPlayer() == Player::kPlayer2);
    return action.getPlayer() == turn_ && board_[action.getActionID()].player == Player::kPlayerNone;
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
                vFeatures.push_back((board_[rotation_pos].player == getNextPlayer(turn_, board_size_) ? 1.0f : 0.0f));
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
    /*
          R
  2  0-0-R-0-0-0-0
     |/|/|/|/|/|/|
B 1  0-R-B-B-0-0-0 B
     |/|/|/|/|/|/|
  0  0-0-R-0-0-0-0
     a b c d e f g
          R
    */
    std::vector<char> rr{};

    for (size_t ii = 0; ii < static_cast<size_t>(board_size_) - 1 + 5; ii++) {
        rr.push_back(' ');
    }
    rr.push_back('R');
    rr.push_back('\n');
    for (size_t ii = 0; ii < static_cast<size_t>(board_size_); ii++) {
        if (ii == static_cast<size_t>(board_size_) / 2) {
            rr.push_back('B');
        } else {
            rr.push_back(' ');
        }
        rr.push_back(' ');
        std::string rowNum{std::to_string(static_cast<size_t>(board_size_) - ii)};
        if (rowNum.size() == 1) {
            rr.push_back(' ');
            rr.push_back(rowNum.at(0));
        } else {
            rr.push_back(rowNum.at(0));
            rr.push_back(rowNum.at(1));
        }
        rr.push_back(' ');
        for (size_t jj = 0; jj < static_cast<size_t>(board_size_); jj++) {
            Cell cc{board_[jj + static_cast<size_t>(board_size_) * (static_cast<size_t>(board_size_) - ii - 1)]};
            if (cc.player == Player::kPlayer1) {
                std::string colored{minizero::utils::getColorText(
                    "B", minizero::utils::TextType::kBold, minizero::utils::TextColor::kBlack,
                    minizero::utils::TextColor::kBlue)};
                for (size_t kk = 0; kk < colored.size(); kk++)
                    rr.push_back(colored.at(kk));
            } else if (cc.player == Player::kPlayer2) {
                std::string colored{minizero::utils::getColorText(
                    "R", minizero::utils::TextType::kBold, minizero::utils::TextColor::kBlack,
                    minizero::utils::TextColor::kRed)};
                for (size_t kk = 0; kk < colored.size(); kk++)
                    rr.push_back(colored.at(kk));
            } else {
                rr.push_back('0');
            }
            if (jj < static_cast<size_t>(board_size_) - 1) {
                rr.push_back('-');
            }
        }
        if (ii == static_cast<size_t>(board_size_) / 2) {
            rr.push_back(' ');
            rr.push_back('B');
        }
        rr.push_back('\n');
        if (ii == static_cast<size_t>(board_size_) - 1) {
            break;
        }
        for (size_t jj = 0; jj < 5; jj++) {
            rr.push_back(' ');
        }
        for (size_t jj = 0; jj < static_cast<size_t>(board_size_) - 1; jj++) {
            rr.push_back('|');
            rr.push_back('/');
        }
        rr.push_back('|');
        rr.push_back('\n');
    }
    for (size_t ii = 0; ii < 5; ii++) {
        rr.push_back(' ');
    }
    for (size_t ii = 0; ii < static_cast<size_t>(board_size_); ii++) {
        rr.push_back(ii + 97 + (ii > 7 ? 1 : 0));
        rr.push_back(' ');
    }
    rr.push_back('\n');
    for (size_t ii = 0; ii < static_cast<size_t>(board_size_) - 1 + 5; ii++) {
        rr.push_back(' ');
    }
    rr.push_back('R');
    rr.push_back('\n');

    std::string ss(rr.begin(), rr.end());
    return ss;
}

std::string HexEnv::toStringDebug() const
{
    /*
              R
  2   0 - 0 - R - 0 - 0 - 0 - 0
      | / | / | / | / | / | / |
B 1   0 - R - B - B - 0 - 0 - 0 B
      | / | / | / | / | / | / |
  0   0 - 0 - R - 0 - 0 - 0 - 0
      a   b   c   d   e   f   g
              R
    */
    std::vector<char> rr{};

    for (size_t ii = 0; ii < static_cast<size_t>(board_size_) - 1 + 5; ii++) {
        rr.push_back(' ');
    }
    rr.push_back('R');
    rr.push_back('\n');
    for (size_t ii = 0; ii < static_cast<size_t>(board_size_); ii++) {
        if (ii == static_cast<size_t>(board_size_) / 2) {
            rr.push_back('B');
        } else {
            rr.push_back(' ');
        }
        rr.push_back(' ');
        std::string rowNum{std::to_string(static_cast<size_t>(board_size_) - ii)};
        if (rowNum.size() == 1) {
            rr.push_back(' ');
            rr.push_back(rowNum.at(0));
        } else {
            rr.push_back(rowNum.at(0));
            rr.push_back(rowNum.at(1));
        }
        rr.push_back(' ');
        for (size_t jj = 0; jj < static_cast<size_t>(board_size_); jj++) {
            Cell cc{board_[jj + static_cast<size_t>(board_size_) * (static_cast<size_t>(board_size_) - ii - 1)]};
            if (static_cast<int>(cc.flags & Flag::BLACK_LEFT) > 0 || static_cast<int>(cc.flags & Flag::WHITE_BOTTOM) > 0) {
                rr.push_back('*');
            } else {
                rr.push_back(' ');
            }
            if (cc.player == Player::kPlayer1) {
                rr.push_back('B');
            } else if (cc.player == Player::kPlayer2) {
                rr.push_back('R');
            } else {
                rr.push_back('0');
            }
            if (static_cast<int>(cc.flags & Flag::BLACK_RIGHT) > 0 || static_cast<int>(cc.flags & Flag::WHITE_TOP) > 0) {
                rr.push_back('*');
            } else {
                rr.push_back(' ');
            }
            if (jj < static_cast<size_t>(board_size_) - 1) {
                rr.push_back('-');
            }
        }
        if (ii == static_cast<size_t>(board_size_) / 2) {
            rr.push_back(' ');
            rr.push_back('B');
        }
        rr.push_back('\n');
        if (ii == static_cast<size_t>(board_size_) - 1) {
            break;
        }
        for (size_t jj = 0; jj < 5; jj++) {
            rr.push_back(' ');
        }
        for (size_t jj = 0; jj < static_cast<size_t>(board_size_) - 1; jj++) {
            rr.push_back(' ');
            rr.push_back('|');
            rr.push_back(' ');
            rr.push_back('/');
        }
        rr.push_back(' ');
        rr.push_back('|');
        rr.push_back('\n');
    }
    for (size_t ii = 0; ii < 5; ii++) {
        rr.push_back(' ');
    }
    for (size_t ii = 0; ii < static_cast<size_t>(board_size_); ii++) {
        rr.push_back(ii + 97 + (ii > 7 ? 1 : 0));
        rr.push_back(' ');
        rr.push_back(' ');
        rr.push_back(' ');
    }
    rr.push_back('\n');
    for (size_t ii = 0; ii < static_cast<size_t>(board_size_) - 1 + 5; ii++) {
        rr.push_back(' ');
    }
    rr.push_back('R');
    rr.push_back('\n');

    std::string ss(rr.begin(), rr.end());
    return ss;
}

std::vector<int> HexEnv::getWinningStonesPosition() const
{
    if (winner_ == Player::kPlayerNone) { return {}; }
    std::vector<int> winning_stones{};
    for (size_t ii = 0; ii < board_size_ * board_size_; ii++) {
        if ((winner_ == Player::kPlayer1 && (static_cast<int>(board_[ii].flags & Flag::BLACK_LEFT) > 0) &&
             (static_cast<int>(board_[ii].flags & Flag::BLACK_RIGHT) > 0)) ||
            (winner_ == Player::kPlayer2 && (static_cast<int>(board_[ii].flags & Flag::WHITE_BOTTOM) > 0) &&
             (static_cast<int>(board_[ii].flags & Flag::WHITE_TOP) > 0))) {
            winning_stones.push_back(ii);
        }
    }
    return winning_stones;
}

Player HexEnv::updateWinner(int actionID)
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
    int neighboorActionIdOffsets[6] = {
        -1 - board_size_, 0 - board_size_,
        -1 - 0 * board_size_, 1 + 0 * board_size_,
        0 + board_size_, 1 + board_size_};
    std::vector<int> neighboorCellsActions{};
    for (size_t ii = 0; ii < 6; ii++) {
        // Outside right/left walls?
        int xx{actionID % board_size_};
        if (xx == 0 && (ii == 0 || ii == 2)) continue;
        if (xx == board_size_ - 1 && (ii == 3 || ii == 5)) continue;

        // Outside top/bottom walls?
        int neighboorActionId = actionID + neighboorActionIdOffsets[ii];
        if (neighboorActionId < 0 || neighboorActionId >= board_size_ * board_size_) continue;

        //
        neighboorCellsActions.push_back(neighboorActionId);
    }

    // Update from surrounding cells.
    Cell* myCell = &board_[actionID];
    for (size_t ii = 0; ii < neighboorCellsActions.size(); ii++) {
        Cell* neighboor{&board_[neighboorCellsActions[ii]]};
        if (myCell->player == neighboor->player) {
            myCell->flags = myCell->flags | neighboor->flags;
        }
    }

    // Update surrounding cells.
    for (size_t ii = 0; ii < neighboorCellsActions.size(); ii++) {
        int neighboorCellsAction{neighboorCellsActions[ii]};
        Cell* neighboor{&board_[neighboorCellsAction]};
        if (myCell->player == neighboor->player && neighboor->flags != myCell->flags) {
            HexEnv::updateWinner(neighboorCellsAction);
        }
    }

    // Check victory.
    if (static_cast<int>(myCell->flags & Flag::BLACK_LEFT) > 0 && static_cast<int>(myCell->flags & Flag::BLACK_RIGHT) > 0) {
        return Player::kPlayer1;
    }
    if (static_cast<int>(myCell->flags & Flag::WHITE_BOTTOM) > 0 && static_cast<int>(myCell->flags & Flag::WHITE_TOP) > 0) {
        return Player::kPlayer2;
    }

    //
    return Player::kPlayerNone;
}

} // namespace minizero::env::hex
