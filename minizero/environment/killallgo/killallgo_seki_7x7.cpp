#include "killallgo_seki_7x7.h"
#include "killallgo_7x7_bitboard.h"
#include "tqdm.h"
#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace minizero::env::killallgo {

using namespace minizero;
using namespace minizero::env;
using namespace minizero::env::go;
using namespace minizero::env::killallgo;
using namespace minizero::utils;

bool Seki7x7Table::lookup(const GamePair<go::GoBitboard>& game_pair) const
{
    return table_.find(game_pair) != table_.end();
}

void Seki7x7Table::insert(GamePair<go::GoBitboard> game_pair)
{
    table_.insert(game_pair);
}

void Seki7x7Table::save(const std::string& path) const
{
    constexpr size_t bitset_size = GoBitboard().size();

    std::ofstream out(path, std::ios::binary);

    size_t table_size = table_.size() * 2 * bitset_size;
    out.write(reinterpret_cast<char*>(&table_size), sizeof(table_size));

    uint8_t write_buffer;
    size_t buffer_idx = 0;
    for (auto& pair : table_) {
        const GoBitboard& stone_bitboard = pair.get(Player::kPlayer1);
        const GoBitboard& empty_bitboard = pair.get(Player::kPlayer2);
        for (size_t i = 0; i < bitset_size; i++) {
            write_buffer |= uint8_t(stone_bitboard[i]) << buffer_idx++;
            if (buffer_idx >= sizeof(write_buffer) * 8) {
                out.write(reinterpret_cast<char*>(&write_buffer), sizeof(write_buffer));
                write_buffer = 0;
                buffer_idx = 0;
            }
        }
        for (size_t i = 0; i < bitset_size; i++) {
            write_buffer |= uint8_t(empty_bitboard[i]) << buffer_idx++;
            if (buffer_idx >= sizeof(write_buffer) * 8) {
                out.write(reinterpret_cast<char*>(&write_buffer), sizeof(write_buffer));
                write_buffer = 0;
                buffer_idx = 0;
            }
        }
    }
    if (buffer_idx > 0) {
        out.write(reinterpret_cast<char*>(&write_buffer), sizeof(write_buffer));
    }
}

bool Seki7x7Table::load(const std::string& path)
{
    constexpr size_t bitset_size = GoBitboard().size();

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) { return false; }

    size_t table_size = 0;
    in.read(reinterpret_cast<char*>(&table_size), sizeof(table_size));

    uint8_t read_buffer;
    GoBitboard stone_bitboard;
    GoBitboard empty_bitboard;
    size_t bit_idx = 0;
    size_t total_size = 0;
    while (true) {
        in.read(reinterpret_cast<char*>(&read_buffer), sizeof(read_buffer));
        size_t size = in.gcount();
        if (size == 0)
            break;
        for (size_t i = 0; i < sizeof(read_buffer) * 8 && i < size * 8; i++) {
            if (bit_idx < bitset_size) {
                stone_bitboard.set(bit_idx, read_buffer & 1ull << i);
                total_size++;
            } else {
                empty_bitboard.set(bit_idx % bitset_size, read_buffer & 1ull << i);
                total_size++;
            }
            if (++bit_idx >= 2 * bitset_size) {
                assert(total_size <= table_size);
                bit_idx = 0;
                table_.insert({stone_bitboard, empty_bitboard});
            }
        }
    }
    return true;
}

int Seki7x7Table::size() const
{
    return table_.size();
}

SekiSearch::BlockSet SekiSearch::getInitBlockSet()
{
    constexpr int drow[] = {-1, 0, 0, 1};
    constexpr int dcol[] = {0, -1, 1, 0};
    BlockSet block_set;
    for (int id = 0; id < kKillAllGoBoardSize * kKillAllGoBoardSize; id++) {
        GoBitboard board;
        std::vector<int> boundary;
        board.set(id, true);
        // check and append new boundary id
        std::pair<int, int> pos = idtorc(id); // pos: (row, col)
        for (int k = 0; k < 4; k++) {
            int r = pos.first + drow[k], c = pos.second + dcol[k];
            // check if index out of range
            if (!rc(r) || !rc(c))
                continue;
            int id = rctoid(r, c);
            // if board[r, c] already occupied
            if (board.test(id))
                continue;
            boundary.emplace_back(id);
        }
        block_set.insert({std::move(board), std::move(boundary)});
    }
    return block_set;
}

