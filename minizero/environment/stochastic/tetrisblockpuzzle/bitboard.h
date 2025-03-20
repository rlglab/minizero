#pragma once
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace minizero::env::tetrisblockpuzzle {
class Bitboard {
public:
    /*
     0  1  2  3  4  5  6  7
     8  9 10 11 12 13 14 15
    16 17 18 19 20 21 22 23
    24 25 26 27 28 29 30 31
    32 33 34 35 36 37 38 39
    40 41 42 43 44 45 46 47
    48 49 50 51 52 53 54 55
    56 57 58 59 60 61 62 63
    */
    Bitboard(uint64_t raw = 0, uint64_t last_place = 0, uint64_t last_crash = 0) : raw_(raw), last_place_(last_place), last_crash_(last_crash) {}
    Bitboard(const Bitboard& b) = default;
    Bitboard& operator=(const Bitboard& b) = default;
    operator uint64_t() const { return raw_; }

    int getRow(int i) const { return ((raw_ >> (i * 8)) & 0xffULL); }

    void setRow(int i, int r) { raw_ = (raw_ & ~(0xffULL << (i * 8))) | (uint64_t(r & 0xff) << (i * 8)); }

    inline void setBit(int i, int j) { raw_ |= (1ULL << (i * 8 + j)); }

    inline void clearRow(int y) { raw_ &= ~(0xffULL << (y * 8)); }
    inline void clearColumn(int x) { raw_ &= ~(0x0101010101010101ULL << x); }
    inline uint64_t getRowMask(int y) const { return (0xffULL << (y * 8)); }
    inline uint64_t getColumnMask(int x) const { return (0x0101010101010101ULL << x); }
    inline bool isRowFull(int y) const { return (raw_ & (0xffULL << (y * 8))) == (0xffULL << (y * 8)); }
    inline bool isColumnFull(int x) const { return (raw_ & (0x0101010101010101ULL << x)) == (0x0101010101010101ULL << x); }

    int get(int index) const { return ((raw_ >> index) & 1); }
    int get(int i, int j) const { return get(i * 8 + j); }

    uint64_t getLastPlace() const { return last_place_; }
    uint64_t getLastCrash() const { return last_crash_; }

    int check(int index, uint64_t last) const { return ((last >> index) & 1); }
    int check(int i, int j, uint64_t last) const { return check(i * 8 + j, last); }

    int crash()
    {
        int count = 0;
        std::vector<int> rows, cols;
        for (int i = 0; i < 8; i++) {
            if (isRowFull(i)) {
                rows.push_back(i);
                count++;
            }
            if (isColumnFull(i)) {
                cols.push_back(i);
                count++;
            }
        }
        last_crash_ = 0;
        for (int i : rows) { 
            clearRow(i);
            last_crash_ = last_crash_ | getRowMask(i); 
        }
        for (int i : cols) { 
            clearColumn(i);
            last_crash_ = last_crash_ | getColumnMask(i); 
        }
        return count;
    }

public:
    bool operator==(const Bitboard& b) const { return raw_ == b.raw_; }
    bool operator<(const Bitboard& b) const { return raw_ < b.raw_; }
    bool operator!=(const Bitboard& b) const { return !(*this == b); }
    bool operator>(const Bitboard& b) const { return b < *this; }
    bool operator<=(const Bitboard& b) const { return !(b < *this); }
    bool operator>=(const Bitboard& b) const { return !(*this < b); }

public:
    /**
     * reset to initial state (or empty state)
     */
    void initialize(bool add_init_tiles = true)
    {
        raw_ = 0;
        last_place_ = 0;
        last_crash_ = 0;
    }

    /**
     * place a tile at an empty position
     */
    bool can_place(uint64_t mask) const
    {
        return (raw_ & mask) == 0;
    }

    bool canPlace(Bitboard mask) const
    {
        return can_place(static_cast<uint64_t>(mask));
    }

    bool place(uint64_t mask)
    {
        if (!can_place(mask)) { return false; }
        last_place_ = mask;
        last_crash_ = 0;
        raw_ |= mask;
        return true;
    }

    bool place(Bitboard mask)
    {
        return place(static_cast<uint64_t>(mask));
    }

    uint64_t transpose(uint64_t board) const
    {
        uint64_t t;

        t = (board ^ (board >> 7)) & 0x00aa00aa00aa00aa;
        board = board ^ t ^ (t << 7);

        t = (board ^ (board >> 14)) & 0x0000cccc0000cccc;
        board = board ^ t ^ (t << 14);

        t = (board ^ (board >> 28)) & 0x00000000f0f0f0f0;
        board = board ^ t ^ (t << 28);

        return board;
    }

    uint64_t flipVertical(uint64_t board) const
    {
        board = ((board >> 1) & 0x5555555555555555) | ((board & 0x5555555555555555) << 1);
        board = ((board >> 2) & 0x3333333333333333) | ((board & 0x3333333333333333) << 2);
        board = ((board >> 4) & 0x0f0f0f0f0f0f0f0f) | ((board & 0x0f0f0f0f0f0f0f0f) << 4);
        return board;
    }

    uint64_t flipHorizontal(uint64_t board) const
    {
        board = ((board >> 8) & 0x00ff00ff00ff00ff) | ((board & 0x00ff00ff00ff00ff) << 8);
        board = ((board >> 16) & 0x0000ffff0000ffff) | ((board & 0x0000ffff0000ffff) << 16);
        board = ((board >> 32) & 0x00000000ffffffff) | ((board & 0x00000000ffffffff) << 32);
        return board;
    }

    /**
     * rotate the board clockwise by given times
     */
    uint64_t rotate(uint64_t board, int r = 1) const
    {
        switch (((r % 4) + 4) % 4) {
            default:
            case 0:
                return board;
            case 1:
                return flipVertical(transpose(board));
            case 2:
                return flipVertical(flipHorizontal(board));
            case 3:
                return flipHorizontal(transpose(board));
        }
    }

    Bitboard flipHorizontal() const
    {
        return Bitboard(flipHorizontal(raw_));
    }

    Bitboard flipVertical() const
    {
        return Bitboard(flipVertical(raw_));
    }

    Bitboard rotate(int r = 1) const
    {
        return Bitboard(rotate(raw_, r));
    }

public:
    friend std::ostream& operator<<(std::ostream& out, const Bitboard& b)
    {
        std::string white_color = "\033[48;2;255;255;255m";
        std::string light_gray_color = "\033[48;2;225;225;225m";
        std::string light_green_color = "\033[48;2;165;255;160m";
        std::string blue_color = "\033[48;2;100;149;237m";
        std::string orange_color = "\033[48;2;255;165;0m";
        std::string light_orange_color = "\033[48;2;210;230;90m";
        
        std::string reset_color = "\033[0m";
        out << "  +------------------+" << std::endl;
        for (int y = 0; y < 8; ++y) {
            out << "  | ";
            for (int x = 0; x < 8; ++x) {
                if (b.get(y, x) && b.check(y, x, b.getLastPlace())) {
                    out << orange_color << "  " << reset_color;
                } else if (b.get(y, x)) {
                    out << blue_color << "  " << reset_color;
                } else if (b.check(y, x, b.getLastCrash()) && b.check(y, x, b.getLastPlace())) {
                    out << light_orange_color << "  " << reset_color;
                } else if (b.check(y, x, b.getLastCrash())) {
                    out << light_green_color << "  " << reset_color;
                } else {
                    out << white_color << "  " << reset_color;
                }
            }
            out << " |" << std::endl;
        }
        out << "  +------------------+" << std::endl;
        return out;
    }

private:
    uint64_t raw_;
    uint64_t last_place_;
    uint64_t last_crash_;
};

} // namespace minizero::env::tetrisblockpuzzle

namespace std {
template <>
struct hash<minizero::env::tetrisblockpuzzle::Bitboard> {
    std::size_t operator()(const minizero::env::tetrisblockpuzzle::Bitboard& b) const
    {
        return std::hash<uint64_t>{}(static_cast<uint64_t>(b));
    }
};
} // namespace std
