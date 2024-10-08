#include "conhex.h"
#include "color_message.h"
#include "random.h"
#include "sgf_loader.h"
#include <algorithm>
#include <iostream>
#include <string>

namespace minizero::env::conhex {

using namespace minizero::utils;

ConHexEnv::ConHexEnv() : BaseBoardEnv<ConHexAction>(kConHexBoardSize), conhex_graph_()
{
    invalid_actions_.set(10);
    invalid_actions_.set(16);
    invalid_actions_.set(20);
    invalid_actions_.set(24);
    invalid_actions_.set(30);
    invalid_actions_.set(32);
    invalid_actions_.set(48);
    invalid_actions_.set(50);
    invalid_actions_.set(56);
    invalid_actions_.set(60);
    invalid_actions_.set(64);
    invalid_actions_.set(70);
    reset();
}

void ConHexEnv::reset()
{
    // reset board
    turn_ = Player::kPlayer1;
    actions_.clear();
    conhex_graph_.reset();
}

bool ConHexEnv::act(const ConHexAction& action)
{
    if (!isLegalAction(action)) { return false; }

    int action_id = action.getActionID();

    if (config::env_conhex_use_swap_rule) {
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
            conhex_graph_.reset();
            action_id = reflected_id;
        }
    }
    actions_.push_back(action);
    conhex_graph_.placeStone(action_id, action.getPlayer());
    turn_ = action.nextPlayer();
    return true;
}

bool ConHexEnv::act(const std::vector<std::string>& action_string_args)
{
    return act(ConHexAction(action_string_args));
}

std::vector<ConHexAction> ConHexEnv::getLegalActions() const
{
    std::vector<ConHexAction> actions;
    for (int pos = 0; pos < board_size_ * board_size_; ++pos) {
        ConHexAction action(pos, turn_);
        if (!isLegalAction(action)) { continue; }
        actions.push_back(action);
    }
    return actions;
}

bool ConHexEnv::isPlaceable(int action_id) const
{
    if (invalid_actions_.test(action_id)) { return false; }
    return true;
}

bool ConHexEnv::isLegalAction(const ConHexAction& action) const
{
    assert(player == Player::kPlayer1 || player == Player::kPlayer2);

    int action_id = action.getActionID();
    Player player = action.getPlayer();
    if (!(action_id >= 0 && action_id < board_size_ * board_size_)) { return false; }

    if (player != turn_) { return false; } // not player's turn

    if (config::env_conhex_use_swap_rule && actions_.size() == 1) {
        // swap rule
        return isPlaceable(action_id);
    }
    if (conhex_graph_.getPlayerAtPos(action_id) == Player::kPlayerNone) {
        // non-swap rule
        // spot not be placed
        return isPlaceable(action_id);
    }
    return false; // illegal move
}

bool ConHexEnv::isTerminal() const
{
    return conhex_graph_.checkWinner() != Player::kPlayerNone;
}

float ConHexEnv::getEvalScore(bool is_resign /*= false*/) const
{
    if (is_resign) {
        return turn_ == Player::kPlayer1 ? -1.0f : 1.0f;
    }
    switch (conhex_graph_.checkWinner()) {
        case Player::kPlayer1: return 1.0f;
        case Player::kPlayer2: return -1.0f;
        default: return 0.0f;
    }
}

std::vector<float> ConHexEnv::getFeatures(utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    /* 6 channels:
        0~1. own/opponent position
        2~3. own/opponent cell position
        4. Player1 turn
        5. Player2 turn
    */
    std::vector<float> features;
    for (int channel = 0; channel < 6; ++channel) {
        for (int pos = 0; pos < board_size_ * board_size_; ++pos) {
            int rotation_pos = pos;
            if (channel == 0) {
                features.push_back((conhex_graph_.getPlayerAtPos(rotation_pos) == turn_ ? 1.0f : 0.0f));
            } else if (channel == 1) {
                features.push_back((conhex_graph_.getPlayerAtPos(rotation_pos) == getNextPlayer(turn_, kConHexNumPlayer) ? 1.0f : 0.0f));
            } else if (channel == 2) {
                features.push_back((conhex_graph_.isCellCapturedByPlayer(pos, turn_)) ? 1.0f : 0.0f);
            } else if (channel == 3) {
                features.push_back((conhex_graph_.isCellCapturedByPlayer(pos, getNextPlayer(turn_, kConHexNumPlayer))) ? 1.0f : 0.0f);
            } else if (channel == 4) {
                features.push_back((turn_ == Player::kPlayer1 ? 1.0f : 0.0f));
            } else if (channel == 5) {
                features.push_back((turn_ == Player::kPlayer2 ? 1.0f : 0.0f));
            }
        }
    }
    return features;
}

std::vector<float> ConHexEnv::getActionFeatures(const ConHexAction& action, utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    std::vector<float> action_features(board_size_ * board_size_, 0.0f);
    action_features[action.getActionID()] = 1.0f;
    return action_features;
}

std::string ConHexEnv::toString() const
{
    return conhex_graph_.toString();
}

std::vector<float> ConHexEnvLoader::getActionFeatures(const int pos, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    const ConHexAction& action = action_pairs_[pos].first;
    std::vector<float> action_features(getBoardSize() * getBoardSize(), 0.0f);
    int action_id = ((pos < static_cast<int>(action_pairs_.size())) ? action.getActionID() : utils::Random::randInt() % action_features.size());
    action_features[action_id] = 1.0f;
    return action_features;
}

} // namespace minizero::env::conhex
