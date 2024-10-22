#include "havannah.h"
#include "color_message.h"
#include "random.h"
#include "sgf_loader.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>

namespace minizero::env::havannah {

using namespace minizero::utils;

HavannahEnv& HavannahEnv::operator=(const HavannahEnv& env)
{
    turn_ = env.turn_;
    actions_ = env.actions_;
    board_size_ = env.board_size_;
    extended_board_size_ = env.extended_board_size_;
    winner_ = env.winner_;
    cells_ = env.cells_;
    paths_ = env.paths_;
    neighbors_ = env.neighbors_;
    board_mask_bitboard_ = env.board_mask_bitboard_;
    free_path_id_bitboard_ = env.free_path_id_bitboard_;
    corner_bitboard_ = env.corner_bitboard_;
    border_bitboards_ = env.border_bitboards_;
    bitboard_pair_ = env.bitboard_pair_;
    bitboard_history_ = env.bitboard_history_;
    // reset cell's path
    for (auto& cell : cells_) {
        if (cell.getPath()) { cell.setPath(&paths_[cell.getPath()->getIndex()]); }
    }
    return *this;
}

void HavannahEnv::initialize()
{
    /*
       the example of board 4x4.

       . . . . X X X
       . . . . . X X
       . . . . . . X
       . . . . . . .
       X . . . . . .
       X X . . . . .
       X X X . . . .

       the '.' are on the board and are legal moves.
       the 'X' are outside the board and are illegal moves.
    */
    cells_.clear();
    paths_.clear();
    neighbors_.clear();
    for (int pos = 0; pos < extended_board_size_ * extended_board_size_; ++pos) {
        cells_.emplace_back(HavannahCell(pos, extended_board_size_));
        paths_.emplace_back(HavannahPath(pos));
        neighbors_.emplace_back(std::array<int, 7>());
    }
    calculateNeighbors();

    // initialize border bitboards (6 sides)
    border_bitboards_.resize(6);
    for (auto& border : border_bitboards_) {
        border.reset();
    }
    for (int i = 0; i < board_size_ - 2; ++i) {
        border_bitboards_[0].set(board_size_ + i);
        border_bitboards_[1].set((i + 1) * extended_board_size_ + board_size_ - 2 - i);
        border_bitboards_[2].set((board_size_ + i) * extended_board_size_);
        border_bitboards_[3].set((extended_board_size_ - 1) * extended_board_size_ + 1 + i);
        border_bitboards_[4].set((extended_board_size_ - 1 - i) * extended_board_size_ - board_size_ + 1 + i);
        border_bitboards_[5].set((board_size_ - 1 - i) * extended_board_size_ - 1);
    }

    // initialize corner bitboard (6 vertices)
    corner_bitboard_.reset();
    corner_bitboard_.set(board_size_ - 1);
    corner_bitboard_.set(extended_board_size_ - 1);
    corner_bitboard_.set((board_size_ - 1) * extended_board_size_);
    corner_bitboard_.set(board_size_ * extended_board_size_ - 1);
    corner_bitboard_.set((extended_board_size_ - 1) * extended_board_size_);
    corner_bitboard_.set((extended_board_size_ - 1) * extended_board_size_ - 1 + board_size_);
}

void HavannahEnv::reset()
{
    winner_ = Player::kPlayerNone;
    turn_ = Player::kPlayer1;
    actions_.clear();
    for (int i = 0; i < extended_board_size_ * extended_board_size_; ++i) {
        cells_[i].reset();
        paths_[i].reset();
        board_mask_bitboard_.set(i);
    }
    free_path_id_bitboard_.reset();
    free_path_id_bitboard_ = ~free_path_id_bitboard_ & board_mask_bitboard_;

    bitboard_pair_.get(Player::kPlayer1).reset();
    bitboard_pair_.get(Player::kPlayer2).reset();
    bitboard_history_.clear();
    bitboard_history_.push_back(bitboard_pair_);
}

bool HavannahEnv::act(const HavannahAction& action)
{
    if (!isLegalAction(action)) { return false; }

    int action_id = action.getActionID();

    if (config::env_havannah_use_swap_rule) {
        // check if it's the second move and the chosen action is same as the first action.
        // if so, the player has chosen to swap.
        if (actions_.size() == 1 && action_id == actions_[0].getActionID()) {
            // clear the location of the first move,
            // and the seoncd move plays the location of the first move.
            removePath(cells_[actions_[0].getActionID()].getPath());
            cells_[actions_[0].getActionID()].setPlayer(Player::kPlayerNone);
        }
    }

    actions_.push_back(action);
    Player player = action.getPlayer();
    cells_[action_id].setPlayer(player);

    // create new path
    HavannahPath* new_path = newPath();
    cells_[action_id].setPath(new_path);
    new_path->setPlayer(player);
    new_path->addCell(action_id);
    for (int neighbor : neighbors_[action_id]) {
        if (neighbor == -1) { break; }

        HavannahCell* neighbor_cell = &cells_[neighbor];
        if (neighbor_cell->getPlayer() != player) { continue; }

        HavannahPath* neighbor_path = neighbor_cell->getPath();
        new_path = combinePaths(new_path, neighbor_path);
        assert(new_path->getPlayer() != Player::kPlayerNone);
    }

    turn_ = action.nextPlayer();
    winner_ = updateWinner(action);

    bitboard_pair_.get(player).set(action_id);
    bitboard_history_.push_back(bitboard_pair_);

    return true;
}

std::vector<HavannahAction> HavannahEnv::getLegalActions() const
{
    std::vector<HavannahAction> actions;
    for (int pos = 0; pos < extended_board_size_ * extended_board_size_; ++pos) {
        HavannahAction action(pos, turn_);
        if (!isLegalAction(action)) { continue; }
        actions.push_back(action);
    }
    return actions;
}

bool HavannahEnv::isLegalAction(const HavannahAction& action) const
{
    assert(action.getActionID() >= 0 && action.getActionID() < extended_board_size_ * extended_board_size_);
    assert(action.getPlayer() == Player::kPlayer1 || action.getPlayer() == Player::kPlayer2);

    int action_id = action.getActionID();
    Player player = action.getPlayer();

    if (!isValidCell(cells_[action_id])) { return false; }

    return player == turn_ &&
           ((config::env_havannah_use_swap_rule && actions_.size() == 1) // swap rule
            || (cells_[action_id].getPlayer() == Player::kPlayerNone));  // non-swap rule
}

bool HavannahEnv::isTerminal() const
{
    // terminal: a winner is determined between players or the board is filled (a draw)
    return (winner_ != Player::kPlayerNone ||
            std::find_if(cells_.begin(), cells_.end(), [this](const HavannahCell& cell) { return isValidCell(cell) && cell.getPlayer() == Player::kPlayerNone; }) == cells_.end());
}

float HavannahEnv::getEvalScore(bool is_resign /* = false */) const
{
    Player eval = (is_resign ? getNextPlayer(turn_, kHavannahNumPlayer) : winner_);
    switch (eval) {
        case Player::kPlayer1: return 1.0f;
        case Player::kPlayer2: return -1.0f;
        default: return 0.0f;
    }
}

std::vector<float> HavannahEnv::getFeatures(utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    /*
      20 channels:
         0~15. own/opponent position for last 8 turns
           16. boundary
           17. swappable
           18. black turn
           19. white turn
    */
    int past_moves = std::min(8, static_cast<int>(bitboard_history_.size()));
    int spatial = extended_board_size_ * extended_board_size_;
    std::vector<float> features(getNumInputChannels() * spatial, 0.f);
    int last_idx = bitboard_history_.size() - 1;

    // 0 ~ 15
    for (int c = 0; c < 2 * past_moves; c += 2) {
        const HavannahBitboard& own_bitboard = bitboard_history_[last_idx - (c / 2)].get(turn_);
        const HavannahBitboard& opponent_bitboard = bitboard_history_[last_idx - (c / 2)].get(getNextPlayer(turn_, kHavannahNumPlayer));
        for (int pos = 0; pos < spatial; ++pos) {
            int rotation_pos = getRotatePosition(pos, utils::reversed_rotation[static_cast<int>(rotation)]);
            features[pos + c * spatial] = (own_bitboard.test(rotation_pos) ? 1.0f : 0.0f);
            features[pos + (c + 1) * spatial] = (opponent_bitboard.test(rotation_pos) ? 1.0f : 0.0f);
        }
    }

    // 16 ~ 19
    for (int pos = 0; pos < spatial; ++pos) {
        int rotation_pos = getRotatePosition(pos, utils::reversed_rotation[static_cast<int>(rotation)]);
        features[pos + 16 * spatial] = (isValidCell(cells_[rotation_pos]) ? 1.0f : 0.0f);
        features[pos + 17 * spatial] = (isSwappable() ? 1.0f : 0.0f);
        features[pos + 18 * spatial] = static_cast<float>(turn_ == Player::kPlayer1);
        features[pos + 19 * spatial] = static_cast<float>(turn_ == Player::kPlayer2);
    }
    return features;
}

std::vector<float> HavannahEnv::getActionFeatures(const HavannahAction& action, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    throw std::runtime_error{"HavannahEnvLoader::getActionFeatures() is not implemented"};
}

std::string HavannahEnv::toString() const
{
    int last_move_pos = -1;
    if (actions_.size() >= 1) { last_move_pos = actions_.back().getActionID(); }

    std::unordered_map<Player, std::pair<std::string, TextColor>> player_to_text_color({{Player::kPlayerNone, {".", TextColor::kBlack}},
                                                                                        {Player::kPlayer1, {"O", TextColor::kBlack}},
                                                                                        {Player::kPlayer2, {"O", TextColor::kWhite}}});
    std::ostringstream oss;
    for (int i = 0; i < extended_board_size_ - 1; ++i) oss << " ";
    oss << getCoordinateString() << std::endl;
    for (int row = extended_board_size_ - 1; row >= 0; --row) {
        for (int i = 0; i < row; ++i) oss << " ";

        oss << getColorText((row + 1 < 10 ? " " : "") + std::to_string(row + 1), TextType::kBold, TextColor::kBlack, TextColor::kYellow);
        for (int col = 0; col < extended_board_size_; ++col) {
            int pos = row * extended_board_size_ + col;
            const std::pair<std::string, TextColor> text_pair = player_to_text_color[cells_[pos].getPlayer()];
            if (pos == last_move_pos) {
                oss << getColorText(">", TextType::kBold, TextColor::kRed, TextColor::kYellow);
                oss << getColorText(text_pair.first, TextType::kBold, text_pair.second, TextColor::kYellow);
            } else if (!isValidCell(cells_[pos])) {
                oss << getColorText(" " + text_pair.first, TextType::kBold, text_pair.second, TextColor::kBlack);
            } else {
                oss << getColorText(" " + text_pair.first, TextType::kBold, text_pair.second, TextColor::kYellow);
            }
        }
        oss << getColorText(" " + std::to_string(row + 1) + (row + 1 < 10 ? " " : ""), TextType::kBold, TextColor::kBlack, TextColor::kYellow);
        oss << std::endl;
    }
    oss << getCoordinateString() << std::endl;
    return oss.str();
}

Player HavannahEnv::updateWinner(const HavannahAction& action)
{
    assert(cells_[action.getActionID()].getPath());
    const HavannahPath* path = cells_[action.getActionID()].getPath();

    // bridge: connect any two corners
    if ((path->getCells() & corner_bitboard_).count() >= 2) { return path->getPlayer(); }

    // fork: connect any three borders (edges)
    int num_connected_borders = 0;
    for (auto& border : border_bitboards_) {
        if ((path->getCells() & border).any()) { ++num_connected_borders; }
        if (num_connected_borders >= 3) { return path->getPlayer(); }
    }

    // cycle: loop around any site
    if (isCycle(path, action)) { return path->getPlayer(); }

    return Player::kPlayerNone;
}

void HavannahEnv::calculateNeighbors()
{
    for (int i = 0; i < extended_board_size_; ++i) {
        for (int j = 0; j < extended_board_size_; ++j) {
            if (!isValidCoor(i, j)) { continue; }

            int index = i * extended_board_size_ + j;
            int k = 0;
            std::array<std::pair<int, int>, 6> nbrs = {{std::make_pair(i - 1, j), std::make_pair(i - 1, j + 1),
                                                        std::make_pair(i, j - 1), std::make_pair(i, j + 1),
                                                        std::make_pair(i + 1, j - 1), std::make_pair(i + 1, j)}};
            for (const std::pair<int, int>& nbr : nbrs) {
                if (isValidCoor(nbr.first, nbr.second)) {
                    neighbors_[index][k] = nbr.first * extended_board_size_ + nbr.second;
                    ++k;
                }
            }
            neighbors_[index][k] = -1;
        }
    }
}

bool HavannahEnv::isValidCell(const HavannahCell& cell) const
{
    const std::pair<int, int>& coor = cell.getCoordinate();
    return isValidCoor(coor.first, coor.second);
}

bool HavannahEnv::isValidCoor(int i, int j) const
{
    // coordinates outside the board are invalid.
    return (i >= 0 && i < extended_board_size_ &&
            j >= 0 && j < extended_board_size_ &&
            i + j >= board_size_ - 1 && i + j <= 3 * board_size_ - 3);
}

std::string HavannahEnv::getCoordinateString() const
{
    std::ostringstream oss;
    oss << "  ";
    for (int i = 0; i < extended_board_size_; ++i) {
        char c = 'A' + i + ('A' + i >= 'I' ? 1 : 0);
        oss << " " + std::string(1, c);
    }
    oss << "   ";
    return getColorText(oss.str(), TextType::kBold, TextColor::kBlack, TextColor::kYellow);
}

HavannahEnv::HavannahPath* HavannahEnv::newPath()
{
    assert(!free_path_id_bitboard_.none());
    int id = free_path_id_bitboard_._Find_first();
    free_path_id_bitboard_.reset(id);
    return &paths_[id];
}

HavannahEnv::HavannahPath* HavannahEnv::combinePaths(HavannahPath* path1, HavannahPath* path2)
{
    assert(path1 && path2);
    assert(path1->getPlayer() != Player::kPlayerNone && path2->getPlayer() != Player::kPlayerNone);

    if (path1 == path2) { return path1; }
    if (path1->getNumCells() < path2->getNumCells()) { return combinePaths(path2, path1); }

    // link cell to new cell
    HavannahBitboard cell_bitboard = path2->getCells();
    while (!cell_bitboard.none()) {
        int pos = cell_bitboard._Find_first();
        cell_bitboard.reset(pos);
        cells_[pos].setPath(path1);
    }

    path1->combineWithPath(path2);
    removePath(path2);
    return path1;
}

void HavannahEnv::removePath(HavannahPath* path)
{
    assert(path && !free_path_id_bitboard_.test(path->getIndex()));
    free_path_id_bitboard_.set(path->getIndex());
    path->reset();
}

bool HavannahEnv::isCycle(const HavannahPath* path, const HavannahAction& last_action) const
{
    // show be at least 6 cells
    if (path->getNumCells() < 6) { return false; }

    int pos = last_action.getActionID();
    Player player = last_action.getPlayer();
    // at least connected two previous cells
    if (computeOwnNeighbors(pos, player) < 2) { return false; }

    // check if there is an interior cell
    for (int neighbor : neighbors_[pos]) {
        if (neighbor == -1) { break; }
        if (computeOwnNeighbors(neighbor, player) == 6) { return true; }
    }

    // check if there's a hole
    return detectHole(path);
}

int HavannahEnv::computeOwnNeighbors(int pos, Player player) const
{
    int count = 0;
    for (int neighbor : neighbors_[pos]) {
        if (neighbor == -1) { break; }
        if (cells_[neighbor].getPlayer() == player) { count++; }
    }
    return count;
}

bool HavannahEnv::detectHole(const HavannahPath* path) const
{
    std::vector<HavannahCell> cells;
    cells.reserve(path->getNumCells());

    HavannahBitboard bitboard = path->getCells();
    while (bitboard.any()) {
        int pos = bitboard._Find_first();
        bitboard.reset(pos);
        cells.emplace_back(HavannahCell(pos, extended_board_size_));
    }

    // find bounding box
    int imin = extended_board_size_;
    int jmin = extended_board_size_;
    int imax = 0;
    int jmax = 0;
    for (const HavannahCell& c : cells) {
        imin = std::min(imin, c.getCoordinate().first);
        imax = std::max(imax, c.getCoordinate().first);
        jmin = std::min(jmin, c.getCoordinate().second);
        jmax = std::max(jmax, c.getCoordinate().second);
    }

    // reset data
    std::vector<std::vector<int>> data(extended_board_size_ + 2, std::vector<int>(extended_board_size_ + 2, 0));
    int di = imax - imin + 3;
    int dj = jmax - jmin + 3;
    for (int i = 0; i < di; i++) {
        data[i][0] = 1;
        data[i][dj - 1] = 1;
    }
    for (int j = 0; j < dj; j++) {
        data[0][j] = 1;
        data[di - 1][j] = 1;
    }

    // write object
    for (const HavannahCell& c : cells) {
        int i = c.getCoordinate().first - imin + 1;
        int j = c.getCoordinate().second - jmin + 1;
        data[i][j] = -1;
    }

    // propagate background
    auto fMaxNeighbor = [&data](int i, int j) {
        int d = data[i][j];
        if (d >= 0) {
            d = std::max(d, data[i - 1][j]);
            d = std::max(d, data[i - 1][j + 1]);
            d = std::max(d, data[i][j - 1]);
            d = std::max(d, data[i][j + 1]);
            d = std::max(d, data[i + 1][j - 1]);
            d = std::max(d, data[i + 1][j]);
        }
        return d;
    };

    bool hasChanged = true;
    while (hasChanged) {
        hasChanged = false;
        for (int i = 1; i < di - 1; i++) {
            for (int j = 1; j < dj - 1; j++) {
                int d = fMaxNeighbor(i, j);
                if (data[i][j] != d) {
                    data[i][j] = d;
                    hasChanged = true;
                }
            }
        }
        for (int i = di - 2; i > 0; i--) {
            for (int j = dj - 2; j > 0; j--) {
                int d = fMaxNeighbor(i, j);
                if (data[i][j] != d) {
                    data[i][j] = d;
                    hasChanged = true;
                }
            }
        }
    }

    // check initial background
    for (int i = 0; i < di; i++) {
        for (int j = 0; j < dj; j++) {
            if (data[i][j] == 0) { return true; }
        }
    }

    return false;
}

std::vector<float> HavannahEnvLoader::getActionFeatures(const int pos, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    throw std::runtime_error{"HavannahEnvLoader::getActionFeatures() is not implemented"};
}

} // namespace minizero::env::havannah
