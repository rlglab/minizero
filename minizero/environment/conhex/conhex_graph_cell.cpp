#include "conhex_graph_cell.h"

namespace minizero::env::conhex {

using namespace minizero::utils;

ConHexGraphCell::ConHexGraphCell(int cell_id, ConHexGraphCellType cell_type)
{
    cell_type_ = cell_type;
    cell_id_ = cell_id;
    capture_player_ = Player::kPlayerNone; // default player
}

void ConHexGraphCell::placeStone(int hole, Player player)
{
    holes_.get(player).set(hole);
    ++captured_count_.get(player);

    if (capture_player_ != Player::kPlayerNone) { return; } // if already captured early return

    if ((cell_type_ == ConHexGraphCellType::OUTER && captured_count_.get(player) == 2) ||
        (cell_type_ == ConHexGraphCellType::INNER && captured_count_.get(player) == 3) ||
        (cell_type_ == ConHexGraphCellType::CENTER && captured_count_.get(player) == 3)) {
        capture_player_ = player;
    }
}

void ConHexGraphCell::setEdgeFlag(ConHexGraphEdgeFlag cell_edge_flag)
{
    edge_flag_ = cell_edge_flag;
}

bool ConHexGraphCell::isEdgeFlag(ConHexGraphEdgeFlag edge_flag)
{
    return static_cast<bool>(edge_flag_ & edge_flag);
}

int ConHexGraphCell::getCellId()
{
    return cell_id_;
}

void ConHexGraphCell::reset()
{
    holes_.get(Player::kPlayer1).reset();
    holes_.get(Player::kPlayer2).reset();
    captured_count_.get(Player::kPlayer1) = 0;
    captured_count_.get(Player::kPlayer2) = 0;
    capture_player_ = Player::kPlayerNone;
}

Player ConHexGraphCell::getCapturedPlayer() const
{
    return capture_player_;
}

} // namespace minizero::env::conhex
