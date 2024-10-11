#include "killallgo_seki_7x7.h"
#include "killallgo_7x7_bitboard.h"
#include "thread_pool.h"
#include "tqdm.h"
#include "utils.h"
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

bool Seki7x7Table::lookup(const std::pair<GamePair<go::GoBitboard>, std::string>& game_pair, std::string& ghi_string) const
{
    auto it = table_.find(game_pair);
    if (it != table_.end()) {
        ghi_string = it->second;
        return true;
    }
    return false;
}

void Seki7x7Table::insert(std::pair<GamePair<go::GoBitboard>, std::string> game_pair)
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
        const GoBitboard& stone_bitboard = pair.first.get(Player::kPlayer1);
        const GoBitboard& empty_bitboard = pair.first.get(Player::kPlayer2);
        std::string ghi_data = pair.second;
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
        if (buffer_idx > 0) {
            while (buffer_idx < sizeof(write_buffer) * 8) {
                write_buffer |= 0 << buffer_idx++;
            }
            out.write(reinterpret_cast<char*>(&write_buffer), sizeof(write_buffer));
            write_buffer = 0;
            buffer_idx = 0;
        }
        out.write(ghi_data.c_str(), ghi_data.size());
        out.write("\0", 1);
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
    std::string ghi_data = "";
    size_t bit_idx = 0;
    size_t total_size = 0;
    size_t size = in.gcount();
    while (size > 0) {
        ghi_data = "";
        bit_idx = 0;

        for (int k = 0; k < 91; k++) {
            in.read(reinterpret_cast<char*>(&read_buffer), sizeof(read_buffer));
            size = in.gcount();
            if (size == 0) { break; }

            for (size_t i = 0; i < sizeof(read_buffer) * 8 && i < size * 8; i++) {
                if (bit_idx < bitset_size) {
                    stone_bitboard.set(bit_idx % bitset_size, read_buffer & 1ull << i);
                    total_size++;
                } else if (bit_idx < 2 * bitset_size) {
                    empty_bitboard.set(bit_idx % bitset_size, read_buffer & 1ull << i);
                    total_size++;
                }
                ++bit_idx;
            }
        }
        if (size == 0) { break; }

        char str[1];
        while (in.read(str, 1) && str[0] != '\0') { ghi_data += str[0]; }

        GamePair<go::GoBitboard> pair{stone_bitboard, empty_bitboard};
        table_.insert({pair, ghi_data});
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

    std::vector<GoBitboard> block_combinations;
    { // Release the memory of block_combinations_set
        auto block_combinations_set = generateBlockCombinations(min_area_size, max_area_size);
        block_combinations.insert(block_combinations.end(), block_combinations_set.begin(), block_combinations_set.end());
    }

    tqdm::Tqdm seki_solving_progress(block_combinations.size(), "Generate seki table: {percentage}|{pbar}| {step}/{size} [seki count: {n_seki}] [{es;h:m:s}<{ps;h:m:s}]");
    int seki_count = 0;
    int ghi_count = 0;
    int total_count = 0;
    std::mutex mtx;

    utils::ThreadPool pool;
    pool.start([&](int wid, int tid) {
        static thread_local KillAllGoEnv env(kKillAllGoBoardSize);
        auto& area_bitboard = block_combinations[wid];
        // Generate all possible seki patterns from given area bitboard
        auto seki_patterns = generateSekiPatterns(area_bitboard);
        for (size_t i = 0; i < seki_patterns.size(); i++) {
            const auto& game_pair = seki_patterns[i];
            env.reset();

            GoBitboard black_bitboard = game_pair.get(Player::kPlayer1);
            GoBitboard white_bitboard = game_pair.get(Player::kPlayer2);
            // Calculate the boundary of the seki area
            GoBitboard boundary_bitboard = env.dilateBitboard(white_bitboard) & ~(area_bitboard | white_bitboard);

            bitboardToEnv(env, black_bitboard, Player::kPlayer1);
            bitboardToEnv(env, white_bitboard, Player::kPlayer2);
            bitboardToEnv(env, boundary_bitboard, Player::kPlayer1);
            // Player 1 must be the last player to act

            // Check no stone is captured
            if (!(env.getStoneBitboard() == GamePair<GoBitboard>{black_bitboard | boundary_bitboard, white_bitboard})) { continue; }

            KillAllGoAction inner_black_action(black_bitboard._Find_first(), Player::kPlayer1);
            std::pair<GamePair<GoBitboard>, std::string> seki_result = searchSekiBitboard(env, inner_black_action);
            GamePair<GoBitboard> b = seki_result.first;
            std::string ghi_data = seki_result.second;
            bool is_seki = !b.get(Player::kPlayer1).none() || !b.get(Player::kPlayer2).none();

            {   // Insert seki pattern to seki table and update progress bar
                std::lock_guard<std::mutex> lock(mtx);
                if (is_seki) {
                    seki_table.insert({getStoneAndEmptyBitboard(env, inner_black_action), ghi_data});
                    seki_count++;
                    if (!ghi_data.empty()) { ghi_count++; }
                }
                total_count++;  // Update total count
                if (i == seki_patterns.size() - 1) {
                    seki_solving_progress.set("n_seki", std::to_string(seki_count));
                    seki_solving_progress.step();
                }
            }
        }
    },
               block_combinations.size(), 16 /* std::thread::hardware_concurrency() */);
}

