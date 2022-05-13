#include "killallgo.h"

namespace minizero::env::killallgo {

using namespace env::go;

bool KillAllGoEnv::isCaptureMove(const KillAllGoAction& action) const
{
    if (isPassAction(action)) { return false; }
    assert(grids_[action.getActionID()].getPlayer() == Player::kPlayerNone);

    Player player = action.getPlayer();
    const int position = action.getActionID();
    const GoGrid& grid = grids_[position];

    for (const auto& neighbor_pos : grid.getNeighbors()) {
        const GoGrid& neighbor_grid = grids_[neighbor_pos];
        const GoBlock* neighbor_block = neighbor_grid.getBlock();
        if (neighbor_block == nullptr) { continue; }
        if (neighbor_block->getPlayer() == player) { continue; }
        if (neighbor_block->getNumLiberty() == 1) { return true; }
    }

    return false;
}

bool KillAllGoEnv::isEatKoMove(const KillAllGoAction& action) const
{
    if (isPassAction(action)) { return false; }
    assert(grids_[action.getActionID()].getPlayer() == Player::kPlayerNone);

    const int position = action.getActionID();
    const GoGrid& grid = grids_[position];
    Player player = action.getPlayer();

    bool is_eat_ko = false;
    for (const auto& neighbor_pos : grid.getNeighbors()) {
        const GoGrid& neighbor_grid = grids_[neighbor_pos];
        const GoBlock* neighbor_block = neighbor_grid.getBlock();

        if (neighbor_block == nullptr) { return false; }
        if (neighbor_block->getPlayer() == player) { return false; }
        if (neighbor_block->getNumLiberty() == 1) {
            if (neighbor_block->getNumStone() > 1) { return false; }
            // lib==1 && num stone==1
            if (is_eat_ko) {
                return false;
            } else {
                is_eat_ko = true;
            }
        }
    }

    return is_eat_ko;
}

GoBitboard KillAllGoEnv::getStoneBitBoardAfterPlay(const KillAllGoAction& action) const
{
    assert(!isPassAction(action));
    assert(grids_[action.getActionID()].getPlayer() == Player::kPlayerNone);

    Player player = action.getPlayer();
    const int position = action.getActionID();
    const GoGrid& grid = grids_[position];
    GoBitboard stone_bitboard_after_play;
    for (const auto& neighbor_pos : grid.getNeighbors()) {
        const GoGrid& nbr_grid = grids_[neighbor_pos];
        if (nbr_grid.getPlayer() == player) {
            stone_bitboard_after_play |= nbr_grid.getBlock()->getGridBitboard();
        }
    }
    stone_bitboard_after_play.set(position);

    return stone_bitboard_after_play;
}

GoBitboard KillAllGoEnv::getLibertyBitBoardAfterPlay(const KillAllGoAction& action) const
{
    assert(!isPassAction(action));
    assert(grids_[action.getActionID()].getPlayer() == Player::kPlayerNone);

    const int position = action.getActionID();
    const GoGrid& grid = grids_[position];

    Player player = action.getPlayer();
    Player opp_player = getNextPlayer(player, kKillAllGoNumPlayer);
    GoBitboard stone_bitboard = (getStoneBitboard().get(env::Player::kPlayer1) | getStoneBitboard().get(env::Player::kPlayer2));
    GoBitboard liberty_bitboard_after_play;
    liberty_bitboard_after_play.set(action.getActionID());
    liberty_bitboard_after_play = dilateBitboard(liberty_bitboard_after_play);

    for (const auto& neighbor_pos : grid.getNeighbors()) {
        const GoGrid& nbr_grid = grids_[neighbor_pos];
        if (nbr_grid.getPlayer() == player) {
            liberty_bitboard_after_play |= dilateBitboard(nbr_grid.getBlock()->getGridBitboard());
        } else if (nbr_grid.getPlayer() == opp_player) {
            if (nbr_grid.getBlock()->getNumLiberty() == 1) {
                stone_bitboard &= ~(nbr_grid.getBlock()->getGridBitboard());
            }
        }
    }

    liberty_bitboard_after_play &= (~stone_bitboard);
    liberty_bitboard_after_play.reset(action.getActionID());

    return liberty_bitboard_after_play;
}

} // namespace minizero::env::killallgo