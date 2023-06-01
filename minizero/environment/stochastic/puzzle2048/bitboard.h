#pragma once
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace minizero::env::puzzle2048 {

/**
 * 64-bit bitboard implementation for 2048
 *
 * index:
 *  0  1  2  3
 *  4  5  6  7
 *  8  9 10 11
 * 12 13 14 15
 *
 * note that the 64-bit value is little endian
 * therefore a board with raw value 0x4312752186532731ull would be
 * +------------------------+
 * |     2     8   128     4|
 * |     8    32    64   256|
 * |     2     4    32   128|
 * |     4     2     8    16|
 * +------------------------+
 *
 */
class Bitboard {
public:
    Bitboard(uint64_t raw = 0) : raw_(raw) {}
    Bitboard(const Bitboard& b) = default;
    Bitboard& operator=(const Bitboard& b) = default;
    operator uint64_t() const { return raw_; }

    /**
     * get a 16-bit row
     */
    int getRow(int i) const { return ((raw_ >> (i << 4)) & 0xffff); }
    /**
     * set a 16-bit row
     */
    void setRow(int i, int r) { raw_ = (raw_ & ~(0xffffULL << (i << 4))) | (uint64_t(r & 0xffff) << (i << 4)); }
    /**
     * get a 4-bit tile
     */
    int get(int i) const { return (raw_ >> (i << 2)) & 0x0f; }
    /**
     * set a 4-bit tile
     */
    void set(int i, int t) { raw_ = (raw_ & ~(0x0fULL << (i << 2))) | (uint64_t(t & 0x0f) << (i << 2)); }

public:
    bool operator==(const Bitboard& b) const { return raw_ == b.raw_; }
    bool operator<(const Bitboard& b) const { return raw_ < b.raw_; }
    bool operator!=(const Bitboard& b) const { return !(*this == b); }
    bool operator>(const Bitboard& b) const { return b < *this; }
    bool operator<=(const Bitboard& b) const { return !(b < *this); }
    bool operator>=(const Bitboard& b) const { return !(*this < b); }

private:
    /**
     * the lookup table for moving board
     */
    struct RowLookup {
        int raw;   // base row (16-bit raw)
        int left;  // left operation
        int right; // right operation
        int score; // merge reward

        void initialize(int r)
        {
            raw = r;

            int V[4] = {(r >> 0) & 0x0f, (r >> 4) & 0x0f, (r >> 8) & 0x0f, (r >> 12) & 0x0f};
            int L[4] = {V[0], V[1], V[2], V[3]};
            int R[4] = {V[3], V[2], V[1], V[0]}; // mirrored

            score = calculateSlideLeft(L);
            left = ((L[0] << 0) | (L[1] << 4) | (L[2] << 8) | (L[3] << 12));

            score = calculateSlideLeft(R);
            std::reverse(R, R + 4);
            right = ((R[0] << 0) | (R[1] << 4) | (R[2] << 8) | (R[3] << 12));
        }

        void applySlideLeft(uint64_t& raw, int& sc, int i) const
        {
            raw |= uint64_t(left) << (i << 4);
            sc += score;
        }

        void applySlideRight(uint64_t& raw, int& sc, int i) const
        {
            raw |= uint64_t(right) << (i << 4);
            sc += score;
        }

        static int calculateSlideLeft(int row[])
        {
            int top = 0;
            int tmp = 0;
            int score = 0;

            for (int i = 0; i < 4; i++) {
                int tile = row[i];
                if (tile == 0) continue;
                row[i] = 0;
                if (tmp != 0) {
                    if (tile == tmp) {
                        tile = tile + 1;
                        row[top++] = tile;
                        score += (1 << tile);
                        tmp = 0;
                    } else {
                        row[top++] = tmp;
                        tmp = tile;
                    }
                } else {
                    tmp = tile;
                }
            }
            if (tmp != 0) row[top] = tmp;
            return score;
        }

        RowLookup()
        {
            static int row = 0;
            initialize(row++);
        }

        static const RowLookup& find(int row)
        {
            static const RowLookup cache[65536];
            return cache[row];
        }
    };

public:
    /**
     * reset to initial state (or empty state)
     */
    void initialize(bool add_init_tiles = true)
    {
        raw_ = 0;
        if (add_init_tiles) {
            popup();
            popup();
        }
    }

    /**
     * add a new random tile on board, or do nothing if the board is full
     * 2-tile: 90%
     * 4-tile: 10%
     */
    bool popup(int rand = std::rand())
    {
        int empty = countEmptyPositions();
        if (empty) {
            set(getNthEmptyPosition((rand & 0xffff) % empty), (rand >> 16) % 10 ? 1 : 2);
            return true;
        }
        return false;
    }

    /**
     * place a tile at an empty position
     */
    bool place(int i, int t)
    {
        if (get(i)) return false;
        set(i, t);
        return true;
    }

    enum SlideDirection {
        kUp = 0,
        kRight = 1,
        kDown = 2,
        kLeft = 3
    };

    /**
     * apply an action to the board
     * return the reward gained by the action, or -1 if the action is illegal
     */
    int slide(int opcode)
    {
        switch (opcode) {
            case 0:
                return slideUp();
            case 1:
                return slideRight();
            case 2:
                return slideDown();
            case 3:
                return slideLeft();
            default:
                return -1;
        }
    }

    int slideLeft()
    {
        uint64_t move = 0;
        uint64_t prev = raw_;
        int score = 0;
        RowLookup::find(getRow(0)).applySlideLeft(move, score, 0);
        RowLookup::find(getRow(1)).applySlideLeft(move, score, 1);
        RowLookup::find(getRow(2)).applySlideLeft(move, score, 2);
        RowLookup::find(getRow(3)).applySlideLeft(move, score, 3);
        raw_ = move;
        return (move != prev) ? score : -1;
    }
    int slideRight()
    {
        uint64_t move = 0;
        uint64_t prev = raw_;
        int score = 0;
        RowLookup::find(getRow(0)).applySlideRight(move, score, 0);
        RowLookup::find(getRow(1)).applySlideRight(move, score, 1);
        RowLookup::find(getRow(2)).applySlideRight(move, score, 2);
        RowLookup::find(getRow(3)).applySlideRight(move, score, 3);
        raw_ = move;
        return (move != prev) ? score : -1;
    }
    int slideUp()
    {
        rotate(1);
        int score = slideRight();
        rotate(-1);
        return score;
    }
    int slideDown()
    {
        rotate(1);
        int score = slideLeft();
        rotate(-1);
        return score;
    }

    /**
     * swap row and column
     * +------------------------+       +------------------------+
     * |     2     8   128     4|       |     2     8     2     4|
     * |     8    32    64   256|       |     8    32     4     2|
     * |     2     4    32   128| ----> |   128    64    32     8|
     * |     4     2     8    16|       |     4   256   128    16|
     * +------------------------+       +------------------------+
     */
    void transpose()
    {
        raw_ = (raw_ & 0xf0f00f0ff0f00f0fULL) | ((raw_ & 0x0000f0f00000f0f0ULL) << 12) | ((raw_ & 0x0f0f00000f0f0000ULL) >> 12);
        raw_ = (raw_ & 0xff00ff0000ff00ffULL) | ((raw_ & 0x00000000ff00ff00ULL) << 24) | ((raw_ & 0x00ff00ff00000000ULL) >> 24);
    }

    /**
     * horizontal reflection
     * +------------------------+       +------------------------+
     * |     2     8   128     4|       |     4   128     8     2|
     * |     8    32    64   256|       |   256    64    32     8|
     * |     2     4    32   128| ----> |   128    32     4     2|
     * |     4     2     8    16|       |    16     8     2     4|
     * +------------------------+       +------------------------+
     */
    void mirror()
    {
        raw_ = ((raw_ & 0x000f000f000f000fULL) << 12) | ((raw_ & 0x00f000f000f000f0ULL) << 4) | ((raw_ & 0x0f000f000f000f00ULL) >> 4) | ((raw_ & 0xf000f000f000f000ULL) >> 12);
    }

    /**
     * vertical reflection
     * +------------------------+       +------------------------+
     * |     2     8   128     4|       |     4     2     8    16|
     * |     8    32    64   256|       |     2     4    32   128|
     * |     2     4    32   128| ----> |     8    32    64   256|
     * |     4     2     8    16|       |     2     8   128     4|
     * +------------------------+       +------------------------+
     */
    void flip()
    {
        raw_ = ((raw_ & 0x000000000000ffffULL) << 48) | ((raw_ & 0x00000000ffff0000ULL) << 16) | ((raw_ & 0x0000ffff00000000ULL) >> 16) | ((raw_ & 0xffff000000000000ULL) >> 48);
    }

    /**
     * rotate the board clockwise by given times
     */
    void rotate(int r = 1)
    {
        switch (((r % 4) + 4) % 4) {
            default:
            case 0:
                break;
            case 1:
                transpose();
                mirror();
                break;
            case 2:
                mirror();
                flip();
                break;
            case 3:
                transpose();
                flip();
                break;
        }
    }

public:
    /**
     * count the number of empty cells
     */
    uint32_t countEmptyPositions() const
    {
        uint64_t x = raw_;
        x |= (x >> 2);
        x |= (x >> 1);
        return __builtin_popcountll(~x & 0x1111111111111111ull);
    }

    /**
     * get the position of the nth empty cell
     * 0 <= n < countEmptyPositions()
     */
    uint32_t getNthEmptyPosition(uint32_t n) const
    {
        uint64_t x = raw_;
        x |= (x >> 2);
        x |= (x >> 1);
        x = ~x & 0x1111111111111111ull;
        // uint64_t nth = __builtin_ia32_pdep_di(1ull << n, x);
        uint64_t nth = x;
        while (n--) nth &= nth - 1;
        return __builtin_ctzll(nth) / 4;
    }

public:
    friend std::ostream& operator<<(std::ostream& out, const Bitboard& b)
    {
        char buff[32];
        out << "+------------------------+" << std::endl;
        for (int i = 0; i < 16; i += 4) {
            std::snprintf(buff, sizeof(buff), "|%6u%6u%6u%6u|",
                          (1 << b.get(i + 0)) & -2u,
                          (1 << b.get(i + 1)) & -2u,
                          (1 << b.get(i + 2)) & -2u,
                          (1 << b.get(i + 3)) & -2u);
            out << buff << std::endl;
        }
        out << "+------------------------+" << std::endl;
        return out;
    }

private:
    uint64_t raw_;
};

} // namespace minizero::env::puzzle2048
