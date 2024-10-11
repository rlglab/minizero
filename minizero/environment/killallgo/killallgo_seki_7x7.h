#pragma once

#include "go.h"
#include "killallgo.h"
#include "killallgo_7x7_bitboard.h"
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
#include <vector>

namespace minizero::env::killallgo {
class GHIPattern {
public:
    go::GoBitboard black_;
    go::GoBitboard white_;
    go::GoBitboard empty_;

    bool operator==(const GHIPattern& other) const
    {
        return black_ == other.black_ && white_ == other.white_ && empty_ == other.empty_;
    }
};

class Seki7x7Table {
public:
    struct GamePairHash {
        std::size_t operator()(const std::pair<GamePair<go::GoBitboard>, std::string>& game_pair) const noexcept
        {
            return std::hash<go::GoBitboard>{}(game_pair.first.get(Player::kPlayer1)) ^ std::hash<go::GoBitboard>{}(game_pair.first.get(Player::kPlayer2));
        }
    };

    struct GamePairEqual {
        bool operator()(const std::pair<GamePair<go::GoBitboard>, std::string>& lhs, const std::pair<GamePair<go::GoBitboard>, std::string>& rhs) const noexcept
        {
            return lhs.first == rhs.first;
        }
    };

    using IsoGamePairSet = std::unordered_set<std::pair<GamePair<go::GoBitboard>, std::string>, GamePairHash, GamePairEqual>;

    bool lookup(const std::pair<GamePair<go::GoBitboard>, std::string>& game_pair, std::string& ghi_string) const;
    void insert(std::pair<GamePair<go::GoBitboard>, std::string> game_pair);

    void save(const std::string& path) const;
    bool load(const std::string& path);

    int size() const;

private:
    IsoGamePairSet table_;
};

class SekiSearch {
private:
    /**
     * `BoardPair` is a `std::pair` that combines two Go boards:
     * - `first` represents a Go board using `GoBitboard`.
     * - `second` represents a Go board using a list of action IDs.
     *
     * `GoBitboard:`          `std::vector<int>:`
     * `+---------------+`    `+---------------+`
     * `| . . . . . . . |`    `| . . . . . . . |`
     * `| . . . . . . . |`    `| . . O . . . . |`
     * `| . . O . . . . |`    `| . O . O O . . |`
     * `| . . O O O . . |`    `| . O . . . O . |`
     * `| . . . . O . . |`    `| . . O O . O . |`
     * `| . . . . . . . |`    `| . . . . O . . |`
     * `| . . . . . . . |`    `| . . . . . . . |`
     * `+---------------+`    `+---------------+`
     */
    using BoardPair = std::pair<go::GoBitboard, std::vector<int>>;

    /**
     * `ActionList` represents a Go board stored as a list of actions.
     */
    using ActionList = std::vector<go::GoAction>;

    struct IsoBitBoardHash {
        std::size_t operator()(const go::GoBitboard& bitboard) const noexcept
        {
            return std::hash<go::GoBitboard>{}(bitboard);
        }
    };

    struct IsoBitBoardEqual {
        bool operator()(const go::GoBitboard& lhs, const go::GoBitboard& rhs) const
        {
            return lhs == rhs;
        }
    };

    struct BoardPairHash {
        std::size_t operator()(const BoardPair& board_pair) const noexcept
        {
            return std::hash<go::GoBitboard>{}(board_pair.first);
        }
    };

    struct IsoBoardPairHash {
        std::size_t operator()(const BoardPair& board_pair) const noexcept
        {
            return IsoBitBoardHash{}(board_pair.first);
        }
    };

    struct IsoBoardPairEqual {
        bool operator()(const BoardPair& lhs, const BoardPair& rhs) const noexcept
        {
            return IsoBitBoardEqual{}(lhs.first, rhs.first);
        }
    };

