#pragma once

#include <bitset>
#include <sstream>
#include <string>

namespace minizero::env::go {

template <class T>
class GoPair {
public:
    GoPair() { reset(); }
    GoPair(T black, T white) : black_(black), white_(white) {}

    inline void reset() { black_ = white_ = T(); }
    inline T& get(Player p) { return (p == Player::kPlayer1 ? black_ : white_); }
    inline const T& get(Player p) const { return (p == Player::kPlayer1 ? black_ : white_); }
    inline void set(Player p, const T& value) { (p == Player::kPlayer1 ? black_ : white_) = value; }

private:
    T black_;
    T white_;
};

const std::string kGoName = "go";
const int kGoNumPlayer = 2;
const int kMaxGoBoardSize = 19;

typedef unsigned long long GoHashKey;
typedef std::bitset<kMaxGoBoardSize * kMaxGoBoardSize> GoBitboard;

} // namespace minizero::env::go