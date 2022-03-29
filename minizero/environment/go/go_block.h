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
        player_ = Player::kPlayerNone;
        hash_key_ = 0;
        grid_bitboard_.reset();
        liberty_bitboard_.reset();
    }

    inline void combineWithBlock(GoBlock* block)
    {
        assert(block);
        addHashKey(block->getHashKey());
        addGrids(block->getGridBitboard());
        addLiberties(block->getLibertyBitboard());
    }

    // setter
    inline void setPlayer(Player p) { player_ = p; }
    inline void addHashKey(GoHashKey key) { hash_key_ ^= key; }
    inline void addGrid(int pos) { grid_bitboard_.set(pos); }
    inline void addGrids(const GoBitboard& grids) { grid_bitboard_ |= grids; }
    inline void addLiberty(int pos) { liberty_bitboard_.set(pos); }
    inline void addLiberties(const GoBitboard& liberties) { liberty_bitboard_ |= liberties; }
    inline void removeLiberty(int pos) { liberty_bitboard_.reset(pos); }
    inline void removeLiberties(const GoBitboard& liberties) { liberty_bitboard_ &= ~liberties; }

    // getter
    inline int getID() const { return id_; }
    inline Player getPlayer() const { return player_; }
    inline GoHashKey getHashKey() const { return hash_key_; }
    inline GoBitboard& getGridBitboard() { return grid_bitboard_; }
    inline const GoBitboard& getGridBitboard() const { return grid_bitboard_; }
    inline GoBitboard& getLibertyBitboard() { return liberty_bitboard_; }
    inline const GoBitboard& getLibertyBitboard() const { return liberty_bitboard_; }
    inline int getNumLiberty() const { return liberty_bitboard_.count(); }
    inline int getNumStone() const { return grid_bitboard_.count(); }

private:
    int id_;
    Player player_;
    GoHashKey hash_key_;
    GoBitboard grid_bitboard_;
    GoBitboard liberty_bitboard_;
};

} // namespace minizero::env::go