    using BlockSet = std::unordered_set<BoardPair, BoardPairHash>;
    using IsoBitboardSet = std::unordered_set<go::GoBitboard, IsoBitBoardHash, IsoBitBoardEqual>;

private:
    /**
     * `getInitBlockSet` and `increaseBlockSetSize` are used to generate all possible block combinations.
     * 
     * `getInitBlockSet`: generate all possible block combinations with size 1.
     * `increaseBlockSetSize`: increase the size of all possible block combinations from given `BlockSet`.
     * 
     *  These two functions returns the list of all possible (GoBitboard, std::vector<int>) pair with the following format:
     * 
     * `GoBitboard:`          `std::vector<int>:`
     * `+---------------+`    `+---------------+`
     * `| . . . . . . . |`    `| . . . . . . . |`
     * `| . . . . . . . |`    `| . . O . . . . |`
     * `| . . O . . . . |`    `| . O . O O . . |`
     * `| . . O O O . . |`    `| . O . . . O . |`
     * `| . . . . O . . |`    `| . . O O . O . |`
     * `| . . . . . . . |`    `| . . . . O . . |`
     * `| . . . . . . . |`    `| . . . . . . . |`
     * `+---------------+`    `+---------------+`
     */
    static BlockSet getInitBlockSet();
    static BlockSet increaseBlockSetSize(BlockSet src_block_set);
    /**
     * Generate block combinations with minihash from size `min_block_size` to `max_block_size`.
     */
    static IsoBitboardSet generateBlockCombinations(int min_block_size, int max_block_size);
    static std::vector<GamePair<go::GoBitboard>> generateSekiPatterns(const go::GoBitboard& board);
    static ActionList bitboardToActionList(const go::GoBitboard& board, Player player);
    static void actionListToEnv(KillAllGoEnv& env, const ActionList& actions);
    static void bitboardToEnv(KillAllGoEnv& env, const go::GoBitboard& board, Player player);
    static GamePair<go::GoBitboard> getStoneAndEmptyBitboard(const KillAllGoEnv& env, const go::GoAction& action);

public:
    /**
     * The following functions are used to generate and query seki table.
     */
    static void generateSekiTable(Seki7x7Table& seki_table, int min_area_size, int max_area_size);
    static GamePair<go::GoBitboard> lookupSekiBitboard(Seki7x7Table& seki_table, const KillAllGoEnv& env, const go::GoAction& action, std::string& ret_ghi_string);
    static bool isSeki(Seki7x7Table& seki_table, const KillAllGoEnv& env);

private:
    /**
     * action id to (row, col)
     */
    static inline std::pair<int, int> idtorc(int id) { return {kKillAllGoBoardSize - 1 - id / kKillAllGoBoardSize, id % kKillAllGoBoardSize}; }

    /**
     * (row, col) to action id
     */
    static inline int rctoid(int r, int c) { return (kKillAllGoBoardSize - 1 - r) * kKillAllGoBoardSize + c; }

    /**
     * index range check
     */
    static inline bool rc(int index) { return index >= 0 && index < kKillAllGoBoardSize; }

    static std::vector<GamePair<go::GoBitboard>> getPatternsFromGHIString(const KillAllGoEnv& env, const std::string& ghi_string);
    static bool hasHistoryGHIIssue(const KillAllGoEnv& env, const std::vector<GamePair<go::GoBitboard>>& checked_patterns, const go::GoBitboard& rzone_bitboard);
    static std::string convertToFormalGHIString(const KillAllGoEnv& env, const std::vector<GamePair<go::GoBitboard>>& checked_patterns, const go::GoBitboard& rzone_bitboard);

public:
    /**
     * The following functions are used to run seki search.
     */
    static std::pair<GamePair<go::GoBitboard>, std::string> searchSekiBitboard(const KillAllGoEnv& env, const go::GoAction& action);

private:
    static std::pair<bool, std::string> isEnclosedSeki(const KillAllGoEnv& env, const go::GoArea* area);
    static std::pair<bool, std::string> enclosedSekiSearch(const KillAllGoEnv& env, const go::GoBlock* block, const go::GoBitboard& search_area_bitboard, Player turn, Player attacker, bool new_board, bool attacker_pass, bool get_ghi);
    static std::pair<KillAllGoEnv, bool> getEnvFromBoard(const KillAllGoEnv& env, Player turn, bool new_board);
    static go::GoBitboard getSearchBitboard(const KillAllGoEnv& env, const go::GoBlock* block, const go::GoBitboard& search_area_bitboard, Player turn, Player attacker);
    static bool checkSSK(const KillAllGoEnv& env, const go::GoBlock* block, const go::GoBitboard& area_bitboard, Player turn, Player attacker);
    static std::vector<go::GoBitboard> findSearchPrioritySet(const KillAllGoEnv& env, const go::GoBlock* block, const go::GoBitboard& area_bitboard, Player turn);
    static std::pair<bool, std::vector<killallgo::GHIPattern>> findLoopPatterns(const KillAllGoEnv& env, const go::GoBitboard& oringin_area_bitboard, Player turn, Player attacker, std::vector<go::GoBitboard> search_proirity_set);
    static std::string setPatternsToGHIString(const std::vector<killallgo::GHIPattern>& ghi_loop, const std::pair<bool, std::string>& defender_win, Player attacker, bool has_ssk, std::string& ghi_string);
    static std::vector<killallgo::GHIPattern> getLoopPatterns(size_t his_count, const go::GoBitboard& oringin_area_bitboard, const std::vector<GamePair<go::GoBitboard>>& history);
    static go::GoHashKey getHashKeyAfterPlay(const KillAllGoEnv& env, const go::GoAction& action);
    static bool isSuicidalMove(const KillAllGoEnv& env, const go::GoAction& action);
};

} // namespace minizero::env::killallgo
