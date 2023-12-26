#pragma once

#include "go.h"
#include <string>

namespace minizero::env::killallgo {

const std::string kKillAllGoName = "killallgo";
const int kKillAllGoNumPlayer = 2;
const int kKillAllGoBoardSize = 7;

class Seki7x7Table;
extern Seki7x7Table g_seki_7x7_table;

void initialize();

typedef go::GoAction KillAllGoAction;

class KillAllGoEnv : public go::GoEnv {
public:
    KillAllGoEnv(int board_size = minizero::config::env_board_size)
        : go::GoEnv(board_size)
    {
        assert(kKillAllGoBoardSize == minizero::config::env_board_size);
    }

    bool isLegalAction(const KillAllGoAction& action) const override;
    bool isTerminal() const override;
    float getEvalScore(bool is_resign = false) const override;

    inline std::string name() const override { return kKillAllGoName + "_" + std::to_string(board_size_) + "x" + std::to_string(board_size_); }
    inline int getNumPlayer() const override { return kKillAllGoNumPlayer; }
};

class KillAllGoEnvLoader : public go::GoEnvLoader {
public:
    inline std::string name() const override { return kKillAllGoName + "_" + std::to_string(getBoardSize()) + "x" + std::to_string(getBoardSize()); }
};

} // namespace minizero::env::killallgo
