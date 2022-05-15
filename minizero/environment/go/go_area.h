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
        num_stone_ = 0;
        player_ = Player::kPlayerNone;
        area_bitboard_.reset();
        neighbor_block_id_.reset();
    }

    inline void combineWithArea(GoArea* area)
    {
        assert(area && player_ == area->getPlayer());
        num_stone_ += area->getNumStone();
        area_bitboard_ |= area->getAreaBitboard();
        neighbor_block_id_ |= area->getNeighborBlockID();
    }

    // setter
    inline void setNumStone(int num_stone) { num_stone_ = num_stone; }
    inline void setPlayer(Player p) { player_ = p; }
    inline void setAreaBitBoard(const GoBitboard& area_bitboard) { area_bitboard_ = area_bitboard; }
    inline void addNeighborBlockID(int block_id) { neighbor_block_id_.set(block_id); }
    inline void removeNeighborBlockID(int block_id) { neighbor_block_id_.reset(block_id); }

    // getter
    inline int getID() const { return id_; }
    inline int getNumStone() const { return num_stone_; }
    inline Player getPlayer() const { return player_; }
    inline GoBitboard& getAreaBitboard() { return area_bitboard_; }
    inline const GoBitboard& getAreaBitboard() const { return area_bitboard_; }
    inline GoBitboard& getNeighborBlockID() { return neighbor_block_id_; }
    inline const GoBitboard& getNeighborBlockID() const { return neighbor_block_id_; }

private:
    int id_;
    int num_stone_;
    Player player_;
    GoBitboard area_bitboard_;
    GoBitboard neighbor_block_id_;
};

} // namespace minizero::env::go