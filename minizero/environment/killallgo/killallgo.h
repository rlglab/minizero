#pragma once

#include "go.h"

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
        minizero::config::env_go_board_size = kKillAllGoBoardSize;
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

    inline std::string name() const override { return kKillAllGoName; }

    go::GoBitboard getWinnerRZoneBitboard(const go::GoBitboard& child_bitboard, const KillAllGoAction& win_action) const;
    go::GoBitboard getLoserRZoneBitboard(const go::GoBitboard& union_bitboard, const Player& player) const;
    bool isRelevantMove(const go::GoBitboard& bitboard, const KillAllGoAction& action) const;

private:
    go::GoBitboard getMoveInfluence(const KillAllGoAction& action) const;
    go::GoBitboard getMoveRZone(const go::GoBitboard& bitboard_rzone, go::GoBitboard bitboard_own_influence) const;
    go::GoBitboard getLegalizeRZone(go::GoBitboard bitboard, const Player& player) const;
    go::GoBitboard getSuicidalRZone(go::GoBitboard bitboard, const Player& player) const;
    bool isCaptureMove(const KillAllGoAction& action) const;
    inline bool isSuicidalMove(const KillAllGoAction& action) const { return (getLibertyBitBoardAfterPlay(action) == 0); }
    bool isEatKoMove(const KillAllGoAction& action) const;
    go::GoBitboard getStoneBitBoardAfterPlay(const KillAllGoAction& action) const;
    go::GoBitboard getLibertyBitBoardAfterPlay(const KillAllGoAction& action) const;
    inline int getNumLibertyAfterPlay(const KillAllGoAction& action) const { return getLibertyBitBoardAfterPlay(action).count(); }
};

class KillAllGoEnvLoader : public go::GoEnvLoader {
public:
    inline std::string getEnvName() const { return kKillAllGoName; }
};

} // namespace minizero::env::killallgo