GamePair<GoBitboard> SekiSearch::lookupSekiBitboard(Seki7x7Table& seki_table, const KillAllGoEnv& env, const GoAction& action, std::string& ret_ghi_string)
{
    if (env.getBoardSize() != kKillAllGoBoardSize) { return GamePair<GoBitboard>(); }
    if (env.isPassAction(action) || !env.getBensonBitboard().get(Player::kPlayer2).none()) { return GamePair<GoBitboard>(); }

    const GoGrid& grid = env.getGrid(action.getActionID());
    assert(grid.getPlayer() != Player::kPlayerNone);

    const GoBitboard stone_bitboard = env.getStoneBitboard().get(Player::kPlayer1);
    const GoBitboard empty_bitboard = ~(env.getStoneBitboard().get(Player::kPlayer1) | env.getStoneBitboard().get(Player::kPlayer2));

    const GoArea* seki_area = nullptr;
    std::string ghi_string;
    if (grid.getPlayer() == Player::kPlayer1) {
        const GoArea* area = grid.getArea(Player::kPlayer2);
        if (area) {
            GoBitboard neighbor_id = area->getNeighborBlockIDBitboard();
            int surrouding_block_count = 0;
            while (!neighbor_id.none()) {
                int block_id = neighbor_id._Find_first();
                neighbor_id.reset(block_id);

                const GoBlock* block = &env.getBlock(block_id);
                assert(block);

                if (block->getNeighborAreaIDBitboard().count() > 1) {
                    surrouding_block_count++;
                    if (surrouding_block_count > 1) { break; }
                }
            }
            if (surrouding_block_count <= 1) {
                const GoBitboard& area_bitboard = area->getAreaBitboard();
                if (seki_table.lookup({{stone_bitboard & area_bitboard, empty_bitboard & area_bitboard}, ""}, ghi_string)) {
                    seki_area = area;
                }
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

            GoBitboard neighbor_id = area->getNeighborBlockIDBitboard();
            int surrouding_block_count = 0;
            while (!neighbor_id.none()) {
                int block_id = neighbor_id._Find_first();
                neighbor_id.reset(block_id);
                const GoBlock* block = &env.getBlock(block_id);
                assert(block);
                if (block->getNeighborAreaIDBitboard().count() > 1) {
                    surrouding_block_count++;
                    if (surrouding_block_count > 1) { break; }
                }
            }
            if (surrouding_block_count <= 1) {
                const GoBitboard& area_bitboard = area->getAreaBitboard();
                if (seki_table.lookup({{stone_bitboard & area_bitboard, empty_bitboard & area_bitboard}, ""}, ghi_string)) {
                    seki_area = area;
                }
            }
        }
    }

    if (!seki_area) { return GamePair<GoBitboard>(); }
    GamePair<GoBitboard> seki_bitboard;
    seki_bitboard.get(Player::kPlayer2) |= seki_area->getAreaBitboard();
    GoBitboard nbrBlockID_bitboard = seki_area->getNeighborBlockIDBitboard();
    while (nbrBlockID_bitboard.any()) {
        int id = nbrBlockID_bitboard._Find_first();
        nbrBlockID_bitboard.reset(id);
        seki_bitboard.get(Player::kPlayer2) |= env.getBlock(id).getGridBitboard();
    }

    std::vector<GamePair<GoBitboard>> check_patterns = getPatternsFromGHIString(env, ghi_string);
    if (hasHistoryGHIIssue(env, check_patterns, seki_bitboard.get(Player::kPlayer2))) { return GamePair<GoBitboard>(); }

    ret_ghi_string = convertToFormalGHIString(env, check_patterns, seki_bitboard.get(Player::kPlayer2));

    return seki_bitboard;
}

bool SekiSearch::isSeki(Seki7x7Table& seki_table, const KillAllGoEnv& env)
{
    if (env.getBoardSize() != kKillAllGoBoardSize) { return false; }
    std::string ghi_string;
    auto b = env.getActionHistory().size()
                 ? lookupSekiBitboard(seki_table, env, env.getActionHistory().back(), ghi_string)
                 : GamePair<GoBitboard>{GoBitboard(), GoBitboard()};
    if (!b.get(Player::kPlayer1).none() || !b.get(Player::kPlayer2).none()) {
        return true;
    }
    return false;
}

std::vector<GamePair<GoBitboard>> SekiSearch::getPatternsFromGHIString(const KillAllGoEnv& env, const std::string& ghi_string)
{
    std::vector<GamePair<GoBitboard>> check_patterns;
    if (ghi_string == "") { return check_patterns; }

    std::vector<std::string> ghi_pattern_strings = utils::stringToVector(ghi_string, "-");
    for (const auto& ghi_pattern : ghi_pattern_strings) {
        std::vector<std::string> bitboard_strings = utils::stringToVector(ghi_pattern, ":");
        GoBitboard black_bitboard(bitboard_strings[0]);
        GoBitboard white_bitboard(bitboard_strings[1]);
        GoBitboard dilated_white_bitboard = white_bitboard;
        while (white_bitboard.any()) {
            int id = white_bitboard._Find_first();
            white_bitboard.reset(id);
            const GoGrid& grid = env.getGrid(id);
            if (grid.getPlayer() != Player::kPlayerNone) { white_bitboard &= ~grid.getBlock()->getGridBitboard(); }
            if (grid.getPlayer() == Player::kPlayer2) { dilated_white_bitboard |= grid.getBlock()->getGridBitboard(); }
        }
        check_patterns.push_back({black_bitboard, dilated_white_bitboard});
    }

    return check_patterns;
}

bool SekiSearch::hasHistoryGHIIssue(const KillAllGoEnv& env, const std::vector<GamePair<GoBitboard>>& checked_patterns, const GoBitboard& rzone_bitboard)
{
    if (checked_patterns.size() == 0) { return false; }

    const std::vector<GamePair<GoBitboard>>& stone_bitboard_history = env.getStoneBitboardHistory();
    for (size_t history_index = 0; history_index < stone_bitboard_history.size(); ++history_index) {
        if (history_index % 2 == 0) { continue; }

        // Only check history stones that White player just played
        const auto& stone_bitboards = stone_bitboard_history.at(history_index);
        const auto& black_in_rzone = stone_bitboards.get(Player::kPlayer1) & rzone_bitboard;
        const auto& white_in_rzone = stone_bitboards.get(Player::kPlayer2) & rzone_bitboard;
        for (const auto& check_pattern : checked_patterns) {
            if (check_pattern.get(Player::kPlayer1) == black_in_rzone && check_pattern.get(Player::kPlayer2) == white_in_rzone) { return true; }
        }
    }

    return false;
}

std::string SekiSearch::convertToFormalGHIString(const KillAllGoEnv& env, const std::vector<GamePair<GoBitboard>>& checked_patterns, const GoBitboard& rzone_bitboard)
{
    if (checked_patterns.size() == 0) { return ""; }

    std::ostringstream oss;
    bool b_first = true;
    for (const auto& pattern : checked_patterns) {
        if (b_first) {
            b_first = false;
        } else {
            oss << "-";
        }
        oss << std::hex
            << rzone_bitboard.to_ullong() << ":"
            << pattern.get(Player::kPlayer1).to_ullong() << ":"
            << pattern.get(Player::kPlayer2).to_ullong();
    }

    oss << ";0";

    return oss.str();
}

std::pair<GamePair<GoBitboard>, std::string> SekiSearch::searchSekiBitboard(const KillAllGoEnv& env, const GoAction& action)
{
    std::string ghi_data = "";
    bool enclosedseki = false;
    if (env.isPassAction(action) || !env.getBensonBitboard().get(Player::kPlayer2).none()) { return {GamePair<GoBitboard>(), ghi_data}; }

    const GoGrid& grid = env.getGrid(action.getActionID());
    assert(grid.getPlayer() != Player::kPlayerNone);

    const GoArea* seki_area = nullptr;
    if (grid.getPlayer() == Player::kPlayer1) {
        if (grid.getArea(Player::kPlayer2)) {
            std::tie(enclosedseki, ghi_data) = isEnclosedSeki(env, grid.getArea(Player::kPlayer2));
            if (enclosedseki) { seki_area = grid.getArea(Player::kPlayer2); }
        }
    } else if (grid.getPlayer() == Player::kPlayer2) {
        const GoBlock* block = grid.getBlock();
        GoBitboard area_bitboard_id = block->getNeighborAreaIDBitboard();
        while (!area_bitboard_id.none()) {
            int area_id = area_bitboard_id._Find_first();
            area_bitboard_id.reset(area_id);

            const GoArea* area = &env.getArea(area_id);
            std::tie(enclosedseki, ghi_data) = isEnclosedSeki(env, area);
            if (!isEnclosedSeki(env, area).first) { continue; }
            seki_area = area;
            break;
        }
    }

    if (!seki_area) { return {GamePair<GoBitboard>(), ghi_data}; }
    GamePair<GoBitboard> seki_bitboard;
    seki_bitboard.get(Player::kPlayer2) |= seki_area->getAreaBitboard();
    seki_bitboard.get(Player::kPlayer2) |= env.getBlock(seki_area->getNeighborBlockIDBitboard()._Find_first()).getGridBitboard();
    return {seki_bitboard, ghi_data};
}

std::pair<bool, std::string> SekiSearch::isEnclosedSeki(const KillAllGoEnv& env, const GoArea* area)
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
    if (inner_stone_bitboard.none()) { return {false, ""}; } // no black stone inside

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
    if (!(area->getAreaBitboard() & ~inner_bitboard & ~surrounding_block->getLibertyBitboard()).none()) { return {false, ""}; }

    bool is_seki = enclosedSekiSearch(env, surrounding_block, search_area_bitboard, Player::kPlayer2, Player::kPlayer2, true, false, false).first & enclosedSekiSearch(env, surrounding_block, search_area_bitboard, Player::kPlayer1, Player::kPlayer1, true, false, false).first;
    if (!is_seki) {
        return {false, ""};
    } else {
        bool is_sekipass = enclosedSekiSearch(env, surrounding_block, search_area_bitboard, Player::kPlayer2, Player::kPlayer2, true, true, false).first & enclosedSekiSearch(env, surrounding_block, search_area_bitboard, Player::kPlayer1, Player::kPlayer1, true, true, false).first;
        if (is_sekipass) {
            std::pair<bool, std::string> BlackAttakcerAnswer = enclosedSekiSearch(env, surrounding_block, search_area_bitboard, Player::kPlayer1, Player::kPlayer1, true, false, true);
            return {is_sekipass, BlackAttakcerAnswer.second};
        } else {
            return {false, ""};
        }
    }
}

