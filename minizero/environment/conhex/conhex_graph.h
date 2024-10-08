#pragma once

#include "base_env.h"
#include "conhex_graph_cell.h"
#include "conhex_graph_flag.h"
#include "disjoint_set_union.h"
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace minizero::env::conhex {

class ConHexGraph {
public:
    ConHexGraph();

    bool isCellCapturedByPlayer(int cell_id, Player player) const;
    void placeStone(int hole_idx, Player player);
    Player getPlayerAtPos(int hole_idx) const;
    inline Player checkWinner() const { return winner_; }
    void reset();
    std::string toString() const;

private:
    void initGraph();
    void addCell(std::vector<int> hole_indexes, ConHexGraphEdgeFlag cell_edge_flag);

    DisjointSetUnion graph_dsu_;
    std::vector<std::vector<int>> hole_to_cell_map_; // hole_idx* -> cell_id, on same hole id may have many cell
    std::vector<std::set<int>> cell_adjacency_list_; // cell_id -> cell_id* , adj list

    std::vector<ConHexGraphCell> cells_;
    std::vector<Player> holes_;
    Player winner_;

    static const int top_id_ = kConHexBoardSize * kConHexBoardSize;
    static const int left_id_ = kConHexBoardSize * kConHexBoardSize + 1;
    static const int right_id_ = kConHexBoardSize * kConHexBoardSize + 2;
    static const int bottom_id_ = kConHexBoardSize * kConHexBoardSize + 3;
};

} // namespace minizero::env::conhex
