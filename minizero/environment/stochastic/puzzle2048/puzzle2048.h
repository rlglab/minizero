#pragma once

#include "bitboard.h"
#include "stochastic_env.h"
#include <algorithm>
#include <string>
#include <vector>

namespace minizero::env::puzzle2048 {

const std::string kPuzzle2048Name = "puzzle2048";
const int kPuzzle2048NumPlayer = 1;
const int kPuzzle2048BoardSize = 4;
const int kPuzzle2048ActionSize = 4;
const int kPuzzle2048ChanceEventSize = 32;
const int kPuzzle2048DiscreteValueSize = 601;
const std::string kPuzzle2048ActionName[5] = {"up", "right", "down", "left", "null"};
const std::string kPuzzle2048ChanceEventName[33] = {
    "2@0", "2@1", "2@2", "2@3", "2@4", "2@5", "2@6", "2@7", "2@8", "2@9", "2@10", "2@11", "2@12", "2@13", "2@14", "2@15",
    "4@0", "4@1", "4@2", "4@3", "4@4", "4@5", "4@6", "4@7", "4@8", "4@9", "4@10", "4@11", "4@12", "4@13", "4@14", "4@15", "null"};
const int kPuzzle2048RotatedActionIndex[9][4] = {
    {0, 1, 2, 3}, // Rotation::kRotationNone
    {3, 0, 1, 2}, // Rotation::kRotation90
    {2, 3, 0, 1}, // Rotation::kRotation180
    {1, 2, 3, 0}, // Rotation::kRotation270
    {2, 1, 0, 3}, // Rotation::kHorizontalRotation
    {1, 0, 3, 2}, // Rotation::kHorizontalRotation90
    {0, 3, 2, 1}, // Rotation::kHorizontalRotation180
    {3, 2, 1, 0}, // Rotation::kHorizontalRotation270
    {0, 1, 2, 3}, // kRotateSize
};

class Puzzle2048Action : public BaseAction {
public:
    Puzzle2048Action() : BaseAction() {}
    Puzzle2048Action(int action_id, Player player) : BaseAction(action_id, player) {}
    Puzzle2048Action(const std::vector<std::string>& action_string_args)
    {
        assert(action_string_args.size() && action_string_args[0].size());
        action_id_ = std::string("URDL").find(std::toupper(action_string_args[0][0]));
        player_ = Player::kPlayer1;
    }

    inline Player nextPlayer() const override { return Player::kPlayer1; } // TODO
    inline std::string toConsoleString() const { return kPuzzle2048ActionName[std::min<int>(action_id_, kPuzzle2048ActionSize)]; }
};

class Puzzle2048ChanceEvent : public Puzzle2048Action {
public:
    Puzzle2048ChanceEvent() : Puzzle2048Action(), pos_(-1), tile_(-1) {}
    Puzzle2048ChanceEvent(int id) : Puzzle2048Action(id, Player::kPlayerNone), pos_((id - kPuzzle2048ActionSize) % 16), tile_((id - kPuzzle2048ActionSize) / 16 + 1)
    {
        assert(id >= kPuzzle2048ActionSize && id < kPuzzle2048ActionSize + kPuzzle2048ChanceEventSize);
    }
    Puzzle2048ChanceEvent(int pos, int tile) : Puzzle2048Action(((tile - 1) * 16 + pos) + kPuzzle2048ActionSize, Player::kPlayerNone), pos_(pos), tile_(tile)
    {
        assert(pos >= 0 && pos < 16 && (tile == 1 || tile == 2));
    }
    Puzzle2048ChanceEvent(const BaseAction& action) : Puzzle2048ChanceEvent(action.getActionID()) {}
    Puzzle2048ChanceEvent(const std::vector<std::string>& action_string_args) = delete;

    inline int getPosition() const { return pos_; }
    inline int getTile() const { return tile_; }
    inline std::string toConsoleString() const { return kPuzzle2048ChanceEventName[std::min(action_id_ - kPuzzle2048ActionSize, kPuzzle2048ChanceEventSize)]; }

protected:
    int pos_;
    int tile_;
};

class Puzzle2048Env : public StochasticEnv<Puzzle2048Action> {
public:
    Puzzle2048Env() {}

