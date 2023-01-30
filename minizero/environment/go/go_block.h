#pragma once

#include "go_unit.h"

namespace minizero::env::go {

class GoBlock {
public:
    GoBlock(int id)
        : id_(id)
    {
        reset();
    }

    inline void reset()
    {
        num_grid_ = 0;
        num_liberty_ = 0;
        player_ = Player::kPlayerNone;
        hash_key_ = 0;
        grid_bitboard_.reset();
        liberty_bitboard_.reset();
        neighbor_area_id_bitboard_.reset();
    }

    inline void combineWithBlock(GoBlock* block)
    {
        assert(block);
        addHashKey(block->getHashKey());
        grid_bitboard_ |= block->getGridBitboard();
        num_grid_ += block->getNumGrid();
        liberty_bitboard_ |= block->getLibertyBitboard();
        num_liberty_ = liberty_bitboard_.count();
        neighbor_area_id_bitboard_ |= block->getNeighborAreaIDBitboard();
    }

    // setter
    inline void setPlayer(Player p) { player_ = p; }
    inline void addHashKey(GoHashKey key) { hash_key_ ^= key; }
    inline void addGrid(int pos)
    {
        assert(!grid_bitboard_.test(pos));
        ++num_grid_;
        grid_bitboard_.set(pos);
    }
    inline void addLiberty(int pos)
    {
        if (liberty_bitboard_.test(pos)) { return; }
        liberty_bitboard_.set(pos);
        ++num_liberty_;
    }
    inline void removeLiberty(int pos)
    {
        if (!liberty_bitboard_.test(pos)) { return; }
        liberty_bitboard_.reset(pos);
        --num_liberty_;
    }
    inline void addNeighborAreaIDBitboard(int area_id) { neighbor_area_id_bitboard_.set(area_id); }
    inline void removeNeighborAreaIDBitboard(int area_id) { neighbor_area_id_bitboard_.reset(area_id); }

    // getter
    inline int getID() const { return id_; }
    inline int getNumGrid() const { return num_grid_; }
    inline int getNumLiberty() const { return num_liberty_; }
    inline Player getPlayer() const { return player_; }
    inline GoHashKey getHashKey() const { return hash_key_; }
    inline GoBitboard& getGridBitboard() { return grid_bitboard_; }
    inline const GoBitboard& getGridBitboard() const { return grid_bitboard_; }
    inline GoBitboard& getLibertyBitboard() { return liberty_bitboard_; }
    inline const GoBitboard& getLibertyBitboard() const { return liberty_bitboard_; }
    inline GoBitboard& getNeighborAreaIDBitboard() { return neighbor_area_id_bitboard_; }
    inline const GoBitboard& getNeighborAreaIDBitboard() const { return neighbor_area_id_bitboard_; }

private:
    int id_;
    int num_grid_;
    int num_liberty_;
    Player player_;
    GoHashKey hash_key_;
    GoBitboard grid_bitboard_;
    GoBitboard liberty_bitboard_;
    GoBitboard neighbor_area_id_bitboard_;
};

} // namespace minizero::env::go