SekiSearch::BlockSet SekiSearch::increaseBlockSetSize(BlockSet src_block_set)
{
    constexpr int drow[] = {-1, 0, 0, 1};
    constexpr int dcol[] = {0, -1, 1, 0};
    BlockSet block_set;
    for (const auto& board_pair : src_block_set) {
        const std::vector<int>& ref_boundary = board_pair.second;
        for (const auto& id : ref_boundary) {
            GoBitboard board = board_pair.first;
            std::vector<int> boundary;
            std::set<int> boundary_set;
            board.set(id, true);
            // check and append old boundary id
            for (const auto& old_id : ref_boundary) {
                if (board.test(old_id))
                    continue;
                boundary_set.insert(old_id);
            }
            // check and append new boundary id
            std::pair<int, int> pos = idtorc(id); // pos: (row, col)
            for (int k = 0; k < 4; k++) {
                int r = pos.first + drow[k], c = pos.second + dcol[k];
                // check if index out of range
                if (!rc(r) || !rc(c))
                    continue;
                int id = rctoid(r, c);
                // if board[r, c] already occupied
                if (board.test(id))
                    continue;
                boundary_set.insert(id);
            }
            std::copy(boundary_set.begin(), boundary_set.end(), std::back_inserter(boundary));
            block_set.insert({std::move(board), std::move(boundary)});
        }
    }
    return block_set;
}

SekiSearch::IsoBitboardSet SekiSearch::generateBlockCombinations(int min_block_size, int max_block_size)
{
    assert(min_block_size >= 1);
    assert(max_block_size >= min_block_size);
    IsoBitboardSet bitboard_set;
    // get all possible block combinations with size 1
    BlockSet block_set = getInitBlockSet();
    // increase block size step by step
    for (int block_size = 1; block_size <= max_block_size; block_size++) {
        if (block_size >= min_block_size) {
            for (const auto& board_pair : block_set) {
                bitboard_set.insert(board_pair.first);
            }
        }
        // early stop to avoid unnecessary computation
        if (block_size >= max_block_size) { break; }
        block_set = increaseBlockSetSize(std::move(block_set));
    }
    return bitboard_set;
}

