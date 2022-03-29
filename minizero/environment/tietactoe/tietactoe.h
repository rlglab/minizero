#pragma once

#include "base_env.h"
#include "sgf_loader.h"
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

    inline Player nextPlayer() const override { return getNextPlayer(player_, kTieTacToeNumPlayer); }
    inline std::string toConsoleString() const override { return minizero::utils::SGFLoader::actionIDToBoardCoordinateString(getActionID(), kTieTacToeBoardSize); }
};

class TieTacToeEnv : public BaseEnv<TieTacToeAction> {
public:
    TieTacToeEnv() { reset(); }

    void reset() override;
    bool act(const TieTacToeAction& action) override;
    bool act(const std::vector<std::string>& action_string_args) override;
    std::vector<TieTacToeAction> getLegalActions() const override;
    bool isLegalAction(const TieTacToeAction& action) const override;
    bool isTerminal() const override;
    float getEvalScore(bool is_resign = false) const override;
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::string toString() const override;
    inline std::string name() const override { return kTieTacToeName; }

private:
    Player eval() const;

    std::vector<Player> board_;
};

class TieTacToeEnvLoader : public BaseEnvLoader<TieTacToeAction, TieTacToeEnv> {
public:
    inline std::vector<float> getActionFeatures(int id, utils::Rotation rotation = utils::Rotation::kRotationNone) const override
    {
        assert(id < static_cast<int>(action_pairs_.size()));
        std::vector<float> action_features(kTieTacToeBoardSize * kTieTacToeBoardSize, 0.0f);
        action_features[getRotatePosition(action_pairs_[id].first.getActionID(), rotation)] = 1.0f;
        return action_features;
    }

    inline int getPolicySize() const override { return kTieTacToeBoardSize * kTieTacToeBoardSize; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return getPositionByRotating(rotation, position, kTieTacToeBoardSize); }
    inline std::string getEnvName() const override { return kTieTacToeName; }
};

} // namespace minizero::env::tietactoe
