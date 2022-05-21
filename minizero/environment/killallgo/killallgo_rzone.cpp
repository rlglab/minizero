#include "killallgo.h"
#include <iostream>

namespace minizero::env::killallgo {

using namespace minizero::env::go;

GoBitboard KillAllGoEnv::getWinnerRZoneBitboard(const GoBitboard& child_bitboard, const KillAllGoAction& win_action) const
{
    GoBitboard updated_rzone_bitboard = child_bitboard;
    GoBitboard move_influence_bitboard = getMoveInfluence(win_action);

    if ((child_bitboard & move_influence_bitboard).any()) {
        GoBitboard own_influence_block = move_influence_bitboard & getStoneBitboard().get(win_action.getPlayer());
        updated_rzone_bitboard = getMoveRZone(child_bitboard, own_influence_block);
        updated_rzone_bitboard |= move_influence_bitboard;
    }

    return updated_rzone_bitboard;
}

GoBitboard KillAllGoEnv::getMoveInfluence(const KillAllGoAction& action) const
{
    GoBitboard move_influecne;
    const Player player = action.getPlayer();
    const Player opp_player = getNextPlayer(player, kKillAllGoNumPlayer);
    if (isCaptureMove(action)) {
        const GoGrid& grid = grids_[action.getActionID()];
        GoBitboard deadstone_bitboard;
        for (const auto& neighbor_pos : grid.getNeighbors()) {
            const GoGrid& nbr_grid = grids_[neighbor_pos];
            if (nbr_grid.getPlayer() != opp_player) { continue; }

            const GoBlock* nbr_block = nbr_grid.getBlock();
            if (nbr_block->getNumLiberty() == 1) { deadstone_bitboard |= nbr_block->getGridBitboard(); }
        }

        GoBitboard nbr_own_block_bitboard = dilateBitboard(deadstone_bitboard) & getStoneBitboard().get(player);
        while (!nbr_own_block_bitboard.none()) {
            int pos = nbr_own_block_bitboard._Find_first();
            nbr_own_block_bitboard.reset(pos);
            const GoBlock* own_block = grids_[pos].getBlock();
            nbr_own_block_bitboard &= ~(own_block->getGridBitboard());
            move_influecne |= own_block->getGridBitboard();
        }
        move_influecne |= deadstone_bitboard;
    }

    if (!isPassAction(action)) {
        move_influecne |= getStoneBitBoardAfterPlay(action);
    }

    return move_influecne;
}

GoBitboard KillAllGoEnv::getMoveRZone(const GoBitboard& rzone_bitboard, GoBitboard own_block_influence) const
{
    GoBitboard result_bitboard = rzone_bitboard;
    // find own block that has no z-liberty in rzone_bitboard
    std::vector<const GoBlock*> own_blocks;
    while (!own_block_influence.none()) {
        int pos = own_block_influence._Find_first();
        own_block_influence.reset(pos);
        const GoBlock* block = grids_[pos].getBlock();
        assert(block != nullptr);
        const GoBitboard& liberty_bitboard = block->getLibertyBitboard();
        // no intersection
        if ((liberty_bitboard & rzone_bitboard).none()) { own_blocks.push_back(block); }
        own_block_influence &= ~block->getGridBitboard();
    }

    if (own_blocks.size() == 1) {
        const GoBlock* block = own_blocks[0];
        const GoBitboard& liberty_bitboard = block->getLibertyBitboard();
        result_bitboard.set(liberty_bitboard._Find_first());
    } else if (own_blocks.size() > 1) {
        GoBitboard common_liberty_bitboard;
        common_liberty_bitboard = own_blocks[0]->getLibertyBitboard();
        for (unsigned int iBlock = 1; iBlock < own_blocks.size(); ++iBlock) {
            const GoBitboard& liberty_bitboard = own_blocks[iBlock]->getLibertyBitboard();
            common_liberty_bitboard &= liberty_bitboard;
        }

        result_bitboard |= common_liberty_bitboard;

        // add one liberty of the block which has no common liberties
        for (unsigned int iBlock = 0; iBlock < own_blocks.size(); ++iBlock) {
            const GoBitboard& liberty_bitboard = own_blocks[iBlock]->getLibertyBitboard();
            if ((common_liberty_bitboard & liberty_bitboard).any()) { continue; }

            result_bitboard.set(liberty_bitboard._Find_first());
        }
    }

    return result_bitboard;
}

GoBitboard KillAllGoEnv::getLoserRZoneBitboard(const GoBitboard& union_bitboard, const Player& player) const
{
    // update 2-liberties and suicidal R-zone
    GoBitboard before_bitboard = union_bitboard;
    GoBitboard after_bitboard;
    do {
        before_bitboard = getLegalizeRZone(before_bitboard, player);
        after_bitboard = getSuicidalRZone(before_bitboard, player);
        if (after_bitboard == before_bitboard) { break; }
        before_bitboard = after_bitboard;
    } while (1);

    return after_bitboard;
}

GoBitboard KillAllGoEnv::getLegalizeRZone(GoBitboard bitboard, const Player& player) const
{
    GoBitboard result_bitboard = bitboard;
    // Legalize R-zone
    while (!bitboard.none()) {
        int pos = bitboard._Find_first();
        bitboard.reset(pos);
        const GoGrid& grid = grids_[pos];
        if (grid.getPlayer() == Player::kPlayerNone) { continue; }

        const GoBlock* block = grid.getBlock();
        bitboard &= (~block->getGridBitboard());
        if (block->getPlayer() != player) { continue; }

        result_bitboard |= block->getGridBitboard();
        const GoBitboard& liberty_bitboard = block->getLibertyBitboard();
        int num_overlapped_lib = (liberty_bitboard & result_bitboard).count();
        if (num_overlapped_lib >= 2) { continue; }

        // numRequiredLib = 2 or 1
        int num_required_lib = 2 - num_overlapped_lib;
        GoBitboard remaining_liberty_bitboard = liberty_bitboard & (~result_bitboard);
        for (int i = 0; i < num_required_lib; i++) {
            if (remaining_liberty_bitboard.none()) { break; }

            int next_liberty = remaining_liberty_bitboard._Find_first();
            result_bitboard.set(next_liberty);
            remaining_liberty_bitboard.reset(next_liberty);
        }
    }

    return result_bitboard;
}

GoBitboard KillAllGoEnv::getSuicidalRZone(GoBitboard bitboard, const Player& player) const
{
    Player opp_color = getNextPlayer(player, kKillAllGoNumPlayer);
    GoBitboard result_bitboard = bitboard;
    while (!bitboard.none()) {
        int pos = bitboard._Find_first();
        bitboard.reset(pos);
        const GoGrid& grid = grids_[pos];
        if (grid.getPlayer() != Player::kPlayerNone) {
            bitboard &= ~(grid.getBlock()->getGridBitboard());
            continue;
        }
        KillAllGoAction action(pos, opp_color);
        if (!isSuicidalMove(action)) { continue; }

        GoBitboard dead_stone_bitboard = getStoneBitBoardAfterPlay(action);
        GoBitboard nbr_block_bitboard = dilateBitboard(dead_stone_bitboard) & getStoneBitboard().get(player);
        result_bitboard |= nbr_block_bitboard;
        result_bitboard |= dead_stone_bitboard;
    }

    return result_bitboard;
}

bool KillAllGoEnv::isRelevantMove(const GoBitboard& rzone_bitboard, const KillAllGoAction& action) const
{
    GoBitboard influence_bitboard = getMoveInfluence(action);
    return (rzone_bitboard & influence_bitboard).any();
}

} // namespace minizero::env::killallgo