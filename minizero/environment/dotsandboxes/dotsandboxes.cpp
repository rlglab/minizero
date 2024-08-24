#include "dotsandboxes.h"
#include "color_message.h"
#include "random.h"
#include "sgf_loader.h"
#include <iomanip>
#include <iostream>

namespace minizero::env::dotsandboxes {

using namespace minizero::utils;

void DotsAndBoxesEnv::reset()
{
    if (kMaxDotsAndBoxesBoardSize < board_size_) {
        throw std::runtime_error{"The env_board_size can not be larger than kMaxDotsAndBoxesBoardSize."};
    }
    assert(kMaxDotsAndBoxesBoardSize >= board_size_);

    // TODO: Support for rectangle board
    board_size_width_ = board_size_;
    board_size_height_ = board_size_;
    full_board_size_width_ = 2 * board_size_width_ + 1;
    full_board_size_height_ = 2 * board_size_height_ + 1;

    turn_ = Player::kPlayer1;
    current_player_continue_ = false;
    legal_player_rates_.clear();
    actions_.clear();

    /*
       The example of board 2x2

       - L - L -
       L B L B L
       - L - L -
       L B L B L
       - L - L -

       B: box
       L: line
       -: dot

       The 'L' are all legal moves.
    */
    board_.resize(full_board_size_width_ * full_board_size_height_);
    fill(board_.begin(), board_.end(), Grid::kGridNoPlayer);

    for (int y = 0; y < full_board_size_height_; ++y) {
        for (int x = 0; x < full_board_size_width_; ++x) {
            int pos = xyToPosition(x, y);
            if (x % 2 == 0 && y % 2 == 0) {
                board_[pos] = Grid::kGridDots;
            } else if (x % 2 == 0 || y % 2 == 0) {
                board_[pos] = Grid::kGridNoLine;
            } else {
                board_[pos] = Grid::kGridNoPlayer;
            }
        }
    }

    continue_history_.clear();
    continue_history_.push_back(Player::kPlayerNone);

    board_history_.clear();
    board_history_.push_back(std::make_shared<Board>(board_));

    direction_.resize(4);
    direction_[0] = {1, 0};
    direction_[1] = {0, 1};
    direction_[2] = {-1, 0};
    direction_[3] = {0, -1};
}

bool DotsAndBoxesEnv::act(const DotsAndBoxesAction& action)
{
    if (!isLegalAction(action)) { return false; }
    int pos = lineIdxToPos(action.getActionID());

    current_player_continue_ = false;

    board_[pos] = Grid::kGridLine;
    int x = pos % full_board_size_width_;
    int y = pos / full_board_size_width_;

    for (int k = 0; k < 4; ++k) {
        int xx = x + direction_[k][0];
        int yy = y + direction_[k][1];

        if (!isOnBoard(xx, yy)) {
            continue;
        }
        int apos = xyToPosition(xx, yy);
        if (shouldMark(apos)) {
            // We get this area. The next player can only
            // play pass move.
            board_[apos] = static_cast<Grid>(turn_);
            current_player_continue_ = true;
        }
    }

    board_history_.push_back(std::make_shared<Board>(board_));
    if (current_player_continue_) {
        continue_history_.push_back(turn_);
    } else {
        continue_history_.push_back(Player::kPlayerNone);
        turn_ = getNextPlayer(turn_, kDotsAndBoxesNumPlayer);
    }
    // std::cerr << playerToChar(turn_) << " ";
    actions_.push_back(action);
    return true;
}

bool DotsAndBoxesEnv::act(const std::vector<std::string>& action_string_args)
{
    // example of play command
    //     play b A1A2  # equal to A2A1
    //     play w A1B1  # equal to B1A1
    //     play b PASS
    return act(DotsAndBoxesAction(action_string_args, board_size_width_));
}

std::vector<DotsAndBoxesAction> DotsAndBoxesEnv::getLegalActions() const
{
    std::vector<DotsAndBoxesAction> actions;
    for (int id = 0; id < getPolicySize(); ++id) {
        DotsAndBoxesAction action(id, turn_);
        if (isLegalAction(action)) {
            actions.push_back(action);
        }
    }
    return actions;
}

bool DotsAndBoxesEnv::isLegalPlayer(const Player player) const
{
    return player == turn_;
}

bool DotsAndBoxesEnv::isLegalAction(const DotsAndBoxesAction& action) const
{
    if (action.getActionID() < 0 || action.getActionID() >= getPolicySize()) return false;
    return board_[lineIdxToPos(action.getActionID())] == Grid::kGridNoLine;
}

bool DotsAndBoxesEnv::isTerminal() const
{
    return std::find(board_.begin(), board_.end(), Grid::kGridNoPlayer) == board_.end();
}

float DotsAndBoxesEnv::getEvalScore(bool is_resign /*= false*/) const
{
    int play1_score = 0;
    int play2_score = 0;
    for (Grid v : board_) {
        if (v == Grid::kGridPlayer1) {
            play1_score += 1;
        } else if (v == Grid::kGridPlayer2) {
            play2_score += 1;
        }
    }
    Player score_winner = Player::kPlayerNone;
    if (play1_score > play2_score) {
        score_winner = Player::kPlayer1;
    } else if (play1_score < play2_score) {
        score_winner = Player::kPlayer2;
    }

    Player result = is_resign ? getNextPlayer(turn_, kDotsAndBoxesNumPlayer) : score_winner;
    switch (result) {
        case Player::kPlayer1: return 1.0f;
        case Player::kPlayer2: return -1.0f;
        default: return 0.0f;
    }
}

std::vector<float> DotsAndBoxesEnv::getFeatures(utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    /*
      90 channels:
            0~87. history board for last 8 turns:
                0. own boxes
                1. opponent boxes
                2. empty boxes
                3. lines not exist
                4. lines exist
                5. one line in box
                6. two lines in box
                7. three lines in box
                8. four lines in box
                9. own continue turn
                10. opponent continue turn
           88. black turn
           89. white turn
    */
    int channels = getNumInputChannels();
    int per_move_channels = 11;
    int last_history_steps = 8;
    int past_moves = std::min(last_history_steps, static_cast<int>(board_history_.size()));
    int spatial = full_board_size_width_ * full_board_size_height_;
    std::vector<float> vFeatures(channels * spatial, 0.f);
    auto it = vFeatures.begin();

    for (int c = 0; c < per_move_channels * past_moves; c += per_move_channels) {
        std::shared_ptr<Board> curr_board = *(board_history_.rbegin() + c / per_move_channels);
        Player continue_player = *(continue_history_.rbegin() + c / per_move_channels);

        auto black_it = it + (turn_ == Player::kPlayer1 ? 0 : spatial);
        auto white_it = it + (turn_ == Player::kPlayer2 ? 0 : spatial);
        auto empty_it = it + 2 * spatial;
        auto no_line_it = it + 3 * spatial;
        auto line_it = it + 4 * spatial;
        auto one_it = it + 5 * spatial;
        auto two_it = it + 6 * spatial;
        auto three_it = it + 7 * spatial;
        auto four_it = it + 8 * spatial;
        auto continue_black_it = it + (turn_ == Player::kPlayer1 ? 9 * spatial : 10 * spatial);
        auto continue_white_it = it + (turn_ == Player::kPlayer2 ? 9 * spatial : 10 * spatial);

        for (int pos = 0; pos < spatial; ++pos) {
            int rotation_pos = getRotatePosition(pos, utils::reversed_rotation[static_cast<int>(rotation)]);

            switch (curr_board->at(rotation_pos)) {
                case Grid::kGridPlayer1:
                    black_it[pos] = 1.0f;
                    break;
                case Grid::kGridPlayer2:
                    white_it[pos] = 1.0f;
                    break;
                case Grid::kGridNoPlayer:
                    empty_it[pos] = 1.0f;
                    break;
                case Grid::kGridNoLine:
                    no_line_it[pos] = 1.0f;
                    break;
                case Grid::kGridLine:
                    line_it[pos] = 1.0f;
                    break;
                default:
                    break;
            }

            int x = pos % full_board_size_width_;
            int y = pos / full_board_size_width_;
            if ((x % 2 == 1) && (y % 2 == 1)) {
                int lines_count = 0;
                for (int k = 0; k < 4; ++k) {
                    int xx = x + direction_[k][0];
                    int yy = y + direction_[k][1];
                    int line_pos = xyToPosition(xx, yy);
                    int rotation_line_pos = getRotatePosition(line_pos, utils::reversed_rotation[static_cast<int>(rotation)]);
                    if (curr_board->at(rotation_line_pos) == Grid::kGridNoLine) {
                        continue;
                    }
                    ++lines_count;
                }
                switch (lines_count) {
                    case 0:
                        break;
                    case 1:
                        one_it[pos] = 1.0f;
                        break;
                    case 2:
                        two_it[pos] = 1.0f;
                        break;
                    case 3:
                        three_it[pos] = 1.0f;
                        break;
                    case 4:
                        four_it[pos] = 1.0f;
                        break;
                }
            }
        }

        switch (continue_player) {
            case Player::kPlayer1:
                std::fill(continue_black_it, continue_black_it + spatial, 1.0f);
                break;
            case Player::kPlayer2:
                std::fill(continue_white_it, continue_white_it + spatial, 1.0f);
                break;
            default:
                break;
        }
        it += per_move_channels * spatial;
    }

    it = vFeatures.begin() + (per_move_channels * last_history_steps) * spatial;
    std::fill(it + 0 * spatial, it + 1 * spatial, static_cast<float>(turn_ == Player::kPlayer1));
    std::fill(it + 1 * spatial, it + 2 * spatial, static_cast<float>(turn_ == Player::kPlayer2));
    return vFeatures;
}

std::vector<float> DotsAndBoxesEnv::getActionFeatures(const DotsAndBoxesAction& action, utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    (void)rotation;
    int num_channels = getNumActionFeatureChannels();
    std::vector<float> action_features(num_channels * full_board_size_width_ * full_board_size_height_, 0.0f);
    // throw std::runtime_error{"getActionFeatures(), not implemented"};
    int pos = lineIdxToPos(action.getActionID());
    // action_features[getRotateAction(lineID, rotation)] = 1.0f;
    action_features[pos] = 1.0f;
    return action_features;
}

std::string DotsAndBoxesEnv::toString() const
{
    auto getXCoordString = [this](int space) {
        std::ostringstream oss;
        oss << std::setw(space + 1) << ' ';
        for (int x = 0; x < full_board_size_width_; ++x) {
            int i = x / 2;
            oss << static_cast<char>(x % 2 == 0 ? (i + 'A' + (i >= 8)) : ' ') << ' ';
        }
        oss << "\n";
        return oss.str();
    };

    auto text_type = minizero::utils::TextType::kBold;
    auto white = minizero::utils::TextColor::kWhite;
    auto red = minizero::utils::TextColor::kRed;
    auto blue = minizero::utils::TextColor::kBlue;
    int space = 2;
    std::ostringstream oss;

    if (full_board_size_height_ / 2 + 1 < 10) {
        space -= 1;
    }
    oss << getXCoordString(space);

    for (int y = full_board_size_height_ - 1; y >= 0; --y) {
        oss << std::setw(space) << (y % 2 == 0 ? (std::to_string(y / 2 + 1)) : " ") << ' ';
        for (int x = 0; x < full_board_size_width_; ++x) {
            switch (board_[xyToPosition(x, y)]) {
                case Grid::kGridNoPlayer:
                    oss << ' ';
                    break;
                case Grid::kGridPlayer1:
                    oss << getColorText("B", text_type, white, red);
                    break;
                case Grid::kGridPlayer2:
                    oss << getColorText("W", text_type, white, blue);
                    break;
                case Grid::kGridNoLine:
                    oss << (y % 2 == 0 ? ' ' : ' ');
                    break;
                case Grid::kGridLine:
                    oss << (y % 2 == 0 ? '-' : '|');
                    break;
                case Grid::kGridDots:
                    oss << '*';
                    break;
                default:
                    oss << 'E';
                    break;
            }
            oss << ' ';
        }
        oss << std::setw(space) << (y % 2 == 0 ? (std::to_string(y / 2 + 1)) : " ");
        oss << '\n';
    }
    oss << getXCoordString(space);

    return oss.str();
}

std::string DotsAndBoxesEnv::toStringDebug() const
{
    std::ostringstream oss;

    for (int y = full_board_size_height_ - 1; y >= 0; --y) {
        for (int x = 0; x < full_board_size_width_; ++x) {
            switch (board_[xyToPosition(x, y)]) {
                case Grid::kGridNoPlayer: oss << ' '; break;
                case Grid::kGridPlayer1: oss << 'X'; break;
                case Grid::kGridPlayer2: oss << 'O'; break;
                case Grid::kGridNoLine: oss << (y % 2 == 0 ? ' ' : ' '); break;
                case Grid::kGridLine: oss << (y % 2 == 0 ? '-' : '|'); break;
                case Grid::kGridDots: oss << '*'; break;
                default: oss << 'E'; break;
            }
            if (x != full_board_size_width_ - 1) oss << ' ';
        }
        oss << '\n';
    }
    return oss.str();
}

std::vector<int> DotsAndBoxesEnv::getAdjacentLinesPos(int pos) const
{
    assert(board_[pos] == Grid::kGridNoPlayer || board_[pos] == Grid::kGridPlayer1 || board_[pos] == Grid::kGridPlayer2);

    int x = pos % full_board_size_width_;
    int y = pos / full_board_size_width_;

    std::vector<int> adjacent_lines(4);
    for (int k = 0; k < 4; ++k) {
        int xx = x + direction_[k][0];
        int yy = y + direction_[k][1];
        adjacent_lines[k] = xyToPosition(xx, yy);
    }
    return adjacent_lines;
}

bool DotsAndBoxesEnv::shouldMark(const int pos) const
{
    // Return true if we can mark the box.
    if (board_[pos] != Grid::kGridNoPlayer) {
        return false;
    }
    std::vector<int> line_pos_list = getAdjacentLinesPos(pos);
    for (int p : line_pos_list) {
        if (board_[p] == Grid::kGridNoLine) {
            return false;
        }
    }
    return true;
}

int DotsAndBoxesEnv::getNumGirdLines() const
{
    return board_size_width_ * (board_size_height_ + 1) + (board_size_width_ + 1) * board_size_height_;
}

bool DotsAndBoxesEnv::isOnBoard(const int x, const int y) const
{
    return x >= 0 && x < full_board_size_width_ && y >= 0 && y < full_board_size_height_;
}

int DotsAndBoxesEnv::xyToPosition(const int x, const int y) const
{
    return y * full_board_size_width_ + x;
}

int DotsAndBoxesEnv::xyToLineIdx(const int x, const int y) const
{
    return y * (board_size_width_ + 1) + x - (y + 1) / 2;
}

int DotsAndBoxesEnv::posToLineIdx(int pos) const
{
    int x = pos % full_board_size_width_;
    int y = pos / full_board_size_width_;
    return xyToLineIdx(x, y);
}

int DotsAndBoxesEnv::lineIdxToPos(int idx) const
{
    // The line index is action id.
    return 2 * idx + 1;
}

std::vector<float> DotsAndBoxesEnvLoader::getActionFeatures(const int pos, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    (void)rotation;
    const DotsAndBoxesAction& action = action_pairs_[pos].first;
    int num_channels = getNumActionFeatureChannels();
    int full_board_size_width = 2 * board_size_ + 1;
    int full_board_size_height = 2 * board_size_ + 1;
    std::vector<float> action_features(num_channels * full_board_size_width * full_board_size_height, 0.0f);
    // throw std::runtime_error{"getActionFeatures(), not implemented"};
    int action_id = ((pos < static_cast<int>(action_pairs_.size())) ? action.getActionID() : utils::Random::randInt() % getPolicySize());
    int action_pos = 2 * action_id + 1;
    action_features[action_pos] = 1.0f;
    return action_features;
}

int DotsAndBoxesEnvLoader::getNumGirdLines() const
{
    int board_size_width = board_size_;
    int board_size_height = board_size_;
    return board_size_width * (board_size_height + 1) + (board_size_width + 1) * board_size_height;
}

} // namespace minizero::env::dotsandboxes
