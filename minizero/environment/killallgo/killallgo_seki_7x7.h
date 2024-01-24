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

class Seki7x7Table {
public:
    struct IsoGamePairHash {
        std::size_t operator()(const GamePair<go::GoBitboard>& game_pair) const noexcept
        {
            uint64_t b1 = game_pair.get(Player::kPlayer1).to_ullong();
            uint64_t b2 = game_pair.get(Player::kPlayer2).to_ullong();
            Zone7x7Bitboard board{b1 | b2, b1, b2};
            board.normalize(true);
            return fmix64(board.black_) ^ fmix64(board.white_);
        }

        constexpr static uint64_t fmix64(uint64_t k)
        {
            k ^= k >> 33;
            k *= 0xff51afd7ed558ccdull;
            k ^= k >> 33;
            k *= 0xc4ceb9fe1a85ec53ull;
            k ^= k >> 33;
            return k;
        }
    };

    struct IsoGamePairEqual {
        bool operator()(const GamePair<go::GoBitboard>& lhs, const GamePair<go::GoBitboard>& rhs) const noexcept
        {
            uint64_t lhs_b1 = lhs.get(Player::kPlayer1).to_ullong();
            uint64_t lhs_b2 = lhs.get(Player::kPlayer2).to_ullong();
            uint64_t rhs_b1 = rhs.get(Player::kPlayer1).to_ullong();
            uint64_t rhs_b2 = rhs.get(Player::kPlayer2).to_ullong();
            Zone7x7Bitboard lhs_board{lhs_b1 | lhs_b2, lhs_b1, lhs_b2};
            Zone7x7Bitboard rhs_board{rhs_b1 | rhs_b2, rhs_b1, rhs_b2};
            lhs_board.normalize(true);
            rhs_board.normalize(true);
            return lhs_board == rhs_board;
        }
    };

    using IsoGamePairSet = std::unordered_set<GamePair<go::GoBitboard>, IsoGamePairHash, IsoGamePairEqual>;

    bool lookup(const GamePair<go::GoBitboard>& game_pair) const;
    void insert(GamePair<go::GoBitboard> game_pair);

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
            Zone7x7Bitboard board{bitboard.to_ullong(), 0, 0};
            board.normalize(true);
            return board.zone_;
        }
    };

    struct IsoBitBoardEqual {
        bool operator()(const go::GoBitboard& lhs, const go::GoBitboard& rhs) const
        {
            Zone7x7Bitboard l{lhs.to_ullong(), 0, 0}, r{rhs.to_ullong(), 0, 0};
            l.normalize(true), r.normalize(true);
            return l.zone_ == r.zone_;
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
    static GamePair<go::GoBitboard> lookupSekiBitboard(Seki7x7Table& seki_table, const KillAllGoEnv& env, const go::GoAction& action);
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

public:
    /**
     * The following functions are used to run seki search.
     */
    static GamePair<go::GoBitboard> searchSekiBitboard(const KillAllGoEnv& env, const go::GoAction& action);

private:
    static bool isEnclosedSeki(const KillAllGoEnv& env, const go::GoArea* area);
    static bool enclosedSekiSearch(const KillAllGoEnv& env, const go::GoBlock* block, const go::GoBitboard& search_area_bitboard, Player turn, Player attacker);
    static std::vector<go::GoBitboard> findSearchPrioritySet(const KillAllGoEnv& env, const go::GoBlock* block, const go::GoBitboard& area_bitboard, Player turn);
};

} // namespace minizero::env::killallgo
