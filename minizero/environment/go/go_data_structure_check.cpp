#include "go.h"

namespace minizero::env::go {

bool GoEnv::checkDataStructure() const
{
    assert(actions_.size() == stone_bitboard_history_.size());
    assert(checkGridDataStructure());
    assert(checkBlockDataStructure());
    assert(checkAreaDataStructure());
    assert(checkBensonDataStructure());
    return true;
}

bool GoEnv::checkGridDataStructure() const
{
    GamePair<GoBitboard> stone_bitboard;
    GoHashKey hash_key = (actions_.size() % 2 == 0 ? 0 : getGoTurnHashKey());
    for (int pos = 0; pos < board_size_ * board_size_; ++pos) {
        const GoGrid& grid = grids_[pos];

        // blocks
        if (grid.getPlayer() != Player::kPlayerNone) {
            assert(grid.getBlock());
            assert(grid.getBlock()->getPlayer() == grid.getPlayer());
            assert(grid.getBlock()->getGridBitboard().test(pos));
            assert(!free_block_id_bitboard_.test(grid.getBlock()->getID()));
            assert(grid.getArea(grid.getPlayer()) == nullptr);
            stone_bitboard.get(grid.getPlayer()).set(pos);
            hash_key ^= getGoGridHashKey(pos, grid.getPlayer());
        }

        // areas
        assert(!grid.getArea(Player::kPlayer1) || (grid.getArea(Player::kPlayer1) && !free_area_id_bitboard_.test(grid.getArea(Player::kPlayer1)->getID())));
        assert(!grid.getArea(Player::kPlayer2) || (grid.getArea(Player::kPlayer2) && !free_area_id_bitboard_.test(grid.getArea(Player::kPlayer2)->getID())));
    }
    assert(hash_key == hash_key_);
    assert(stone_bitboard.get(Player::kPlayer1) == stone_bitboard_.get(Player::kPlayer1));
    assert(stone_bitboard.get(Player::kPlayer2) == stone_bitboard_.get(Player::kPlayer2));
    return true;
}

bool GoEnv::checkBlockDataStructure() const
{
    GamePair<GoBitboard> stone_bitboard;
    GoHashKey hash_key = actions_.size() % 2 == 0 ? 0 : getGoTurnHashKey();
    GoBitboard block_id_bitboard = ~free_block_id_bitboard_ & board_mask_bitboard_;
    while (!block_id_bitboard.none()) {
        int id = block_id_bitboard._Find_first();
        block_id_bitboard.reset(id);

        assert(id < static_cast<int>(blocks_.size()));
        const GoBlock* block = &blocks_[id];
        assert(block->getNumLiberty() > 0 && block->getNumGrid() > 0);
        assert(block->getNumGrid() == static_cast<int>(block->getGridBitboard().count()));
        hash_key ^= block->getHashKey();
        stone_bitboard.get(block->getPlayer()) |= block->getGridBitboard();

        // grids
        GoHashKey block_hash_key = 0;
        GoBitboard liberty_bitboard;
        GoBitboard grid_bitboard = block->getGridBitboard();
        while (!grid_bitboard.none()) {
            int pos = grid_bitboard._Find_first();
            grid_bitboard.reset(pos);

            const GoGrid& grid = grids_[pos];
            assert(grid.getBlock() == block);
            assert(grid.getPlayer() == block->getPlayer());
            block_hash_key ^= getGoGridHashKey(pos, block->getPlayer());
            for (const auto& neighbor_pos : grid.getNeighbors()) {
                const GoGrid& neighbor_grid = grids_[neighbor_pos];
                if (neighbor_grid.getPlayer() != Player::kPlayerNone) { continue; }
                liberty_bitboard.set(neighbor_pos);
            }
        }
        assert(block_hash_key == block->getHashKey());
        assert(liberty_bitboard == block->getLibertyBitboard());

        // areas
        GoBitboard area_id = block->getNeighborAreaIDBitboard();
        while (!area_id.none()) {
            int id = area_id._Find_first();
            area_id.reset(id);
            assert(areas_[id].getPlayer() == block->getPlayer());
            assert(!free_area_id_bitboard_.test(areas_[id].getID()));
            assert(!(areas_[id].getAreaBitboard() & (dilateBitboard(block->getGridBitboard()) & ~block->getGridBitboard())).none());
        }
    }
    assert(hash_key == hash_key_);
    assert(stone_bitboard.get(Player::kPlayer1) == stone_bitboard_.get(Player::kPlayer1));
    assert(stone_bitboard.get(Player::kPlayer2) == stone_bitboard_.get(Player::kPlayer2));
    return true;
}

bool GoEnv::checkAreaDataStructure() const
{
    GamePair<GoBitboard> area_bitboard_pair;
    GoBitboard area_id_bitboard = ~free_area_id_bitboard_ & board_mask_bitboard_;
    while (!area_id_bitboard.none()) {
        int id = area_id_bitboard._Find_first();
        area_id_bitboard.reset(id);

        assert(id < static_cast<int>(areas_.size()));
        const GoArea* area = &areas_[id];
        assert(!area->getAreaBitboard().none());
        assert(area->getNumGrid() == static_cast<int>(area->getAreaBitboard().count()));
        assert(floodFillBitBoard(area->getAreaBitboard()._Find_first(), (~stone_bitboard_.get(area->getPlayer()) & board_mask_bitboard_)) == area->getAreaBitboard());
        assert((area->getAreaBitboard() & area_bitboard_pair.get(area->getPlayer())).none());
        area_bitboard_pair.get(area->getPlayer()) |= area->getAreaBitboard();

        // grids
        GoBitboard area_bitboard = area->getAreaBitboard();
        while (!area_bitboard.none()) {
            int pos = area_bitboard._Find_first();
            area_bitboard.reset(pos);
            assert(grids_[pos].getPlayer() != area->getPlayer());
            assert(grids_[pos].getArea(area->getPlayer()) == area);
        }

        // blocks
        GoBitboard area_neighbor_block_bitboard = dilateBitboard(area->getAreaBitboard()) & stone_bitboard_.get(area->getPlayer());
        while (!area_neighbor_block_bitboard.none()) {
            int pos = area_neighbor_block_bitboard._Find_first();
            const GoBlock* block = grids_[pos].getBlock();
            assert(block);
            assert(block->getNeighborAreaIDBitboard().test(area->getID()));
            assert(area->getNeighborBlockIDBitboard().test(block->getID()));
            area_neighbor_block_bitboard &= ~block->getGridBitboard();
        }
    }
    if (stone_bitboard_.get(Player::kPlayer1).none()) { assert(area_bitboard_pair.get(Player::kPlayer1).none()); }
    if (stone_bitboard_.get(Player::kPlayer2).none()) { assert(area_bitboard_pair.get(Player::kPlayer2).none()); }
    if (!stone_bitboard_.get(Player::kPlayer1).none()) { assert((area_bitboard_pair.get(Player::kPlayer1) | stone_bitboard_.get(Player::kPlayer1)) == board_mask_bitboard_); }
    if (!stone_bitboard_.get(Player::kPlayer2).none()) { assert((area_bitboard_pair.get(Player::kPlayer2) | stone_bitboard_.get(Player::kPlayer2)) == board_mask_bitboard_); }
    return true;
}

bool GoEnv::checkBensonDataStructure() const
{
    assert(findBensonBitboard(stone_bitboard_.get(Player::kPlayer1)) == benson_bitboard_.get(Player::kPlayer1));
    assert(findBensonBitboard(stone_bitboard_.get(Player::kPlayer2)) == benson_bitboard_.get(Player::kPlayer2));
    return true;
}

} // namespace minizero::env::go
