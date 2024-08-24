#include "connect6.h"
#include "color_message.h"
#include "random.h"
#include "sgf_loader.h"
#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>

namespace minizero::env::connect6 {

using namespace minizero::utils;

void Connect6Env::reset()
{
    winner_ = Player::kPlayerNone;
    turn_ = Player::kPlayer1;
    actions_.clear();
    bitboard_.reset();
    bitboard_history_.clear();
}

bool Connect6Env::act(const Connect6Action& action)
{
    if (!isLegalAction(action)) { return false; }
    actions_.push_back(action);
    bitboard_.get(action.getPlayer()).set(action.getActionID());
    turn_ = action.nextPlayer(actions_.size());
    winner_ = updateWinner(action);
    bitboard_history_.push_back(bitboard_);
    return true;
}

bool Connect6Env::act(const std::vector<std::string>& action_string_args)
{
    return act(Connect6Action(action_string_args));
}

std::vector<Connect6Action> Connect6Env::getLegalActions() const
{
    std::vector<Connect6Action> actions;
    for (int pos = 0; pos < board_size_ * board_size_; ++pos) {
        Connect6Action action(pos, turn_);
        if (!isLegalAction(action)) { continue; }
        actions.push_back(action);
    }
    return actions;
}

bool Connect6Env::isLegalPlayer(const Player player) const
{
    return player == turn_;
}

bool Connect6Env::isLegalAction(const Connect6Action& action) const
{
    int action_id = action.getActionID();
    Player player = action.getPlayer();

    assert(action_id >= 0 && action_id < board_size_ * board_size_);
    assert(player == Player::kPlayer1 || player == Player::kPlayer2);

    return (action_id >= 0 &&
            action_id < board_size_ * board_size_ &&
            getPlayerAtBoardPos(action_id) == Player::kPlayerNone);
}

bool Connect6Env::isTerminal() const
{
    // terminal: any player wins or board is filled
    return (winner_ != Player::kPlayerNone || actions_.size() >= board_size_ * board_size_);
}

float Connect6Env::getEvalScore(bool is_resign /*= false*/) const
{
    Player result = (is_resign ? getNextPlayer(turn_, kConnect6NumPlayer) : winner_);
    switch (result) {
        case Player::kPlayer1: return 1.0f;
        case Player::kPlayer2: return -1.0f;
        default: return 0.0f;
    }
}

Connect6Bitboard Connect6Env::scanThreadSpace(Player p, int target_cnt) const
{
    int spatial = board_size_ * board_size_;

    assert(target_cnt > 0 && target_cnt < kConnect6NumWinConnectStone);

    Connect6Bitboard space;
    Connect6Bitboard buf_space;
    std::vector<int> color_cnt(static_cast<int>(Player::kPlayerSize));
    std::vector<std::array<int, 2>> direct = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}};

    for (auto& dd : direct) {
        for (int pos = 0; pos < spatial; ++pos) {
            buf_space.reset();
            int x = pos % board_size_;
            int y = pos / board_size_;

            if (dd[0] < 0) {
                x = board_size_ - 1 - x;
            }

            int bound_x = x + (kConnect6NumWinConnectStone - 1) * dd[0];
            int bound_y = y + (kConnect6NumWinConnectStone - 1) * dd[1];
            if (bound_x < 0 || bound_x >= board_size_ || bound_y < 0 || bound_y >= board_size_) {
                continue;
            }

            std::fill(color_cnt.begin(), color_cnt.end(), 0);

            for (int i = 0; i < kConnect6NumWinConnectStone; ++i) {
                int xx = x + i * dd[0];
                int yy = y + i * dd[1];

                int apos = yy * board_size_ + xx;
                Player color = getPlayerAtBoardPos(apos);
                if (color == Player::kPlayerNone) {
                    buf_space.set(apos);
                }
                ++color_cnt[static_cast<int>(color)];
            }
            if (color_cnt[static_cast<int>(p)] == target_cnt && color_cnt[static_cast<int>(getNextPlayer(p, kConnect6NumPlayer))] == 0) {
                // threat space
                space |= buf_space;
            }
        }
    }
    return space;
}

