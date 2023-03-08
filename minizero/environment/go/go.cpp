#include "go.h"
#include "color_message.h"
#include "sgf_loader.h"
#include <random>
#include <sstream>
#include <string>
#include <utility>

namespace minizero::env::go {

using namespace minizero::utils;

GoHashKey turn_hash_key;
std::vector<GoHashKey> empty_hash_key;
std::vector<GamePair<GoHashKey>> grids_hash_key;
std::vector<std::vector<GamePair<GoHashKey>>> sequence_hash_key;

void initialize()
{
    std::mt19937_64 generator;
    generator.seed(0);
    turn_hash_key = generator();

    // empty & grid hash key
    empty_hash_key.resize(kMaxGoBoardSize * kMaxGoBoardSize);
    grids_hash_key.resize(kMaxGoBoardSize * kMaxGoBoardSize);
    for (int pos = 0; pos < kMaxGoBoardSize * kMaxGoBoardSize; ++pos) {
        empty_hash_key[pos] = generator();
        grids_hash_key[pos].get(Player::kPlayer1) = generator();
        grids_hash_key[pos].get(Player::kPlayer2) = generator();
    }

    // sequence hash key
    sequence_hash_key.resize(2 * kMaxGoBoardSize * kMaxGoBoardSize);
    for (int move = 0; move < 2 * kMaxGoBoardSize * kMaxGoBoardSize; ++move) {
        sequence_hash_key[move].resize(kMaxGoBoardSize * kMaxGoBoardSize + 1);
        for (int pos = 0; pos < kMaxGoBoardSize * kMaxGoBoardSize + 1; ++pos) {
            sequence_hash_key[move][pos].get(Player::kPlayer1) = generator();
            sequence_hash_key[move][pos].get(Player::kPlayer2) = generator();
        }
    }
}

GoHashKey getGoTurnHashKey()
{
    assert(config::env_go_ko_rule == "positional" || config::env_go_ko_rule == "situational");
    return (config::env_go_ko_rule == "positional" ? 0 : turn_hash_key);
}

GoHashKey getGoEmptyHashKey(int position)
{
    assert(position >= 0 && position < kMaxGoBoardSize * kMaxGoBoardSize);
    return empty_hash_key[position];
}

GoHashKey getGoGridHashKey(int position, Player p)
{
    assert(position >= 0 && position < kMaxGoBoardSize * kMaxGoBoardSize);
    assert(p == Player::kPlayer1 || p == Player::kPlayer2);
    return grids_hash_key[position].get(p);
}

GoHashKey getGoSequenceHashKey(int move, int position, Player p)
{
    assert(move >= 0 && move < 2 * kMaxGoBoardSize * kMaxGoBoardSize);
    assert(position >= 0 && position <= kMaxGoBoardSize * kMaxGoBoardSize);
    assert(p == Player::kPlayer1 || p == Player::kPlayer2);
    return sequence_hash_key[move][position].get(p);
}

GoAction::GoAction(const std::vector<std::string>& action_string_args, int board_size)
{
    assert(action_string_args.size() == 2);
    assert(action_string_args[0].size() == 1 && (charToPlayer(action_string_args[0][0]) == Player::kPlayer1 || charToPlayer(action_string_args[0][0]) == Player::kPlayer2));
    player_ = charToPlayer(action_string_args[0][0]);
    action_id_ = SGFLoader::boardCoordinateStringToActionID(action_string_args[1], board_size);
}

GoEnv& GoEnv::operator=(const GoEnv& env)
{
    board_size_ = env.board_size_;
    komi_ = env.komi_;
    turn_ = env.turn_;
    hash_key_ = env.hash_key_;
    board_mask_bitboard_ = env.board_mask_bitboard_;
    board_left_boundary_bitboard_ = env.board_left_boundary_bitboard_;
    board_right_boundary_bitboard_ = env.board_right_boundary_bitboard_;
    free_area_id_bitboard_ = env.free_area_id_bitboard_;
    free_block_id_bitboard_ = env.free_block_id_bitboard_;
    stone_bitboard_ = env.stone_bitboard_;
    benson_bitboard_ = env.benson_bitboard_;
    grids_ = env.grids_;
    areas_ = env.areas_;
    blocks_ = env.blocks_;
    actions_ = env.actions_;
    stone_bitboard_history_ = env.stone_bitboard_history_;
    hashkey_history_ = env.hashkey_history_;
    hash_table_ = env.hash_table_;

    // reset grid's block and area pointer
    for (auto& grid : grids_) {
        if (grid.getBlock()) { grid.setBlock(&blocks_[grid.getBlock()->getID()]); }
        if (grid.getArea(Player::kPlayer1)) { grid.setArea(Player::kPlayer1, &areas_[grid.getArea(Player::kPlayer1)->getID()]); }
        if (grid.getArea(Player::kPlayer2)) { grid.setArea(Player::kPlayer2, &areas_[grid.getArea(Player::kPlayer2)->getID()]); }
    }
    return *this;
}

void GoEnv::reset()
{
    komi_ = minizero::config::env_go_komi;
    turn_ = Player::kPlayer1;
    hash_key_ = 0;
    stone_bitboard_.reset();
    benson_bitboard_.reset();
    board_mask_bitboard_.reset();
    for (int i = 0; i < board_size_ * board_size_; ++i) {
        grids_[i].reset(board_size_);
        areas_[i].reset();
        blocks_[i].reset();
        board_mask_bitboard_.set(i);
    }
    free_area_id_bitboard_.reset();
    free_area_id_bitboard_ = ~free_area_id_bitboard_ & board_mask_bitboard_;
    free_block_id_bitboard_.reset();
    free_block_id_bitboard_ = ~free_block_id_bitboard_ & board_mask_bitboard_;
    board_left_boundary_bitboard_.reset();
    board_right_boundary_bitboard_.reset();
    for (int i = 0; i < board_size_; ++i) {
        board_left_boundary_bitboard_.set(i * board_size_);
        board_right_boundary_bitboard_.set(i * board_size_ + (board_size_ - 1));
    }
    actions_.clear();
    stone_bitboard_history_.clear();
    hashkey_history_.clear();
    hash_table_.clear();
}

bool GoEnv::act(const GoAction& action)
{
    if (!isLegalAction(action)) { return false; }

    const int position = action.getActionID();
    const Player player = action.getPlayer();

    // handle global status
    turn_ = action.nextPlayer();
    hash_key_ ^= getGoTurnHashKey();
    actions_.push_back(action);

    if (isPassAction(action)) {
        stone_bitboard_history_.push_back(stone_bitboard_);
        hashkey_history_.push_back(hash_key_);
        hash_table_.insert(hash_key_);
        return true;
    }

    // set grid color
    GoGrid& grid = grids_[position];
    grid.setPlayer(player);
    hash_key_ ^= getGoGridHashKey(position, player);

    // create new block
    GoBlock* new_block = newBlock();
    grid.setBlock(new_block);
    new_block->setPlayer(player);
    new_block->addGrid(position);
    new_block->addHashKey(getGoGridHashKey(position, player));

    // combine with neighbor own blocks and capture neighbor opponent blocks
    for (const auto& neighbor_pos : grid.getNeighbors()) {
        GoGrid& neighbor_grid = grids_[neighbor_pos];
        if (neighbor_grid.getPlayer() == Player::kPlayerNone) {
            new_block->addLiberty(neighbor_pos);
        } else {
            GoBlock* neighbor_block = neighbor_grid.getBlock();
            neighbor_block->removeLiberty(position);
            if (neighbor_block->getPlayer() == player) {
                new_block = combineBlocks(new_block, neighbor_block);
            } else {
                if (neighbor_block->getNumLiberty() == 0) { removeBlockFromBoard(neighbor_block); }
            }
        }
    }

    stone_bitboard_.get(player) |= new_block->getGridBitboard();
    stone_bitboard_history_.push_back(stone_bitboard_);
    hashkey_history_.push_back(hash_key_);
    hash_table_.insert(hash_key_);

    // update area & benson
    updateArea(action);
    updateBenson(action);

    assert(checkDataStructure());
    return true;
}

bool GoEnv::act(const std::vector<std::string>& action_string_args)
{
    return act(GoAction(action_string_args, board_size_));
}

std::vector<GoAction> GoEnv::getLegalActions() const
{
    std::vector<GoAction> actions;
    for (int pos = 0; pos <= board_size_ * board_size_; ++pos) {
        GoAction action(pos, turn_);
        if (!isLegalAction(action)) { continue; }
        actions.push_back(action);
    }
    return actions;
}

bool GoEnv::isLegalAction(const GoAction& action) const
{
    assert(action.getActionID() >= 0 && action.getActionID() <= board_size_ * board_size_);
    assert(action.getPlayer() == Player::kPlayer1 || action.getPlayer() == Player::kPlayer2);

    if (isPassAction(action)) { return true; }

    const int position = action.getActionID();
    const Player player = action.getPlayer();
    const GoGrid& grid = grids_[position];
    if (grid.getPlayer() != Player::kPlayerNone) { return false; }

    bool is_legal = false;
    GoBitboard check_neighbor_block_bitboard;
    GoHashKey new_hash_key = hash_key_ ^ getGoTurnHashKey() ^ getGoGridHashKey(position, player);
    for (const auto& neighbor_pos : grid.getNeighbors()) {
        const GoGrid& neighbor_grid = grids_[neighbor_pos];
        if (neighbor_grid.getPlayer() == Player::kPlayerNone) {
            is_legal = true;
        } else {
            const GoBlock* neighbor_block = neighbor_grid.getBlock();
            if (check_neighbor_block_bitboard.test(neighbor_block->getID())) { continue; }

            check_neighbor_block_bitboard.set(neighbor_block->getID());
            if (neighbor_block->getPlayer() == player) {
                if (neighbor_block->getNumLiberty() > 1) { is_legal = true; }
            } else {
                if (neighbor_block->getNumLiberty() == 1) {
                    new_hash_key ^= neighbor_block->getHashKey();
                    is_legal = true;
                }
            }
        }
    }

    return (is_legal && hash_table_.count(new_hash_key) == 0);
}

bool GoEnv::isTerminal() const
{
    // two consecutive passes
    if (actions_.size() >= 2 &&
        isPassAction(actions_.back()) &&
        isPassAction(actions_[actions_.size() - 2])) { return true; }

    // game length exceeds 2 * boardsize * boardsize
    if (static_cast<int>(actions_.size()) > 2 * board_size_ * board_size_) { return true; }

    return false;
}

float GoEnv::getEvalScore(bool is_resign /*= false*/) const
{
    Player eval;
    if (is_resign) {
        eval = getNextPlayer(turn_, kGoNumPlayer);
    } else {
        GamePair<float> territory = calculateTrompTaylorTerritory();
        eval = (territory.get(Player::kPlayer1) > territory.get(Player::kPlayer2))
                   ? Player::kPlayer1
                   : ((territory.get(Player::kPlayer1) < territory.get(Player::kPlayer2))
                          ? Player::kPlayer2
                          : Player::kPlayerNone);
    }

    switch (eval) {
        case Player::kPlayer1: return 1.0f;
        case Player::kPlayer2: return -1.0f;
        default: return 0.0f;
    }
}

std::vector<float> GoEnv::getFeatures(utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    /* 18 channels:
        0~15. own/opponent position for last 8 turns
        16. black turn
        17. white turn
    */
    std::vector<float> vFeatures;
    for (int channel = 0; channel < 18; ++channel) {
        for (int pos = 0; pos < board_size_ * board_size_; ++pos) {
            int rotation_pos = getPositionByRotating(utils::reversed_rotation[static_cast<int>(rotation)], pos, board_size_);
            if (channel < 16) {
                int last_n_turn = stone_bitboard_history_.size() - 1 - channel / 2;
                if (last_n_turn < 0) {
                    vFeatures.push_back(0.0f);
                } else {
                    const GamePair<GoBitboard>& last_n_trun_stone_bitboard = stone_bitboard_history_[last_n_turn];
                    Player player = (channel % 2 == 0 ? turn_ : getNextPlayer(turn_, kGoNumPlayer));
                    vFeatures.push_back(last_n_trun_stone_bitboard.get(player).test(rotation_pos) ? 1.0f : 0.0f);
                }
            } else if (channel == 16) {
                vFeatures.push_back((turn_ == Player::kPlayer1 ? 1.0f : 0.0f));
            } else if (channel == 17) {
                vFeatures.push_back((turn_ == Player::kPlayer2 ? 1.0f : 0.0f));
            }
        }
    }
    return vFeatures;
}

std::vector<float> GoEnv::getActionFeatures(const GoAction& action, utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    std::vector<float> action_features(board_size_ * board_size_, 0.0f);
    if (!isPassAction(action)) { action_features[getPositionByRotating(rotation, action.getActionID(), board_size_)] = 1.0f; }
    return action_features;
}

std::string GoEnv::toString() const
{
    int last_move_pos = -1, last2_move_pos = -1;
    if (actions_.size() >= 1) { last_move_pos = actions_.back().getActionID(); }
    if (actions_.size() >= 2) { last2_move_pos = actions_[actions_.size() - 2].getActionID(); }

    std::unordered_map<Player, std::pair<std::string, TextColor>> player_to_text_color({{Player::kPlayerNone, {".", TextColor::kBlack}},
                                                                                        {Player::kPlayer1, {"O", TextColor::kBlack}},
                                                                                        {Player::kPlayer2, {"O", TextColor::kWhite}}});
    std::ostringstream oss;
    oss << getCoordinateString() << std::endl;
    for (int row = board_size_ - 1; row >= 0; --row) {
        oss << getColorText((row + 1 < 10 ? " " : "") + std::to_string(row + 1), TextType::kBold, TextColor::kBlack, TextColor::kYellow);
        for (int col = 0; col < board_size_; ++col) {
            int pos = row * board_size_ + col;
            const GoGrid& grid = grids_[pos];
            const std::pair<std::string, TextColor> text_pair = player_to_text_color[grid.getPlayer()];
            if (pos == last_move_pos) {
                oss << getColorText(">", TextType::kBold, TextColor::kRed, TextColor::kYellow);
                oss << getColorText(text_pair.first, TextType::kBold, text_pair.second, TextColor::kYellow);
            } else if (pos == last2_move_pos) {
                oss << getColorText(">", TextType::kNormal, TextColor::kRed, TextColor::kYellow);
                oss << getColorText(text_pair.first, TextType::kBold, text_pair.second, TextColor::kYellow);
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

GoBitboard GoEnv::dilateBitboard(const GoBitboard& bitboard) const
{
    return ((bitboard << board_size_) |                           // move up
            (bitboard >> board_size_) |                           // move down
            ((bitboard & ~board_left_boundary_bitboard_) >> 1) |  // move left
            ((bitboard & ~board_right_boundary_bitboard_) << 1) | // move right
            bitboard) &
           board_mask_bitboard_;
}

void GoEnv::initialize()
{
    grids_.clear();
    areas_.clear();
    blocks_.clear();
    for (int pos = 0; pos < board_size_ * board_size_; ++pos) {
        grids_.emplace_back(GoGrid(pos, board_size_));
        areas_.emplace_back(GoArea(pos));
        blocks_.emplace_back(GoBlock(pos));
    }
}

GoBlock* GoEnv::newBlock()
{
    assert(!free_block_id_bitboard_.none());
    int id = free_block_id_bitboard_._Find_first();
    free_block_id_bitboard_.reset(id);
    return &blocks_[id];
}

void GoEnv::removeBlock(GoBlock* block)
{
    assert(block && !free_block_id_bitboard_.test(block->getID()));
    free_block_id_bitboard_.set(block->getID());
    block->reset();
}

void GoEnv::removeBlockFromBoard(GoBlock* block)
{
    assert(block);

    // update area
    GoArea* area = nullptr;
    GoBitboard area_id = block->getNeighborAreaIDBitboard();
    while (!area_id.none()) {
        int id = area_id._Find_first();
        area_id.reset(id);
        if (!area) {
            area = &areas_[id];
        } else {
            area = mergeArea(area, &areas_[id]);
        }
    }
    assert(area);
    area->setNumGrid(area->getNumGrid() + block->getNumGrid());
    area->setAreaBitBoard(area->getAreaBitboard() | block->getGridBitboard());
    area->removeNeighborBlockIDBitboard(block->getID());
    if (area->getNumGrid() == board_size_ * board_size_) {
        // remove area when area include whole board
        removeArea(area);
        area = nullptr;
    }

    // remove block
    GoBitboard grid_bitboard = block->getGridBitboard();
    while (!grid_bitboard.none()) {
        int pos = grid_bitboard._Find_first();
        grid_bitboard.reset(pos);

        GoGrid& grid = grids_[pos];
        grid.setPlayer(Player::kPlayerNone);
        grid.setBlock(nullptr);
        grid.setArea(block->getPlayer(), area);
        for (const auto& neighbor_pos : grid.getNeighbors()) {
            GoGrid& neighbor_grid = grids_[neighbor_pos];
            if (neighbor_grid.getPlayer() != getNextPlayer(block->getPlayer(), kGoNumPlayer)) { continue; }
            neighbor_grid.getBlock()->addLiberty(pos);
        }
    }
    hash_key_ ^= block->getHashKey();
    stone_bitboard_.get(block->getPlayer()) &= ~block->getGridBitboard();
    removeBlock(block);
}

GoBlock* GoEnv::combineBlocks(GoBlock* block1, GoBlock* block2)
{
    assert(block1 && block2);

    if (block1 == block2) { return block1; }
    if (block1->getNumGrid() < block2->getNumGrid()) { return combineBlocks(block2, block1); }

    // link grid to new block
    GoBitboard grid_bitboard = block2->getGridBitboard();
    while (!grid_bitboard.none()) {
        int pos = grid_bitboard._Find_first();
        grid_bitboard.reset(pos);
        grids_[pos].setBlock(block1);
    }

    // link area to new block
    GoBitboard new_area_id_bitboard = block2->getNeighborAreaIDBitboard();
    while (!new_area_id_bitboard.none()) {
        int id = new_area_id_bitboard._Find_first();
        new_area_id_bitboard.reset(id);
        areas_[id].removeNeighborBlockIDBitboard(block2->getID());
        areas_[id].addNeighborBlockIDBitboard(block1->getID());
    }

    block1->combineWithBlock(block2);
    removeBlock(block2);
    return block1;
}

void GoEnv::updateArea(const GoAction& action)
{
    if (isPassAction(action)) { return; }
    assert(grids_[action.getActionID()].getPlayer() != Player::kPlayerNone);

    // use last move to update area:
    //    1. last move in own area:
    //        a. last move splits area => remove current area and find area
    //        b. last move didn't split area => remove current move from area
    //    2. last move not in own area => find area
    GoGrid& grid = grids_[action.getActionID()];
    Player own_player = grid.getPlayer();
    GoArea* own_area = grid.getArea(own_player);
    std::vector<GoBitboard> areas_bitboard = findAreas(action);
    if (own_area && areas_bitboard.size() == 1) {
        if (own_area->getNumGrid() == 1) {
            removeArea(own_area);
        } else {
            grid.setArea(action.getPlayer(), nullptr);
            grid.getBlock()->addNeighborAreaIDBitboard(own_area->getID());
            own_area->setNumGrid(own_area->getNumGrid() - 1);
            own_area->getAreaBitboard().reset(grid.getPosition());
            own_area->getNeighborBlockIDBitboard().set(grid.getBlock()->getID());
        }
    } else {
        if (own_area) { removeArea(own_area); }
        for (const auto& area_bitboard : areas_bitboard) { addArea(own_player, area_bitboard); }
    }
}

void GoEnv::addArea(Player player, const GoBitboard& area_bitboard)
{
    assert(!free_area_id_bitboard_.none());

    // get available area id
    int area_id = free_area_id_bitboard_._Find_first();
    free_area_id_bitboard_.reset(area_id);

    GoArea* area = &areas_[area_id];
    area->setNumGrid(area_bitboard.count());
    area->setPlayer(player);
    area->setAreaBitBoard(area_bitboard);

    // link grids pointer
    GoBitboard grid_bitboard = area_bitboard;
    while (!grid_bitboard.none()) {
        int pos = grid_bitboard._Find_first();
        grid_bitboard.reset(pos);
        grids_[pos].setArea(player, area);
    }

    // link blocks pointer
    GoBitboard neighbor_block_bitboard = dilateBitboard(area_bitboard) & stone_bitboard_.get(player);
    while (!neighbor_block_bitboard.none()) {
        int pos = neighbor_block_bitboard._Find_first();
        GoBlock* block = grids_[pos].getBlock();
        block->addNeighborAreaIDBitboard(area->getID());
        area->addNeighborBlockIDBitboard(block->getID());
        neighbor_block_bitboard &= ~block->getGridBitboard();
    }
}

void GoEnv::removeArea(GoArea* area)
{
    assert(area && !free_area_id_bitboard_.test(area->getID()));

    // remove grids pointer
    GoBitboard area_bitboard = area->getAreaBitboard();
    while (!area_bitboard.none()) {
        int pos = area_bitboard._Find_first();
        area_bitboard.reset(pos);
        grids_[pos].setArea(area->getPlayer(), nullptr);
    }

    // remove blocks pointer
    GoBitboard neighbor_block_id = area->getNeighborBlockIDBitboard();
    while (!neighbor_block_id.none()) {
        int block_id = neighbor_block_id._Find_first();
        neighbor_block_id.reset(block_id);
        blocks_[block_id].removeNeighborAreaIDBitboard(area->getID());
    }

    // remove area
    free_area_id_bitboard_.set(area->getID());
    area->reset();
}

GoArea* GoEnv::mergeArea(GoArea* area1, GoArea* area2)
{
    assert(area1 && area2);
    if (area1->getNumGrid() < area2->getNumGrid()) { return mergeArea(area2, area1); }

    GoBitboard area2_bitboard = area2->getAreaBitboard(); // save area2 bitboard before removing area2
    GoBitboard area2_nbr_block_id = area2->getNeighborBlockIDBitboard();
    area1->combineWithArea(area2);
    removeArea(area2);
    while (!area2_bitboard.none()) { // link grid to area
        int pos = area2_bitboard._Find_first();
        area2_bitboard.reset(pos);
        grids_[pos].setArea(area1->getPlayer(), area1);
    }
    while (!area2_nbr_block_id.none()) { // link block to area
        int id = area2_nbr_block_id._Find_first();
        area2_nbr_block_id.reset(id);
        blocks_[id].addNeighborAreaIDBitboard(area1->getID());
    }
    return area1;
}

std::vector<GoBitboard> GoEnv::findAreas(const GoAction& action)
{
    const GoGrid& grid = grids_[action.getActionID()];
    const std::vector<int>& neighbors = grid.getNeighbors();
    std::vector<GoBitboard> areas;
    GoBitboard checked_area;
    GoBitboard boundary_bitboard = ~stone_bitboard_.get(action.getPlayer()) & board_mask_bitboard_;
    for (const auto& pos : neighbors) {
        const GoGrid& neighbor_grid = grids_[pos];
        if (neighbor_grid.getPlayer() == action.getPlayer()) { continue; }
        if (checked_area.test(pos)) { continue; }
        GoBitboard area_bitboard = floodFillBitBoard(pos, boundary_bitboard);
        checked_area |= area_bitboard;
        areas.push_back(area_bitboard);
    }
    return areas;
}

void GoEnv::updateBenson(const GoAction& action)
{
    if (isPassAction(action)) { return; }
    assert(grids_[action.getActionID()].getPlayer() != Player::kPlayerNone);

    const GoGrid& grid = grids_[action.getActionID()];
    const GoBlock* block = grid.getBlock();

    // update own benson
    GoBitboard& own_benson_bitboard = benson_bitboard_.get(action.getPlayer());
    if (own_benson_bitboard.test(action.getActionID()) || block->getNeighborAreaIDBitboard().count() > 1) {
        own_benson_bitboard = findBensonBitboard(stone_bitboard_.get(block->getPlayer()));
    }

    // update opponent benson
    Player next_player = action.nextPlayer();
    const GoArea* opponent_area = grid.getArea(next_player);
    if (opponent_area && !benson_bitboard_.get(next_player).test(action.getActionID()) &&
        (opponent_area->getAreaBitboard() & ~dilateBitboard(stone_bitboard_.get(next_player)) & ~stone_bitboard_.get(action.getPlayer())).none()) {
        benson_bitboard_.get(next_player) |= findBensonBitboard(stone_bitboard_.get(next_player));
    }
}

GoBitboard GoEnv::findBensonBitboard(GoBitboard block_bitboard) const
{
    // construct vital areas for each block
    GoBitboard benson_area_id, benson_block_id;
    std::vector<GoBitboard> block_neighbor_vital_areas(board_size_ * board_size_, GoBitboard());
    GoBitboard stone_bitboard = stone_bitboard_.get(Player::kPlayer1) | stone_bitboard_.get(Player::kPlayer2);
    while (!block_bitboard.none()) {
        int pos = block_bitboard._Find_first();
        const GoBlock* block = grids_[pos].getBlock();
        block_bitboard &= ~block->getGridBitboard();

        GoBitboard block_neighbor_area_id = block->getNeighborAreaIDBitboard();
        while (!block_neighbor_area_id.none()) {
            int area_id = block_neighbor_area_id._Find_first();
            block_neighbor_area_id.reset(area_id);

            const GoArea* area = &areas_[area_id];
            if (!(area->getAreaBitboard() & ~block->getLibertyBitboard() & ~stone_bitboard).none()) { continue; }
            block_neighbor_vital_areas[block->getID()].set(area_id);
            benson_block_id.set(block->getID());
            benson_area_id.set(area_id);
        }
    }

    // Benson's algorithm
    bool is_over = false;
    GoBitboard benson_bitboard;
    while (!is_over && !benson_block_id.none()) {
        is_over = true;
        benson_bitboard.reset();

        // 1. Remove from X all Black chains with less than two vital Black-enclosed regions in R
        GoBitboard next_benson_block_id;
        while (!benson_block_id.none()) {
            int block_id = benson_block_id._Find_first();
            benson_block_id.reset(block_id);

            if ((block_neighbor_vital_areas[block_id] & benson_area_id).count() < 2) {
                is_over = false;
                continue;
            }
            next_benson_block_id.set(block_id);
            benson_bitboard |= blocks_[block_id].getGridBitboard();
        }
        benson_block_id = next_benson_block_id;

        // 2. Remove from R all Black - enclosed regions with a surrounding stone in a chain not in X
        GoBitboard next_benson_area_id;
        while (!benson_area_id.none()) {
            int area_id = benson_area_id._Find_first();
            benson_area_id.reset(area_id);

            if (!(areas_[area_id].getNeighborBlockIDBitboard() & ~benson_block_id).none()) {
                is_over = false;
                continue;
            }
            next_benson_area_id.set(area_id);
            benson_bitboard |= areas_[area_id].getAreaBitboard();
        }
        benson_area_id = next_benson_area_id;
    }
    return benson_bitboard;
}

std::string GoEnv::getCoordinateString() const
{
    std::ostringstream oss;
    oss << "  ";
    for (int i = 0; i < board_size_; ++i) {
        char c = 'A' + i + ('A' + i >= 'I' ? 1 : 0);
        oss << " " + std::string(1, c);
    }
    oss << "   ";
    return getColorText(oss.str(), TextType::kBold, TextColor::kBlack, TextColor::kYellow);
}

GoBitboard GoEnv::floodFillBitBoard(int start_position, const GoBitboard& boundary_bitboard) const
{
    GoBitboard flood_fill_bitboard;
    flood_fill_bitboard.set(start_position);
    bool need_dilate = true;
    while (need_dilate) {
        GoBitboard dilate_bitboard = dilateBitboard(flood_fill_bitboard) & boundary_bitboard;
        need_dilate = (flood_fill_bitboard != dilate_bitboard);
        flood_fill_bitboard = dilate_bitboard;
    }
    return flood_fill_bitboard;
}

GamePair<float> GoEnv::calculateTrompTaylorTerritory() const
{
    GamePair<float> territory(stone_bitboard_.get(Player::kPlayer1).count(), stone_bitboard_.get(Player::kPlayer2).count() + komi_);
    GoBitboard empty_stone_bitboard = ~(stone_bitboard_.get(Player::kPlayer1) | stone_bitboard_.get(Player::kPlayer2)) & board_mask_bitboard_;
    while (!empty_stone_bitboard.none()) {
        int pos = empty_stone_bitboard._Find_first();

        // check is surrounded by only one's color
        GoBitboard flood_fill_bitboard = floodFillBitBoard(pos, empty_stone_bitboard);
        GoBitboard surrounding_bitboard = dilateBitboard(flood_fill_bitboard) & ~flood_fill_bitboard;
        if ((surrounding_bitboard & ~stone_bitboard_.get(Player::kPlayer1)).none()) {
            territory.get(Player::kPlayer1) += flood_fill_bitboard.count();
        } else if ((surrounding_bitboard & ~stone_bitboard_.get(Player::kPlayer2)).none()) {
            territory.get(Player::kPlayer2) += flood_fill_bitboard.count();
        }

        empty_stone_bitboard &= ~flood_fill_bitboard;
    }

    return territory;
}

std::vector<float> GoEnvLoader::getActionFeatures(const int pos, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    assert(pos < static_cast<int>(action_pairs_.size()));
    const GoAction& action = action_pairs_[pos].first;
    std::vector<float> action_features(getBoardSize() * getBoardSize(), 0.0f);
    if (!isPassAction(action)) { action_features[getPositionByRotating(rotation, action.getActionID(), getBoardSize())] = 1.0f; }
    return action_features;
}

} // namespace minizero::env::go
