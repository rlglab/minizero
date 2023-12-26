#pragma once

#include <algorithm>
#include <iostream>

namespace minizero::env::killallgo {

/**
 * 7x7 Go bitboard with R-Zone support
 *
 *  +---------------+
 * 7| + + + + + + + | G7 is the highest bit, i.e., the 49th low bit
 * 6| + + + + + + + |
 * 5| + + + + + + + |
 * 4| + + + + + + + |
 * 3| + + + + + + + |
 * 2| + + + + + + + | A2 is the 8th low bit
 * 1| + + + + + + + | A1 is the lowest bit
 *  +---------------+
 *    A B C D E F G
 */
class Zone7x7Bitboard {
public:
    /**
     * common definitions
     */
    using u64 = unsigned long long int;
    using u32 = unsigned int;
    static constexpr u64 BOARD_MASK = (-1ull >> 15);
    static constexpr u64 ROW_MASK(u32 y) { return 0b1111111ull << (y * 7); }
    static constexpr u64 COL_MASK(u32 x) { return 0b0000001000000100000010000001000000100000010000001ull << x; }
    static constexpr u64 BIT_MASK(u32 x, u32 y) { return 1ull << (y * 7 + x); }

public:
    /**
     * bitmaps are public for easy access, be careful
     */
    u64 zone_;  // zone-relevant pieces
    u64 black_; // black pieces in zone
    u64 white_; // white pieces in zone
public:
    /**
     * construct a bitboard with R-zone and black/white stones
     * note that the provided bitmaps should NOT contain any set bits higher than the 49th bit
     * @param
     *  zone  the bitmap of R-zone
     *  black the bitmap of black stones
     *  white the bitmap of white stones
     */
    inline constexpr Zone7x7Bitboard(u64 zone, u64 black, u64 white) : zone_(zone), black_(black), white_(white) {}
    /**
     * construct a bitboard with black/white stones, with all 49 locations set as relevant
     * note that the provided bitmaps should NOT contain any set bits higher than the 49th bit
     * @param
     *  black the bitmap of black stones
     *  white the bitmap of white stones
     */
    inline constexpr Zone7x7Bitboard(u64 black, u64 white) : Zone7x7Bitboard(BOARD_MASK, black, white) {}
    /**
     * construct an empty bitboard, with all 49 locations set as irrelevant
     * note: to construct a "relevant" empty board, use Zone7x7Bitboard(0, 0) instead
     */
    inline constexpr Zone7x7Bitboard() : Zone7x7Bitboard(0, 0, 0) {}
    inline constexpr Zone7x7Bitboard(const Zone7x7Bitboard&) = default;
    inline constexpr Zone7x7Bitboard& operator=(const Zone7x7Bitboard&) = default;
    inline constexpr Zone7x7Bitboard& operator&=(const Zone7x7Bitboard& z) { return operator=(operator&(z)); }
    inline constexpr Zone7x7Bitboard& operator|=(const Zone7x7Bitboard& z) { return operator=(operator|(z)); }
    inline constexpr Zone7x7Bitboard& operator^=(const Zone7x7Bitboard& z) { return operator=(operator^(z)); }
    inline constexpr Zone7x7Bitboard operator&(const Zone7x7Bitboard& z) const { return {zone_ & z.zone_, black_ & z.black_, white_ & z.white_}; }
    inline constexpr Zone7x7Bitboard operator|(const Zone7x7Bitboard& z) const { return {zone_ | z.zone_, black_ | z.black_, white_ | z.white_}; }
    inline constexpr Zone7x7Bitboard operator^(const Zone7x7Bitboard& z) const { return {zone_ ^ z.zone_, black_ ^ z.black_, white_ ^ z.white_}; }
    inline constexpr Zone7x7Bitboard operator~() const { return {~zone_ & BOARD_MASK, ~black_ & BOARD_MASK, ~white_ & BOARD_MASK}; }
    inline constexpr Zone7x7Bitboard& operator&=(u64 x) { return operator=(operator&(x)); }
    inline constexpr Zone7x7Bitboard& operator|=(u64 x) { return operator=(operator|(x)); }
    inline constexpr Zone7x7Bitboard& operator^=(u64 x) { return operator=(operator^(x)); }
    inline constexpr Zone7x7Bitboard& operator<<=(u32 i) { return operator=(operator<<(i)); }
    inline constexpr Zone7x7Bitboard& operator>>=(u32 i) { return operator=(operator>>(i)); }
    inline constexpr Zone7x7Bitboard operator&(u64 x) const { return {zone_ & x, black_ & x, white_ & x}; }
    inline constexpr Zone7x7Bitboard operator|(u64 x) const { return {zone_ | x, black_ | x, white_ | x}; }
    inline constexpr Zone7x7Bitboard operator^(u64 x) const { return {zone_ ^ x, black_ ^ x, white_ ^ x}; }
    inline constexpr Zone7x7Bitboard operator<<(u32 i) const { return {zone_ << i, black_ << i, white_ << i}; }
    inline constexpr Zone7x7Bitboard operator>>(u32 i) const { return {zone_ >> i, black_ >> i, white_ >> i}; }
    inline constexpr bool operator==(const Zone7x7Bitboard& z) const { return (zone_ == z.zone_) & (black_ == z.black_) & (white_ == z.white_); }
    inline constexpr bool operator<(const Zone7x7Bitboard& z) const
    {
        return (zone_ != z.zone_) ? (zone_ < z.zone_) : ((black_ != z.black_) ? (black_ < z.black_) : (white_ < z.white_));
    }
    inline constexpr bool operator!=(const Zone7x7Bitboard& z) const { return !(*this == z); }
    inline constexpr bool operator>(const Zone7x7Bitboard& z) const { return (z < *this); }
    inline constexpr bool operator<=(const Zone7x7Bitboard& z) const { return !(z < *this); }
    inline constexpr bool operator>=(const Zone7x7Bitboard& z) const { return !(*this < z); }

public:
    enum PieceType {
        kZoneEmpty = 0b000u,  // a relevant empty location
        kZoneBlack = 0b001u,  // a relevant black stone
        kZoneWhite = 0b010u,  // a relevant white stone
        kIrrelevant = 0b100u, // a irrelevant piece
    };
    /**
     * get the piece at (x, y)
     * this function will NOT correct wrong bitmaps,
     * e.g., will return (ZONE_BLACK | ZONE_WHITE) when both black and white are set
     * @param
     *  x the x-axis position (A-G) in decimal number (0-6)
     *  y the y-axis position (1-7) in decimal number (0-6)
     * @return
     *  the piece at (x, y) in PieceType format
     */
    inline constexpr PieceType get(u32 x, u32 y) const
    {
        u64 mask = 1ull << (y * 7 + x);
        return static_cast<PieceType>((black_ & mask ? 1 : 0) | (white_ & mask ? 2 : 0) | (zone_ & mask ? 0 : 4));
    }
    /**
     * set the piece at (x, y)
     * this function will NOT correct wrong arguments,
     * e.g., will set both black and white for (ZONE_BLACK | ZONE_WHITE)
     * @param
     *  x the x-axis position (A-G) in decimal number (0-6)
     *  y the y-axis position (1-7) in decimal number (0-6)
     *  type the PieceType to be set
     */
    inline constexpr void set(u32 x, u32 y, u32 type)
    {
        u64 mask = 1ull << (y * 7 + x);
        black_ = (type & PieceType::kZoneBlack) ? (black_ | mask) : (black_ & ~mask);
        white_ = (type & PieceType::kZoneWhite) ? (white_ | mask) : (white_ & ~mask);
        zone_ = (type & PieceType::kIrrelevant) ? (zone_ & ~mask) : (zone_ | mask);
    }

public:
    /**
     * transpose this board (reflection line is A1 - G7)
     */
    inline constexpr void transpose()
    {
        zone_ = transpose(zone_);
        black_ = transpose(black_);
        white_ = transpose(white_);
    }
    /**
     * flip this board (reflection line is A4 - G4)
     */
    inline constexpr void flip()
    {
        zone_ = flip(zone_);
        black_ = flip(black_);
        white_ = flip(white_);
    }
    /**
     * mirror this board (reflection line is D1 - D7)
     */
    inline constexpr void mirror()
    {
        zone_ = mirror(zone_);
        black_ = mirror(black_);
        white_ = mirror(white_);
    }
    /**
     * transform this board to a specific isomorphic form
     * @param
     *  i the isomorphic id number: [0, 8)
     */
    inline constexpr void transform(u32 i)
    {
        zone_ = Isomorphisms::transform(zone_, i);
        black_ = Isomorphisms::transform(black_, i);
        white_ = Isomorphisms::transform(white_, i);
    }

public:
    /**
     * slide all the pieces as close to the origin (A1) as possible
     * if the structure touches the X-axis (or Y-axis) border,
     * its result also touches the X-axis (or Y-axis) border, and vise versa.
     *
     * e.g.,
     * +---------------+     +---------------+   +---------------+     +---------------+
     * |               |     |               |   |         ●     |     |     ●         |
     * |               |     |               |   |       · · ○   |     |   · · ○       |
     * |               |     |               |   |         ·     |     |     ·         |
     * |               | >>> |               | ; |               | >>> |               |
     * |         ●     |     |     ●         |   |               |     |               |
     * |       · · ○   |     |   · · ○       |   |               |     |               |
     * |         ·     |     |     ·         |   |               |     |               |
     * +---------------+     +---------------+   +---------------+     +---------------+
     *
     * +---------------+     +---------------+   +---------------+     +---------------+
     * |               |     |               |   |               |     |               |
     * |   ●           |     |               |   |           ●   |     |               |
     * | · · ○         |     |               |   |         · · ○ |     |               |
     * |   ·           | >>> |   ●           | ; |           ·   | >>> |           ●   |
     * |               |     | · · ○         |   |               |     |         · · ○ |
     * |               |     |   ·           |   |               |     |           ·   |
     * |               |     |               |   |               |     |               |
     * +---------------+     +---------------+   +---------------+     +---------------+
     *
     * +---------------+     +---------------+   +---------------+     +---------------+
     * |               |     |               |   |           ●   |     |           ●   |
     * |               |     |               |   |         · · ○ |     |         · · ○ |
     * |               |     |               |   |           ·   |     |           ·   |
     * |               | >>> |               | ; |               | >>> |               |
     * |           ●   |     |           ●   |   |               |     |               |
     * |         · · ○ |     |         · · ○ |   |               |     |               |
     * |           ·   |     |           ·   |   |               |     |               |
     * +---------------+     +---------------+   +---------------+     +---------------+
     *
     * +---------------+     +---------------+   +---------------+     +---------------+
     * |               |     |               |   |               |     |               |
     * |         ●     |     |               |   |               |     |               |
     * |       · · ○   |     |               |   |         ●     |     |               |
     * |         ·     | >>> |     ●         | ; |       · · ○   | >>> |     ●         |
     * |               |     |   · · ○       |   |         ·     |     |   · · ○       |
     * |               |     |     ·         |   |               |     |     ·         |
     * |               |     |               |   |               |     |               |
     * +---------------+     +---------------+   +---------------+     +---------------+
     */
    inline constexpr void slide()
    {
        u32 offset = 0;
        if ((zone_ & 0b1111111000000000000000000000000000000000000000000ull) == 0) {
            offset += ((__builtin_ctzll(zone_ | (1ull << 49)) / 7 ?: 1) - 1) * 7;
        }
        if ((zone_ & 0b1000000100000010000001000000100000010000001000000ull) == 0) {
            u64 squz = zone_;
            squz = squz | (squz >> 21);
            squz = squz | (squz >> 14);
            squz = squz | (squz >> 7);
            offset += ((__builtin_ctzll(squz | (0b10000000)) ?: 1) - 1);
        }
        *this >>= offset;
    }
    /**
     * normalize this board to the minimal isomorphisms
     * see slide() and operator<() for the details of minimization
     * @param
     *  allow_slide set as true to invoke slide() for all isomorphisms, default is false
     */
    inline constexpr void normalize(bool allow_slide = false)
    {
        Isomorphisms isoz = zone_, isob = black_, isow = white_;
        if (allow_slide) slide();
        for (u32 i = 1; i < 8; i++) {
            Zone7x7Bitboard iso(isoz[i], isob[i], isow[i]);
            if (allow_slide) iso.slide();
            if (iso < *this) *this = iso;
        }
    }

protected:
    /**
     * 	transpose the given 49-bit board
     * 	(1)                             (2)                             (3)                             (4)                             (5)
     * 	q r s t u v w   0 1 0 0 0 1 0   q l s t u p w   1 0 0 0 1 0 0   e l s t i p w   0 0 0 1 0 0 0   e l s b i p w   1 1 1 0 0 0 0   G N U b i p w
     * 	j k l m n o p   1 0 * 0 1 0 *   d k r m h o v   0 0 0 1 0 0 0   d k r a h o v   0 0 0 0 0 0 0   d k r a h o v   1 1 1 0 0 0 0   F M T a h o v
     * 	c d e f g h i   0 * 0 1 0 * 0   c j e Z g n i   0 0 * 0 0 0 *   c j q Z g n u   0 0 0 0 0 0 0   c j q Z g n u   1 1 1 0 0 0 0   E L S Z g n u
     * 	V W X Y Z a b   0 0 1 0 * 0 0   V W R Y f a b   0 1 0 0 0 * 0   V K R Y f m b   1 0 0 0 0 0 *   D K R Y f m t   0 0 0 0 0 0 0   D K R Y f m t
     * 	O P Q R S T U   0 1 0 * 0 1 0   O J Q X S N U   1 0 0 0 1 0 0   C J Q X G N U   0 0 0 0 0 0 0   C J Q X G N U   0 0 0 0 * * *   C J Q X e l s
     * 	H I J K L M N   1 0 * 0 1 0 *   B I P K F M T   0 0 0 * 0 0 0   B I P W F M T   0 0 0 0 0 0 0   B I P W F M T   0 0 0 0 * * *   B I P W d k r
     * 	A B C D E F G   0 * 0 0 0 * 0   A H C D E L G   0 0 * 0 0 0 *   A H O D E L S   0 0 0 * 0 0 0   A H O V E L S   0 0 0 0 * * *   A H O V c j q
     */
    static inline constexpr u64 transpose(u64 x)
    {
        u64 z = x; // (1)
        z = z ^ (z << 6);
        z = z & 0b0100010001000100010000000100010001000100010000000ull;
        x = z = x ^ (z | (z >> 6)); // (2)
        z = z ^ (z << 12);
        z = z & 0b0010001000100000000000000010001000100000000000000ull;
        x = z = x ^ (z | (z >> 12)); // (3)
        z = z ^ (z << 18);
        z = z & 0b0001000000000000000000000001000000000000000000000ull;
        x = z = x ^ (z | (z >> 18)); // (4)
        z = z ^ (z << 24);
        z = z & 0b0000111000011100001110000000000000000000000000000ull;
        x = x ^ (z | (z >> 24)); // (5)
        return x;
    }
    /**
     * 	flip the given 49-bit board
     * 	(1)               (2)                                                   (3)               (4)
     * 	0 0 0 0 0 0 0 0   0 0 0 0 0 0 0 0   0 1 0 0 0 0 0 0   B A A A A A A A   0 0 0 0 0 0 0 0   0 0 0 0 0 0 0 0
     * 	G 0 0 0 0 0 0 0   0 0 0 0 0 0 0 0   0 0 0 0 0 0 0 0   A A B B B B B B   A 0 0 0 0 0 0 0   A 0 0 0 0 0 0 0
     * 	F F G G G G G G   0 0 0 0 0 0 0 0   0 0 0 1 0 0 0 0   0 0 0 A A A A A   B B A A A A A A   B B A A A A A A
     * 	E E E F F F F F   E E E 0 0 0 0 0   0 0 0 0 0 0 0 0   E E E E 0 0 0 0   A A A B B B B B   0 0 0 B B B B B
     * 	D D D D E E E E   0 0 0 0 E E E E   0 0 0 0 0 0 0 0   0 0 0 0 0 E E E   0 0 0 0 A A A A   0 0 0 0 0 0 0 0
     * 	C C C C C D D D   0 0 0 0 0 0 0 0   0 0 0 0 0 0 0 0   0 0 0 0 0 0 0 0   E E E E E 0 0 0   E E E E E 0 0 0
     * 	B B B B B B C C   B B B B B B 0 0   0 0 0 0 0 0 0 0   B B B B B B B 0   0 0 0 0 0 0 E E   0 0 0 0 0 0 E E
     * 	A A A A A A A B   A A A A A A A B   0 1 0 0 0 0 0 0   0 A A A A A A A   0 0 0 0 0 0 0 0   0 0 0 0 0 0 0 0
     *
     * 	(5)               (6)               (7)                                                   (8)               (9)
     * 	0 0 0 0 0 0 0 0   0 0 0 0 0 0 0 0   0 0 0 0 0 0 0 0   0 1 0 0 0 0 0 0   0 L L L L L L L   0 0 0 0 0 0 0 0   0 0 0 0 0 0 0 0
     * 	G 0 0 0 0 0 0 0   K 0 0 0 0 0 0 0   0 0 0 0 0 0 0 0   0 0 0 0 0 0 0 0   K K 0 0 0 0 0 0   0 0 0 0 0 0 0 0   0 0 0 0 0 0 0 0
     * 	F F G G G G G G   M M K K K K K K   0 0 0 0 0 0 0 0   0 0 0 0 0 0 0 0   M M M K K K K K   0 0 0 0 0 0 0 0   0 0 0 0 0 0 0 0
     * 	0 0 0 F F F F F   0 0 0 M M M M M   K K K 0 0 0 0 0   0 0 0 0 0 0 0 0   K K K K M M M M   L L L 0 0 0 0 0   L L L 0 0 0 0 0
     * 	0 0 0 0 0 0 0 0   0 0 0 0 0 0 0 0   M M M M K K K K   0 0 0 0 0 0 0 0   M M M M M K K K   0 0 0 0 L L L L   0 0 0 0 L L L L
     * 	C C C C C 0 0 0   L L L L L 0 0 0   0 0 0 0 0 M M M   0 0 0 0 0 0 0 0   L L L L L L M M   K K K K K 0 0 0   0 0 0 0 0 0 0 0
     * 	0 0 0 0 0 0 C C   0 0 0 0 0 0 L L   0 0 0 0 0 0 0 0   0 0 0 0 0 0 0 1   0 0 0 0 0 0 0 L   M M M M M M K K   M M M M M M 0 0
     * 	0 0 0 0 0 0 0 0   0 0 0 0 0 0 0 0   L L L L L L L 0   0 1 0 0 0 0 0 0   0 L L L L L L L   K K K K K K K M   K K K K K K K M
     *
     * 	(10)              (11)
     * 	0 0 0 0 0 0 0 0   0 0 0 0 0 0 0 0
     * 	K 0 0 0 0 0 0 0   A 0 0 0 0 0 0 0
     * 	M M K K K K K K   B B A A A A A A
     * 	L L L M M M M M   C C C B B B B B
     * 	0 0 0 0 L L L L   D D D D C C C C
     * 	L L L L L 0 0 0   E E E E E D D D
     * 	M M M M M M L L   F F F F F F E E
     * 	K K K K K K K M   G G G G G G G F
     */
    static inline constexpr u64 flip(u64 x)
    {
        u64 p = x;                           // (1)
        p = p & 0x00000007f0003fffull;       // (2)
        p = p * 0x0200080000000002ull >> 15; // (3)
        p = p & 0x0001fff8001fc000ull;       // (4)
        u64 q = x;                           // (1)
        q = q & 0x0001fff8001fc000ull;       // (5)
        q = q ^ p;                           // (6)
        p = q >> 14;                         // (7)
        p = p * 0x0200000000008002ull >> 29; // (8)
        p = p & 0x00000007f0003fffull;       // (9)
        u64 z = p | q;                       // (10)
        return x ^ z;                        // (11)
    }
    /**
     * 	mirror the given 49-bit board
     * 	                (1)             (2)             (3)             (4)             (5)             (6)             (7)
     * 	q r s t u v w   0 0 0 t 0 0 0   0 0 0 t s 0 0   0 0 u t s 0 0   0 0 u t s r 0   0 v u t s r 0   0 v u t s r q   w v u t s r q
     * 	j k l m n o p   0 0 0 m 0 0 0   0 0 0 m l 0 0   0 0 n m l 0 0   0 0 n m l k 0   0 o n m l k 0   0 o n m l k j   p o n m l k j
     * 	c d e f g h i   0 0 0 f 0 0 0   0 0 0 f e 0 0   0 0 g f e 0 0   0 0 g f e d 0   0 h g f e d 0   0 h g f e d c   i h g f e d c
     * 	V W X Y Z a b   0 0 0 Y 0 0 0   0 0 0 Y X 0 0   0 0 Z Y X 0 0   0 0 Z Y X W 0   0 a Z Y X W 0   0 a Z Y X W V   b a Z Y X W V
     * 	O P Q R S T U   0 0 0 R 0 0 0   0 0 0 R Q 0 0   0 0 S R Q 0 0   0 0 S R Q P 0   0 T S R Q P 0   0 T S R Q P O   U T S R Q P O
     * 	H I J K L M N   0 0 0 K 0 0 0   0 0 0 K J 0 0   0 0 L K J 0 0   0 0 L K J I 0   0 M L K J I 0   0 M L K J I H   N M L K J I H
     * 	A B C D E F G   0 0 0 D 0 0 0   0 0 0 D C 0 0   0 0 E D C 0 0   0 0 E D C B 0   0 F E D C B 0   0 F E D C B A   G F E D C B A
     */
    static inline constexpr u64 mirror(u64 x)
    {
        u64 z = (x & 0b0001000000100000010000001000000100000010000001000ull);      // (1)
        z = z | (x & 0b0000100000010000001000000100000010000001000000100ull) << 2; // (2)
        z = z | (x & 0b0010000001000000100000010000001000000100000010000ull) >> 2; // (3)
        z = z | (x & 0b0000010000001000000100000010000001000000100000010ull) << 4; // (4)
        z = z | (x & 0b0100000010000001000000100000010000001000000100000ull) >> 4; // (5)
        z = z | (x & 0b0000001000000100000010000001000000100000010000001ull) << 6; // (6)
        z = z | (x & 0b1000000100000010000001000000100000010000001000000ull) >> 6; // (7)
        return z;
    }
    /**
     * generate all isomorphisms of a given 64-bit 7x7 board by flip/transpose consecutively
     * note that the output of transform(x, i) is the ith isomorphism in iso[], i.e., Isomorphisms(x)[i]
     * transform(x, i) may be more efficient when only several isomorphisms are needed
     */
    struct Isomorphisms {
        u64 iso[8];
        inline constexpr Isomorphisms(u64 x) : iso{}
        {
            iso[0] = x;
            iso[1] = flip(iso[0]);
            iso[2] = transpose(iso[1]);
            iso[3] = flip(iso[2]);
            iso[4] = transpose(iso[3]);
            iso[5] = flip(iso[4]);
            iso[6] = transpose(iso[5]);
            iso[7] = flip(iso[6]);
        }
        static inline constexpr u64 transform(u64 x, u32 i)
        {
            switch (i % 8) {
                default:
                case 0: return x;
                case 6: x = transpose(x);
                case 1:
                    x = flip(x);
                    return x;
                case 4: x = flip(x);
                case 5:
                    x = mirror(x);
                    return x;
                case 3: x = mirror(x);
                case 2: x = flip(x);
                case 7:
                    x = transpose(x);
                    return x;
            }
        }
        inline constexpr u64& operator[](u32 i) { return iso[i]; }
        inline constexpr const u64& operator[](u32 i) const { return iso[i]; }
        inline constexpr u64* begin() { return iso; }
        inline constexpr const u64* begin() const { return iso; }
        inline constexpr u64* end() { return iso + 8; }
        inline constexpr const u64* end() const { return iso + 8; }
    };

public:
    /**
     * print the given board to the given ostream
     * @param
     *  out the ostream to print
     *  z   the board to be printed
     * @return
     *  the given ostream (out)
     */
    friend std::ostream& operator<<(std::ostream& out, const Zone7x7Bitboard& z)
    {
        const bool axis = true;                                          // whether to print axis labels (A-G, 1-7)
        const char* symbol[] = {"\u00B7", "\u25CF", "\u25CB", "\u00A0"}; // empty  black  white  irrelevant
        const char* corner[] = {"\u250C", "\u250F", "\u2510", "\u2513",  // top-left{normal, bold}  top-right{normal, bold}
                                "\u2514", "\u2517", "\u2518", "\u251B"}; // bottom-left{normal, bold}  bottom-right{normal, bold}
        const char* border[] = {"\u2500", "\u2501", "\u2502", "\u2503"}; // horizontal{normal, bold}  vertical{normal, bold}
        // gather r-zone info
        bool zone[7][7] = {};
        for (int y = 0; y < 7; y++) {
            for (int x = 0; x < 7; x++) {
                zone[x][y] = (z.get(x, y) & PieceType::kIrrelevant) != PieceType::kIrrelevant;
            }
        }
        // print top border
        out << (axis ? " " : "") << corner[0 + zone[0][6]];
        for (int last = zone[0][6], x = 0; x < 7; last = zone[x++][6]) {
            out << border[0 + (zone[x][6] | last)] << border[0 + zone[x][6]];
        }
        out << border[0 + zone[6][6]] << corner[2 + zone[6][6]] << std::endl;
        // print left border, pieces, and right border (row by row)
        for (int y = 6; y >= 0; y--) {
            if (axis) out << char('1' + y);
            out << border[2 + zone[0][y]] << " ";
            for (int x = 0; x < 7; x++) {
                out << symbol[std::min<u32>(z.get(x, y), 3)] << " ";
            }
            out << border[2 + zone[6][y]] << std::endl;
        }
        // print bottom border
        out << (axis ? " " : "") << corner[4 + zone[0][0]];
        for (int last = zone[0][0], x = 0; x < 7; last = zone[x++][0]) {
            out << border[0 + (zone[x][0] | last)] << border[0 + zone[x][0]];
        }
        out << border[0 + zone[6][0]] << corner[6 + zone[6][0]] << std::endl;
        if (axis) out << "   A B C D E F G  " << std::endl;
        return out;
    }
};

} // namespace minizero::env::killallgo
