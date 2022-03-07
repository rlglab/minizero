#pragma once

#include "go_unit.h"

namespace minizero::env::go {

class GoBlock {
public:
    GoBlock(int id)
        : id_(id)
    {
        Reset();
    }

    inline void Reset()
    {
        player_ = Player::kPlayerNone;
        hash_key_ = 0;
        grid_bitboard_.reset();
        liberty_bitboard_.reset();
    }

    inline void CombineWithBlock(GoBlock* block)
    {
        assert(block);
        AddHashKey(block->GetHashKey());
        AddGrids(block->GetGridBitboard());
        AddLiberties(block->GetLibertyBitboard());
    }

    // setter
    inline void SetPlayer(Player p) { player_ = p; }
    inline void AddHashKey(GoHashKey key) { hash_key_ ^= key; }
    inline void AddGrid(int pos) { grid_bitboard_.set(pos); }
    inline void AddGrids(const GoBitboard& grids) { grid_bitboard_ |= grids; }
    inline void AddLiberty(int pos) { liberty_bitboard_.set(pos); }
    inline void AddLiberties(const GoBitboard& liberties) { liberty_bitboard_ |= liberties; }
    inline void RemoveLiberty(int pos) { liberty_bitboard_.reset(pos); }
    inline void RemoveLiberties(const GoBitboard& liberties) { liberty_bitboard_ &= ~liberties; }

    // getter
    inline int GetID() const { return id_; }
    inline Player GetPlayer() const { return player_; }
    inline GoHashKey GetHashKey() const { return hash_key_; }
    inline GoBitboard& GetGridBitboard() { return grid_bitboard_; }
    inline const GoBitboard& GetGridBitboard() const { return grid_bitboard_; }
    inline GoBitboard& GetLibertyBitboard() { return liberty_bitboard_; }
    inline const GoBitboard& GetLibertyBitboard() const { return liberty_bitboard_; }
    inline int GetNumLiberty() const { return liberty_bitboard_.count(); }
    inline int GetNumStone() const { return grid_bitboard_.count(); }

private:
    int id_;
    Player player_;
    GoHashKey hash_key_;
    GoBitboard grid_bitboard_;
    GoBitboard liberty_bitboard_;
};

} // namespace minizero::env::go