std::pair<bool, std::string> SekiSearch::enclosedSekiSearch(const KillAllGoEnv& env, const GoBlock* block, const GoBitboard& search_area_bitboard, Player turn, Player attacker, bool new_board, bool allow_attacker_pass, bool need_ghi)
{
    assert(block);
    std::string ghi_string = "";
    std::pair<bool, std::string> or_player_answer = {false, ""};
    std::vector<killallgo::GHIPattern> ghi_loop;
    bool has_ssk = false;
    bool possible_seki = false;

    // terminal conditions
    if (env.getBensonBitboard().get(Player::kPlayer2).any()) { // white win if is Benson
        possible_seki = (turn == Player::kPlayer1) ? (turn == attacker ? true : false) : (turn == attacker ? false : true);
        return {possible_seki, ""};
    } else if ((block->getGridBitboard() & env.getStoneBitboard().get(Player::kPlayer2)).none() ||
               !(search_area_bitboard & env.getBensonBitboard().get(Player::kPlayer1)).none()) { // black win if white block has been eaten or inner block become benson
        possible_seki = turn == Player::kPlayer1 ? (turn == attacker ? false : true) : (turn == attacker ? true : false);
        return {possible_seki, ""};
    }

    GoBitboard area_bitboard = getSearchBitboard(env, block, search_area_bitboard, turn, attacker);
    KillAllGoEnv env_copy = env;

    if (allow_attacker_pass) {
        std::tie(env_copy, new_board) = getEnvFromBoard(env, turn, new_board);
        if (!new_board && turn == attacker) { area_bitboard.set(env.getBoardSize() * env.getBoardSize()); } // attack player can choose to play PASS if not the first action

        bool has_ssk = checkSSK(env_copy, block, area_bitboard, turn, attacker);
        if (!has_ssk && turn == attacker) {
            area_bitboard.reset(env.getBoardSize() * env.getBoardSize());
        }
    }

    std::vector<GoBitboard> search_proirity_set = findSearchPrioritySet(env_copy, block, area_bitboard, turn);
    if (need_ghi) {
        std::tie(has_ssk, ghi_loop) = findLoopPatterns(env, search_area_bitboard, turn, attacker, search_proirity_set);
    }

    for (size_t set_index = 0; set_index < search_proirity_set.size(); set_index++) {
        GoBitboard search_bitboard = search_proirity_set[set_index];
        while (!search_bitboard.none()) {
            int pos = search_bitboard._Find_first();
            search_bitboard.reset(pos);
            KillAllGoAction action(pos, turn);
            if (!env_copy.isLegalAction(action)) { continue; }

            KillAllGoEnv current_env = env_copy;
            current_env.act(action);

            int block_pos = block->getGridBitboard()._Find_first();
            const GoBlock* new_block = env_copy.getGrid(block_pos).getBlock();

            if (turn == attacker) { // attack player
                std::pair<bool, std::string> defender_win = enclosedSekiSearch(current_env, new_block, search_area_bitboard, getNextPlayer(turn, kKillAllGoNumPlayer), attacker, new_board, allow_attacker_pass, need_ghi);

                if (need_ghi) {
                    ghi_string = setPatternsToGHIString(ghi_loop, defender_win, attacker, has_ssk, ghi_string);
                }

                if (defender_win.first) {
                    continue;
                } else {
                    return {false, ""};
                }
            } else { // defend player
                std::pair<bool, std::string> attacker_lose = enclosedSekiSearch(current_env, new_block, search_area_bitboard, getNextPlayer(turn, kKillAllGoNumPlayer), attacker, new_board, allow_attacker_pass, need_ghi);
                if (attacker_lose.first) {
                    if (!attacker_lose.second.empty() && need_ghi) {
                        or_player_answer = {true, attacker_lose.second}; // the return ghi string came from a deeper layer, ghi string must be longer
                    } else {
                        return {true, ""}; // no ghi problem from below layer
                    }
                } else {
                    continue;
                }
            }
        }
    }

    possible_seki = (turn == attacker ? true : false);
    if (turn == attacker) { // and player return ghi string
        return {possible_seki, ghi_string};
    } else { // or player
        return or_player_answer;
    }
}

