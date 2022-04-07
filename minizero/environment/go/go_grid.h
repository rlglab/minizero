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
        reset(board_size);
    }

    inline void reset(int board_size)
    {
        player_ = Player::kPlayerNone;
        block_ = nullptr;
        initializeNeighbors(board_size);
    }

    // setter
    inline void setPlayer(Player p) { player_ = p; }
    inline void setBlock(GoBlock* b) { block_ = b; }

    // getter
    inline Player getPlayer() const { return player_; }
    inline int getPosition() const { return position_; }
    inline GoBlock* getBlock() { return block_; }
    inline const GoBlock* getBlock() const { return block_; }
    inline std::vector<int> getNeighbors() { return neighbors_; }
    inline const std::vector<int> getNeighbors() const { return neighbors_; }

private:
    void initializeNeighbors(int board_size)
    {
        const std::vector<int> directions = {0, 1, 0, -1};
        int x = position_ % board_size, y = position_ / board_size;
        neighbors_.clear();
        for (size_t i = 0; i < directions.size(); ++i) {
            int new_x = x + directions[i];
            int new_y = y + directions[(i + 1) % directions.size()];
            if (!isInBoard(new_x, new_y, board_size)) { continue; }
            neighbors_.push_back(new_y * board_size + new_x);
        }
    }

    inline bool isInBoard(int x, int y, int board_size)
    {
        return (x >= 0 && x < board_size && y >= 0 && y < board_size);
    }

    int position_;
    Player player_;
    GoBlock* block_;
    std::vector<int> neighbors_;
};

} // namespace minizero::env::go