std::vector<float> Connect6Env::getFeatures(utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    /* 24 channels:
        0~15.  own/opponent position for last 8 turns
        16~19. threat space
        20. black's turn, two moves left
        21. black's turn, one move left
        22. white's turn, two moves left
        23. white's turn, one move left
    */

    int past_moves = std::min(8, static_cast<int>(bitboard_history_.size()));
    int spatial = board_size_ * board_size_;
    std::vector<float> features(getNumInputChannels() * spatial, 0.f);
    int last_idx = bitboard_history_.size() - 1;

    // 0 ~ 15
    for (int c = 0; c < 2 * past_moves; c += 2) {
        const Connect6Bitboard& own_bitboard = bitboard_history_[last_idx - (c / 2)].get(turn_);
        const Connect6Bitboard& opponent_bitboard = bitboard_history_[last_idx - (c / 2)].get(getNextPlayer(turn_, kConnect6NumPlayer));
        for (int pos = 0; pos < spatial; ++pos) {
            int rotation_pos = getRotatePosition(pos, utils::reversed_rotation[static_cast<int>(rotation)]);
            features[pos + c * spatial] = (own_bitboard.test(rotation_pos) ? 1.0f : 0.0f);
            features[pos + (c + 1) * spatial] = (opponent_bitboard.test(rotation_pos) ? 1.0f : 0.0f);
        }
    }

    // 16 ~ 19
    Connect6Bitboard space1 = scanThreadSpace(turn_, 5);
    Connect6Bitboard space2 = scanThreadSpace(turn_, 4);
    Connect6Bitboard space3 = scanThreadSpace(getNextPlayer(turn_, kConnect6NumPlayer), 5);
    Connect6Bitboard space4 = scanThreadSpace(getNextPlayer(turn_, kConnect6NumPlayer), 4);

    for (int pos = 0; pos < spatial; ++pos) {
        int rotation_pos = getRotatePosition(pos, utils::reversed_rotation[static_cast<int>(rotation)]);
        features[pos + 16 * spatial] = (space1.test(rotation_pos) ? 1.0f : 0.0f);
        features[pos + 17 * spatial] = (space2.test(rotation_pos) ? 1.0f : 0.0f);
        features[pos + 18 * spatial] = (space3.test(rotation_pos) ? 1.0f : 0.0f);
        features[pos + 19 * spatial] = (space4.test(rotation_pos) ? 1.0f : 0.0f);
    }

    // 20 ~ 23
    auto it = features.begin() + 20 * spatial;
    int turn_idx = 2 * (turn_ == Player::kPlayer2) + ((actions_.size() + 1) % 2);
    std::fill(it + turn_idx * spatial, it + (turn_idx + 1) * spatial, 1.0f);
    return features;
}

std::vector<float> Connect6Env::getActionFeatures(const Connect6Action& action, utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    std::vector<float> action_features(board_size_ * board_size_, 0.0f);
    action_features[getRotateAction(action.getActionID(), rotation)] = 1.0f;
    return action_features;
}

