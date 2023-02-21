#pragma once

#include "base_env.h"
#include "configuration.h"
#include "sgf_loader.h"
#include <string>
#include <utility>
#include <vector>

namespace minizero::env::gomoku {

const std::string kGomokuName = "gomoku";
const int kGomokuNumPlayer = 2;
const int kMaxGomokuBoardSize = 19;

class GomokuAction : public BaseAction {
public:
    GomokuAction() : BaseAction() {}
    GomokuAction(int action_id, Player player) : BaseAction(action_id, player) {}
    GomokuAction(const std::vector<std::string>& action_string_args);

    inline Player nextPlayer() const override { return getNextPlayer(player_, kGomokuNumPlayer); }
    inline std::string toConsoleString() const override { return minizero::utils::SGFLoader::actionIDToBoardCoordinateString(getActionID(), minizero::config::env_board_size); }
};

class GomokuEnv : public BaseEnv<GomokuAction> {
public:
    GomokuEnv()
        : board_size_(minizero::config::env_board_size)
    {
        assert(board_size_ > 0 && board_size_ <= kMaxGomokuBoardSize);
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
    inline std::string name() const override { return kGomokuName; }
    inline int getNumPlayer() const override { return kGomokuNumPlayer; }

private:
    Player updateWinner(const GomokuAction& action);
    int calculateNumberOfConnection(int start_pos, std::pair<int, int> direction);
    std::string getCoordinateString() const;

    int board_size_;
    Player winner_;
    std::vector<Player> board_;
};

class GomokuEnvLoader : public BaseEnvLoader<GomokuAction, GomokuEnv> {
public:
    inline int getPolicySize() const override { return minizero::config::env_board_size * minizero::config::env_board_size; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return getPositionByRotating(rotation, position, minizero::config::env_board_size); }
    inline std::string getEnvName() const override { return kGomokuName; }
};

} // namespace minizero::env::gomoku
