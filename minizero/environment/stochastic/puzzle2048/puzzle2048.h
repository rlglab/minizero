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
const int kPuzzle2048DiscreteValueSize = 601;
const std::string kPuzzle2048ActionName[] = {"up", "right", "down", "left", "null"};

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
    inline std::string toConsoleString() const { return kPuzzle2048ActionName[std::min<unsigned>(action_id_, 4)]; }
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
    int getMaxChanceEventSize() const override { return 32; }
    float getChanceEventProbability(const Puzzle2048Action& action) const override;
    bool isLegalAction(const Puzzle2048Action& action) const override;
    bool isLegalChanceEvent(const Puzzle2048Action& action) const override;
    bool isTerminal() const override;

    int getRotatePosition(int position, utils::Rotation rotation) const override { return utils::getPositionByRotating(rotation, position, kPuzzle2048BoardSize); }
    int getRotateAction(int action_id, utils::Rotation rotation) const override;
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const Puzzle2048Action& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline int getNumInputChannels() const override { return 16; }
    inline int getNumActionFeatureChannels() const override { return 1; }
    inline int getInputChannelHeight() const override { return kPuzzle2048BoardSize; }
    inline int getInputChannelWidth() const override { return kPuzzle2048BoardSize; }
    inline int getHiddenChannelHeight() const override { return kPuzzle2048BoardSize; }
    inline int getHiddenChannelWidth() const override { return kPuzzle2048BoardSize; }
    inline int getPolicySize() const override { return kPuzzle2048ActionSize; }
    inline int getDiscreteValueSize() const override { return kPuzzle2048DiscreteValueSize; }

    std::string toString() const override;
    std::string name() const override { return kPuzzle2048Name; }
    int getNumPlayer() const override { return kPuzzle2048NumPlayer; }
    float getReward() const override { return reward_; }
    float getEvalScore(bool is_resign = false) const override { return total_reward_; }

private:
    struct Puzzle2048ChanceEvent {
        int pos_;
        int tile_;
        static Puzzle2048ChanceEvent toChanceEvent(const Puzzle2048Action& action);
        static Puzzle2048Action toAction(const Puzzle2048ChanceEvent& event);
    };

private:
    Bitboard board_;
    int reward_;
    int total_reward_;
};

class Puzzle2048EnvLoader : public StochasticEnvLoader<Puzzle2048Action, Puzzle2048Env> {
public:
    std::vector<float> getActionFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override { return Puzzle2048Env().getActionFeatures(pos < static_cast<int>(action_pairs_.size()) ? action_pairs_[pos].first : Puzzle2048Action(), rotation); }
    std::vector<float> getValue(const int pos) const override { return toDiscreteValue(pos < static_cast<int>(action_pairs_.size()) ? utils::transformValue(calculateNStepValue(pos)) : 0.0f); }
    std::vector<float> getReward(const int pos) const override { return toDiscreteValue(pos < static_cast<int>(action_pairs_.size()) ? utils::transformValue(BaseEnvLoader::getReward(pos)[0]) : 0.0f); }
    float getPriority(const int pos) const override { return fabs(calculateNStepValue(pos) - BaseEnvLoader::getValue(pos)[0]); }

    std::string name() const override { return kPuzzle2048Name; }
    int getPolicySize() const override { return 4; }
    int getRotatePosition(int position, utils::Rotation rotation) const override { return utils::getPositionByRotating(rotation, position, kPuzzle2048BoardSize); }
    int getRotateAction(int action_id, utils::Rotation rotation) const override { return Puzzle2048Env().getRotateAction(action_id, rotation); }

private:
    float calculateNStepValue(const int pos) const;
    std::vector<float> toDiscreteValue(float value) const;
};

} // namespace minizero::env::puzzle2048
