#include "go.h"
#include "color_message.h"
#include "sgf_loader.h"
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>

namespace minizero::env::go {

using namespace minizero::utils;

GoHashKey turn_hash_key;
GoPair<std::vector<GoHashKey>> grids_hash_key;

void Initialize()
{
    std::mt19937_64 generator;
    generator.seed(0);
    turn_hash_key = generator();
    for (int pos = 0; pos < kMaxGoBoardSize * kMaxGoBoardSize; ++pos) {
        grids_hash_key.Get(Player::kPlayer1).push_back(generator());
        grids_hash_key.Get(Player::kPlayer2).push_back(generator());
    }
}

GoHashKey GetGoTurnHashKey()
{
    return turn_hash_key;
}

GoHashKey GetGoGridHashKey(int position, Player p)
{
    assert(position >= 0 && position < kMaxGoBoardSize * kMaxGoBoardSize);
    assert(p == Player::kPlayer1 || p == Player::kPlayer2);
    return grids_hash_key.Get(p)[position];
}

GoAction::GoAction(const std::vector<std::string>& action_string_args, int board_size)
{
    assert(action_string_args.size() == 2);
    assert(action_string_args[0].size() == 1 && (CharToPlayer(action_string_args[0][0]) == Player::kPlayer1 || CharToPlayer(action_string_args[0][0]) == Player::kPlayer2));
    player_ = CharToPlayer(action_string_args[0][0]);
    action_id_ = SGFLoader::BoardCoordinateStringToActionID(action_string_args[1], board_size);
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
        if (!grid.GetBlock()) { continue; }
        grid.SetBlock(&blocks_[grid.GetBlock()->GetID()]);
    }
}

void GoEnv::Reset()
{
    Reset(board_size_);
}

void GoEnv::Reset(int board_size)
{
    assert(board_size > 0 && board_size <= kMaxGoBoardSize);

    board_size_ = board_size;
    komi_ = minizero::config::env_go_komi;
    turn_ = Player::kPlayer1;
    hash_key_ = 0;
    free_block_id_bitboard_.reset();
    stone_bitboard_.Reset();
    board_mask_bitboard_.reset();
    for (int i = 0; i < board_size * board_size; ++i) {
        grids_[i].Reset(board_size);
        blocks_[i].Reset();
        board_mask_bitboard_.set(i);
    }
    board_left_boundary_bitboard_.reset();
    board_right_boundary_bitboard_.reset();
    for (int i = 0; i < board_size; ++i) {
        board_left_boundary_bitboard_.set(i * board_size);
        board_right_boundary_bitboard_.set(i * board_size + (board_size - 1));
    }
    free_block_id_bitboard_.reset();
    free_block_id_bitboard_ = ~free_block_id_bitboard_ & board_mask_bitboard_;
    actions_.clear();
    stone_bitboard_history_.clear();
    hash_table_.clear();
}

bool GoEnv::Act(const GoAction& action)
{
    if (!IsLegalAction(action)) { return false; }

    const int position = action.GetActionID();
    const Player player = action.GetPlayer();

    // handle global status
    turn_ = action.NextPlayer();
    hash_key_ ^= GetGoTurnHashKey();
    actions_.push_back(action);

    if (IsPassAction(action)) {
        stone_bitboard_history_.push_back(stone_bitboard_);
        hash_table_.insert(hash_key_);
        return true;
    }

    // set grid color
    GoGrid& grid = grids_[position];
    grid.SetPlayer(player);
    hash_key_ ^= GetGoGridHashKey(position, player);

    // create new block
    GoBlock* new_block = NewBlock();
    grid.SetBlock(new_block);
    new_block->SetPlayer(player);
    new_block->AddGrid(position);
    new_block->AddHashKey(GetGoGridHashKey(position, player));

    // combine with neighbor own blocks and capture neighbor opponent blocks
    for (const auto& neighbor_pos : grid.GetNeighbors()) {
        GoGrid& neighbor_grid = grids_[neighbor_pos];
        if (neighbor_grid.GetPlayer() == Player::kPlayerNone) {
            new_block->AddLiberty(neighbor_pos);
        } else {
            GoBlock* neighbor_block = neighbor_grid.GetBlock();
            neighbor_block->RemoveLiberty(position);
            if (neighbor_block->GetPlayer() == player) {
                new_block = CombineBlocks(new_block, neighbor_block);
            } else {
                if (neighbor_block->GetNumLiberty() == 0) { RemoveBlockFromBoard(neighbor_block); }
            }
        }
    }

    stone_bitboard_.Get(player) |= new_block->GetGridBitboard();
    stone_bitboard_history_.push_back(stone_bitboard_);
    hash_table_.insert(hash_key_);
    assert(CheckDataStructure());
    return true;
}

