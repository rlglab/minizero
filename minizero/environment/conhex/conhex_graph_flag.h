#pragma once

#include <set>
#include <string>
#include <utility>
#include <vector>

namespace minizero::env::conhex {

enum class ConHexGraphEdgeFlag {
    NONE = 0x0,
    TOP = 0x1,
    RIGHT = 0x2,
    LEFT = 0x4,
    BOTTOM = 0x8,
};

inline ConHexGraphEdgeFlag operator|(ConHexGraphEdgeFlag a, ConHexGraphEdgeFlag b)
{
    return static_cast<ConHexGraphEdgeFlag>(static_cast<uint64_t>(a) | static_cast<uint64_t>(b));
}

inline ConHexGraphEdgeFlag operator&(ConHexGraphEdgeFlag a, ConHexGraphEdgeFlag b)
{
    return static_cast<ConHexGraphEdgeFlag>(static_cast<uint64_t>(a) & static_cast<uint64_t>(b));
}

enum class ConHexGraphCellType {
    NONE = -1,
    OUTER = 3,
    INNER = 6,
    CENTER = 5,
};

inline ConHexGraphCellType operator|(ConHexGraphCellType a, ConHexGraphCellType b)
{
    return static_cast<ConHexGraphCellType>(static_cast<uint64_t>(a) | static_cast<uint64_t>(b));
}

inline ConHexGraphCellType operator&(ConHexGraphCellType a, ConHexGraphCellType b)
{
    return static_cast<ConHexGraphCellType>(static_cast<uint64_t>(a) & static_cast<uint64_t>(b));
}

inline bool operator==(int a, ConHexGraphCellType b)
{
    return a == static_cast<int>(b);
}

inline bool operator==(ConHexGraphCellType a, int b)
{
    return static_cast<int>(a) == b;
}

} // namespace minizero::env::conhex
