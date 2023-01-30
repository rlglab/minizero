#pragma once

#include "go_unit.h"

namespace minizero::env::go {

class GoArea {
public:
    GoArea(int id)
        : id_(id)
    {
        reset();
    }

    inline void reset()
    {
        num_grid_ = 0;
        player_ = Player::kPlayerNone;
        area_bitboard_.reset();
        neighbor_block_id_bitboard_.reset();
    }

    inline void combineWithArea(GoArea* area)
    {
        assert(area && player_ == area->getPlayer());
        num_grid_ += area->getNumGrid();
        area_bitboard_ |= area->getAreaBitboard();
        neighbor_block_id_bitboard_ |= area->getNeighborBlockIDBitboard();
    }

    // setter
    inline void setNumGrid(int num_grid) { num_grid_ = num_grid; }
    inline void setPlayer(Player p) { player_ = p; }
    inline void setAreaBitBoard(const GoBitboard& area_bitboard) { area_bitboard_ = area_bitboard; }
    inline void addNeighborBlockIDBitboard(int block_id) { neighbor_block_id_bitboard_.set(block_id); }
    inline void removeNeighborBlockIDBitboard(int block_id) { neighbor_block_id_bitboard_.reset(block_id); }

    // getter
    inline int getID() const { return id_; }
    inline int getNumGrid() const { return num_grid_; }
    inline Player getPlayer() const { return player_; }
    inline GoBitboard& getAreaBitboard() { return area_bitboard_; }
    inline const GoBitboard& getAreaBitboard() const { return area_bitboard_; }
    inline GoBitboard& getNeighborBlockIDBitboard() { return neighbor_block_id_bitboard_; }
    inline const GoBitboard& getNeighborBlockIDBitboard() const { return neighbor_block_id_bitboard_; }

private:
    int id_;
    int num_grid_;
    Player player_;
    GoBitboard area_bitboard_;
    GoBitboard neighbor_block_id_bitboard_;
};

} // namespace minizero::env::go
