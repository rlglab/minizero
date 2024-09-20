#pragma once
#include <stdint.h>
#include <vector>

namespace minizero::env::santorini {

class Bitboard {
public:
    Bitboard() : raw_(0ULL) {}
    Bitboard(uint64_t v) : raw_(v) {}
    Bitboard(const Bitboard& b) = default;
    ~Bitboard() = default;
    Bitboard& operator=(const Bitboard& b) = default;
    operator uint64_t() const { return raw_; }

    Bitboard& operator|=(const Bitboard& b)
    {
        raw_ |= b.raw_;
        return (*this);
    }
    Bitboard operator|(const Bitboard& b) const
    {
        Bitboard ret(raw_);
        ret |= b;
        return ret;
    }
    Bitboard& operator&=(const Bitboard& b)
    {
        raw_ &= b.raw_;
        return (*this);
    }
    Bitboard operator&(const Bitboard& b) const
    {
        Bitboard ret(raw_);
        ret &= b;
        return ret;
    }
    Bitboard& operator^=(const Bitboard& b)
    {
        raw_ ^= b.raw_;
        return (*this);
    }
    Bitboard operator^(const Bitboard& b) const
    {
        Bitboard ret(raw_);
        ret ^= b;
        return ret;
    }
    Bitboard& operator<<=(int shift)
    {
        raw_ <<= shift;
        return (*this);
    }
    Bitboard operator<<(int shift) const
    {
        Bitboard ret(raw_);
        ret <<= shift;
        return ret;
    }
    Bitboard& operator>>=(int shift)
    {
        raw_ >>= shift;
        return (*this);
    }
    Bitboard operator>>(int shift) const
    {
        Bitboard ret(raw_);
        ret >>= shift;
        return ret;
    }
    Bitboard operator~() const
    {
        Bitboard ret(~raw_);
        return ret;
    }

    std::vector<int> toList() const
    {
        std::vector<int> ret;
        uint64_t b = 1ULL;
        for (int i = 0; i < kRowSize * kColSize; ++i) {
            if (b & raw_) { ret.push_back(i); }
            b <<= 1;
        }
        return ret;
    }

    static Bitboard getNeighbor(int idx)
    {
        Bitboard ret(kNeighbor);
        ret <<= (idx - 8);
        ret &= kValidSpace;
        return ret;
    }

    static constexpr int kRowSize = 7;
    static constexpr int kColSize = 7;

    // valid space:
    // -------
    // -*****-
    // -*****-
    // -*****-
    // -*****-
    // -*****-
    // -------
    static constexpr uint64_t kValidSpace = 2147077824256ULL;

    // neighbor:
    // ***----
    // *-*----
    // ***----
    // -------
    // -------
    // -------
    // -------
    static constexpr uint64_t kNeighbor = 115335ULL;

private:
    uint64_t raw_ = 0;
};

} // namespace minizero::env::santorini
