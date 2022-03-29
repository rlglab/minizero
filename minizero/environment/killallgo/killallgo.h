#pragma once

#include "go.h"

namespace minizero::env::killallgo {

const std::string kKillAllGoName = "killallgo";
const int kKillAllGoNumPlayer = 2;
const int kKillAllGoBoardSize = 7;

typedef go::GoAction KillAllGoAction;

class KillAllGoEnv : public go::GoEnv {
public:
    KillAllGoEnv()
        : go::GoEnv(kKillAllGoBoardSize)
    {
    }

    bool isLegalAction(const KillAllGoAction& action) const override
    {
        if (actions_.size() == 1) { return isPassAction(action); }
        return go::GoEnv::isLegalAction(action);
    }

    bool isTerminal() const override
    {
        // TODO: rewrite this
        return go::GoEnv::isTerminal();
    }

    float getEvalScore(bool is_resign = false) const override
    {
        // TODO: rewrite this
        return go::GoEnv::getEvalScore();
    }

    inline std::string name() const override { return kKillAllGoName; }
};

class KillAllGoEnvLoader : public go::GoEnvLoader {
public:
    inline std::string getEnvName() const { return kKillAllGoName; }
};

} // namespace minizero::env::killallgo