bool GoEnv::Act(const std::vector<std::string>& action_string_args)
{
    return Act(GoAction(action_string_args, board_size_));
}

std::vector<GoAction> GoEnv::GetLegalActions() const
{
    std::vector<GoAction> actions;
    for (int pos = 0; pos <= board_size_ * board_size_; ++pos) {
        GoAction action(pos, turn_);
        if (!IsLegalAction(action)) { continue; }
        actions.push_back(action);
    }
    return actions;
}

bool GoEnv::IsLegalAction(const GoAction& action) const
{
    assert(action.GetActionID() >= 0 && action.GetActionID() <= board_size_ * board_size_);
    assert(action.GetPlayer() == Player::kPlayer1 || action.GetPlayer() == Player::kPlayer2);

    if (IsPassAction(action)) { return true; }

    const int position = action.GetActionID();
    const Player player = action.GetPlayer();
    const GoGrid& grid = grids_[position];
    if (grid.GetPlayer() != Player::kPlayerNone) { return false; }

    bool is_legal = false;
    GoBitboard check_neighbor_block_bitboard;
    GoHashKey new_hash_key = hash_key_ ^ GetGoTurnHashKey() ^ GetGoGridHashKey(position, player);
    for (const auto& neighbor_pos : grid.GetNeighbors()) {
        const GoGrid& neighbor_grid = grids_[neighbor_pos];
        if (neighbor_grid.GetPlayer() == Player::kPlayerNone) {
            is_legal = true;
        } else {
            const GoBlock* neighbor_block = neighbor_grid.GetBlock();
            if (check_neighbor_block_bitboard.test(neighbor_block->GetID())) { continue; }

            check_neighbor_block_bitboard.set(neighbor_block->GetID());
            if (neighbor_block->GetPlayer() == player) {
                if (neighbor_block->GetNumLiberty() > 1) { is_legal = true; }
            } else {
                if (neighbor_block->GetNumLiberty() == 1) {
                    new_hash_key ^= neighbor_block->GetHashKey();
                    is_legal = true;
                }
            }
        }
    }

    return (is_legal && hash_table_.count(new_hash_key) == 0);
}

bool GoEnv::IsTerminal() const
{
    // two consecutive passes
    if (actions_.size() >= 2 &&
        IsPassAction(actions_.back()) &&
        IsPassAction(actions_[actions_.size() - 2])) { return true; }

    // game length exceeds 2 * boardsize * boardsize
    if (static_cast<int>(actions_.size()) > 2 * board_size_ * board_size_) { return true; }

    return false;
}

float GoEnv::GetEvalScore(bool is_resign /*= false*/) const
{
    Player eval;
    if (is_resign) {
        eval = GetNextPlayer(turn_, kGoNumPlayer);
    } else {
        GoPair<float> territory = CalculateTrompTaylorTerritory();
        eval = (territory.Get(Player::kPlayer1) > territory.Get(Player::kPlayer2))
                   ? Player::kPlayer1
                   : ((territory.Get(Player::kPlayer1) < territory.Get(Player::kPlayer2))
                          ? Player::kPlayer2
                          : Player::kPlayerNone);
    }

    switch (eval) {
        case Player::kPlayer1: return 1.0f;
        case Player::kPlayer2: return -1.0f;
        default: return 0.0f;
    }
}

