#include "tetris.h"
#include "random.h"
#include <algorithm>
#include <string>

namespace minizero::env::tetris {

using namespace minizero::utils;

void TetrisEnv::reset(int seed)
{
    random_.seed(seed_ = seed);
    actions_.clear();
    reward_ = 0;
    total_reward_ = 0;

    board_.clear();
    turn_ = Player::kPlayerNone; //輪到 chance event
    actChanceEvent();
}

bool TetrisEnv::act(const TetrisAction& action, bool with_chance /* = true */)
{
    if (turn_ != Player::kPlayer1) { return false; }

    bool valid = false;
    if (action.getActionID() == kTetrisActionSize - 2) {
        valid = board_.drop();
    } else if (action.getActionID() == kTetrisActionSize - 1) {
        valid = board_.down();
    } else {
        int rotation = action.getRotation();
        int movement = action.getMovement();

        if (!board_.isLegalAction(rotation, movement)) {
            return false;
        }

        board_.rotate(rotation);
        if (movement < 0) {
            board_.moveLeft(-movement);
        } else if (movement > 0) {
            board_.moveRight(movement);
        }
        valid = true;
    }

    if (valid) {
        actions_.push_back(action);
        reward_ = 0;

        if (board_.isAtBottom()) {
            board_.placePiece();
            int lines_cleared = board_.clearFullLines();
            reward_ = lines_cleared * lines_cleared;
            total_reward_ += reward_;

            if (with_chance) {
                turn_ = Player::kPlayerNone;
                actChanceEvent();
            }
        } else if (with_chance) {
            turn_ = Player::kPlayerNone;
            actChanceEvent();
        }
    }

    return valid;
}

bool TetrisEnv::actChanceEvent(const TetrisAction& action)
{
    if (turn_ != Player::kPlayerNone) { return false; }
    if (isTerminal()) { return false; }

    TetrisChanceEvent event = TetrisChanceEvent::toChanceEvent(action);
    if (event.type_ == TetrisChanceEvent::EventType::NewPiece) {
        generateNewPiece();
    } else {
        fall();
    }

    turn_ = Player::kPlayer1;
    return true;
}

bool TetrisEnv::actChanceEvent()
{
    if (turn_ != Player::kPlayerNone) { return false; }
    if (isTerminal()) { return false; }

    if (board_.isAtBottom()) {
        generateNewPiece();
    } else {
        fall();
    }

    turn_ = Player::kPlayer1;
    return true;
}

std::vector<TetrisAction> TetrisEnv::getLegalActions() const
{
    if (turn_ != Player::kPlayer1) { return {}; }
    if (isTerminal()) { return {}; }
    std::vector<TetrisAction> actions;
    for (int rotation = 0; rotation < 4; ++rotation) {
        for (int movement = -kTetrisMaxMovement; movement <= kTetrisMaxMovement; ++movement) {
            if (board_.isLegalAction(rotation, movement)) {
                actions.emplace_back(rotation * (kTetrisMaxMovement * 2 + 1) + (movement + kTetrisMaxMovement), Player::kPlayer1);
            }
        }
    }
    actions.emplace_back(kTetrisActionSize - 2, Player::kPlayer1); // drop
    actions.emplace_back(kTetrisActionSize - 1, Player::kPlayer1); // down
    return actions;
}

std::vector<TetrisAction> TetrisEnv::getLegalChanceEvents() const
{
    if (turn_ != Player::kPlayerNone) { return {}; }
    if (isTerminal()) { return {}; }
    std::vector<TetrisAction> events;
    if (board_.isAtBottom()) {
        for (int i = 0; i < 7; ++i) {
            events.push_back(TetrisChanceEvent::toAction(TetrisChanceEvent(TetrisChanceEvent::EventType::NewPiece, i)));
        }
    } else {
        events.push_back(TetrisChanceEvent::toAction(TetrisChanceEvent(TetrisChanceEvent::EventType::Fall)));
    }
    return events;
}

float TetrisEnv::getChanceEventProbability(const TetrisAction& action) const
{
    TetrisChanceEvent event = TetrisChanceEvent::toChanceEvent(action);
    if (event.type_ == TetrisChanceEvent::EventType::NewPiece) {
        return board_.isAtBottom() ? 1.0f / 7 : 0.0f;
    } else {
        return board_.isAtBottom() ? 0.0f : 1.0f;
    }
}

bool TetrisEnv::isLegalAction(const TetrisAction& action) const
{
    return turn_ == Player::kPlayer1 &&
           board_.isLegalAction(action.getRotation(), action.getMovement()) &&
           action.getPlayer() == Player::kPlayer1;
}

bool TetrisEnv::isLegalChanceEvent(const TetrisAction& action) const
{
    TetrisChanceEvent event = TetrisChanceEvent::toChanceEvent(action);
    if (event.type_ == TetrisChanceEvent::EventType::NewPiece) {
        return board_.isAtBottom() && event.piece_type_ >= 0 && event.piece_type_ < 7 && action.getPlayer() == Player::kPlayerNone;
    } else {
        return !board_.isAtBottom() && action.getPlayer() == Player::kPlayerNone;
    }
}

bool TetrisEnv::isTerminal() const
{
    return board_.isGameOver();
}

void TetrisEnv::generateNewPiece()
{
    int piece_type = std::uniform_int_distribution<int>(0, 6)(random_);
    board_.generateNewPiece(piece_type);
}

bool TetrisEnv::fall()
{
    return board_.fall();
}

std::vector<float> TetrisEnv::getFeatures(utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    std::vector<float> features;
    for (int y = 0; y < kTetrisBoardHeight; ++y) { // first channel
        for (int x = 0; x < kTetrisBoardWidth; ++x) {
            features.push_back(board_.isOccupied(x, y) ? 1.0f : 0.0f);
        }
    }
    for (int y = 0; y < kTetrisBoardHeight; ++y) { // second channel
        for (int x = 0; x < kTetrisBoardWidth; ++x) {
            features.push_back(board_.isCurrentPiece(x, y) ? 1.0f : 0.0f);
        }
    }
    return features;
}

std::vector<float> TetrisEnv::getActionFeatures(const TetrisAction& action, utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    std::vector<float> action_features(kTetrisActionSize, 0.0f);
    action_features[action.getActionID()] = 1.0f;
    return action_features;
}

TetrisEnv::TetrisChanceEvent TetrisEnv::TetrisChanceEvent::toChanceEvent(const TetrisAction& action)
{
    if (action.getActionID() < 7) {
        return TetrisChanceEvent(EventType::NewPiece, action.getActionID());
    } else {
        return TetrisChanceEvent(EventType::Fall);
    }
}

TetrisAction TetrisEnv::TetrisChanceEvent::toAction(const TetrisEnv::TetrisChanceEvent& event)
{
    if (event.type_ == EventType::NewPiece) {
        return TetrisAction(event.piece_type_, Player::kPlayerNone);
    } else {
        return TetrisAction(7, Player::kPlayerNone);
    }
}

std::string TetrisEnv::toString() const
{
    std::ostringstream oss;
    oss << board_;
    return oss.str();
}

float TetrisEnvLoader::calculateNStepValue(const int pos) const
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

std::vector<float> TetrisEnvLoader::toDiscreteValue(float value) const
{
    std::vector<float> discrete_value(kTetrisDiscreteValueSize, 0.0f);
    int value_floor = floor(value);
    int value_ceil = ceil(value);
    int shift = kTetrisDiscreteValueSize / 2;
    int value_floor_shift = std::min(std::max(value_floor + shift, 0), kTetrisDiscreteValueSize - 1);
    int value_ceil_shift = std::min(std::max(value_ceil + shift, 0), kTetrisDiscreteValueSize - 1);
    if (value_floor == value_ceil) {
        discrete_value[value_floor_shift] = 1.0f;
    } else {
        discrete_value[value_floor_shift] = value_ceil - value;
        discrete_value[value_ceil_shift] = value - value_floor;
    }
    return discrete_value;
}

} // namespace minizero::env::tetris
