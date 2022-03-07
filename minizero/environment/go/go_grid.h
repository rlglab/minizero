#pragma once

#include "go_block.h"
#include "go_unit.h"
#include <vector>

namespace minizero::env::go {

class GoGrid {
public:
    GoGrid(int position, int board_size)
        : position_(position)
    {
        Reset(board_size);
    }

    inline void Reset(int board_size)
    {
        player_ = Player::kPlayerNone;
        block_ = nullptr;
        InitializeNeighbors(board_size);
    }

    // setter
    inline void SetPlayer(Player p) { player_ = p; }
    inline void SetBlock(GoBlock* b) { block_ = b; }

    // getter
    inline Player GetPlayer() const { return player_; }
    inline int GetPosition() const { return position_; }
    inline GoBlock* GetBlock() { return block_; }
    inline const GoBlock* GetBlock() const { return block_; }
    inline std::vector<int> GetNeighbors() { return neighbors_; }
    inline const std::vector<int> GetNeighbors() const { return neighbors_; }

private:
    void InitializeNeighbors(int board_size)
    {
        const std::vector<int> directions = {0, 1, 0, -1};
        int x = position_ % board_size, y = position_ / board_size;
        neighbors_.clear();
        for (size_t i = 0; i < directions.size(); ++i) {
            int new_x = x + directions[i];
            int new_y = y + directions[(i + 1) % directions.size()];
            if (!IsInBoard(new_x, new_y, board_size)) { continue; }
            neighbors_.push_back(new_y * board_size + new_x);
        }
    }

    inline bool IsInBoard(int x, int y, int board_size)
    {
        return (x >= 0 && x < board_size && y >= 0 && y < board_size);
    }

    int position_;
    Player player_;
    GoBlock* block_;
    std::vector<int> neighbors_;
};

} // namespace minizero::env::go