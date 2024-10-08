#pragma once

#include "base_env.h"
#include "conhex_graph_flag.h"
#include <bitset>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace minizero::env::conhex {

const int kConHexBoardSize = 9;
typedef std::bitset<kConHexBoardSize * kConHexBoardSize> ConHexBitboard;

class ConHexGraphCell {
public:
    ConHexGraphCell() = default;
    ConHexGraphCell(int cell_id, ConHexGraphCellType cell_type);

    int getCellId();

    Player getCapturedPlayer() const;
    void placeStone(int hole_id, Player player);
    void setEdgeFlag(ConHexGraphEdgeFlag cell_edge_flag);
    bool isEdgeFlag(ConHexGraphEdgeFlag cell_edge_flag);
    void reset();

private:
    ConHexGraphEdgeFlag edge_flag_;
    ConHexGraphCellType cell_type_;
    Player capture_player_;
    GamePair<ConHexBitboard> holes_;
    GamePair<int> captured_count_;
    int cell_id_;
};

} // namespace minizero::env::conhex
