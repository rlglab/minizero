#include "tietactoe.h"
#include "sgf_loader.h"

namespace minizero::env::tietactoe {

using namespace minizero::utils;

TieTacToeAction::TieTacToeAction(const std::vector<std::string>& action_string_args)
{
    assert(action_string_args.size() == 2);
    assert(action_string_args[0].size() == 1 && (CharToPlayer(action_string_args[0][0]) == Player::kPlayer1 || CharToPlayer(action_string_args[0][0]) == Player::kPlayer2));
    player_ = CharToPlayer(action_string_args[0][0]);
    action_id_ = SGFLoader::BoardCoordinateStringToActionID(action_string_args[1], kTieTacToeBoardSize);
}

void TieTacToeEnv::Reset()
{
    turn_ = Player::kPlayer1;
    actions_.clear();
    board_.resize(kTieTacToeBoardSize * kTieTacToeBoardSize);
    fill(board_.begin(), board_.end(), Player::kPlayerNone);
}

bool TieTacToeEnv::Act(const TieTacToeAction& action)
{
    if (!IsLegalAction(action)) { return false; }
    actions_.push_back(action);
    board_[action.GetActionID()] = action.GetPlayer();
    turn_ = action.NextPlayer();
    return true;
}

bool TieTacToeEnv::Act(const std::vector<std::string>& action_string_args)
{
    return Act(TieTacToeAction(action_string_args));
}

std::vector<TieTacToeAction> TieTacToeEnv::GetLegalActions() const
{
    std::vector<TieTacToeAction> actions;
    for (int pos = 0; pos < kTieTacToeBoardSize * kTieTacToeBoardSize; ++pos) {
        TieTacToeAction action(pos, turn_);
        if (!IsLegalAction(action)) { continue; }
        actions.push_back(action);
    }
    return actions;
}

bool TieTacToeEnv::IsLegalAction(const TieTacToeAction& action) const
{
    assert(action.GetActionID() >= 0 && action.GetActionID() < kTieTacToeBoardSize * kTieTacToeBoardSize);
    assert(action.GetPlayer() == Player::kPlayer1 || action.GetPlayer() == Player::kPlayer2);
    return (board_[action.GetActionID()] == Player::kPlayerNone);
}

bool TieTacToeEnv::IsTerminal() const
{
    // terminal: any player wins or board is filled
    return (Eval() != Player::kPlayerNone || std::find(board_.begin(), board_.end(), Player::kPlayerNone) == board_.end());
}

float TieTacToeEnv::GetEvalScore() const
{
    Player eval = Eval();
    switch (eval) {
        case Player::kPlayer1: return 1.0f;
        case Player::kPlayer2: return -1.0f;
        default: return 0.0f;
    }
}

std::vector<float> TieTacToeEnv::GetFeatures() const
{
    /* 4 channels:
        0. Nought position
        1. Cross position
        2. Nought turn
        3. Cross turn
    */
    std::vector<float> vFeatures;
    for (int channel = 0; channel < 4; ++channel) {
        for (int pos = 0; pos < kTieTacToeBoardSize * kTieTacToeBoardSize; ++pos) {
            if (channel == 0) {
                vFeatures.push_back((board_[pos] == Player::kPlayer1 ? 1.0f : 0.0f));
            } else if (channel == 1) {
                vFeatures.push_back((board_[pos] == Player::kPlayer2 ? 1.0f : 0.0f));
            } else if (channel == 2) {
                vFeatures.push_back((turn_ == Player::kPlayer1 ? 1.0f : 0.0f));
            } else if (channel == 3) {
                vFeatures.push_back((turn_ == Player::kPlayer2 ? 1.0f : 0.0f));
            }
        }
    }
    return vFeatures;
}

std::string TieTacToeEnv::ToString() const
{
    std::ostringstream oss;
    oss << "   A  B  C" << std::endl;
    for (int row = kTieTacToeBoardSize - 1; row >= 0; --row) {
        oss << row + 1 << " ";
        for (int col = 0; col < kTieTacToeBoardSize; ++col) {
            if (board_[row * kTieTacToeBoardSize + col] == Player::kPlayerNone) {
                oss << " . ";
            } else if (board_[row * kTieTacToeBoardSize + col] == Player::kPlayer1) {
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

Player TieTacToeEnv::Eval() const
{
    int c;
    for (int i = 0; i < kTieTacToeBoardSize; ++i) {
        // rows
        c = 3;
        for (int j = 0; j < kTieTacToeBoardSize; ++j) { c &= static_cast<int>(board_[i * kTieTacToeBoardSize + j]); }
        if (c != static_cast<int>(Player::kPlayerNone)) { return static_cast<Player>(c); }

        // columns
        c = 3;
        for (int j = 0; j < kTieTacToeBoardSize; ++j) { c &= static_cast<int>(board_[j * kTieTacToeBoardSize + i]); }
        if (c != static_cast<int>(Player::kPlayerNone)) { return static_cast<Player>(c); }
    }

    // diagonal (left-up to right-down)
    c = 3;
    for (int i = 0; i < kTieTacToeBoardSize; ++i) { c &= static_cast<int>(board_[i * kTieTacToeBoardSize + i]); }
    if (c != static_cast<int>(Player::kPlayerNone)) { return static_cast<Player>(c); }

    // diagonal (right-up to left-down)
    c = 3;
    for (int i = 0; i < kTieTacToeBoardSize; ++i) { c &= static_cast<int>(board_[i * kTieTacToeBoardSize + (kTieTacToeBoardSize - 1 - i)]); }
    if (c != static_cast<int>(Player::kPlayerNone)) { return static_cast<Player>(c); }

    return Player::kPlayerNone;
}

} // namespace minizero::env::tietactoe