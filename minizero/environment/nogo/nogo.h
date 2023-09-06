#pragma once

#include "go.h"
#include "go_block.h"
#include "go_grid.h"
#include <string>

namespace minizero::env::nogo {

const std::string kNoGoName = "nogo";
const int kNoGoNumPlayer = 2;
const int kNoGoBoardSize = 9;

typedef go::GoAction NoGoAction;

class NoGoEnv : public go::GoEnv {
public:
    NoGoEnv() : go::GoEnv()
    {
        assert(kNoGoBoardSize == minizero::config::env_board_size);
    }

    bool isLegalAction(const NoGoAction& action) const override
    {
        assert(action.getActionID() >= 0 && action.getActionID() <= board_size_ * board_size_);
        assert(action.getPlayer() == Player::kPlayer1 || action.getPlayer() == Player::kPlayer2);

        if (isPassAction(action)) { return false; }

        const int position = action.getActionID();
        const Player player = action.getPlayer();
        const go::GoGrid& grid = grids_[position];
        if (grid.getPlayer() != Player::kPlayerNone) { return false; }

        // illegal when suicide or capture opponent's stones
        bool is_legal = false;
        go::GoBitboard check_neighbor_block_bitboard;
        for (const auto& neighbor_pos : grid.getNeighbors()) {
            const go::GoGrid& neighbor_grid = grids_[neighbor_pos];
            if (neighbor_grid.getPlayer() == Player::kPlayerNone) {
                is_legal = true;
            } else {
                const go::GoBlock* neighbor_block = neighbor_grid.getBlock();
                if (check_neighbor_block_bitboard.test(neighbor_block->getID())) { continue; }

                check_neighbor_block_bitboard.set(neighbor_block->getID());
                if (neighbor_block->getPlayer() == player) {
                    if (neighbor_block->getNumLiberty() > 1) { is_legal = true; }
                } else {
                    if (neighbor_block->getNumLiberty() == 1) { return false; }
                }
            }
        }
        return is_legal;
    }

    bool isTerminal() const override
    {
        for (int pos = 0; pos < board_size_ * board_size_; ++pos) {
            NoGoAction action(pos, turn_);
            if (isLegalAction(action)) { return false; }
        }
        return true;
    }

    float getEvalScore(bool is_resign = false) const override
    {
        Player eval = getNextPlayer(turn_, kNoGoNumPlayer);
        switch (eval) {
            case Player::kPlayer1: return 1.0f;
            case Player::kPlayer2: return -1.0f;
            default: return 0.0f;
        }
    }

    inline std::string name() const override { return kNoGoName + "_" + std::to_string(board_size_) + "x" + std::to_string(board_size_); }
    inline int getNumPlayer() const override { return kNoGoNumPlayer; }
};

class NoGoEnvLoader : public go::GoEnvLoader {
public:
    inline std::string name() const override { return kNoGoName + "_" + std::to_string(getBoardSize()) + "x" + std::to_string(getBoardSize()); }
};

} // namespace minizero::env::nogo
