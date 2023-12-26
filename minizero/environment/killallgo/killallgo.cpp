#include "killallgo.h"
#include "configuration.h"
#include "killallgo_seki_7x7.h"
#include <iostream>

namespace minizero::env::killallgo {

Seki7x7Table g_seki_7x7_table;

void initialize()
{
    go::initialize();

    constexpr int kSekiTableMinAreaSize = 5;
    constexpr int kSekiTableMaxAreaSize = 8;
    constexpr auto kSekiDBPath = "7x7_seki.db";

    if (!g_seki_7x7_table.load(kSekiDBPath)) {
        SekiSearch::generateSekiTable(g_seki_7x7_table, kSekiTableMinAreaSize, kSekiTableMaxAreaSize);
        g_seki_7x7_table.save(kSekiDBPath);

        std::cerr << "Generate " << kSekiDBPath << " done!" << std::endl;
        std::cerr << "Size: " << g_seki_7x7_table.size() << std::endl;
    }
}

bool KillAllGoEnv::isLegalAction(const KillAllGoAction& action) const
{
    if (actions_.size() == 1) { return isPassAction(action); }
    if (actions_.size() < 3) { return !isPassAction(action) && go::GoEnv::isLegalAction(action); }
    return go::GoEnv::isLegalAction(action);
}

bool KillAllGoEnv::isTerminal() const
{
    if (board_size_ == 7 && config::env_killallgo_use_seki && SekiSearch::isSeki(g_seki_7x7_table, *this)) { return true; }
    // all black's benson or any white's benson
    if (benson_bitboard_.get(Player::kPlayer1).count() == board_size_ * board_size_ || benson_bitboard_.get(Player::kPlayer2).count() > 0) { return true; }
    return go::GoEnv::isTerminal();
}

float KillAllGoEnv::getEvalScore(bool is_resign) const
{
    if (stone_bitboard_.get(Player::kPlayer2).count() == 0 || benson_bitboard_.get(Player::kPlayer1).count() == board_size_ * board_size_)
        return 1.0f; // player1 wins
    else
        return -1.0f; // player2 wins
}

} // namespace minizero::env::killallgo