std::string Connect6Env::toString() const
{
    int last_move_pos = -1, last2_move_pos = -1;
    if (actions_.size() >= 1) { last_move_pos = actions_.back().getActionID(); }
    if (actions_.size() >= 2) { last2_move_pos = actions_[actions_.size() - 2].getActionID(); }

    std::unordered_map<Player, std::pair<std::string, TextColor>> player_to_text_color({{Player::kPlayerNone, {".", TextColor::kBlack}},
                                                                                        {Player::kPlayer1, {"O", TextColor::kBlack}},
                                                                                        {Player::kPlayer2, {"O", TextColor::kWhite}}});
    std::ostringstream oss;
    oss << getCoordinateString() << std::endl;
    for (int row = board_size_ - 1; row >= 0; --row) {
        oss << getColorText((row + 1 < 10 ? " " : "") + std::to_string(row + 1), TextType::kBold, TextColor::kBlack, TextColor::kYellow);
        for (int col = 0; col < board_size_; ++col) {
            int pos = row * board_size_ + col;
            const std::pair<std::string, TextColor> text_pair = player_to_text_color[getPlayerAtBoardPos(pos)];
            if (pos == last_move_pos) {
                oss << getColorText(">", TextType::kBold, TextColor::kRed, TextColor::kYellow);
                oss << getColorText(text_pair.first, TextType::kBold, text_pair.second, TextColor::kYellow);
            } else if (pos == last2_move_pos) {
                oss << getColorText(">", TextType::kNormal, TextColor::kRed, TextColor::kYellow);
                oss << getColorText(text_pair.first, TextType::kBold, text_pair.second, TextColor::kYellow);
            } else {
                oss << getColorText(" " + text_pair.first, TextType::kBold, text_pair.second, TextColor::kYellow);
            }
        }
        oss << getColorText(" " + std::to_string(row + 1) + (row + 1 < 10 ? " " : ""), TextType::kBold, TextColor::kBlack, TextColor::kYellow);
        oss << std::endl;
    }
    oss << getCoordinateString() << std::endl;
    return oss.str();
}

Player Connect6Env::updateWinner(const Connect6Action& action)
{
    if (calculateNumberOfConnection(action, {1, 0}) + calculateNumberOfConnection(action, {-1, 0}) - 1 >= kConnect6NumWinConnectStone) { return action.getPlayer(); }  // row
    if (calculateNumberOfConnection(action, {0, 1}) + calculateNumberOfConnection(action, {0, -1}) - 1 >= kConnect6NumWinConnectStone) { return action.getPlayer(); }  // column
    if (calculateNumberOfConnection(action, {1, 1}) + calculateNumberOfConnection(action, {-1, -1}) - 1 >= kConnect6NumWinConnectStone) { return action.getPlayer(); } // diagonal (right-up to left-down)
    if (calculateNumberOfConnection(action, {1, -1}) + calculateNumberOfConnection(action, {-1, 1}) - 1 >= kConnect6NumWinConnectStone) { return action.getPlayer(); } // diagonal (left-up to right-down)
    return Player::kPlayerNone;
}

int Connect6Env::calculateNumberOfConnection(const Connect6Action& action, std::pair<int, int> direction)
{
    int start_pos = action.getActionID();
    const Connect6Bitboard& player_bitboard = bitboard_.get(action.getPlayer());
    int count = 0;
    int x = start_pos % board_size_;
    int y = start_pos / board_size_;
    while (x >= 0 && x < board_size_ && y >= 0 && y < board_size_) {
        if (!player_bitboard.test(y * board_size_ + x)) { break; }
        ++count;
        x += direction.first;
        y += direction.second;
    }
    return count;
}

std::string Connect6Env::getCoordinateString() const
{
    std::ostringstream oss;
    oss << "  ";
    for (int i = 0; i < board_size_; ++i) {
        char c = 'A' + i + ('A' + i >= 'I' ? 1 : 0);
        oss << " " + std::string(1, c);
    }
    oss << "   ";
    return getColorText(oss.str(), TextType::kBold, TextColor::kBlack, TextColor::kYellow);
}

Player Connect6Env::getPlayerAtBoardPos(int position) const
{
    if (bitboard_.get(Player::kPlayer1).test(position)) {
        return Player::kPlayer1;
    } else if (bitboard_.get(Player::kPlayer2).test(position)) {
        return Player::kPlayer2;
    }
    return Player::kPlayerNone;
}

std::vector<float> Connect6EnvLoader::getActionFeatures(const int pos, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    const Connect6Action& action = action_pairs_[pos].first;
    std::vector<float> action_features(board_size_ * board_size_, 0.0f);
    int action_id = ((pos < static_cast<int>(action_pairs_.size())) ? getRotateAction(action.getActionID(), rotation) : utils::Random::randInt() % action_features.size());
    action_features[action_id] = 1.0f;
    return action_features;
}

} // namespace minizero::env::connect6