std::vector<float> GoEnv::GetFeatures() const
{
    /* 18 channels:
        0~15. black/white position for last 8 turns
        16. black turn
        17. white turn
    */
    std::vector<float> vFeatures;
    for (int channel = 0; channel < 18; ++channel) {
        for (int pos = 0; pos < board_size_ * board_size_; ++pos) {
            if (channel < 16) {
                int last_n_turn = stone_bitboard_history_.size() - 1 - channel / 2;
                if (last_n_turn < 0) {
                    vFeatures.push_back(0.0f);
                } else {
                    const GoPair<GoBitboard>& last_n_trun_stone_bitboard = stone_bitboard_history_[last_n_turn];
                    Player player = (channel % 2 == 0 ? Player::kPlayer1 : Player::kPlayer2);
                    vFeatures.push_back(last_n_trun_stone_bitboard.Get(player).test(pos) ? 1.0f : 0.0f);
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

std::string GoEnv::ToString() const
{
    int last_move_pos = -1, last2_move_pos = -1;
    if (actions_.size() >= 1) { last_move_pos = actions_.back().GetActionID(); }
    if (actions_.size() >= 2) { last2_move_pos = actions_[actions_.size() - 2].GetActionID(); }

    std::ostringstream oss;
    oss << GetCoordinateString() << std::endl;
    for (int row = board_size_ - 1; row >= 0; --row) {
        oss << GetTextWithFormat((row + 1 < 10 ? " " : "") + std::to_string(row + 1), TextType::kBold, TextColor::kBlack, TextColor::kYellow);
        for (int col = 0; col < board_size_; ++col) {
            int pos = row * board_size_ + col;
            std::string prefix = GetTextWithFormat(" ", TextType::kBold, TextColor::kBlack, TextColor::kYellow);
            if (pos == last_move_pos) {
                prefix = GetTextWithFormat(">", TextType::kBold, TextColor::kRed, TextColor::kYellow);
            } else if (pos == last2_move_pos) {
                prefix = GetTextWithFormat(">", TextType::kNormal, TextColor::kRed, TextColor::kYellow);
            }

            const GoGrid& grid = grids_[pos];
            if (grid.GetPlayer() == Player::kPlayer1) {
                oss << prefix << GetTextWithFormat("O", TextType::kBold, TextColor::kBlack, TextColor::kYellow);
            } else if (grid.GetPlayer() == Player::kPlayer2) {
                oss << prefix << GetTextWithFormat("O", TextType::kBold, TextColor::kWhite, TextColor::kYellow);
            } else {
                oss << prefix << GetTextWithFormat(".", TextType::kBold, TextColor::kBlack, TextColor::kYellow);
            }
        }
        oss << GetTextWithFormat(" " + std::to_string(row + 1) + (row + 1 < 10 ? " " : ""), TextType::kBold, TextColor::kBlack, TextColor::kYellow);
        oss << std::endl;
    }
    oss << GetCoordinateString() << std::endl;
    return oss.str();
}

void GoEnv::Initialize()
{
    grids_.clear();
    blocks_.clear();
    for (int pos = 0; pos < board_size_ * board_size_; ++pos) {
        grids_.emplace_back(GoGrid(pos, board_size_));
        blocks_.emplace_back(GoBlock(pos));
    }
}

GoBlock* GoEnv::NewBlock()
{
    assert(!free_block_id_bitboard_.none());

    int id = free_block_id_bitboard_._Find_first();
    free_block_id_bitboard_.reset(id);
    return &blocks_[id];
}

void GoEnv::RemoveBlock(GoBlock* block)
{
    assert(block && !free_block_id_bitboard_.test(block->GetID()));

    free_block_id_bitboard_.set(block->GetID());
    block->Reset();
}

void GoEnv::RemoveBlockFromBoard(GoBlock* block)
{
    assert(block);

    GoBitboard grid_bitboard = block->GetGridBitboard();
    while (!grid_bitboard.none()) {
        int pos = grid_bitboard._Find_first();
        grid_bitboard.reset(pos);

        GoGrid& grid = grids_[pos];
        grid.SetPlayer(Player::kPlayerNone);
        grid.SetBlock(nullptr);
        for (const auto& neighbor_pos : grid.GetNeighbors()) {
            GoGrid& neighbor_grid = grids_[neighbor_pos];
            if (neighbor_grid.GetPlayer() != GetNextPlayer(block->GetPlayer(), kGoNumPlayer)) { continue; }
            neighbor_grid.GetBlock()->AddLiberty(pos);
        }
    }

    hash_key_ ^= block->GetHashKey();
    stone_bitboard_.Get(block->GetPlayer()) &= ~block->GetGridBitboard();
    RemoveBlock(block);
}

GoBlock* GoEnv::CombineBlocks(GoBlock* block1, GoBlock* block2)
{
    assert(block1 && block2);

    if (block1 == block2) { return block1; }
    if (block1->GetNumStone() < block2->GetNumStone()) { return CombineBlocks(block2, block1); }

    block1->CombineWithBlock(block2);
    GoBitboard grid_bitboard = block2->GetGridBitboard();
    while (!grid_bitboard.none()) {
        int pos = grid_bitboard._Find_first();
        grid_bitboard.reset(pos);
        grids_[pos].SetBlock(block1);
    }

    RemoveBlock(block2);
    return block1;
}

std::string GoEnv::GetCoordinateString() const
{
    std::ostringstream oss;
    TextType text_type = TextType::kBold;
    TextColor text_color = TextColor::kBlack;
    TextColor text_background = TextColor::kYellow;

    oss << GetTextWithFormat("  ", text_type, text_color, text_background);
    for (int i = 0; i < board_size_; ++i) {
        char c = 'A' + i + ('A' + i >= 'I' ? 1 : 0);
        oss << GetTextWithFormat(" " + std::string(1, c), text_type, text_color, text_background);
    }
    oss << GetTextWithFormat("   ", text_type, text_color, text_background);
    return oss.str();
}

GoBitboard GoEnv::DilateBitboard(const GoBitboard& bitboard) const
{
    return ((bitboard << board_size_) |                           // move up
            (bitboard >> board_size_) |                           // move down
            ((bitboard & ~board_left_boundary_bitboard_) >> 1) |  // move left
            ((bitboard & ~board_right_boundary_bitboard_) << 1) | // move right
            bitboard) &
           board_mask_bitboard_;
}

GoPair<float> GoEnv::CalculateTrompTaylorTerritory() const
{
    GoPair<float> territory(stone_bitboard_.Get(Player::kPlayer1).count(), stone_bitboard_.Get(Player::kPlayer2).count() + komi_);
    GoBitboard empty_stone_bitboard = ~(stone_bitboard_.Get(Player::kPlayer1) | stone_bitboard_.Get(Player::kPlayer2)) & board_mask_bitboard_;
    while (!empty_stone_bitboard.none()) {
        int pos = empty_stone_bitboard._Find_first();

        // flood fill
        GoBitboard flood_fill_bitboard;
        flood_fill_bitboard.set(pos);
        bool need_dilate = true;
        while (need_dilate) {
            GoBitboard dilate_bitboard = DilateBitboard(flood_fill_bitboard) & empty_stone_bitboard;
            need_dilate = (flood_fill_bitboard != dilate_bitboard);
            flood_fill_bitboard = dilate_bitboard;
        }

        // check is surrounded by only one's color
        GoBitboard surrounding_bitboard = DilateBitboard(flood_fill_bitboard) & ~flood_fill_bitboard;
        if ((surrounding_bitboard & ~stone_bitboard_.Get(Player::kPlayer1)).none()) {
            territory.Get(Player::kPlayer1) += flood_fill_bitboard.count();
        } else if ((surrounding_bitboard & ~stone_bitboard_.Get(Player::kPlayer2)).none()) {
            territory.Get(Player::kPlayer2) += flood_fill_bitboard.count();
        }

        empty_stone_bitboard &= ~flood_fill_bitboard;
    }

    return territory;
}

bool GoEnv::CheckDataStructure() const
{
    // check grids
    GoPair<GoBitboard> stone_bitboard_from_grid;
    for (int pos = 0; pos < board_size_ * board_size_; ++pos) {
        const GoGrid& grid = grids_[pos];
        if (grid.GetPlayer() == Player::kPlayerNone) { continue; }

        assert(grid.GetBlock() && grid.GetBlock()->GetPlayer() == grid.GetPlayer());
        assert(grid.GetBlock()->GetGridBitboard().test(pos));
        assert(!free_block_id_bitboard_.test(grid.GetBlock()->GetID()));
        stone_bitboard_from_grid.Get(grid.GetPlayer()).set(pos);
    }
    assert(stone_bitboard_from_grid.Get(Player::kPlayer1) == stone_bitboard_.Get(Player::kPlayer1));
    assert(stone_bitboard_from_grid.Get(Player::kPlayer2) == stone_bitboard_.Get(Player::kPlayer2));

    // check blocks
    GoPair<GoBitboard> stone_bitboard_from_block;
    GoBitboard free_block_id = ~free_block_id_bitboard_ & board_mask_bitboard_;
    GoHashKey board_hash_key = actions_.size() % 2 == 0 ? 0 : GetGoTurnHashKey();
    while (!free_block_id.none()) {
        int id = free_block_id._Find_first();
        free_block_id.reset(id);

        const GoBlock* block = &blocks_[id];
        assert(block->GetNumLiberty() > 0 && block->GetNumStone() > 0);
        board_hash_key ^= block->GetHashKey();
        stone_bitboard_from_block.Get(block->GetPlayer()) |= block->GetGridBitboard();

        // check grids of block
        GoHashKey block_hash_key = 0;
        GoBitboard grid_bitboard = block->GetGridBitboard();
        while (!grid_bitboard.none()) {
            int pos = grid_bitboard._Find_first();
            grid_bitboard.reset(pos);

            const GoGrid& grid = grids_[pos];
            assert(grid.GetBlock() == block);
            assert(grid.GetPlayer() == block->GetPlayer());
            block_hash_key ^= GetGoGridHashKey(pos, block->GetPlayer());
            for (const auto& neighbor_pos : grid.GetNeighbors()) {
                const GoGrid& neighbor_grid = grids_[neighbor_pos];
                if (neighbor_grid.GetPlayer() != Player::kPlayerNone) { continue; }
                assert(block->GetLibertyBitboard().test(neighbor_pos));
            }
        }
        assert(block_hash_key == block->GetHashKey());

        // check liberties of block
        GoBitboard stone_bitboard = stone_bitboard_.Get(Player::kPlayer1) | stone_bitboard_.Get(Player::kPlayer2);
        GoBitboard liberties_bitboard = DilateBitboard(block->GetGridBitboard()) & ~stone_bitboard;
        assert(liberties_bitboard == block->GetLibertyBitboard());
        while (!liberties_bitboard.none()) {
            int pos = liberties_bitboard._Find_first();
            liberties_bitboard.reset(pos);

            assert(grids_[pos].GetPlayer() == Player::kPlayerNone);
            assert(grids_[pos].GetBlock() == nullptr);
        }
    }
    assert(board_hash_key == hash_key_);
    assert(stone_bitboard_from_block.Get(Player::kPlayer1) == stone_bitboard_.Get(Player::kPlayer1));
    assert(stone_bitboard_from_block.Get(Player::kPlayer2) == stone_bitboard_.Get(Player::kPlayer2));

    // check global status
    assert(actions_.size() == stone_bitboard_history_.size());

    return true;
}

} // namespace minizero::env::go