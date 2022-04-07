#include "go.h"
#include "color_message.h"
#include "go_benson.h"
#include "sgf_loader.h"
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>

namespace minizero::env::go {

using namespace minizero::utils;

GoHashKey turn_hash_key;
GoPair<std::vector<GoHashKey>> grids_hash_key;

void initialize()
{
    std::mt19937_64 generator;
    generator.seed(0);
    assert(config::env_go_ko_rule == "positional" || config::env_go_ko_rule == "situational");
    turn_hash_key = (config::env_go_ko_rule == "positional" ? 0 : generator());
    for (int pos = 0; pos < kMaxGoBoardSize * kMaxGoBoardSize; ++pos) {
        grids_hash_key.get(Player::kPlayer1).push_back(generator());
        grids_hash_key.get(Player::kPlayer2).push_back(generator());
    }
}

GoHashKey getGoTurnHashKey()
{
    return turn_hash_key;
}

GoHashKey getGoGridHashKey(int position, Player p)
{
    assert(position >= 0 && position < kMaxGoBoardSize * kMaxGoBoardSize);
    assert(p == Player::kPlayer1 || p == Player::kPlayer2);
    return grids_hash_key.get(p)[position];
}

GoAction::GoAction(const std::vector<std::string>& action_string_args, int board_size)
{
    assert(action_string_args.size() == 2);
    assert(action_string_args[0].size() == 1 && (charToPlayer(action_string_args[0][0]) == Player::kPlayer1 || charToPlayer(action_string_args[0][0]) == Player::kPlayer2));
    player_ = charToPlayer(action_string_args[0][0]);
    action_id_ = SGFLoader::boardCoordinateStringToActionID(action_string_args[1], board_size);
}

GoEnv::GoEnv(const GoEnv& env)
{
    board_size_ = env.board_size_;
    komi_ = env.komi_;
    turn_ = env.turn_;
    hash_key_ = env.hash_key_;
    board_mask_bitboard_ = env.board_mask_bitboard_;
    board_left_boundary_bitboard_ = env.board_left_boundary_bitboard_;
    board_right_boundary_bitboard_ = env.board_right_boundary_bitboard_;
    free_block_id_bitboard_ = env.free_block_id_bitboard_;
    stone_bitboard_ = env.stone_bitboard_;
    grids_ = env.grids_;
    blocks_ = env.blocks_;
    actions_ = env.actions_;
    stone_bitboard_history_ = env.stone_bitboard_history_;
    hash_table_ = env.hash_table_;

    // reset grid's block pointer
    for (auto& grid : grids_) {
        if (!grid.getBlock()) { continue; }
        grid.setBlock(&blocks_[grid.getBlock()->getID()]);
    }
}

void GoEnv::reset()
{
    komi_ = minizero::config::env_go_komi;
    turn_ = Player::kPlayer1;
    hash_key_ = 0;
    free_block_id_bitboard_.reset();
    stone_bitboard_.reset();
    benson_bitboard_.reset();
    board_mask_bitboard_.reset();
    for (int i = 0; i < board_size_ * board_size_; ++i) {
        grids_[i].reset(board_size_);
        blocks_[i].reset();
        board_mask_bitboard_.set(i);
    }
    board_left_boundary_bitboard_.reset();
    board_right_boundary_bitboard_.reset();
    for (int i = 0; i < board_size_; ++i) {
        board_left_boundary_bitboard_.set(i * board_size_);
        board_right_boundary_bitboard_.set(i * board_size_ + (board_size_ - 1));
    }
    free_block_id_bitboard_.reset();
    free_block_id_bitboard_ = ~free_block_id_bitboard_ & board_mask_bitboard_;
    actions_.clear();
    stone_bitboard_history_.clear();
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
    hash_table_.insert(hash_key_);
    assert(checkDataStructure());

    // Benson
    benson_bitboard_ = go::GoBenson::getBensonBitboard(benson_bitboard_, stone_bitboard_, board_size_, board_left_boundary_bitboard_,
                                                       board_right_boundary_bitboard_, board_mask_bitboard_);
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
        GoPair<float> territory = calculateTrompTaylorTerritory();
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
                    const GoPair<GoBitboard>& last_n_trun_stone_bitboard = stone_bitboard_history_[last_n_turn];
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

void GoEnv::initialize()
{
    grids_.clear();
    blocks_.clear();
    for (int pos = 0; pos < board_size_ * board_size_; ++pos) {
        grids_.emplace_back(GoGrid(pos, board_size_));
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

    GoBitboard grid_bitboard = block->getGridBitboard();
    while (!grid_bitboard.none()) {
        int pos = grid_bitboard._Find_first();
        grid_bitboard.reset(pos);

        GoGrid& grid = grids_[pos];
        grid.setPlayer(Player::kPlayerNone);
        grid.setBlock(nullptr);
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
    if (block1->getNumStone() < block2->getNumStone()) { return combineBlocks(block2, block1); }

    block1->combineWithBlock(block2);
    GoBitboard grid_bitboard = block2->getGridBitboard();
    while (!grid_bitboard.none()) {
        int pos = grid_bitboard._Find_first();
        grid_bitboard.reset(pos);
        grids_[pos].setBlock(block1);
    }

    removeBlock(block2);
    return block1;
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

GoBitboard GoEnv::dilateBitboard(const GoBitboard& bitboard) const
{
    return ((bitboard << board_size_) |                           // move up
            (bitboard >> board_size_) |                           // move down
            ((bitboard & ~board_left_boundary_bitboard_) >> 1) |  // move left
            ((bitboard & ~board_right_boundary_bitboard_) << 1) | // move right
            bitboard) &
           board_mask_bitboard_;
}

GoPair<float> GoEnv::calculateTrompTaylorTerritory() const
{
    GoPair<float> territory(stone_bitboard_.get(Player::kPlayer1).count(), stone_bitboard_.get(Player::kPlayer2).count() + komi_);
    GoBitboard empty_stone_bitboard = ~(stone_bitboard_.get(Player::kPlayer1) | stone_bitboard_.get(Player::kPlayer2)) & board_mask_bitboard_;
    while (!empty_stone_bitboard.none()) {
        int pos = empty_stone_bitboard._Find_first();

        // flood fill
        GoBitboard flood_fill_bitboard;
        flood_fill_bitboard.set(pos);
        bool need_dilate = true;
        while (need_dilate) {
            GoBitboard dilate_bitboard = dilateBitboard(flood_fill_bitboard) & empty_stone_bitboard;
            need_dilate = (flood_fill_bitboard != dilate_bitboard);
            flood_fill_bitboard = dilate_bitboard;
        }

        // check is surrounded by only one's color
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

bool GoEnv::checkDataStructure() const
{
    // check grids
    GoPair<GoBitboard> stone_bitboard_from_grid;
    for (int pos = 0; pos < board_size_ * board_size_; ++pos) {
        const GoGrid& grid = grids_[pos];
        if (grid.getPlayer() == Player::kPlayerNone) { continue; }

        assert(grid.getBlock() && grid.getBlock()->getPlayer() == grid.getPlayer());
        assert(grid.getBlock()->getGridBitboard().test(pos));
        assert(!free_block_id_bitboard_.test(grid.getBlock()->getID()));
        stone_bitboard_from_grid.get(grid.getPlayer()).set(pos);
    }
    assert(stone_bitboard_from_grid.get(Player::kPlayer1) == stone_bitboard_.get(Player::kPlayer1));
    assert(stone_bitboard_from_grid.get(Player::kPlayer2) == stone_bitboard_.get(Player::kPlayer2));

    // check blocks
    GoPair<GoBitboard> stone_bitboard_from_block;
    GoBitboard free_block_id = ~free_block_id_bitboard_ & board_mask_bitboard_;
    GoHashKey board_hash_key = actions_.size() % 2 == 0 ? 0 : getGoTurnHashKey();
    while (!free_block_id.none()) {
        int id = free_block_id._Find_first();
        free_block_id.reset(id);

        const GoBlock* block = &blocks_[id];
        assert(block->getNumLiberty() > 0 && block->getNumStone() > 0);
        board_hash_key ^= block->getHashKey();
        stone_bitboard_from_block.get(block->getPlayer()) |= block->getGridBitboard();

        // check grids of block
        GoHashKey block_hash_key = 0;
        GoBitboard grid_bitboard = block->getGridBitboard();
        while (!grid_bitboard.none()) {
            int pos = grid_bitboard._Find_first();
            grid_bitboard.reset(pos);

            const GoGrid& grid = grids_[pos];
            assert(grid.getBlock() == block);
            assert(grid.getPlayer() == block->getPlayer());
            block_hash_key ^= getGoGridHashKey(pos, block->getPlayer());
            for (const auto& neighbor_pos : grid.getNeighbors()) {
                const GoGrid& neighbor_grid = grids_[neighbor_pos];
                if (neighbor_grid.getPlayer() != Player::kPlayerNone) { continue; }
                assert(block->getLibertyBitboard().test(neighbor_pos));
            }
        }
        assert(block_hash_key == block->getHashKey());

        // check liberties of block
        GoBitboard stone_bitboard = stone_bitboard_.get(Player::kPlayer1) | stone_bitboard_.get(Player::kPlayer2);
        GoBitboard liberties_bitboard = dilateBitboard(block->getGridBitboard()) & ~stone_bitboard;
        assert(liberties_bitboard == block->getLibertyBitboard());
        while (!liberties_bitboard.none()) {
            int pos = liberties_bitboard._Find_first();
            liberties_bitboard.reset(pos);

            assert(grids_[pos].getPlayer() == Player::kPlayerNone);
            assert(grids_[pos].getBlock() == nullptr);
        }
    }
    assert(board_hash_key == hash_key_);
    assert(stone_bitboard_from_block.get(Player::kPlayer1) == stone_bitboard_.get(Player::kPlayer1));
    assert(stone_bitboard_from_block.get(Player::kPlayer2) == stone_bitboard_.get(Player::kPlayer2));

    // check global status
    assert(actions_.size() == stone_bitboard_history_.size());

    return true;
}

} // namespace minizero::env::go