std::pair<KillAllGoEnv, bool> SekiSearch::getEnvFromBoard(const KillAllGoEnv& env, Player turn, bool new_board)
{
    KillAllGoEnv env_copy(env.getBoardSize());
    if (env.getActionHistory().size() >= 2 &&
        env.isPassAction(env.getActionHistory().back()) &&
        env.isPassAction(env.getActionHistory()[env.getActionHistory().size() - 2])) // if both players pass, send a new env with the same stone bitboard
    {
        env_copy.reset();
        bitboardToEnv(env_copy, env.getStoneBitboard().get(Player::kPlayer1), Player::kPlayer1);
        bitboardToEnv(env_copy, env.getStoneBitboard().get(Player::kPlayer2), Player::kPlayer2);
        env_copy.setTurn(turn);
        new_board = true;
    } else {
        env_copy = env;
        new_board = false;
    }
    return {env_copy, new_board};
}

GoBitboard SekiSearch::getSearchBitboard(const KillAllGoEnv& env, const GoBlock* block, const GoBitboard& search_area_bitboard, Player turn, Player attacker)
{
    assert(block);
    GoBitboard area_bitboard = search_area_bitboard;
    area_bitboard |= block->getGridBitboard(); // search area adds the new block
    area_bitboard = area_bitboard & ~env.getStoneBitboard().get(Player::kPlayer1) & ~env.getStoneBitboard().get(Player::kPlayer2);

    GoBitboard liberty_area_bitboard = block->getLibertyBitboard(); // only black can act on liberty board
    if (turn == Player::kPlayer1) { area_bitboard |= liberty_area_bitboard; }
    if (turn != attacker) { area_bitboard.set(kKillAllGoBoardSize * kKillAllGoBoardSize); } // defend player can choose to play PASS

    return area_bitboard;
}

