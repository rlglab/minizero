#pragma once

#include "go.h"
#include <string>

namespace minizero::env::killallgo {

const std::string kKillAllGoName = "killallgo";
const int kKillAllGoNumPlayer = 2;
const int kKillAllGoBoardSize = 7;

typedef go::GoAction KillAllGoAction;

class KillAllGoEnv : public go::GoEnv {
public:
    KillAllGoEnv()
        : go::GoEnv(minizero::config::env_board_size = kKillAllGoBoardSize)
    {
    }

    bool isLegalAction(const KillAllGoAction& action) const override
    {
        if (actions_.size() == 1) { return isPassAction(action); }
        if (actions_.size() < 3) { return !isPassAction(action) && go::GoEnv::isLegalAction(action); }
        return go::GoEnv::isLegalAction(action);
    }

    bool isTerminal() const override
    {
        // all black's benson or any white's benson
        if (benson_bitboard_.get(Player::kPlayer1).count() == board_size_ * board_size_ || benson_bitboard_.get(Player::kPlayer2).count() > 0)
            return true;
        return go::GoEnv::isTerminal();
    }

    float getEvalScore(bool is_resign = false) const override
    {
        if (stone_bitboard_.get(Player::kPlayer2).count() == 0 || benson_bitboard_.get(Player::kPlayer1).count() == board_size_ * board_size_)
            return 1.0f; // player1 wins
        else
            return -1.0f; // player2 wins
    }

    inline std::string name() const override { return kKillAllGoName + "_" + std::to_string(board_size_) + "x" + std::to_string(board_size_); }
    inline int getNumPlayer() const override { return kKillAllGoNumPlayer; }
};

} // namespace minizero::env::killallgo