    void reset() override { reset(utils::Random::randInt()); }
    void reset(int seed) override;
    bool act(const Puzzle2048Action& action, bool with_chance = true) override;
    bool act(const std::vector<std::string>& action_string_args, bool with_chance = true) override { return act(Puzzle2048Action(action_string_args), with_chance); }
    bool actChanceEvent(const Puzzle2048Action& action) override;
    bool actChanceEvent();

    std::vector<Puzzle2048Action> getLegalActions() const override;
    std::vector<Puzzle2048Action> getLegalChanceEvents() const override;
    float getChanceEventProbability(const Puzzle2048Action& action) const override;
    bool isLegalAction(const Puzzle2048Action& action) const override;
    bool isLegalChanceEvent(const Puzzle2048Action& action) const override;
    bool isTerminal() const override;

    int getRotatePosition(int position, utils::Rotation rotation) const override { return utils::getPositionByRotating(rotation, position, kPuzzle2048BoardSize); }
    int getRotateAction(int action_id, utils::Rotation rotation) const override { return action_id != -1 ? kPuzzle2048RotatedActionIndex[static_cast<int>(rotation)][action_id] : action_id; }
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const Puzzle2048Action& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getChanceEventFeatures(const Puzzle2048Action& event, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    int getRotateChanceEvent(int event_id, utils::Rotation rotation) const override;
    inline int getNumInputChannels() const override { return 16; }
    inline int getNumActionFeatureChannels() const override { return kPuzzle2048ActionSize; }
    inline int getNumChanceEventFeatureChannels() const override { return kPuzzle2048ChanceEventSize; }
    inline int getInputChannelHeight() const override { return kPuzzle2048BoardSize; }
    inline int getInputChannelWidth() const override { return kPuzzle2048BoardSize; }
    inline int getHiddenChannelHeight() const override { return kPuzzle2048BoardSize; }
    inline int getHiddenChannelWidth() const override { return kPuzzle2048BoardSize; }
    inline int getPolicySize() const override { return kPuzzle2048ActionSize; }
    inline int getChanceEventSize() const override { return kPuzzle2048ChanceEventSize; }
    inline int getDiscreteValueSize() const override { return kPuzzle2048DiscreteValueSize; }

    std::string toString() const override;
    std::string name() const override { return kPuzzle2048Name; }
    int getNumPlayer() const override { return kPuzzle2048NumPlayer; }
    float getReward() const override { return reward_; }
    float getEvalScore(bool is_resign = false) const override { return total_reward_; }

private:
    Bitboard board_;
    int reward_;
    int total_reward_;
};

class Puzzle2048EnvLoader : public StochasticEnvLoader<Puzzle2048Action, Puzzle2048Env> {
public:
    std::vector<float> getActionFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getChanceEventFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getChance(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getValue(const int pos) const override { return toDiscreteValue(pos < static_cast<int>(action_pairs_.size()) ? utils::transformValue(calculateNStepValue(pos)) : 0.0f); }
    std::vector<float> getAfterstateValue(const int pos) const override { return toDiscreteValue(pos < static_cast<int>(action_pairs_.size()) ? utils::transformValue(calculateNStepValue(pos) - BaseEnvLoader::getReward(pos)[0]) : 0.0f); }
    std::vector<float> getReward(const int pos) const override { return toDiscreteValue(pos < static_cast<int>(action_pairs_.size()) ? utils::transformValue(BaseEnvLoader::getReward(pos)[0]) : 0.0f); }
    float getPriority(const int pos) const override { return fabs(calculateNStepValue(pos) - BaseEnvLoader::getValue(pos)[0]); }

    std::string name() const override { return kPuzzle2048Name; }
    int getPolicySize() const override { return kPuzzle2048ActionSize; }
    int getChanceEventSize() const override { return kPuzzle2048ChanceEventSize; }
    int getRotatePosition(int position, utils::Rotation rotation) const override { return utils::getPositionByRotating(rotation, position, kPuzzle2048BoardSize); }
    int getRotateAction(int action_id, utils::Rotation rotation) const override { return Puzzle2048Env().getRotateAction(action_id, rotation); }
    int getRotateChanceEvent(int event_id, utils::Rotation rotation) const override { return Puzzle2048Env().getRotateChanceEvent(event_id, rotation); }

private:
    float calculateNStepValue(const int pos) const;
    std::vector<float> toDiscreteValue(float value) const;
};

} // namespace minizero::env::puzzle2048
