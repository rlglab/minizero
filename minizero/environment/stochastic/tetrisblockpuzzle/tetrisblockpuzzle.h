#pragma once

#include "bitboard.h"
#include "stochastic_env.h"
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

namespace minizero::env::tetrisblockpuzzle {

const std::string kTetrisBlockPuzzleName = "tetrisblockpuzzle";
const int kTetrisBlockPuzzleNumPlayer = 1;
const int kTetrisBlockPuzzleBoardSize = 8;
const int kTetrisBlockPuzzleActionSize = 801;
const int kTetrisChanceEventSize = 19;
const int kTetrisEmptyHoldingBlock = -1;
const int kTetrisBlockPuzzleMaxBlockSize = 4;
const int kTetrisBlockPuzzleDiscreteValueSize = 601;
extern std::vector<std::string> kTetrisBlockPuzzleActionName;
extern std::unordered_map<std::string, int> kTetrisBlockPuzzleActionNameToID;
void initialize();

class TetrisBlockPuzzleAction : public BaseAction {
public:
    TetrisBlockPuzzleAction() : BaseAction() {}
    TetrisBlockPuzzleAction(int action_id, Player player) : BaseAction(action_id, player) {}
    TetrisBlockPuzzleAction(const std::vector<std::string>& action_string_args)
    {
        assert(action_string_args.size() && action_string_args[1].size());
        action_id_ = kTetrisBlockPuzzleActionNameToID[action_string_args[1]];
        player_ = Player::kPlayer1;
    }

    inline Player nextPlayer() const override { return Player::kPlayer1; }
    inline std::string toConsoleString() const { return kTetrisBlockPuzzleActionName[std::min<int>(action_id_, kTetrisBlockPuzzleActionName.size() - 1)]; }
};

class TetrisBlockPuzzleEnv : public StochasticEnv<TetrisBlockPuzzleAction> {
public:
    TetrisBlockPuzzleEnv() {}

    void reset() override { reset(utils::Random::randInt()); }
    void reset(int seed) override;
    bool act(const TetrisBlockPuzzleAction& action, bool with_chance = true) override;
    bool act(const std::vector<std::string>& action_string_args, bool with_chance = true) override { return act(TetrisBlockPuzzleAction(action_string_args), with_chance); }
    bool actChanceEvent(const TetrisBlockPuzzleAction& action) override;
    bool actChanceEvent();

    const Bitboard& getBitboard() const { return board_; }
    const std::vector<int>& getHoldingBlocks() const { return holding_blocks_; }
    const std::deque<int>& getPreviewHoldingBlocks() const { return preview_holding_blocks_; }
    std::vector<TetrisBlockPuzzleAction> getLegalActions() const override;
    std::vector<TetrisBlockPuzzleAction> getLegalChanceEvents() const override;
    int getRotateChanceEvent(int event_id, utils::Rotation rotation) const override { return event_id; } //todo
    float getChanceEventProbability(const TetrisBlockPuzzleAction& action) const override;
    bool isLegalAction(const TetrisBlockPuzzleAction& action) const override;
    bool isLegalChanceEvent(const TetrisBlockPuzzleAction& action) const override;
    bool isTerminal() const override;

