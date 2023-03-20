#pragma once

#include "base_env.h"
#include "configuration.h"
#include <string>
#include <utility>
#include <vector>

namespace minizero::env::gomoku {

const std::string kGomokuName = "gomoku";
const int kGomokuNumPlayer = 2;
const int kMaxGomokuBoardSize = 19;

typedef BaseBoardAction<kGomokuNumPlayer> GomokuAction;

class GomokuEnv : public BaseBoardEnv<GomokuAction> {
public:
    GomokuEnv()
    {
        assert(getBoardSize() <= kMaxGomokuBoardSize);
        reset();
    }

    void reset() override;
    bool act(const GomokuAction& action) override;
    bool act(const std::vector<std::string>& action_string_args) override;
    std::vector<GomokuAction> getLegalActions() const override;
    bool isLegalAction(const GomokuAction& action) const override;
    bool isTerminal() const override;
    float getReward() const override { return 0.0f; }
    float getEvalScore(bool is_resign = false) const override;
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const GomokuAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::string toString() const override;
    inline std::string name() const override { return kGomokuName + "_" + std::to_string(getBoardSize()) + "x" + std::to_string(getBoardSize()); }
    inline int getNumPlayer() const override { return kGomokuNumPlayer; }

private:
    Player updateWinner(const GomokuAction& action);
    int calculateNumberOfConnection(int start_pos, std::pair<int, int> direction);
    std::string getCoordinateString() const;

    Player winner_;
    std::vector<Player> board_;
};

class GomokuEnvLoader : public BaseBoardEnvLoader<GomokuAction, GomokuEnv> {
public:
    std::vector<float> getActionFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline std::vector<float> getValue(const int pos) const { return {getReturn()}; }
    inline std::string name() const override { return kGomokuName + "_" + std::to_string(getBoardSize()) + "x" + std::to_string(getBoardSize()); }
    inline int getPolicySize() const override { return getBoardSize() * getBoardSize(); }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return getPositionByRotating(rotation, position, getBoardSize()); }
};

} // namespace minizero::env::gomoku
