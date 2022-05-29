#include "killallgo.h"

namespace minizero::env::killallgo {

using namespace env::go;

void KillAllGoEnv::addSekiIntoBenson(const KillAllGoAction& action)
{
    if (isPassAction(action)) { return; }
    assert(grids_[action.getActionID()].getPlayer() != Player::kPlayerNone);

    const int position = action.getActionID();
    const GoGrid& grid = grids_[position];

    const GoArea* seki_area = nullptr;
    if (grid.getPlayer() == Player::kPlayer1) {
        if (grid.getArea(Player::kPlayer2) && isSeki(grid.getArea(Player::kPlayer2))) { seki_area = grid.getArea(Player::kPlayer2); }
    } else if (grid.getPlayer() == Player::kPlayer2) {
        const GoBlock* block = grid.getBlock();
        GoBitboard area_bitboard_id = block->getNeighborAreaID();
        while (!area_bitboard_id.none()) {
            int area_id = area_bitboard_id._Find_first();
            area_bitboard_id.reset(area_id);

            GoArea* area = &areas_[area_id];
            if (!isSeki(area)) { continue; }
            seki_area = area;
            break;
        }
    }

    if (!seki_area) { return; }
    benson_bitboard_.get(Player::kPlayer2) |= seki_area->getAreaBitboard();
    benson_bitboard_.get(Player::kPlayer2) |= blocks_[seki_area->getNeighborBlockID()._Find_first()].getGridBitboard();
}

bool KillAllGoEnv::isSeki(const GoArea* area) const
{
    // we only handle the following three seki shapes:
    //     1.              2.                3.
    //        O O O           O O O O           O O O O O O O
    //        O . O           O . X O O O       O . X X X . O
    //        O X O O O       O O X X . O
    //        O X X . O
    // if X becomes the following three shapes after playing on any of its liberty positions, then it is a seki:
    //     1.        2.        3.
    //        X         X         X X X X
    //        X         X X
    //        X X         X
    // Note: case 1 shouldn't be in the corner

    assert(area && area->getPlayer() == Player::kPlayer2);
    if (area->getNumGrid() != 5 || area->getNeighborBlockID().count() != 1) { return false; }

    GoBitboard seki_stone_bitboard = area->getAreaBitboard() & stone_bitboard_.get(Player::kPlayer1);
    if (seki_stone_bitboard.count() != 3) { return false; }

    const GoBlock* block = grids_[seki_stone_bitboard._Find_first()].getBlock();
    if (block->getGridBitboard() != seki_stone_bitboard || block->getNumLiberty() != 2) { return false; }

    GoBitboard liberty_bitboard = block->getLibertyBitboard();
    while (!liberty_bitboard.none()) {
        int pos = liberty_bitboard._Find_first();
        liberty_bitboard.reset(pos);

        GoBitboard dilation_bitboard;
        dilation_bitboard.set(pos);
        for (int i = 0; i < 2; ++i) { dilation_bitboard = dilateBitboard(dilation_bitboard); }
        if ((block->getGridBitboard() & ~dilation_bitboard).none()) { return false; }

        GoBitboard shape_bitboard = block->getGridBitboard();
        shape_bitboard.set(pos);
        if ((dilateBitboard(shape_bitboard) & ~shape_bitboard).count() < 5) { return false; }
    }
    return true;
}

bool KillAllGoEnv::isCaptureMove(const KillAllGoAction& action) const
{
    if (isPassAction(action)) { return false; }
    assert(grids_[action.getActionID()].getPlayer() == Player::kPlayerNone);

    const Player player = action.getPlayer();
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

    const Player player = action.getPlayer();
    const int position = action.getActionID();
    const GoGrid& grid = grids_[position];

    bool is_eat_ko = false;
    for (const auto& neighbor_pos : grid.getNeighbors()) {
        const GoGrid& neighbor_grid = grids_[neighbor_pos];
        const GoBlock* neighbor_block = neighbor_grid.getBlock();

        if (neighbor_block == nullptr) { return false; }
        if (neighbor_block->getPlayer() == player) { return false; }
        if (neighbor_block->getNumLiberty() == 1) {
            if (neighbor_block->getNumGrid() > 1) { return false; }
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

    const Player player = action.getPlayer();
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

    const Player player = action.getPlayer();
    const Player opp_player = getNextPlayer(player, kKillAllGoNumPlayer);
    const int position = action.getActionID();
    const GoGrid& grid = grids_[position];

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