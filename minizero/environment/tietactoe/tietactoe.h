#pragma once

#include "base_env.h"
#include <algorithm>
#include <vector>

namespace minizero::env::tietactoe {

const std::string kTieTacToeName = "tietactoe";
const int kTieTacToeNumPlayer = 2;
const int kTieTacToeBoardSize = 3;

class TieTacToeAction : public BaseAction {
public:
    TieTacToeAction() : BaseAction() {}
    TieTacToeAction(int action_id, Player player) : BaseAction(action_id, player) {}
    TieTacToeAction(const std::vector<std::string>& action_string_args);

    inline Player NextPlayer() const override { return GetNextPlayer(player_, kTieTacToeNumPlayer); }
};

class TieTacToeEnv : public BaseEnv<TieTacToeAction> {
public:
    TieTacToeEnv() { Reset(); }

    void Reset() override;
    bool Act(const TieTacToeAction& action) override;
    bool Act(const std::vector<std::string>& action_string_args) override;
    std::vector<TieTacToeAction> GetLegalActions() const override;
    bool IsLegalAction(const TieTacToeAction& action) const override;
    bool IsTerminal() const override;
    float GetEvalScore() const override;
    std::vector<float> GetFeatures() const override;
    std::string ToString() const override;
    inline std::string Name() const override { return kTieTacToeName; }

private:
    Player Eval() const;

    std::vector<Player> board_;
};

class TieTacToeEnvLoader : public BaseEnvLoader<TieTacToeAction, TieTacToeEnv> {
public:
    inline std::vector<float> GetPolicyDistribution(int id) const
    {
        std::vector<float> policy(kTieTacToeBoardSize * kTieTacToeBoardSize, 0.0f);
        SetPolicyDistribution(id, policy);
        return policy;
    }

    inline std::string GetEnvName() const override { return kTieTacToeName; }
};

} // namespace minizero::env::tietactoe
