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
    Puzzle2048ChanceEvent event = Puzzle2048ChanceEvent::toChanceEvent(action);
    if (event.pos_ == -1 || event.tile_ == -1 || board_.get(event.pos_) != 0) { return false; }
    board_.set(event.pos_, event.tile_);
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
            events.push_back(Puzzle2048ChanceEvent::toAction({pos, 1}));
            events.push_back(Puzzle2048ChanceEvent::toAction({pos, 2}));
        }
    }
    return events;
}

float Puzzle2048Env::getChanceEventProbability(const Puzzle2048Action& action) const
{
    if (turn_ != Player::kPlayerNone) { return 0; }
    return (Puzzle2048ChanceEvent::toChanceEvent(action).tile_ == 1 ? 0.9f : 0.1f) / board_.countEmptyPositions();
}

bool Puzzle2048Env::isLegalAction(const Puzzle2048Action& action) const
{
    return turn_ == Player::kPlayer1 && Bitboard(board_).slide(action.getActionID()) != -1 && action.getPlayer() == Player::kPlayer1;
}

bool Puzzle2048Env::isLegalChanceEvent(const Puzzle2048Action& action) const
{
    Puzzle2048ChanceEvent event = Puzzle2048ChanceEvent::toChanceEvent(action);
    int pos = event.pos_, tile = event.tile_;
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

int Puzzle2048Env::getRotateAction(int action_id, utils::Rotation rotation) const
{
    if (action_id == -1) { return action_id; }
    std::array<int, 4> index;
    switch (rotation) {
        case Rotation::kRotationNone:
            index = {0, 1, 2, 3};
            break;
        case Rotation::kRotation90:
            index = {3, 0, 1, 2};
            break;
        case Rotation::kRotation180:
            index = {2, 3, 0, 1};
            break;
        case Rotation::kRotation270:
            index = {1, 2, 3, 0};
            break;
        case Rotation::kHorizontalRotation:
            index = {2, 1, 0, 3};
            break;
        case Rotation::kHorizontalRotation90:
            index = {1, 0, 3, 2};
            break;
        case Rotation::kHorizontalRotation180:
            index = {0, 3, 2, 1};
            break;
        case Rotation::kHorizontalRotation270:
            index = {3, 2, 1, 0};
            break;
        default:
            break;
    }
    return index[action_id];
}

std::vector<float> Puzzle2048Env::getActionFeatures(const Puzzle2048Action& action, utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    int action_id = action.getActionID();
    std::vector<float> action_features(4, 0.0f);
    if (action_id != -1) { action_features[getRotateAction(action_id, rotation)] = 1.0f; }
    return action_features;
}

Puzzle2048Env::Puzzle2048ChanceEvent Puzzle2048Env::Puzzle2048ChanceEvent::toChanceEvent(const Puzzle2048Action& action)
{
    int id = action.getActionID();
    if (id == -1) { return {-1, -1}; }
    return {id & 0xf, id >> 4};
}

Puzzle2048Action Puzzle2048Env::Puzzle2048ChanceEvent::toAction(const Puzzle2048Env::Puzzle2048ChanceEvent& event)
{
    if (event.pos_ == -1 || event.tile_ == -1) return {};
    return {(event.tile_ << 4) | (event.pos_), Player::kPlayerNone};
}

std::string Puzzle2048Env::toString() const
{
    std::ostringstream oss;
    oss << board_;
    return oss.str();
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
    std::vector<float> discrete_value(config::nn_discrete_value_size, 0.0f);
    int value_floor = floor(value);
    int value_ceil = ceil(value);
    int shift = config::nn_discrete_value_size / 2;
    if (value_floor == value_ceil) {
        discrete_value[value_floor + shift] = 1.0f;
    } else {
        discrete_value[value_floor + shift] = value_ceil - value;
        discrete_value[value_ceil + shift] = value - value_floor;
    }
    return discrete_value;
}

} // namespace minizero::env::puzzle2048