bool SekiSearch::checkSSK(const KillAllGoEnv& env, const GoBlock* block, const GoBitboard& area_bitboard, Player turn, Player attacker)
{
    bool has_ssk = false;
    std::vector<GoBitboard> search_proirity_set = findSearchPrioritySet(env, block, area_bitboard, turn);
    for (size_t set_index = 0; set_index < search_proirity_set.size(); set_index++) {
        GoBitboard search_bitboard = search_proirity_set[set_index];
        while (!search_bitboard.none()) {
            int pos = search_bitboard._Find_first();
            search_bitboard.reset(pos);
            KillAllGoAction action(pos, turn);
            GoHashKey hashkey_after_play;
            if (env.getGrid(action.getActionID()).getPlayer() == Player::kPlayerNone && !env.isPassAction(action) && !isSuicidalMove(env, action)) {
                hashkey_after_play = getHashKeyAfterPlay(env, action);
            }
            if (env.getHashTable().count(hashkey_after_play) && turn == attacker) {
                has_ssk = true;
                break;
            }
        }
    }
    return has_ssk;
}

std::vector<GoBitboard> SekiSearch::findSearchPrioritySet(const KillAllGoEnv& env, const GoBlock* block, const GoBitboard& area_bitboard, Player turn)
{
    std::vector<GoBitboard> search_proirity_set;

    GoBitboard act_area_bitboard = area_bitboard;
    GoBitboard eat_stone_act_bitboard = act_area_bitboard.reset();
    GoBitboard pass_act_bitboard = act_area_bitboard.reset();
    while (!act_area_bitboard.none()) { // first order: action that can eat stone
        int pos = act_area_bitboard._Find_first();
        act_area_bitboard.reset(pos);
        KillAllGoAction action(pos, turn);
        if (!env.isLegalAction(action)) { continue; }
        if (env.isPassAction(action)) {
            pass_act_bitboard.set(pos);
            continue;
        }
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

    GoBitboard area_bitboard_remain = area_bitboard & ~eat_stone_act_bitboard & ~block_liberty_bitboard & ~pass_act_bitboard;
    search_proirity_set.emplace_back(area_bitboard_remain);
    search_proirity_set.emplace_back(pass_act_bitboard);

    return search_proirity_set;
}

std::pair<bool, std::vector<killallgo::GHIPattern>> SekiSearch::findLoopPatterns(const KillAllGoEnv& env, const GoBitboard& oringin_area_bitboard, Player turn, Player attacker, std::vector<GoBitboard> search_proirity_set)
{
    std::vector<killallgo::GHIPattern> loop_patterns;
    bool has_ssk = false;
    for (size_t set_index = 0; set_index < search_proirity_set.size(); set_index++) { // check ghi loop
        GoBitboard search_bitboard = search_proirity_set[set_index];
        while (!search_bitboard.none()) {
            int pos = search_bitboard._Find_first();
            search_bitboard.reset(pos);
            KillAllGoAction action(pos, turn);
            GoHashKey hashkey_after_play;
            if (env.getGrid(action.getActionID()).getPlayer() == Player::kPlayerNone && !env.isPassAction(action) && !isSuicidalMove(env, action)) {
                hashkey_after_play = getHashKeyAfterPlay(env, action);
            }

            if (env.getHashTable().count(hashkey_after_play) && attacker == Player::kPlayer1 && turn == Player::kPlayer1) { // only check white win situation
                has_ssk = true;

                const std::vector<GoHashKey>& hashkey_history = env.getHashKeyHistory();
                size_t virtual_hashkey_history_size = hashkey_history.size() + 1;
                size_t longest_loop_start_index = virtual_hashkey_history_size - 1;

                size_t repetitive_index = 0;
                for (size_t i = 0; i < hashkey_history.size() || (i + 1) <= longest_loop_start_index; ++i) {
                    if (hashkey_history[i] != hashkey_after_play) { continue; }
                    repetitive_index = i + 1;
                    break;
                }
                if (repetitive_index >= longest_loop_start_index) { continue; }

                longest_loop_start_index = repetitive_index;

                loop_patterns = getLoopPatterns(longest_loop_start_index, oringin_area_bitboard, env.getStoneBitboardHistory());
            }
        }
    }
    return {has_ssk, loop_patterns};
}

std::vector<killallgo::GHIPattern> SekiSearch::getLoopPatterns(size_t longest_loop_start_index, const GoBitboard& original_area_bitboard, const std::vector<GamePair<GoBitboard>>& history)
{
    std::vector<killallgo::GHIPattern> loop_patterns;
    size_t loop_count = longest_loop_start_index;

    while (loop_count < history.size()) {
        if ((loop_count - longest_loop_start_index) % 2 == 0) {
            env::GamePair<env::go::GoBitboard> history_pair = history.at(loop_count);
            GoBitboard black_pattern = original_area_bitboard & history_pair.get(Player::kPlayer1);
            GoBitboard white_pattern = original_area_bitboard & history_pair.get(Player::kPlayer2);
            GoBitboard empty_pattern = original_area_bitboard & ~black_pattern & ~white_pattern;
            killallgo::GHIPattern ghi_data{black_pattern, white_pattern, empty_pattern};
            bool exist = false;
            for (size_t index = 0; index < loop_patterns.size(); index++) {
                if (loop_patterns[index] == ghi_data) {
                    exist = true;
                    break;
                }
            }
            if (!exist) { loop_patterns.emplace_back(ghi_data); }
        }
        ++loop_count;
    }

    return loop_patterns;
}

std::string SekiSearch::setPatternsToGHIString(const std::vector<killallgo::GHIPattern>& ghi_loop, const std::pair<bool, std::string>& defender_win, Player attacker, bool has_ssk, std::string& ghi_string)
{
    if (!defender_win.second.empty()) { // return ghi string, from below layer
        if (ghi_string.empty()) {
            ghi_string = defender_win.second;
        } else {
            ghi_string = ghi_string + "-" + defender_win.second;
        }
    } else if (defender_win.first && has_ssk && attacker == Player::kPlayer1) { // only care about white win situation, return one ghi string from current layer
        bool is_first = true;
        std::ostringstream oss;
        for (auto& pattern : ghi_loop) {
            if (is_first) {
                is_first = false;
            } else {
                oss << "-";
            }

            oss << pattern.black_ << ":"
                << pattern.white_ << ":"
                << pattern.empty_;
        }
        ghi_string = oss.str();
    }
    return ghi_string;
}

GoHashKey SekiSearch::getHashKeyAfterPlay(const KillAllGoEnv& env, const go::GoAction& action)
{
    const Player player = action.getPlayer();
    const GoGrid& grid = env.getGrid(action.getActionID());
    GoHashKey new_hash_key = env.getHashKey() ^ go::getGoTurnHashKey() ^ go::getGoGridHashKey(action.getActionID(), player);
    GoBitboard check_neighbor_block_bitboard;
    for (const auto& neighbor_pos : grid.getNeighbors()) {
        const GoGrid& neighbor_grid = env.getGrid(neighbor_pos);
        if (neighbor_grid.getPlayer() == Player::kPlayerNone) {
            continue;
        } else {
            const GoBlock* neighbor_block = neighbor_grid.getBlock();
            if (check_neighbor_block_bitboard.test(neighbor_block->getID())) { continue; }
            check_neighbor_block_bitboard.set(neighbor_block->getID());
            if (neighbor_block->getPlayer() == player) { continue; }
            if (neighbor_block->getNumLiberty() == 1) { new_hash_key ^= neighbor_block->getHashKey(); }
        }
    }

    return new_hash_key;
}

bool SekiSearch::isSuicidalMove(const KillAllGoEnv& env, const go::GoAction& action)
{
    const Player player = action.getPlayer();
    const Player opp_player = getNextPlayer(player, kKillAllGoNumPlayer);
    const GoGrid& grid = env.getGrid(action.getActionID());

    GoBitboard stone_bitboard = (env.getStoneBitboard().get(env::Player::kPlayer1) | env.getStoneBitboard().get(env::Player::kPlayer2));
    GoBitboard liberty_bitboard_after_play;
    liberty_bitboard_after_play.set(action.getActionID());
    liberty_bitboard_after_play = env.dilateBitboard(liberty_bitboard_after_play);

    for (const auto& neighbor_pos : grid.getNeighbors()) {
        const GoGrid& nbr_grid = env.getGrid(neighbor_pos);
        if (nbr_grid.getPlayer() == player) {
            liberty_bitboard_after_play |= env.dilateBitboard(nbr_grid.getBlock()->getGridBitboard());
        } else if (nbr_grid.getPlayer() == opp_player) {
            if (nbr_grid.getBlock()->getNumLiberty() == 1) {
                stone_bitboard &= ~(nbr_grid.getBlock()->getGridBitboard());
            }
        }
    }

    liberty_bitboard_after_play &= (~stone_bitboard);
    liberty_bitboard_after_play.reset(action.getActionID());
    return liberty_bitboard_after_play.count() == 0;
}

} // namespace minizero::env::killallgo