std::vector<GamePair<GoBitboard>> SekiSearch::generateSekiPatterns(const GoBitboard& board)
{
    // get placed stone pos
    std::vector<int> stone_id;
    for (int id = 0; id < kKillAllGoBoardSize * kKillAllGoBoardSize; ++id) {
        if (board.test(id))
            stone_id.emplace_back(id);
    }
    // generate the boundary of board
    // ActionList boundary_actions;
    GoBitboard boundary;
    constexpr int drow[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    constexpr int dcol[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    for (const auto& id : stone_id) {
        std::pair<int, int> pos = idtorc(id); // pos: (row, col)
        for (int k = 0; k < 8; ++k) {
            const int crow = pos.first + drow[k], ccol = pos.second + dcol[k];
            // check index range
            if (!rc(crow) || !rc(ccol)) { continue; }
            const int id = rctoid(crow, ccol);
            // check already has stone
            if (board.test(id) || boundary.test(id)) { continue; }
            boundary.set(id, true);
        }
    }
    // select all combinations of two liberty
    std::vector<GamePair<GoBitboard>> board_list;
    constexpr int EYES[] = {2, 3};
    for (const auto eyes : EYES) {
        std::vector<bool> combination_mask(eyes, 1);
        combination_mask.resize(board.count(), 0);
        do {
            // selected stones
            GoBitboard selected_stones;
            for (std::size_t i = 0; i < combination_mask.size(); i++) {
                // select the non-eye pos
                if (!combination_mask[i]) {
                    selected_stones.set(stone_id[i], true);
                }
            }
            // add to board list
            board_list.emplace_back(GamePair<GoBitboard>{selected_stones, boundary});
        } while (std::prev_permutation(combination_mask.begin(), combination_mask.end()));
    }
    return board_list;
}

SekiSearch::ActionList SekiSearch::bitboardToActionList(const GoBitboard& board, Player player)
{
    ActionList actions;
    for (int id = 0; id < kKillAllGoBoardSize * kKillAllGoBoardSize; ++id) {
        if (board.test(id)) { actions.emplace_back(GoAction(id, player)); }
    }
    return actions;
}

void SekiSearch::actionListToEnv(KillAllGoEnv& env, const ActionList& actions)
{
    static const GoAction p1_pass_action = {kKillAllGoBoardSize * kKillAllGoBoardSize, Player::kPlayer1};
    static const GoAction p2_pass_action = {kKillAllGoBoardSize * kKillAllGoBoardSize, Player::kPlayer2};
    for (const auto& action : actions) {
        env.setTurn(action.getPlayer());
        if (env.isLegalAction(action)) {
            env.act(action);
        } else {
            env.act(action.getPlayer() == Player::kPlayer1 ? p1_pass_action : p2_pass_action);
            env.setTurn(action.getPlayer());
            env.act(action);
        }
    }
}

void SekiSearch::bitboardToEnv(KillAllGoEnv& env, const go::GoBitboard& board, Player player)
{
    actionListToEnv(env, bitboardToActionList(board, player));
}

GamePair<GoBitboard> SekiSearch::getStoneAndEmptyBitboard(const KillAllGoEnv& env, const GoAction& action)
{
    const GoGrid& grid = env.getGrid(action.getActionID());
    GoBitboard area_bitboard = grid.getArea(Player::kPlayer2)->getAreaBitboard();
    GoBitboard stone_bitboard = area_bitboard & env.getStoneBitboard().get(Player::kPlayer1);
    GoBitboard empty_bitboard = area_bitboard & ~(env.getStoneBitboard().get(Player::kPlayer1) | env.getStoneBitboard().get(Player::kPlayer2));
    return {stone_bitboard, empty_bitboard};
}

void SekiSearch::generateSekiTable(Seki7x7Table& seki_table, int min_area_size, int max_area_size)
{
    assert(min_area_size >= 1);
    assert(max_area_size >= min_area_size);
    KillAllGoEnv env(7);
    for (const auto& area_bitboard : tqdm::tqdm(generateBlockCombinations(min_area_size, max_area_size), "Generate seki table: {percentage}|{pbar}| {step}/{size} [{es;h:m:s}<{ps;h:m:s}]")) {
        // Generate all possible seki patterns from given area bitboard
        for (const auto& game_pair : generateSekiPatterns(area_bitboard)) {
            env.reset();

            GoBitboard black_bitboard = game_pair.get(Player::kPlayer1);
            GoBitboard white_bitboard = game_pair.get(Player::kPlayer2);
            // Calculate the boundary of the seki area
            GoBitboard boundary_bitboard = env.dilateBitboard(white_bitboard) & ~(area_bitboard | white_bitboard);

            bitboardToEnv(env, white_bitboard, Player::kPlayer2);
            bitboardToEnv(env, boundary_bitboard, Player::kPlayer1);
            // Player 1 must be the last player to act
            bitboardToEnv(env, black_bitboard, Player::kPlayer1);

            // Check no stone is captured
            if (!(env.getStoneBitboard() == GamePair<GoBitboard>{black_bitboard | boundary_bitboard, white_bitboard})) { continue; }

            GamePair<GoBitboard> b = searchSekiBitboard(env, env.getActionHistory().back());
            if (!b.get(Player::kPlayer1).none() || !b.get(Player::kPlayer2).none()) {
                seki_table.insert(getStoneAndEmptyBitboard(env, env.getActionHistory().back()));
            }
        }
    }
}

GamePair<GoBitboard> SekiSearch::lookupSekiBitboard(Seki7x7Table& seki_table, const KillAllGoEnv& env, const GoAction& action)
{
    if (env.getBoardSize() != 7) { return GamePair<GoBitboard>(); }
    if (env.isPassAction(action) || !env.getBensonBitboard().get(Player::kPlayer2).none()) { return GamePair<GoBitboard>(); }

    const GoGrid& grid = env.getGrid(action.getActionID());
    assert(grid.getPlayer() != Player::kPlayerNone);

    const GoBitboard stone_bitboard = env.getStoneBitboard().get(Player::kPlayer1);
    const GoBitboard empty_bitboard = ~(env.getStoneBitboard().get(Player::kPlayer1) | env.getStoneBitboard().get(Player::kPlayer2));

    const GoArea* seki_area = nullptr;
    if (grid.getPlayer() == Player::kPlayer1) {
        const GoArea* area = grid.getArea(Player::kPlayer2);
        if (area && area->getNeighborBlockIDBitboard().count() == 1) {
            const GoBitboard& area_bitboard = area->getAreaBitboard();
            if (seki_table.lookup({stone_bitboard & area_bitboard, empty_bitboard & area_bitboard})) {
                seki_area = area;
            }
        }
    } else if (grid.getPlayer() == Player::kPlayer2) {
        const GoBlock* block = grid.getBlock();
        GoBitboard area_bitboard_id = block->getNeighborAreaIDBitboard();
        while (!area_bitboard_id.none()) {
            int area_id = area_bitboard_id._Find_first();
            area_bitboard_id.reset(area_id);

            const GoArea* area = &env.getArea(area_id);
            assert(area);
            if (area->getNeighborBlockIDBitboard().count() != 1) { continue; }
            const GoBitboard& area_bitboard = area->getAreaBitboard();
            if (!seki_table.lookup({stone_bitboard & area_bitboard, empty_bitboard & area_bitboard})) { continue; }
            seki_area = area;
            break;
        }
    }

    if (!seki_area) { return GamePair<GoBitboard>(); }
    GamePair<GoBitboard> seki_bitboard;
    seki_bitboard.get(Player::kPlayer2) |= seki_area->getAreaBitboard();
    seki_bitboard.get(Player::kPlayer2) |= env.getBlock(seki_area->getNeighborBlockIDBitboard()._Find_first()).getGridBitboard();
    return seki_bitboard;
}

bool SekiSearch::isSeki(Seki7x7Table& seki_table, const KillAllGoEnv& env)
{
    if (env.getBoardSize() != 7) { return false; }
    auto b = env.getActionHistory().size()
                 ? lookupSekiBitboard(seki_table, env, env.getActionHistory().back())
                 : GamePair<GoBitboard>{GoBitboard(), GoBitboard()};
    if (!b.get(Player::kPlayer1).none() || !b.get(Player::kPlayer2).none()) {
        return true;
    }
    return false;
}

GamePair<GoBitboard> SekiSearch::searchSekiBitboard(const KillAllGoEnv& env, const GoAction& action)
{
    if (env.isPassAction(action) || !env.getBensonBitboard().get(Player::kPlayer2).none()) { return GamePair<GoBitboard>(); }

    const GoGrid& grid = env.getGrid(action.getActionID());
    assert(grid.getPlayer() != Player::kPlayerNone);

    const GoArea* seki_area = nullptr;
    if (grid.getPlayer() == Player::kPlayer1) {
        if (grid.getArea(Player::kPlayer2) && isEnclosedSeki(env, grid.getArea(Player::kPlayer2))) { seki_area = grid.getArea(Player::kPlayer2); }
    } else if (grid.getPlayer() == Player::kPlayer2) {
        const GoBlock* block = grid.getBlock();
        GoBitboard area_bitboard_id = block->getNeighborAreaIDBitboard();
        while (!area_bitboard_id.none()) {
            int area_id = area_bitboard_id._Find_first();
            area_bitboard_id.reset(area_id);

            const GoArea* area = &env.getArea(area_id);
            if (!isEnclosedSeki(env, area)) { continue; }
            seki_area = area;
            break;
        }
    }

    if (!seki_area) { return GamePair<GoBitboard>(); }
    GamePair<GoBitboard> seki_bitboard;
    seki_bitboard.get(Player::kPlayer2) |= seki_area->getAreaBitboard();
    seki_bitboard.get(Player::kPlayer2) |= env.getBlock(seki_area->getNeighborBlockIDBitboard()._Find_first()).getGridBitboard();
    return seki_bitboard;
}

bool SekiSearch::isEnclosedSeki(const KillAllGoEnv& env, const GoArea* area)
{
    assert(area);

    // find the white surrounding block with maximum stones
    GoBitboard neighbor_block_id = area->getNeighborBlockIDBitboard(); // find white surrounding blocks
    const GoBlock* surrounding_block = &env.getBlock(neighbor_block_id._Find_first());
    while (!neighbor_block_id.none()) {
        int block_id = neighbor_block_id._Find_first();
        neighbor_block_id.reset(block_id);
        const GoBlock* neighbor_block = &env.getBlock(block_id);
        if (neighbor_block->getPlayer() == Player::kPlayer2) {
            if (neighbor_block->getGridBitboard().count() > surrounding_block->getGridBitboard().count()) {
                surrounding_block = neighbor_block;
            }
        }
    }

    // not seki if there are no black stones in the area
    GoBitboard inner_stone_bitboard = area->getAreaBitboard() & env.getStoneBitboard().get(Player::kPlayer1);
    if (inner_stone_bitboard.none()) { return false; } // no black stone inside

    // find black stones and their libarities bitboard
    GoBitboard inner_bitboard = env.dilateBitboard(inner_stone_bitboard) & ~env.getStoneBitboard().get(Player::kPlayer2); // remove while loop, inner black stone and their liberty

    // search_area_bitboard is composed of neighbor white blocks and their liberties and dilate bitboard of inner black stone
    GoBitboard search_area_bitboard = inner_bitboard;
    GoBitboard area_neighbor_block_id = area->getNeighborBlockIDBitboard(); // find white surrounding block
    while (!area_neighbor_block_id.none()) {
        int area_block_id = area_neighbor_block_id._Find_first();
        area_neighbor_block_id.reset(area_block_id);
        const GoBlock* neighbor_block = &env.getBlock(area_block_id);
        if (neighbor_block->getPlayer() == Player::kPlayer2) {
            search_area_bitboard |= (neighbor_block->getGridBitboard());
            search_area_bitboard |= neighbor_block->getLibertyBitboard();
        }
    }

    // if the area is too sparse, stop the search. It must be the combination of inner bitboard and white block's liberty
    if (!(area->getAreaBitboard() & ~inner_bitboard & ~surrounding_block->getLibertyBitboard()).none()) { return false; }
    return enclosedSekiSearch(env, surrounding_block, search_area_bitboard, Player::kPlayer2, Player::kPlayer2) & enclosedSekiSearch(env, surrounding_block, search_area_bitboard, Player::kPlayer1, Player::kPlayer1);
}

bool SekiSearch::enclosedSekiSearch(const KillAllGoEnv& env, const GoBlock* block, const GoBitboard& search_area_bitboard, Player turn, Player attacker)
{
    assert(block);
    if (!env.getBensonBitboard().get(Player::kPlayer2).none()) { // white win if is Benson
        return turn == Player::kPlayer1 ? (turn == attacker ? true : false) : (turn == attacker ? false : true);
    } else if ((block->getGridBitboard() & env.getStoneBitboard().get(Player::kPlayer2)).none() ||
               !(search_area_bitboard & env.getBensonBitboard().get(Player::kPlayer1)).none()) { // black win if white block has been eaten or inner block become benson
        return turn == Player::kPlayer1 ? (turn == attacker ? false : true) : (turn == attacker ? true : false);
    }

    GoBitboard area_bitboard = search_area_bitboard;
    area_bitboard |= block->getGridBitboard(); // search area adds the new block
    area_bitboard = area_bitboard & ~env.getStoneBitboard().get(Player::kPlayer1) & ~env.getStoneBitboard().get(Player::kPlayer2);

    GoBitboard liberty_area_bitboard = block->getLibertyBitboard(); // only black can act on liberty board
    if (turn == Player::kPlayer1) { area_bitboard |= liberty_area_bitboard; }
    if (turn != attacker) { area_bitboard.set(kKillAllGoBoardSize * kKillAllGoBoardSize); } // defend player can choose to play PASS

    std::vector<GoBitboard> search_proirity_set = findSearchPrioritySet(env, block, area_bitboard, turn);
    for (unsigned long set_num = 0; set_num < search_proirity_set.size(); set_num++) {
        GoBitboard search_bitboard = search_proirity_set[set_num];
        while (!search_bitboard.none()) {
            int pos = search_bitboard._Find_first();
            search_bitboard.reset(pos);
            KillAllGoAction action(pos, turn);
            if (!env.isLegalAction(action)) { continue; }
            KillAllGoEnv env_copy = env;
            env_copy.act(action);

            int block_pos = block->getGridBitboard()._Find_first();
            const GoBlock* new_block = env.getGrid(block_pos).getBlock();

            if (turn == attacker) { // attack player
                bool defender_win = enclosedSekiSearch(env_copy, new_block, search_area_bitboard, getNextPlayer(turn, kKillAllGoNumPlayer), attacker);
                if (defender_win) {
                    continue;
                } else {
                    return false;
                }
            } else { // defend player
                bool attacker_lose = enclosedSekiSearch(env_copy, new_block, search_area_bitboard, getNextPlayer(turn, kKillAllGoNumPlayer), attacker);
                if (attacker_lose) {
                    return true;
                } else {
                    continue;
                }
            }
        }
    }

    return turn == attacker ? true : false;
}

std::vector<GoBitboard> SekiSearch::findSearchPrioritySet(const KillAllGoEnv& env, const GoBlock* block, const GoBitboard& area_bitboard, Player turn)
{
    std::vector<GoBitboard> search_proirity_set;

    GoBitboard act_area_bitboard = area_bitboard;
    GoBitboard eat_stone_act_bitboard = act_area_bitboard.reset();
    while (!act_area_bitboard.none()) { // first order: action that can eat stone
        int pos = act_area_bitboard._Find_first();
        act_area_bitboard.reset(pos);
        KillAllGoAction action(pos, turn);
        if (!env.isLegalAction(action)) { continue; }
        if (env.isPassAction(action)) { continue; }
        const GoGrid& grid = env.getGrid(pos);
        for (const auto& neighbor_pos : grid.getNeighbors()) {
            const GoGrid& neighbor_grid = env.getGrid(neighbor_pos);
            if (neighbor_grid.getPlayer() == Player::kPlayerNone) {
                continue;
            } else {
                const GoBlock* neighbor_block = neighbor_grid.getBlock();
                if (neighbor_block->getNumLiberty() == 1) {
                    eat_stone_act_bitboard.set(pos);
                }
            }
        }
    }
    search_proirity_set.emplace_back(eat_stone_act_bitboard);
    GoBitboard block_liberty_bitboard = block->getLibertyBitboard() & ~eat_stone_act_bitboard; // second order, action can reduce the surrouding block's liberty
    search_proirity_set.emplace_back(block_liberty_bitboard);
    GoBitboard area_bitboard_remain = area_bitboard & ~eat_stone_act_bitboard & ~block_liberty_bitboard;
    search_proirity_set.emplace_back(area_bitboard_remain);

    return search_proirity_set;
}
} // namespace minizero::env::killallgo
