#pragma once

#include "base_env.h"
#include <vector>

namespace minizero::env::othello {

const std::string kOthelloName = "othello";
const int kOthelloNumPlayer = 2;
const int kOthelloBoardSize = 8;

class OthelloAction : public BaseAction {
public:
    OthelloAction() : BaseAction() {}
    OthelloAction(int action_id, Player player) : BaseAction(action_id, player) {}
    OthelloAction(const std::vector<std::string>& action_string_args)
    {
        // TODO
    }

    inline Player NextPlayer() const override { return GetNextPlayer(player_, kOthelloNumPlayer); }
};

class OthelloEnv : public BaseEnv<OthelloAction> {
public:
    OthelloEnv() {}

    void Reset() override {}
    bool Act(const OthelloAction& action) override { return true; }
    bool Act(const std::vector<std::string>& action_string_args) override { return true; }
    std::vector<OthelloAction> GetLegalActions() const override { return {}; }
    bool IsLegalAction(const OthelloAction& action) const override { return true; }
    bool IsTerminal() const override { return true; }
    float GetEvalScore(bool is_resign = false) const override { return 0.0f; }
    std::vector<float> GetFeatures() const override { return {}; }
    std::string ToString() const override { return ""; }
    inline std::string Name() const override { return kOthelloName; }
};

class OthelloEnvLoader : public BaseEnvLoader<OthelloAction, OthelloEnv> {
public:
    inline std::vector<float> GetPolicyDistribution(int id) const override
    {
        std::vector<float> policy(kOthelloBoardSize * kOthelloBoardSize, 0.0f);
        SetPolicyDistribution(id, policy);
        return policy;
    }

    inline std::string GetEnvName() const override { return kOthelloName; }
};

} // namespace minizero::env::othello