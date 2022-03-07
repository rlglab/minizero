#pragma once

#include "go.h"

namespace minizero::env::go {

class GoBenson {
public:
    static std::pair<GoBitboard, GoBitboard> GetBensonBitboard(const GoEnv& go_env)
    {
        return std::pair<GoBitboard, GoBitboard>();
    }
};

} // namespace minizero::env::go