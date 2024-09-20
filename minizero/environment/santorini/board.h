#pragma once
#include "base_env.h"
#include "bitboard.h"
#include <array>
#include <string>
#include <utility>
#include <vector>

namespace minizero::env::santorini {

class Board {
public:
    Board();
    Board(const Board& board_);
    ~Board() = default;

    std::vector<std::pair<int, int>> getLegalMove(int p_id) const;
    std::vector<int> getLegalBuild(int idx) const;
    std::pair<int, int> getPlayerIdx(int p_id) const;
    const std::array<Bitboard, 5>& getPlanes() const;
    bool isTerminal(int p_id) const;
    bool checkWin(int p_id) const;

    std::string toConsole() const;

    bool setPlayer(int p_id, int idx_1 = 0, int idx_2 = 0);
    bool movePiece(int from, int to);
    bool movePiece(Bitboard from, Bitboard to);
    bool buildTower(int idx);

private:
    // One bitboard for playes's pieces and four bitboards for building.
    std::array<Bitboard, 5> plane_;

    // Each player has two pieces. Totally use four bitboards.
    std::array<std::array<Bitboard, 2>, 2> player_;
};

} // namespace minizero::env::santorini
