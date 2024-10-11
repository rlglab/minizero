#include "conhex_graph.h"
#include "color_message.h"
#include "random.h"
#include "sgf_loader.h"
#include <algorithm>
#include <iostream>
#include <string>

namespace minizero::env::conhex {

using namespace minizero::utils;

void ConHexGraph::initGraph()
{
    addCell({0, 1, 9}, ConHexGraphEdgeFlag::TOP | ConHexGraphEdgeFlag::LEFT);
    addCell({1, 2, 3}, ConHexGraphEdgeFlag::TOP);
    addCell({3, 4, 5}, ConHexGraphEdgeFlag::TOP);
    addCell({5, 6, 7}, ConHexGraphEdgeFlag::TOP);
    addCell({7, 8, 17}, ConHexGraphEdgeFlag::TOP | ConHexGraphEdgeFlag::RIGHT);
    addCell({17, 26, 35}, ConHexGraphEdgeFlag::RIGHT);
    addCell({35, 44, 53}, ConHexGraphEdgeFlag::RIGHT);
    addCell({53, 62, 71}, ConHexGraphEdgeFlag::RIGHT);
    addCell({71, 79, 80}, ConHexGraphEdgeFlag::RIGHT | ConHexGraphEdgeFlag::BOTTOM);
    addCell({77, 78, 79}, ConHexGraphEdgeFlag::BOTTOM);
    addCell({75, 76, 77}, ConHexGraphEdgeFlag::BOTTOM);
    addCell({73, 74, 75}, ConHexGraphEdgeFlag::BOTTOM);
    addCell({63, 72, 73}, ConHexGraphEdgeFlag::BOTTOM | ConHexGraphEdgeFlag::LEFT);
    addCell({45, 54, 63}, ConHexGraphEdgeFlag::LEFT);
    addCell({27, 36, 45}, ConHexGraphEdgeFlag::LEFT);
    addCell({9, 18, 27}, ConHexGraphEdgeFlag::LEFT);
    addCell({1, 2, 9, 11, 18, 19}, ConHexGraphEdgeFlag::NONE);
    addCell({2, 3, 4, 11, 12, 13}, ConHexGraphEdgeFlag::NONE);
    addCell({4, 5, 6, 13, 14, 15}, ConHexGraphEdgeFlag::NONE);
    addCell({6, 7, 15, 17, 25, 26}, ConHexGraphEdgeFlag::NONE);
    addCell({25, 26, 34, 35, 43, 44}, ConHexGraphEdgeFlag::NONE);
    addCell({43, 44, 52, 53, 61, 62}, ConHexGraphEdgeFlag::NONE);
    addCell({61, 62, 69, 71, 78, 79}, ConHexGraphEdgeFlag::NONE);
    addCell({67, 68, 69, 76, 77, 78}, ConHexGraphEdgeFlag::NONE);
    addCell({65, 66, 67, 74, 75, 76}, ConHexGraphEdgeFlag::NONE);
    addCell({54, 55, 63, 65, 73, 74}, ConHexGraphEdgeFlag::NONE);
    addCell({36, 37, 45, 46, 54, 55}, ConHexGraphEdgeFlag::NONE);
    addCell({18, 19, 27, 28, 36, 37}, ConHexGraphEdgeFlag::NONE);
    addCell({11, 12, 19, 21, 28, 29}, ConHexGraphEdgeFlag::NONE);
    addCell({12, 13, 14, 21, 22, 23}, ConHexGraphEdgeFlag::NONE);
    addCell({14, 15, 23, 25, 33, 34}, ConHexGraphEdgeFlag::NONE);
    addCell({33, 34, 42, 43, 51, 52}, ConHexGraphEdgeFlag::NONE);
    addCell({51, 52, 59, 61, 68, 69}, ConHexGraphEdgeFlag::NONE);
    addCell({57, 58, 59, 66, 67, 68}, ConHexGraphEdgeFlag::NONE);
    addCell({46, 47, 55, 57, 65, 66}, ConHexGraphEdgeFlag::NONE);
    addCell({28, 29, 37, 38, 46, 47}, ConHexGraphEdgeFlag::NONE);
    addCell({21, 22, 29, 31, 38, 39}, ConHexGraphEdgeFlag::NONE);
    addCell({22, 23, 31, 33, 41, 42}, ConHexGraphEdgeFlag::NONE);
    addCell({41, 42, 49, 51, 58, 59}, ConHexGraphEdgeFlag::NONE);
    addCell({38, 39, 47, 49, 57, 58}, ConHexGraphEdgeFlag::NONE);
    addCell({31, 39, 40, 41, 49}, ConHexGraphEdgeFlag::NONE);

    for (int hole_idx = 0; hole_idx < kConHexBoardSize * kConHexBoardSize; ++hole_idx) {
        if (hole_to_cell_map_[hole_idx].size() == 1) { continue; }
        if (hole_to_cell_map_[hole_idx].size() == 2) { continue; }
        if (hole_to_cell_map_[hole_idx].size() == 3) {
            std::array<int, 3> combination = {hole_to_cell_map_[hole_idx][0], hole_to_cell_map_[hole_idx][1], hole_to_cell_map_[hole_idx][2]};
            for (int i = 0; i < static_cast<int>(combination.size()); ++i) {
                for (int j = 0; j < static_cast<int>(combination.size()); ++j) {
                    if (i == j) { continue; }
                    cell_adjacency_list_[combination[i]].insert(combination[j]);
                }
            }
        }
    }
}

void ConHexGraph::addCell(std::vector<int> hole_indexes, ConHexGraphEdgeFlag cell_edge_flag)
{
    ConHexGraphCellType cell_type = ConHexGraphCellType::NONE;
    if (hole_indexes.size() == ConHexGraphCellType::INNER) { cell_type = ConHexGraphCellType::INNER; }
    if (hole_indexes.size() == ConHexGraphCellType::OUTER) { cell_type = ConHexGraphCellType::OUTER; }
    if (hole_indexes.size() == ConHexGraphCellType::CENTER) { cell_type = ConHexGraphCellType::CENTER; }

    assert(cell_type == ConHexGraphCellType::NONE);

    int cell_id = cells_.size();
    cells_.emplace_back(ConHexGraphCell(cell_id, cell_type));
    cells_.back().setEdgeFlag(cell_edge_flag);

    // add
    for (auto& hole_index : hole_indexes) {
        hole_to_cell_map_[hole_index].push_back(cell_id);
    }
}

ConHexGraph::ConHexGraph() : graph_dsu_(kConHexBoardSize * kConHexBoardSize)
{
    cell_adjacency_list_ = std::vector<std::set<int>>(kConHexBoardSize * kConHexBoardSize, std::set<int>());
    hole_to_cell_map_ = std::vector<std::vector<int>>(kConHexBoardSize * kConHexBoardSize, std::vector<int>());
    cells_ = std::vector<ConHexGraphCell>();
    initGraph();
    reset();
}

void ConHexGraph::reset()
{
    graph_dsu_.reset();
    holes_ = std::vector<Player>(kConHexBoardSize * kConHexBoardSize, Player::kPlayerNone);
    winner_ = Player::kPlayerNone;
    // cell reset
    for (ConHexGraphCell& cell : cells_) {
        cell.reset();
    }
}

bool ConHexGraph::isCellCapturedByPlayer(int cell_id, Player player) const
{
    if (cell_id >= static_cast<int>(cells_.size())) return false;
    return cells_[cell_id].getCapturedPlayer() == player;
}

std::string ConHexGraph::toString() const
{
    const std::array<std::string, 23> pattern = {
        "   A B  C  D  E  F  G  H I",
        " []=======================[]",
        "  lo |  *  |  *  |  *  | ol",
        "1 l *o--o--o--o--o--o--o* l 1",
        "  l /   |  *  |  *  |   \\ l",
        "2 l-o*  o--o--o--o--o  *o-l 2",
        "  l |  /   |  *  |   \\  | l",
        "3 l*o--o*  o--o--o  *o--o*l 3",
        "  l |  |   /  |  \\   |  | l",
        "4 l-o* o--o*  o  *o--o *o-l 4",
        "  l |  |  |  /*\\  |  |  | l",
        "5 l*o--o* o--ooo--o* o--o*l 5",
        "  l |  |  |  \\*/  |  |  | l",
        "6 l-o* o--o*  o  *o--o *o-l 6",
        "  l |  |  \\   |   /  |  | l",
        "7 l*o--o*  o--o--o  *o--o*l 7",
        "  l |  \\   |  *  |   /  | l",
        "8 l-o*  o--o--o--o--o  *o-l 8",
        "  l \\   |  *  |  *  |   / l",
        "9 l *o--o--o--o--o--o--o* l 9",
        "  lo |  *  |  *  |  *  | ol",
        " []=======================[]",
        "   A B  C  D  E  F  G  H I"};
    auto color_player_1 = minizero::utils::TextColor::kBlue;
    auto color_player_2 = minizero::utils::TextColor::kRed;
    /***
     *
    []_______________________[]
     |o |  *  |  *  |  *  | o|
     | *o--o--o--o--o--o--o* |
     | /   |  *  |  *  |   \ |
     |-o*  o--o--o--o--o  *o-|
     | |  /   |  *  |   \  | |
     |*o--o*  o--o--o  *o--o*|
     | |  |   /  |  \   |  | |
     |-o* o--o*  o  *o--o *o-|
     | |  |  |  / \  |  |  | |
     |*o--o* o--ooo--o* o--o*|
     | |  |  |  \ /  |  |  | |
     |-o* o--o*  o  *o--o *o-|
     | |  |  \   |   /  |  | |
     |*o--o*  o--o--o  *o--o*|
     | |  \   |  *  |   /  | |
     |-o*  o--o--o--o--o  *o-|
     | \   |  *  |  *  |   / |
     | *o--o--o--o--o--o--o* |
     |o |  *  |  *  |  *  | o|
    []-----------------------[]
    */
    std::string colored_red_edge{minizero::utils::getColorText(
        "│", minizero::utils::TextType::kBold, minizero::utils::TextColor::kWhite,
        color_player_2)};
    std::string colored_blue_edge{minizero::utils::getColorText(
        "─", minizero::utils::TextType::kBold, minizero::utils::TextColor::kWhite,
        color_player_1)};

    std::string colored_blue_cell{minizero::utils::getColorText(
        " ", minizero::utils::TextType::kBold, minizero::utils::TextColor::kWhite,
        color_player_1)};

    std::string colored_red_cell{minizero::utils::getColorText(
        " ", minizero::utils::TextType::kBold, minizero::utils::TextColor::kWhite,
        color_player_2)};

    std::string colored_blue_node{minizero::utils::getColorText(
        "o", minizero::utils::TextType::kBold, color_player_1,
        minizero::utils::TextColor::kBlack)};

    std::string colored_red_node{minizero::utils::getColorText(
        "o", minizero::utils::TextType::kBold, color_player_2,
        minizero::utils::TextColor::kBlack)};

    constexpr std::array<int, 69> node_id_mapping = {0, 8, 1, 2, 3, 4, 5, 6, 7, 9, 11, 12, 13, 14, 15, 17, 18, 19, 21, 22, 23, 25, 26, 27, 28, 29, 31, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 49, 51, 52, 53, 54, 55, 57, 58, 59, 61, 62, 63, 65, 66, 67, 68, 69, 71, 73, 74, 75, 76, 77, 78, 79, 72, 80};
    constexpr std::array<int, 42> cell_id_mapping = {1, 2, 3, 0, 4, 17, 18, 16, 19, 29, 15, 28, 30, 5, 27, 36, 37, 20, 40, 14, 35, 31, 6, 40, 26, 39, 38, 21, 13, 34, 32, 7, 33, 25, 22, 24, 23, 12, 8, 11, 10, 9};
    std::string out;
    int cell_id_cnt = 0;
    int node_id_cnt = 0;
    for (std::string line : pattern) {
        for (char c : line) {
            if (c == '=') {
                out += colored_blue_edge;
            } else if (c == 'l') {
                out += colored_red_edge;
            } else if (c == '*') {
                int cell_id = cell_id_mapping[cell_id_cnt];
                if (cells_[cell_id].getCapturedPlayer() == Player::kPlayer1) { out += colored_blue_cell; }
                if (cells_[cell_id].getCapturedPlayer() == Player::kPlayer2) { out += colored_red_cell; }
                if (cells_[cell_id].getCapturedPlayer() == Player::kPlayerNone) { out += " "; }
                cell_id_cnt += 1;
            } else if (c == 'o') {
                int node_id = node_id_mapping[node_id_cnt];
                if (holes_[node_id] == Player::kPlayer1) { out += colored_blue_node; }
                if (holes_[node_id] == Player::kPlayer2) { out += colored_red_node; }
                if (holes_[node_id] == Player::kPlayerNone) { out += "o"; }
                node_id_cnt += 1;
            } else {
                out += c;
            }
        }
        out += "\n";
    }
    return out;
}

Player ConHexGraph::getPlayerAtPos(int hole_idx) const
{
    return holes_[hole_idx];
}

void ConHexGraph::placeStone(int hole_idx, Player player)
{
    assert(holes_[hole_idx] != Player::kPlayerNone);
    holes_[hole_idx] = player;

    for (int& cell_id : hole_to_cell_map_[hole_idx]) {
        ConHexGraphCell& cell = cells_[cell_id];
        // may have many cell on same hole, at most 3 layers (cells)
        cell.placeStone(hole_idx, player);
        Player cell_captured_player = cell.getCapturedPlayer();
        if (cell_captured_player == Player::kPlayerNone) { continue; } // no capture action happens

        // near edge or not
        if (cell_captured_player == Player::kPlayer1 && cell.isEdgeFlag(ConHexGraphEdgeFlag::TOP)) {
            graph_dsu_.connect(cell_id, top_id_);
        }
        if (cell_captured_player == Player::kPlayer2 && cell.isEdgeFlag(ConHexGraphEdgeFlag::LEFT)) {
            graph_dsu_.connect(cell_id, left_id_);
        }
        if (cell_captured_player == Player::kPlayer2 && cell.isEdgeFlag(ConHexGraphEdgeFlag::RIGHT)) {
            graph_dsu_.connect(cell_id, right_id_);
        }
        if (cell_captured_player == Player::kPlayer1 && cell.isEdgeFlag(ConHexGraphEdgeFlag::BOTTOM)) {
            graph_dsu_.connect(cell_id, bottom_id_);
        }

        for (int near_cell_id : cell_adjacency_list_[cell_id]) {
            if (cells_[near_cell_id].getCapturedPlayer() == cell_captured_player) {
                graph_dsu_.connect(near_cell_id, cell_id);
            }
        }
        if (player == Player::kPlayer1 && graph_dsu_.find(top_id_) == graph_dsu_.find(bottom_id_)) {
            winner_ = player;
        }
        if (player == Player::kPlayer2 && graph_dsu_.find(left_id_) == graph_dsu_.find(right_id_)) {
            winner_ = player;
        }
    }
}

} // namespace minizero::env::conhex
