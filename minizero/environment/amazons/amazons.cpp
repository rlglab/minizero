#include "amazons.h"
#include "color_message.h"
#include "random.h"
#include "sgf_loader.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <utility>

namespace minizero::env::amazons {

using namespace minizero::utils;

std::unordered_map<int, int> kAmazonsPolicySize;
std::unordered_map<int, std::vector<int>> kAmazonsActionIdSplit;
std::unordered_map<int, std::vector<int>> kAmazonsActionIdToCoord;

void initialize()
{
    // calculate each action id and store it in cache.
    // the action id is divided into two parts:
    // the first part represents all possible moves of the amazons from the old position to the new position (with moves that
    // go out of the board's boundaries removed), and the second part represents all possible actions for placing the arrow.
    const std::array<std::array<int, 2>, 9> direct = {{{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {0, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}}};
    for (int curr_bsize = kMinAmazonsBoardSize; curr_bsize <= kMaxAmazonsBoardSize; ++curr_bsize) {
        int pos = 0;
        int cum_action_id = 0;
        std::vector<int> action_id_split(1 + curr_bsize * curr_bsize * 9);
        std::vector<int> action_id_coord;
        action_id_split[0] = 0;
        for (int y = 0; y < curr_bsize; ++y) {
            for (int x = 0; x < curr_bsize; ++x) {
                std::array<int, 9> dir_move_length = getDirectionMoveLength(curr_bsize, x, y);
                for (int dir = 0; dir < 9; ++dir) {
                    cum_action_id += dir_move_length[dir];
                    action_id_split[++pos] = cum_action_id;
                    int dest_x = x;
                    int dest_y = y;
                    for (int move_length = 1; move_length <= dir_move_length[dir]; ++move_length) {
                        dest_x += direct[dir][0];
                        dest_y += direct[dir][1];
                        int encode_coord = (x << 18) | (y << 12) | (dest_x << 6) | dest_y;
                        action_id_coord.emplace_back(encode_coord);
                    }
                }
            }
        }
        for (int y = 0; y < curr_bsize; ++y) {
            for (int x = 0; x < curr_bsize; ++x) {
                int encode_coord = (1 << 24) | (x << 6) | y;
                action_id_coord.emplace_back(encode_coord);
            }
        }
        kAmazonsPolicySize[curr_bsize] = cum_action_id + curr_bsize * curr_bsize;
        kAmazonsActionIdSplit[curr_bsize] = action_id_split;
        kAmazonsActionIdToCoord[curr_bsize] = action_id_coord;
        assert(static_cast<int>(action_id_coord.size()) == kAmazonsPolicySize[curr_bsize]);
    }
}

std::array<int, 9> getDirectionMoveLength(const int board_size, const int x, const int y)
{
    int move_length = board_size - 1;
    std::array<int, 9> move_dir_size = {
        std::min(x, y),
        y,
        std::min(move_length - x, y),
        x,
        0,
        move_length - x,
        std::min(x, move_length - y),
        move_length - y,
        std::min(move_length - x, move_length - y)};
    return move_dir_size;
}

AmazonsAction::AmazonsAction(const std::vector<std::string>& action_string_args)
{
    // action string format
    // move amazons: <FROM_X> <FROM_Y> <DEST_X> <DEST_Y>
    //     e.g. A5B4
    // place arrows: <DEST_X> <DEST_Y>
    //     e.g. E1
    assert(action_string_args.size() == 2);
    assert(action_string_args[0].size() == 1);
    player_ = charToPlayer(action_string_args[0][0]);
    assert(static_cast<int>(player_) > 0 && static_cast<int>(player_) <= kAmazonsNumPlayer); // assume kPlayer1 == 1, kPlayer2 == 2, ...

    int board_size = minizero::config::env_board_size;
    std::array<int, 4> coordinate = {-1, -1, -1, -1};
    std::string action_string = action_string_args[1];
    std::transform(action_string.begin(), action_string.end(), action_string.begin(), ::toupper);
    int index = -1;
    int end_index = coordinate.size() - 1;
    for (char ch : action_string) {
        if (isdigit(ch)) {
            coordinate[index] = (coordinate[index] + 1) * 10 + (ch - '1');
        } else if (index >= end_index) {
            break;
        } else if (isalpha(ch)) {
            coordinate[index + 1] = ch - 'A' + (ch > 'I' ? -1 : 0);
            index += 2;
        }
    }
    int pos = coordinate[1] * board_size + coordinate[0];
    if (index == 1) {
        action_id_ = kAmazonsActionIdSplit[board_size].back() + pos;
    } else if (index == 3) {
        int ox = coordinate[2] - coordinate[0];
        int oy = coordinate[3] - coordinate[1];
        int move_length = std::max(std::abs(ox), std::abs(oy));
        int direction = ((ox > 0) - (ox < 0) + 1) + ((oy > 0) - (oy < 0) + 1) * 3;
        action_id_ = kAmazonsActionIdSplit[board_size][pos * 9 + direction] + move_length - 1;
    } else {
        action_id_ = -1;
    }
}

std::string AmazonsAction::toConsoleString() const
{
    // encoded coordinates use 30 bits.
    // move amazons: (000000 xxxxxx yyyyyy XXXXXX YYYYYY)
    //     xxxxxx and yyyyyy is from position and XXXXXX and YYYYYY is dest position.
    // place arrows: (000001 000000 000000 XXXXXX YYYYYY)
    //     XXXXXX and YYYYYY is placed position.
    std::ostringstream oss;
    int encode_coord = kAmazonsActionIdToCoord[minizero::config::env_board_size][action_id_];
    int from_x = (encode_coord >> 18) & 0b111111;
    int from_y = (encode_coord >> 12) & 0b111111;
    int dest_x = (encode_coord >> 6) & 0b111111;
    int dest_y = encode_coord & 0b111111;
    if (((encode_coord >> 24) & 0b1) == 0) { oss << static_cast<char>(from_x + 'A' + (from_x >= 8)) << from_y + 1; }
    oss << static_cast<char>(dest_x + 'A' + (dest_x >= 8)) << dest_y + 1;
    return oss.str();
}

void AmazonsEnv::reset()
{
    assert(board_size_ >= kMinAmazonsBoardSize);
    assert(board_size_ <= kMaxAmazonsBoardSize);
    turn_ = Player::kPlayer1;
    actions_.clear();

    bitboard_.reset();
    int interval = board_size_ / 3 - (board_size_ % 3 == 0 ? 1 : 0);
    std::vector<int> player_init_position = {
        xyToPosition(0, interval),
        xyToPosition(interval, 0),
        xyToPosition(board_size_ - 1 - interval, 0),
        xyToPosition(board_size_ - 1, interval)};
    for (auto& init_pos : player_init_position) {
        bitboard_.setPlayer(Player::kPlayer1, init_pos);
    }
    std::vector<int> opponent_init_position = {
        xyToPosition(0, board_size_ - 1 - interval),
        xyToPosition(interval, board_size_ - 1),
        xyToPosition(board_size_ - 1 - interval, board_size_ - 1),
        xyToPosition(board_size_ - 1, board_size_ - 1 - interval)};
    for (auto& init_pos : opponent_init_position) {
        bitboard_.setPlayer(Player::kPlayer2, init_pos);
    }

    winner_ = Player::kPlayerNone;
    actions_mask_.resize(getPolicySize());
    updateLegalAction();

    bitboard_history_.clear();
    // initial board
    bitboard_history_.push_back(bitboard_);
    bitboard_history_.push_back(bitboard_);
}

bool AmazonsEnv::act(const AmazonsAction& action)
{
    if (!isLegalAction(action)) { return false; }
    if (actions_.size() % 2 == 0) {
        // move amazons
        bitboard_.resetPlayer(turn_, getStartPosition(action.getActionID()));
        bitboard_.setPlayer(turn_, getEndPosition(action.getActionID()));
    } else {
        // place arrows
        bitboard_.setArrow(action.getActionID() - kAmazonsActionIdSplit[board_size_].back());
    }
    actions_.push_back(action);
    turn_ = action.nextPlayer(actions_.size());
    bitboard_history_.push_back(bitboard_);
    updateLegalAction();
    return true;
}

std::vector<AmazonsAction> AmazonsEnv::getLegalActions() const
{
    std::vector<AmazonsAction> actions;
    for (int action_id = 0; action_id < getPolicySize(); ++action_id) {
        AmazonsAction action(action_id, turn_);
        if (!isLegalAction(action)) { continue; }
        actions.push_back(action);
    }
    return actions;
}

bool AmazonsEnv::isLegalAction(const AmazonsAction& action) const
{
    int action_id = action.getActionID();
    Player player = action.getPlayer();
    assert(action_id >= 0 && action_id < getPolicySize());
    assert(player == Player::kPlayer1 || player == Player::kPlayer2);
    return (player == turn_ && actions_mask_.test(action_id));
}

float AmazonsEnv::getEvalScore(bool is_resign /* = false */) const
{
    Player result = (is_resign ? getNextPlayer(turn_, kAmazonsNumPlayer) : winner_);
    switch (result) {
        case Player::kPlayer1: return 1.0f;
        case Player::kPlayer2: return -1.0f;
        default: return 0.0f;
    }
}

std::vector<float> AmazonsEnv::getFeatures(utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    /* 28 channels:
        0~15.  own/opponent position for last 8 turns
        16~23. arrow position for last 8 turns
        24. black's move amazon turn
        25. black's shoots arrow turn
        26. white's move amazon turn
        27. white's shoots arrow turn
    */

    int past_moves = std::min(16, static_cast<int>(bitboard_history_.size()));
    int spatial = board_size_ * board_size_;
    std::vector<float> features(getNumInputChannels() * spatial, 0.f);
    int last_idx = bitboard_history_.size() - 1;

    // 0 ~ 15
    for (int move_amazon_turn = (bitboard_history_.size() - 1) % 2, c = 0; move_amazon_turn < past_moves; move_amazon_turn += 2, c += 2) {
        const AmazonsBitboard& own_bitboard = bitboard_history_[last_idx - move_amazon_turn].get(turn_);
        const AmazonsBitboard& opponent_bitboard = bitboard_history_[last_idx - move_amazon_turn].get(getNextPlayer(turn_, kAmazonsNumPlayer));
        for (int pos = 0; pos < spatial; ++pos) {
            int rotation_pos = getRotatePosition(pos, utils::reversed_rotation[static_cast<int>(rotation)]);
            features[pos + c * spatial] = (own_bitboard.test(rotation_pos) ? 1.0f : 0.0f);
            features[pos + (c + 1) * spatial] = (opponent_bitboard.test(rotation_pos) ? 1.0f : 0.0f);
        }
    }

    // 16 ~ 23
    for (int shoots_arrow_turn = bitboard_history_.size() % 2, c = 0; shoots_arrow_turn < past_moves; shoots_arrow_turn += 2, ++c) {
        const AmazonsBoard& current_board = bitboard_history_[last_idx - shoots_arrow_turn];
        for (int pos = 0; pos < spatial; ++pos) {
            int rotation_pos = getRotatePosition(pos, utils::reversed_rotation[static_cast<int>(rotation)]);
            features[pos + (c + 16) * spatial] = (current_board.isArrow(rotation_pos) ? 1.0f : 0.0f);
        }
    }

    // 24 ~ 27
    for (int pos = 0; pos < spatial; ++pos) {
        features[pos + 24 * spatial] = static_cast<float>(actions_.size() % 4 == 0);
        features[pos + 25 * spatial] = static_cast<float>(actions_.size() % 4 == 1);
        features[pos + 26 * spatial] = static_cast<float>(actions_.size() % 4 == 2);
        features[pos + 27 * spatial] = static_cast<float>(actions_.size() % 4 == 3);
    }
    return features;
}

std::vector<float> AmazonsEnv::getActionFeatures(const AmazonsAction& action, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    throw std::runtime_error{"AmazonsEnv::getActionFeatures() is not implemented"};
}

std::string AmazonsEnv::toString() const
{
    int last_move_start_pos = -1, last_move_end_pos = -1;
    if (actions_.size() >= 1) {
        if (actions_.size() % 2 == 1) {
            last_move_start_pos = getStartPosition(actions_.back().getActionID());
        } else {
            last_move_start_pos = getEndPosition(actions_[actions_.size() - 2].getActionID());
        }
        last_move_end_pos = getEndPosition(actions_.back().getActionID());
    }
    bool space_prefix;
    std::unordered_map<Player, std::pair<std::string, TextColor>> player_to_text_color({{Player::kPlayerNone, {".", TextColor::kBlack}},
                                                                                        {Player::kPlayer1, {"O", TextColor::kBlack}},
                                                                                        {Player::kPlayer2, {"O", TextColor::kWhite}}});
    std::ostringstream oss;
    oss << getCoordinateString() << std::endl;
    for (int row = board_size_ - 1; row >= 0; --row) {
        oss << getColorText((row + 1 < 10 ? " " : "") + std::to_string(row + 1), TextType::kBold, TextColor::kBlack, TextColor::kYellow);
        for (int col = 0; col < board_size_; ++col) {
            int pos = row * board_size_ + col;
            if (pos == last_move_start_pos || pos == last_move_end_pos) {
                oss << getColorText(">", TextType::kBold, TextColor::kRed, TextColor::kYellow);
                space_prefix = false;
            } else {
                space_prefix = true;
            }
            if (bitboard_.isArrow(pos)) {
                oss << getColorText((space_prefix ? " #" : "#"), TextType::kBold, TextColor::kBlue, TextColor::kYellow);
            } else {
                const std::pair<std::string, TextColor> text_pair = player_to_text_color[bitboard_.getPlayer(pos)];
                oss << getColorText((space_prefix ? " " : "") + text_pair.first, TextType::kBold, text_pair.second, TextColor::kYellow);
            }
        }
        oss << getColorText(" " + std::to_string(row + 1) + (row + 1 < 10 ? " " : ""), TextType::kBold, TextColor::kBlack, TextColor::kYellow);
        oss << std::endl;
    }
    oss << getCoordinateString() << std::endl;
    return oss.str();
}

void AmazonsEnv::updateLegalAction()
{
    winner_ = Player::kPlayerNone;
    actions_mask_.reset();
    int last_position = (actions_.size() > 0 ? getEndPosition(actions_.back().getActionID()) : -1);
    bool is_move_action = (actions_.size() % 2 == 0);
    const AmazonsBitboard& player_bitboard = bitboard_.get(turn_);
    for (int pos = 0; pos < board_size_ * board_size_; ++pos) {
        if ((is_move_action && player_bitboard.test(pos)) || (!is_move_action && last_position == pos)) {
            for (int dir = 0; dir < 9; ++dir) {
                int action_id = kAmazonsActionIdSplit[board_size_][pos * 9 + dir];
                int end_action_id = kAmazonsActionIdSplit[board_size_][pos * 9 + dir + 1];
                for (; action_id < end_action_id; ++action_id) {
                    if (!bitboard_.isEmpty(getEndPosition(action_id))) { break; }
                    if (is_move_action) {
                        // legal action of move amazons
                        actions_mask_.set(action_id);
                    } else {
                        // legal action of place arrows
                        actions_mask_.set(kAmazonsActionIdSplit[board_size_].back() + getEndPosition(action_id));
                    }
                }
            }
        }
    }
    // current player no any legal actions
    if (actions_mask_.none()) { winner_ = getNextPlayer(turn_, kAmazonsNumPlayer); }
}

std::string AmazonsEnv::getCoordinateString() const
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

std::vector<float> AmazonsEnvLoader::getActionFeatures(const int pos, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    throw std::runtime_error{"AmazonsEnvLoader::getActionFeatures() is not implemented"};
}

} // namespace minizero::env::amazons
