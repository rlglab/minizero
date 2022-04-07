#pragma once

#include "go.h"
#include "go_benson.h"

namespace minizero::env::killallgo {

const std::string kKillAllGoName = "killallgo";
const int kKillAllGoNumPlayer = 2;
const int kKillAllGoBoardSize = 7;

typedef go::GoAction KillAllGoAction;

class KillAllGoEnv : public go::GoEnv {
public:
    KillAllGoEnv()
        : go::GoEnv(kKillAllGoBoardSize)
    {
    }

    bool isLegalAction(const KillAllGoAction& action) const override
    {
        if (actions_.size() == 1) { return isPassAction(action); }
        return go::GoEnv::isLegalAction(action);
    }
    bool isTerminal() const override
    {
        // TODO: rewrite this
        (*benson_bitboard_p) = go::GoBenson::getBensonBitboard(benson_bitboard, stone_bitboard_, board_size_, board_left_boundary_bitboard_,
                                                               board_right_boundary_bitboard_, board_mask_bitboard_);
        // all black's benson or any white's benson
        if (benson_bitboard.get(Player::kPlayer1).count() == board_size_ * board_size_ || benson_bitboard.get(Player::kPlayer2).count() > 0)
            return true;
        return go::GoEnv::isTerminal();
    }

    float getEvalScore(bool is_resign = false) const override
    {
        // TODO: rewrite this
        if (stone_bitboard_.get(Player::kPlayer2).count() == 0 || benson_bitboard.get(Player::kPlayer1).count() == board_size_ * board_size_)
            return 1.0f;// player1 wins
        else
           return -1.0f;// player2 wins
    }

    inline std::string name() const override { return kKillAllGoName; }
};

class KillAllGoEnvLoader : public go::GoEnvLoader {
public:
    inline std::string getEnvName() const { return kKillAllGoName; }
};

} // namespace minizero::env::killallgo