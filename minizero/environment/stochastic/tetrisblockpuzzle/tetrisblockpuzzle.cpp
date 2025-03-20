#include "tetrisblockpuzzle.h"
#include "random.h"
#include "sgf_loader.h"
#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>

namespace minizero::env::tetrisblockpuzzle {

using namespace minizero::utils;

const int kMaxActionSteps = 13500;
std::vector<Bitboard> kTetrisBlockPuzzleActions;
std::unordered_map<Bitboard, int> kTetrisBlockPuzzleActionToID;
std::unordered_map<std::string, int> kTetrisBlockPuzzleActionNameToID;
std::vector<std::string> kTetrisBlockPuzzleActionName;
std::unordered_map<int, std::vector<int>> kEachBlockWithLegalActionId;
std::unordered_map<int, int> kActionIdtoBlockId;
std::vector<std::vector<std::pair<int, int>>> kBlocksPos = {
    {{0, 0}, {0, 1}, {1, 0}, {1, 1}}, // 0 // O

    {{0, 0}, {0, 1}, {1, 1}, {1, 2}}, // 1 // Z
    {{0, 1}, {1, 0}, {1, 1}, {2, 0}}, // 2

    {{0, 1}, {0, 2}, {1, 0}, {1, 1}}, // 3 // Z
    {{0, 0}, {1, 0}, {1, 1}, {2, 1}}, // 4

    {{0, 0}, {0, 1}, {0, 2}, {0, 3}}, // 5 // I
    {{0, 0}, {1, 0}, {2, 0}, {3, 0}}, // 6

    {{0, 0}, {1, 0}, {1, 1}, {1, 2}}, // 7 // L
    {{0, 0}, {0, 1}, {1, 0}, {2, 0}}, // 8
    {{0, 0}, {0, 1}, {0, 2}, {1, 2}}, // 9
    {{0, 1}, {1, 1}, {2, 0}, {2, 1}}, // 10

    {{0, 2}, {1, 0}, {1, 1}, {1, 2}}, // 11 // L
    {{0, 0}, {1, 0}, {2, 0}, {2, 1}}, // 12
    {{0, 0}, {0, 1}, {0, 2}, {1, 0}}, // 13
    {{0, 0}, {0, 1}, {1, 1}, {2, 1}}, // 14

    {{0, 0}, {0, 1}, {0, 2}, {1, 1}}, // 15 // T
    {{0, 1}, {1, 0}, {1, 1}, {2, 1}}, // 16
    {{0, 1}, {1, 0}, {1, 1}, {1, 2}}, // 17
    {{0, 0}, {1, 0}, {1, 1}, {2, 0}}, // 18
};

std::vector<std::vector<int>> kBlocksRotationID = {
    // Rotation::kRotationNone,
    // Rotation::kRotation90,
    // Rotation::kRotation180,
    // Rotation::kRotation270,
    // Rotation::kHorizontalRotation,
    // Rotation::kHorizontalRotation90,
    // Rotation::kHorizontalRotation180,
    // Rotation::kHorizontalRotation270
    {0, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 1, 2, 3, 4, 3, 4},
    {2, 1, 2, 1, 4, 3, 4, 3},
    {3, 4, 3, 4, 1, 2, 1, 2},
    {4, 3, 4, 3, 2, 1, 2, 1},

    {5, 6, 5, 6, 5, 6, 5, 6},
    {6, 5, 6, 5, 6, 5, 6, 5},

    {7, 10, 9, 8, 13, 12, 11, 14},
    {8, 7, 10, 9, 12, 11, 14, 13},
    {9, 8, 7, 10, 11, 14, 13, 12},
    {10, 9, 8, 7, 14, 13, 12, 11},

    {11, 14, 13, 12, 9, 8, 7, 10},
    {12, 11, 14, 13, 8, 7, 10, 9},
    {13, 12, 11, 14, 7, 10, 9, 8},
    {14, 13, 12, 11, 10, 9, 8, 7},

    {15, 18, 17, 16, 17, 16, 15, 18},
    {16, 15, 18, 17, 16, 15, 18, 17},
    {17, 16, 15, 18, 15, 18, 17, 16},
    {18, 17, 16, 15, 18, 17, 16, 15},
};

bool inbound(int x, int y) { return x >= 0 && x < kTetrisBlockPuzzleBoardSize && y >= 0 && y < kTetrisBlockPuzzleBoardSize; }
bool inbound(std::pair<int, int> pos) { return inbound(pos.first, pos.second); }

void initialize()
{
    kTetrisBlockPuzzleActions.clear();
    kTetrisBlockPuzzleActionToID.clear();
    kTetrisBlockPuzzleActionNameToID.clear();
    kTetrisBlockPuzzleActionName.clear();
    kEachBlockWithLegalActionId.clear();
    kActionIdtoBlockId.clear();
    for (int b = 0; b < static_cast<int>(kBlocksPos.size()); b++) {
        std::vector<std::pair<int, int>> block = kBlocksPos[b];
        for (int i = 0; i < kTetrisBlockPuzzleBoardSize; i++) {
            for (int j = 0; j < kTetrisBlockPuzzleBoardSize; j++) {
                Bitboard board;
                bool valid = true;
                for (std::pair<int, int> pos : block) {
                    if (inbound(i + pos.first, j + pos.second)) {
                        board.setBit(i + pos.first, j + pos.second);
                    } else {
                        valid = false;
                        break;
                    }
                }
                if (valid) {
                    kTetrisBlockPuzzleActions.push_back(board);
                    kTetrisBlockPuzzleActionName.push_back(std::string(1, b + 'A') + '@' + std::string(1, i + 'a') + std::to_string(j + 1));
                    int action_id = kTetrisBlockPuzzleActions.size() - 1;
                    kTetrisBlockPuzzleActionToID[board] = action_id;
                    kTetrisBlockPuzzleActionNameToID[kTetrisBlockPuzzleActionName.back()] = action_id;
                    kEachBlockWithLegalActionId[b].push_back(action_id);
                    kActionIdtoBlockId[action_id] = b;
                }
            }
        }
    }
    for (int chance_event_block = 0; chance_event_block < kTetrisChanceEventSize; ++chance_event_block) {
        kTetrisBlockPuzzleActionName.push_back("Gen " + std::string(1, chance_event_block + 'A'));
    }
    kTetrisBlockPuzzleActionName.push_back("Null");
}

void TetrisBlockPuzzleEnv::reset(int seed)
{
    random_.seed(seed_ = seed);
    actions_.clear();
    events_.clear();
    holding_blocks_.clear();
    preview_holding_blocks_.clear();
    reward_ = 0;
    total_reward_ = 0;
    num_holding_block_ = config::env_tetris_block_puzzle_num_holding_block;
    num_preview_holding_block_ = config::env_tetris_block_puzzle_num_preview_holding_block;

    board_.initialize(false);
    for (int i = 1; i <= num_holding_block_; i++) {
        TetrisBlockPuzzleAction action(kTetrisBlockPuzzleActionSize + std::uniform_int_distribution<int>(0, kTetrisChanceEventSize - 1)(random_), Player::kPlayerNone);
        TetrisBlockPuzzleChanceEvent event = TetrisBlockPuzzleChanceEvent::toChanceEvent(action);
        holding_blocks_.push_back(event.block);
        events_.push_back(action);
    }
    for (int i = 1; i <= num_preview_holding_block_; i++) {
        TetrisBlockPuzzleAction action(kTetrisBlockPuzzleActionSize + std::uniform_int_distribution<int>(0, kTetrisChanceEventSize - 1)(random_), Player::kPlayerNone);
        TetrisBlockPuzzleChanceEvent event = TetrisBlockPuzzleChanceEvent::toChanceEvent(action);
        preview_holding_blocks_.push_back(event.block);
        events_.push_back(action);
    }
    std::sort(holding_blocks_.begin(), holding_blocks_.end());
    turn_ = Player::kPlayer1;
    assert(static_cast<int>(holding_blocks_.size()) == num_holding_block_);
    assert(static_cast<int>(preview_holding_blocks_.size()) == num_preview_holding_block_);
}

bool TetrisBlockPuzzleEnv::act(const TetrisBlockPuzzleAction& action, bool with_chance /* = true */)
{
    if (turn_ != Player::kPlayer1) { return false; }
    assert(std::find(holding_blocks_.begin(), holding_blocks_.end(), kActionIdtoBlockId[action.getActionID()]) != holding_blocks_.end());
    Bitboard mask = kTetrisBlockPuzzleActions[action.getActionID()];
    int reward = board_.place(mask) ? board_.crash() : -1;
    if (reward == -1) { return false; }

    int block_id = kActionIdtoBlockId[action.getActionID()];
    holding_blocks_.erase(std::find(holding_blocks_.begin(), holding_blocks_.end(), block_id));
    holding_blocks_.push_back(kTetrisEmptyHoldingBlock);
    assert(static_cast<int>(holding_blocks_.size()) == num_holding_block_);

    actions_.push_back(action);
    reward_ = reward;
    total_reward_ += reward;
    turn_ = Player::kPlayerNone;
    if (with_chance) { actChanceEvent(); }
    return true;
}

bool TetrisBlockPuzzleEnv::actChanceEvent(const TetrisBlockPuzzleAction& action)
{
    if (turn_ != Player::kPlayerNone) { return false; }
    assert(static_cast<int>(holding_blocks_.size()) == num_holding_block_);
    assert(static_cast<int>(holding_blocks_.back()) == kTetrisEmptyHoldingBlock);
    TetrisBlockPuzzleChanceEvent event = TetrisBlockPuzzleChanceEvent::toChanceEvent(action);
    if (num_preview_holding_block_) {
        assert(static_cast<int>(preview_holding_blocks_.size()) == num_preview_holding_block_);
        holding_blocks_.back() = preview_holding_blocks_.front();
        preview_holding_blocks_.pop_front();
        preview_holding_blocks_.push_back(event.block);
    } else {
        holding_blocks_.back() = event.block;
    }
    std::sort(holding_blocks_.begin(), holding_blocks_.end());
    events_.push_back(action);
    turn_ = Player::kPlayer1;
    return true;
}

bool TetrisBlockPuzzleEnv::actChanceEvent()
{
    if (turn_ != Player::kPlayerNone) { return false; }
    assert(static_cast<int>(holding_blocks_.size()) == num_holding_block_);
    assert(holding_blocks_.back() == kTetrisEmptyHoldingBlock);
    TetrisBlockPuzzleAction action(kTetrisBlockPuzzleActionSize + std::uniform_int_distribution<int>(0, kTetrisChanceEventSize - 1)(random_), Player::kPlayerNone);
    return actChanceEvent(action);
}

std::vector<TetrisBlockPuzzleAction> TetrisBlockPuzzleEnv::getLegalActions() const
{
    if (turn_ != Player::kPlayer1) { return {}; }
    std::vector<int> holding_blocks = holding_blocks_;
    std::sort(holding_blocks.begin(), holding_blocks.end());
    holding_blocks.erase(std::unique(holding_blocks.begin(), holding_blocks.end()), holding_blocks.end());
    std::vector<TetrisBlockPuzzleAction> actions;
    for (int block_id : holding_blocks) {
        for (int action_id : kEachBlockWithLegalActionId[block_id]) {
            if (board_.canPlace(kTetrisBlockPuzzleActions[action_id])) {
                actions.push_back(TetrisBlockPuzzleAction(action_id, Player::kPlayer1));
            }
        }
    }
    return actions;
}

std::vector<TetrisBlockPuzzleAction> TetrisBlockPuzzleEnv::getLegalChanceEvents() const
{
    if (turn_ != Player::kPlayerNone) { return {}; }
    std::vector<TetrisBlockPuzzleAction> events;
    for (int block_id = 0; block_id < kTetrisChanceEventSize; ++block_id) {
        events.push_back(TetrisBlockPuzzleChanceEvent::toAction({block_id}));
    }
    return events;
}

float TetrisBlockPuzzleEnv::getChanceEventProbability(const TetrisBlockPuzzleAction& action) const
{
    if (turn_ != Player::kPlayerNone) { return 0; }
    return 1.0 / kTetrisChanceEventSize;
}

bool TetrisBlockPuzzleEnv::isLegalAction(const TetrisBlockPuzzleAction& action) const
{
    return turn_ == Player::kPlayer1 && std::find(holding_blocks_.begin(), holding_blocks_.end(), kActionIdtoBlockId[action.getActionID()]) != holding_blocks_.end() && board_.can_place(kTetrisBlockPuzzleActions[action.getActionID()]) && action.getPlayer() == Player::kPlayer1;
}

bool TetrisBlockPuzzleEnv::isLegalChanceEvent(const TetrisBlockPuzzleAction& action) const
{
    return turn_ == Player::kPlayerNone && action.getPlayer() == Player::kPlayerNone;
}

bool TetrisBlockPuzzleEnv::isTerminal() const
{
    if (turn_ == Player::kPlayerNone) { return false; }
    return getActionHistory().size() >= kMaxActionSteps || getLegalActions().empty();
}

int TetrisBlockPuzzleEnv::getRotateHoldingBlockID(int holding_block_id, utils::Rotation rotation) const
{
    if (holding_block_id == kTetrisEmptyHoldingBlock) { return holding_block_id; }

    assert(holding_block_id >= 0 && holding_block_id < kTetrisChanceEventSize);
    std::vector<Rotation> rotations = {Rotation::kRotationNone, Rotation::kRotation90, Rotation::kRotation180, Rotation::kRotation270, Rotation::kHorizontalRotation, Rotation::kHorizontalRotation90, Rotation::kHorizontalRotation180, Rotation::kHorizontalRotation270};
    if (std::find(rotations.begin(), rotations.end(), rotation) == rotations.end()) {
        throw std::runtime_error("Invalid rotation");
    }
    return kBlocksRotationID[holding_block_id][std::find(rotations.begin(), rotations.end(), rotation) - rotations.begin()];
}

std::vector<float> TetrisBlockPuzzleEnv::getFeatures(utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    std::vector<float> features;
    for (int tile = 0; tile <= 1; ++tile) {
        for (int pos = 0; pos < kTetrisBlockPuzzleBoardSize * kTetrisBlockPuzzleBoardSize; ++pos) {
            features.push_back(board_.get(getRotatePosition(pos, utils::reversed_rotation[static_cast<int>(rotation)])) == tile ? 1.0f : 0.0f);
        }
    }
    std::vector<int> holding_blocks = holding_blocks_;
    std::transform(holding_blocks.begin(), holding_blocks.end(), holding_blocks.begin(), [this, rotation](int block_id) { return getRotateHoldingBlockID(block_id, rotation); });
    for (int i = 0; i < num_holding_block_; i++) {
        for (int block = 0; block < kTetrisChanceEventSize; block++) {
            bool is_option = (block == holding_blocks[i]);
            for (int pos = 0; pos < kTetrisBlockPuzzleBoardSize * kTetrisBlockPuzzleBoardSize; ++pos) {
                features.push_back(is_option ? 1.0f : 0.0f);
            }
        }
    }
    std::deque<int> preview_holding_blocks = preview_holding_blocks_;
    std::transform(preview_holding_blocks.begin(), preview_holding_blocks.end(), preview_holding_blocks.begin(), [this, rotation](int block_id) { return getRotateHoldingBlockID(block_id, rotation); });
    for (int i = 0; i < num_preview_holding_block_; i++) {
        for (int block = 0; block < kTetrisChanceEventSize; block++) {
            bool is_option = (block == preview_holding_blocks[i]);
            for (int pos = 0; pos < kTetrisBlockPuzzleBoardSize * kTetrisBlockPuzzleBoardSize; ++pos) {
                features.push_back(is_option ? 1.0f : 0.0f);
            }
        }
    }
    return features;
}

int TetrisBlockPuzzleEnv::getRotateAction(int action_id, utils::Rotation rotation) const
{
    if (action_id == -1) { return action_id; }
    Bitboard mask = kTetrisBlockPuzzleActions[action_id];
    switch (rotation) {
        case Rotation::kRotationNone:
            mask = mask;
            break;
        case Rotation::kRotation90:
            mask = mask.rotate(1);
            break;
        case Rotation::kRotation180:
            mask = mask.rotate(2);
            break;
        case Rotation::kRotation270:
            mask = mask.rotate(3);
            break;
        case Rotation::kHorizontalRotation:
            mask = mask.flipHorizontal();
            break;
        case Rotation::kHorizontalRotation90:
            mask = mask.flipHorizontal().rotate(1);
            break;
        case Rotation::kHorizontalRotation180:
            mask = mask.flipHorizontal().rotate(2);
            break;
        case Rotation::kHorizontalRotation270:
            mask = mask.flipHorizontal().rotate(3);
            break;
        default:
            break;
    }
    assert(kTetrisBlockPuzzleActionToID.count(mask));
    return kTetrisBlockPuzzleActionToID[mask];
}

std::vector<float> TetrisBlockPuzzleEnv::getActionFeatures(const TetrisBlockPuzzleAction& action, utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    throw std::runtime_error("TetrisBlockPuzzleEnv::getActionFeatures() is not implemented");
}

std::vector<float> TetrisBlockPuzzleEnv::getChanceEventFeatures(const TetrisBlockPuzzleAction& event, utils::Rotation rotation) const
{
    TetrisBlockPuzzleChanceEvent chance_event = TetrisBlockPuzzleChanceEvent().toChanceEvent(event);
    int chance_event_id = chance_event.block;

    std::vector<float> features(kTetrisChanceEventSize * kTetrisBlockPuzzleBoardSize * kTetrisBlockPuzzleBoardSize, 0.0f);
    std::fill(features.begin() + chance_event_id * kTetrisBlockPuzzleBoardSize * kTetrisBlockPuzzleBoardSize, features.begin() + (chance_event_id + 1) * kTetrisBlockPuzzleBoardSize * kTetrisBlockPuzzleBoardSize, 1.0f);
    return features;
}

std::vector<float> TetrisBlockPuzzleEnvLoader::getChance(const int pos, utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    std::vector<float> chance(getChanceEventSize(), 0.0f);
    if (pos < static_cast<int>(action_pairs_.size())) {
        TetrisBlockPuzzleEnv env;
        env.reset(getSeed());
        for (int i = 0; i <= std::min<int>(pos, action_pairs_.size() - 1); ++i) { env.act(action_pairs_[i].first); }
        chance[env.getChanceEventHistory().back().getActionID() - env.getPolicySize()] = 1.0f;
    } else { // absorbing states
        std::fill(chance.begin(), chance.end(), 1.0f / getChanceEventSize());
    }
    return chance;
}

TetrisBlockPuzzleEnv::TetrisBlockPuzzleChanceEvent TetrisBlockPuzzleEnv::TetrisBlockPuzzleChanceEvent::toChanceEvent(const TetrisBlockPuzzleAction& action)
{
    int id = action.getActionID();
    assert(id >= kTetrisBlockPuzzleActionSize && id < kTetrisBlockPuzzleActionSize + kTetrisChanceEventSize);
    return {id - kTetrisBlockPuzzleActionSize};
}

TetrisBlockPuzzleAction TetrisBlockPuzzleEnv::TetrisBlockPuzzleChanceEvent::toAction(const TetrisBlockPuzzleEnv::TetrisBlockPuzzleChanceEvent& event)
{
    assert(event.block >= 0 && event.block < kTetrisChanceEventSize);
    return {kTetrisBlockPuzzleActionSize + event.block, Player::kPlayerNone};
}

std::string TetrisBlockPuzzleEnv::toString() const
{
    std::ostringstream oss;
    std::string white_color = "\033[48;2;255;255;255m";
    std::string asure_color = "\033[48;2;173;216;230m";
    std::string blue_color = "\033[48;2;100;149;237m";
    std::string reset_color = "\033[0m";
    oss << board_ << std::endl;
    oss << "holding blocks: \n";

    for (int i = 0; i < kTetrisBlockPuzzleMaxBlockSize; i++) {
        for (int block_id : holding_blocks_) {
            for (int j = 0; j < kTetrisBlockPuzzleMaxBlockSize; j++) {
                bool contain_block = false;
                if (block_id != kTetrisEmptyHoldingBlock) {
                    for (std::pair<int, int> pos : kBlocksPos[block_id]) {
                        if (pos.first == i && pos.second == j) {
                            contain_block = true;
                            break;
                        }
                    }
                }
                if (contain_block) {
                    oss << blue_color << "  " << reset_color;
                } else {
                    oss << white_color << "  " << reset_color;
                }
            }
            oss << " ";
        }
        oss << "\n";
    }
    oss << "\n";

    if (num_preview_holding_block_) {
        oss << "preview holding blocks: \n";
        for (int i = 0; i < kTetrisBlockPuzzleMaxBlockSize; i++) {
            for (int block_id : preview_holding_blocks_) {
                for (int j = 0; j < kTetrisBlockPuzzleMaxBlockSize; j++) {
                    bool contain_block = false;
                    if (block_id != kTetrisEmptyHoldingBlock) {
                        for (std::pair<int, int> pos : kBlocksPos[block_id]) {
                            if (pos.first == i && pos.second == j) {
                                contain_block = true;
                                break;
                            }
                        }
                    }
                    if (contain_block) {
                        oss << blue_color << "  " << reset_color;
                    } else {
                        oss << asure_color << "  " << reset_color;
                    }
                }
                oss << " ";
            }
            oss << "\n";
        }
        oss << "\n";
    }
    return oss.str();
}

float TetrisBlockPuzzleEnvLoader::calculateNStepValue(const int pos) const
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

std::vector<float> TetrisBlockPuzzleEnvLoader::toDiscreteValue(float value) const
{
    std::vector<float> discrete_value(kTetrisBlockPuzzleDiscreteValueSize, 0.0f);
    int value_floor = floor(value);
    int value_ceil = ceil(value);
    int shift = kTetrisBlockPuzzleDiscreteValueSize / 2;
    int value_floor_shift = std::min(std::max(value_floor + shift, 0), kTetrisBlockPuzzleDiscreteValueSize - 1);
    int value_ceil_shift = std::min(std::max(value_ceil + shift, 0), kTetrisBlockPuzzleDiscreteValueSize - 1);
    if (value_floor == value_ceil) {
        discrete_value[value_floor_shift] = 1.0f;
    } else {
        discrete_value[value_floor_shift] = value_ceil - value;
        discrete_value[value_ceil_shift] = value - value_floor;
    }
    return discrete_value;
}

} // namespace minizero::env::tetrisblockpuzzle
