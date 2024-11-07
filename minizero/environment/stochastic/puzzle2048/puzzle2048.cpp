#include "puzzle2048.h"
#include "random.h"
#include "sgf_loader.h"
#include <algorithm>
#include <string>

namespace minizero::env::puzzle2048 {

using namespace minizero::utils;

void Puzzle2048Env::reset(int seed)
{
    random_.seed(seed_ = seed);
    actions_.clear();
    events_.clear();
    reward_ = 0;
    total_reward_ = 0;

    board_.initialize(false);
    turn_ = Player::kPlayerNone;
    actChanceEvent();
    turn_ = Player::kPlayerNone;
    actChanceEvent();
}

bool Puzzle2048Env::act(const Puzzle2048Action& action, bool with_chance /* = true */)
{
    if (turn_ != Player::kPlayer1) { return false; }
    int reward = board_.slide(action.getActionID());
    if (reward == -1) { return false; }
    actions_.push_back(action);
    reward_ = reward;
    total_reward_ += reward;
    turn_ = Player::kPlayerNone;
    if (with_chance) { actChanceEvent(); }
    return true;
}

bool Puzzle2048Env::actChanceEvent(const Puzzle2048Action& action)
{
    if (turn_ != Player::kPlayerNone) { return false; }
    Puzzle2048ChanceEvent event(action);
    if (event.getActionID() == -1 || board_.get(event.getPosition()) != 0) { return false; }
    events_.push_back(event);
    board_.set(event.getPosition(), event.getTile());
    turn_ = Player::kPlayer1;
    // TODO: inconsistent env random_ state?
    return true;
}

bool Puzzle2048Env::actChanceEvent()
{
    if (turn_ != Player::kPlayerNone) { return false; }
    int empty = board_.countEmptyPositions();
    if (empty == 0) { return false; }
    int pos = board_.getNthEmptyPosition(std::uniform_int_distribution<int>(0, empty - 1)(random_));
    int tile = std::uniform_int_distribution<int>(0, 9)(random_) ? 1 : 2;
    events_.push_back(Puzzle2048ChanceEvent(pos, tile));
    board_.set(pos, tile);
    turn_ = Player::kPlayer1;
    return true;
}

std::vector<Puzzle2048Action> Puzzle2048Env::getLegalActions() const
{
    if (turn_ != Player::kPlayer1) { return {}; }
    std::vector<Puzzle2048Action> actions;
    for (int move = 0; move < 4; ++move) {
        if (Bitboard(board_).slide(move) != -1) { actions.emplace_back(move, Player::kPlayer1); }
    }
    return actions;
}

std::vector<Puzzle2048Action> Puzzle2048Env::getLegalChanceEvents() const
{
    if (turn_ != Player::kPlayerNone) { return {}; }
    std::vector<Puzzle2048Action> events;
    for (int pos = 0; pos < 16; ++pos) {
        if (board_.get(pos) == 0) {
            events.push_back(Puzzle2048ChanceEvent(pos, 1));
            events.push_back(Puzzle2048ChanceEvent(pos, 2));
        }
    }
    return events;
}

float Puzzle2048Env::getChanceEventProbability(const Puzzle2048Action& action) const
{
    if (turn_ != Player::kPlayerNone) { return 0; }
    return (Puzzle2048ChanceEvent(action).getTile() == 1 ? 0.9f : 0.1f) / board_.countEmptyPositions();
}

bool Puzzle2048Env::isLegalAction(const Puzzle2048Action& action) const
{
    return turn_ == Player::kPlayer1 && Bitboard(board_).slide(action.getActionID()) != -1 && action.getPlayer() == Player::kPlayer1;
}

bool Puzzle2048Env::isLegalChanceEvent(const Puzzle2048Action& action) const
{
    Puzzle2048ChanceEvent event(action);
    int pos = event.getPosition(), tile = event.getTile();
    return turn_ == Player::kPlayerNone && (pos != -1 && board_.get(pos) == 0) && (tile == 1 || tile == 2) && action.getPlayer() == Player::kPlayerNone;
}

bool Puzzle2048Env::isTerminal() const
{
    for (int move = 0; move < 4; ++move) {
        if (Bitboard(board_).slide(move) != -1) { return false; }
    }
    return true;
}

std::vector<float> Puzzle2048Env::getFeatures(utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    // 16 channels: the nth channel represents the position of the nth tile
    std::vector<float> features;
    for (int tile = 0; tile < 16; ++tile) {
        for (int pos = 0; pos < 16; ++pos) {
            features.push_back(board_.get(getRotatePosition(pos, rotation)) == tile ? 1.0f : 0.0f);
        }
    }
    return features;
}

std::vector<float> Puzzle2048Env::getActionFeatures(const Puzzle2048Action& action, utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    int hidden_size = kPuzzle2048BoardSize * kPuzzle2048BoardSize;
    std::vector<float> action_features(kPuzzle2048ActionSize * hidden_size, 0.0f);
    int action_id = getRotateAction(action.getActionID(), rotation);
    std::fill(action_features.begin() + action_id * hidden_size, action_features.begin() + (action_id + 1) * hidden_size, 1.0f);
    return action_features;
}

std::vector<float> Puzzle2048Env::getChanceEventFeatures(const Puzzle2048Action& event, utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    int hidden_size = kPuzzle2048BoardSize * kPuzzle2048BoardSize;
    std::vector<float> event_features(kPuzzle2048ChanceEventSize * hidden_size, 0.0f);
    int event_id = getRotateChanceEvent(event.getActionID(), rotation) - kPuzzle2048ActionSize;
    std::fill(event_features.begin() + event_id * hidden_size, event_features.begin() + (event_id + 1) * hidden_size, 1.0f);
    return event_features;
}

int Puzzle2048Env::getRotateChanceEvent(int event_id, utils::Rotation rotation) const
{
    Puzzle2048ChanceEvent event(event_id);
    return Puzzle2048ChanceEvent(getRotatePosition(event.getPosition(), rotation), event.getTile()).getActionID();
}

std::string Puzzle2048Env::toString() const
{
    std::ostringstream oss;
    oss << board_;
    return oss.str();
}

std::vector<float> Puzzle2048EnvLoader::getActionFeatures(const int pos, utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    int hidden_size = kPuzzle2048BoardSize * kPuzzle2048BoardSize;
    std::vector<float> action_features(getPolicySize() * hidden_size, 0.0f);
    Puzzle2048Action action = (pos < static_cast<int>(action_pairs_.size())) ? action_pairs_[pos].first : Puzzle2048Action(utils::Random::randInt() % kPuzzle2048ActionSize, Player::kPlayer1);
    return Puzzle2048Env().getActionFeatures(action, rotation);
}

std::vector<float> Puzzle2048EnvLoader::getChanceEventFeatures(const int pos, utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    Puzzle2048Env env;
    Puzzle2048Action event;
    if (pos < static_cast<int>(action_pairs_.size())) {
        env.reset(getSeed());
        for (int i = 0; i <= std::min<int>(pos, action_pairs_.size() - 1); ++i) { env.act(action_pairs_[i].first); }
        event = env.getChanceEventHistory().back();
    } else { // absorbing states
        event = Puzzle2048ChanceEvent(utils::Random::randInt() % 16, utils::Random::randInt() % 10 ? 1 : 2);
    }
    return env.getChanceEventFeatures(event, rotation);
}

std::vector<float> Puzzle2048EnvLoader::getChance(const int pos, utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    std::vector<float> chance(getChanceEventSize(), 0.0f);
    if (pos < static_cast<int>(action_pairs_.size())) {
        Puzzle2048Env env;
        env.reset(getSeed());
        for (int i = 0; i <= std::min<int>(pos, action_pairs_.size() - 1); ++i) { env.act(action_pairs_[i].first); }
        Puzzle2048ChanceEvent rotated_event = getRotateChanceEvent(env.getChanceEventHistory().back().getActionID(), rotation);
        chance[rotated_event.getActionID() - env.getPolicySize()] = 1.0f;
    } else { // absorbing states
        std::fill(chance.begin(), chance.end(), 1.0f / getChanceEventSize());
    }
    return chance;
}

float Puzzle2048EnvLoader::calculateNStepValue(const int pos) const
{
    assert(pos < static_cast<int>(action_pairs_.size()));

    const int n_step = config::learner_n_step_return;
    const float discount = config::actor_mcts_reward_discount;
    size_t bootstrap_index = pos + n_step;
    float value = 0.0f;
    float n_step_value = ((bootstrap_index < action_pairs_.size()) ? std::pow(discount, n_step) * BaseEnvLoader::getValue(bootstrap_index)[0] : 0.0f);
    for (size_t index = pos; index < std::min(bootstrap_index, action_pairs_.size()); ++index) {
        float reward = BaseEnvLoader::getReward(index)[0];
        value += std::pow(discount, index - pos) * reward;
    }
    value += n_step_value;
    return value;
}

std::vector<float> Puzzle2048EnvLoader::toDiscreteValue(float value) const
{
    std::vector<float> discrete_value(kPuzzle2048DiscreteValueSize, 0.0f);
    int value_floor = floor(value);
    int value_ceil = ceil(value);
    int shift = kPuzzle2048DiscreteValueSize / 2;
    int value_floor_shift = std::min(std::max(value_floor + shift, 0), kPuzzle2048DiscreteValueSize - 1);
    int value_ceil_shift = std::min(std::max(value_ceil + shift, 0), kPuzzle2048DiscreteValueSize - 1);
    if (value_floor == value_ceil) {
        discrete_value[value_floor_shift] = 1.0f;
    } else {
        discrete_value[value_floor_shift] = value_ceil - value;
        discrete_value[value_ceil_shift] = value - value_floor;
    }
    return discrete_value;
}

} // namespace minizero::env::puzzle2048