    int getRotatePosition(int position, utils::Rotation rotation) const override { return utils::getPositionByRotating(rotation, position, kTetrisBlockPuzzleBoardSize); }
    int getRotateAction(int action_id, utils::Rotation rotation) const override;
    int getRotateHoldingBlockID(int holding_block_id, utils::Rotation rotation) const;
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const TetrisBlockPuzzleAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getChanceEventFeatures(const TetrisBlockPuzzleAction& event, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline int getNumInputChannels() const override { return 2 + kTetrisChanceEventSize * (config::env_tetris_block_puzzle_num_holding_block + config::env_tetris_block_puzzle_num_preview_holding_block); } // with or without block + blocks type * holding blocks
    inline int getNumActionFeatureChannels() const override { return kTetrisBlockPuzzleActionSize; }
    inline int getNumChanceEventFeatureChannels() const override { return kTetrisChanceEventSize; }
    inline int getInputChannelHeight() const override { return kTetrisBlockPuzzleBoardSize; }
    inline int getInputChannelWidth() const override { return kTetrisBlockPuzzleBoardSize; }
    inline int getHiddenChannelHeight() const override { return kTetrisBlockPuzzleBoardSize; }
    inline int getHiddenChannelWidth() const override { return kTetrisBlockPuzzleBoardSize; }
    inline int getPolicySize() const override { return kTetrisBlockPuzzleActionSize; }
    inline int getChanceEventSize() const override { return kTetrisChanceEventSize; }
    inline int getDiscreteValueSize() const override { return kTetrisBlockPuzzleDiscreteValueSize; }

    std::string toString() const override;
    std::string name() const override { return kTetrisBlockPuzzleName; }
    int getNumPlayer() const override { return kTetrisBlockPuzzleNumPlayer; }
    float getReward() const override { return reward_; }
    float getEvalScore(bool is_resign = false) const override { return total_reward_; }

private:
    struct TetrisBlockPuzzleChanceEvent {
        int block;
        static TetrisBlockPuzzleChanceEvent toChanceEvent(const TetrisBlockPuzzleAction& action);
        static TetrisBlockPuzzleAction toAction(const TetrisBlockPuzzleChanceEvent& event);
    };

private:
    Bitboard board_;
    std::vector<int> holding_blocks_;
    std::deque<int> preview_holding_blocks_;
    int reward_;
    int total_reward_;
    int num_holding_block_;
    int num_preview_holding_block_;
};

class TetrisBlockPuzzleEnvLoader : public StochasticEnvLoader<TetrisBlockPuzzleAction, TetrisBlockPuzzleEnv> {
public:
    std::vector<float> getActionFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override { return TetrisBlockPuzzleEnv().getActionFeatures(pos < static_cast<int>(action_pairs_.size()) ? action_pairs_[pos].first : TetrisBlockPuzzleAction(), rotation); }
    std::vector<float> getChanceEventFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override { return TetrisBlockPuzzleEnv().getChanceEventFeatures(pos < static_cast<int>(action_pairs_.size()) ? action_pairs_[pos].first : TetrisBlockPuzzleAction(), rotation); }
    std::vector<float> getChance(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getValue(const int pos) const override { return toDiscreteValue(pos < static_cast<int>(action_pairs_.size()) ? utils::transformValue(calculateNStepValue(pos)) : 0.0f); }
    std::vector<float> getAfterstateValue(const int pos) const override { return toDiscreteValue(pos < static_cast<int>(action_pairs_.size()) ? utils::transformValue(calculateNStepValue(pos) - BaseEnvLoader::getReward(pos)[0]) : 0.0f); }
    std::vector<float> getReward(const int pos) const override { return toDiscreteValue(pos < static_cast<int>(action_pairs_.size()) ? utils::transformValue(BaseEnvLoader::getReward(pos)[0]) : 0.0f); }
    float getPriority(const int pos) const override { return fabs(calculateNStepValue(pos) - BaseEnvLoader::getValue(pos)[0]); }

    std::string name() const override { return kTetrisBlockPuzzleName; }
    int getPolicySize() const override { return kTetrisBlockPuzzleActionSize; }
    int getChanceEventSize() const override { return kTetrisChanceEventSize; }
    int getRotatePosition(int position, utils::Rotation rotation) const override { return utils::getPositionByRotating(rotation, position, kTetrisBlockPuzzleBoardSize); }
    int getRotateAction(int action_id, utils::Rotation rotation) const override { return TetrisBlockPuzzleEnv().getRotateAction(action_id, rotation); }
    int getRotateChanceEvent(int event_id, utils::Rotation rotation) const override { return event_id; }

private:
    float calculateNStepValue(const int pos) const;
    std::vector<float> toDiscreteValue(float value) const;
};

} // namespace minizero::env::tetrisblockpuzzle
