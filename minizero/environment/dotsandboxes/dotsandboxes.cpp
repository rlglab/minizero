#include "dotsandboxes.h"
#include "color_message.h"
#include "random.h"
#include "sgf_loader.h"
#include <iomanip>

namespace minizero::env::dotsandboxes {

using namespace minizero::utils;

int DotsAndBoxesAction::coordToActionID(const std::string& coord, int board_size) const
{
    auto parse = [](std::string& buf, int size) {
        int width = size;
        int x = std::toupper(buf[0]) - 'A' + (std::toupper(buf[0]) > 'I' ? -1 : 0);
        int y = atoi(buf.substr(1).c_str()) - 1;
        if (x >= size || y >= size) { return -1; }
        return x + width * y;
    };
    std::string tmp = coord;
    std::transform(tmp.begin(), tmp.end(), tmp.begin(), ::toupper);

    int size = coord.size();
    int dots_size = board_size + 1;
    std::string from_buf, dest_buf;
    if (size == 4) {
        // example, A1A2
        from_buf = tmp.substr(0, 2);
        dest_buf = tmp.substr(2);
    } else if (size == 5) {
        // example, A9A10 or A10A9
        int mid = std::isdigit(tmp[2]) ? 3 : 2;
        from_buf = tmp.substr(0, mid);
        dest_buf = tmp.substr(mid);
    } else if (size == 6) {
        // example, A10A11
        from_buf = tmp.substr(0, 3);
        dest_buf = tmp.substr(3);
    } else {
        // invalid
        return -1;
    }

    int from_pos = parse(from_buf, dots_size);
    int dest_pos = parse(dest_buf, dots_size);
    if (from_pos == -1 || dest_pos == -1) {
        return -1;
    }
    if (from_pos > dest_pos) {
        // make sure the frompos is lower
        std::swap(from_pos, dest_pos);
    }
    int diff = dest_pos - from_pos;
    if (diff != 1 && diff != dots_size) {
        // diff = 1, means the line is horizontal
        // diff = dots_size, means the line is vertical
        // otherwise, the line is invalid
        return -1;
    }

    // shift coordinate
    int level = from_pos / dots_size;
    from_pos -= (level * dots_size);
    dest_pos -= (level * dots_size);
    return (diff == 1 ? from_pos : dest_pos - 1) + level * (2 * dots_size - 1);
}

std::string DotsAndBoxesAction::getCoordString(int board_size /* = minizero::config::env_board_size */) const
{
    auto parse = [](int pos, int size) {
        int x = pos % size;
        int y = pos / size;
        std::ostringstream oss;
        oss << static_cast<char>(x + 'A' + (x >= 8)) << y + 1;
        return oss.str();
    };
    std::ostringstream oss;
    int dots_size = board_size + 1;
    int x2 = action_id_ % (2 * board_size + 1);
    int y2 = action_id_ / (2 * board_size + 1);
    int shift = y2 * dots_size;
    if (x2 < board_size) {
        int from_pos = x2 + shift;
        int dest_pos = x2 + 1 + shift;
        oss << parse(from_pos, dots_size) << parse(dest_pos, dots_size);
    } else {
        int from_pos = x2 - board_size + shift;
        int dest_pos = x2 + 1 + shift;
        oss << parse(from_pos, dots_size) << parse(dest_pos, dots_size);
    }
    return oss.str();
}

void DotsAndBoxesEnv::reset()
{
    assert(kMaxDotsAndBoxesBoardSize >= board_size_);

    board_size_width_ = board_size_;
    board_size_height_ = board_size_;
    full_board_size_width_ = 2 * board_size_width_ + 1;
    full_board_size_height_ = 2 * board_size_height_ + 1;

    turn_ = Player::kPlayer1;
    current_player_continue_ = false;
    actions_.clear();

    /*
       the example of board 2x2.

       - L - L -
       L B L B L
       - L - L -
       L B L B L
       - L - L -

       B: box
       L: line
       -: dot

       the 'L' are all legal moves.
       the 'B' and '-' have no action id.
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
    board_history_.push_back(board_);

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

        if (!isOnBoard(xx, yy)) { continue; }
        int apos = xyToPosition(xx, yy);
        if (shouldMark(apos)) {
            // current player get this box.
            // the current player can continue the next move.
            board_[apos] = static_cast<Grid>(turn_);
            current_player_continue_ = true;
        }
    }

    board_history_.push_back(board_);
    if (current_player_continue_) {
        continue_history_.push_back(turn_);
    } else {
        continue_history_.push_back(Player::kPlayerNone);
        turn_ = getNextPlayer(turn_, kDotsAndBoxesNumPlayer);
    }
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
    for (int action_id = 0; action_id < getPolicySize(); ++action_id) {
        DotsAndBoxesAction action(action_id, turn_);
        if (isLegalAction(action)) { actions.emplace_back(action); }
    }
    return actions;
}

bool DotsAndBoxesEnv::isLegalAction(const DotsAndBoxesAction& action) const
{
    if (action.getActionID() < 0 || action.getActionID() >= getPolicySize()) { return false; }
    if (action.getPlayer() != turn_) { return false; }
    return board_[lineIdxToPos(action.getActionID())] == Grid::kGridNoLine;
}

bool DotsAndBoxesEnv::isTerminal() const
{
    // terminal: the board is filled
    return std::find(board_.begin(), board_.end(), Grid::kGridNoPlayer) == board_.end();
}

float DotsAndBoxesEnv::getEvalScore(bool is_resign /* = false */) const
{
    int play1_score = 0;
    int play2_score = 0;
    for (Grid grid : board_) {
        // the player who gets more boxes wins
        if (grid == Grid::kGridPlayer1) {
            play1_score += 1;
        } else if (grid == Grid::kGridPlayer2) {
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

std::vector<float> DotsAndBoxesEnv::getFeatures(utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
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
    int past_moves = std::min(8, static_cast<int>(board_history_.size()));
    int spatial = full_board_size_width_ * full_board_size_height_;
    std::vector<float> features(getNumInputChannels() * spatial, 0.f);

    // 0 ~ 87
    for (int c = 0; c < 11 * past_moves; c += 11) {
        const Board& curr_board = board_history_[board_history_.size() - 1 - c / 11];
        Player continue_player = continue_history_[continue_history_.size() - 1 - c / 11];
        Player own_player = (turn_ == Player::kPlayer1 ? Player::kPlayer1 : Player::kPlayer2);
        Player opponent_player = (turn_ == Player::kPlayer2 ? Player::kPlayer1 : Player::kPlayer2);
        Grid own_grid_player = (turn_ == Player::kPlayer1 ? Grid::kGridPlayer1 : Grid::kGridPlayer2);
        Grid opponent_grid_player = (turn_ == Player::kPlayer2 ? Grid::kGridPlayer1 : Grid::kGridPlayer2);
        for (int pos = 0; pos < spatial; ++pos) {
            int rotation_pos = getRotatePosition(pos, utils::reversed_rotation[static_cast<int>(rotation)]);
            Grid grid = curr_board.at(rotation_pos);
            if (grid == own_grid_player) {
                features[pos + c * spatial] = 1.0f;
            } else if (grid == opponent_grid_player) {
                features[pos + (c + 1) * spatial] = 1.0f;
            } else if (grid == Grid::kGridNoPlayer) {
                features[pos + (c + 2) * spatial] = 1.0f;
            } else if (grid == Grid::kGridNoLine) {
                features[pos + (c + 3) * spatial] = 1.0f;
            } else if (grid == Grid::kGridLine) {
                features[pos + (c + 4) * spatial] = 1.0f;
            }
            // number of line in box
            int x = pos % full_board_size_width_;
            int y = pos / full_board_size_width_;
            if ((x % 2 == 1) && (y % 2 == 1)) {
                int lines_count = 0;
                for (int k = 0; k < 4; ++k) {
                    int xx = x + direction_[k][0];
                    int yy = y + direction_[k][1];
                    int line_pos = xyToPosition(xx, yy);
                    int rotation_line_pos = getRotatePosition(line_pos, utils::reversed_rotation[static_cast<int>(rotation)]);
                    if (curr_board.at(rotation_line_pos) == Grid::kGridNoLine) { continue; }
                    ++lines_count;
                }
                switch (lines_count) {
                    case 0: break;
                    case 1: features[pos + (c + 5) * spatial] = 1.0f; break;
                    case 2: features[pos + (c + 6) * spatial] = 1.0f; break;
                    case 3: features[pos + (c + 7) * spatial] = 1.0f; break;
                    case 4: features[pos + (c + 8) * spatial] = 1.0f; break;
                }
            }
            // continue turn
            features[pos + (c + 9) * spatial] = (continue_player == own_player ? 1.0f : 0.0f);
            features[pos + (c + 10) * spatial] = (continue_player == opponent_player ? 1.0f : 0.0f);
        }
    }

    // 88 ~ 89
    for (int pos = 0; pos < spatial; ++pos) {
        features[pos + 88 * spatial] = static_cast<float>(turn_ == Player::kPlayer1);
        features[pos + 89 * spatial] = static_cast<float>(turn_ == Player::kPlayer2);
    }
    return features;
}

std::vector<float> DotsAndBoxesEnv::getActionFeatures(const DotsAndBoxesAction& action, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    throw std::runtime_error{"DotsAndBoxesEnv::getActionFeatures() is not implemented"};
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

    if (full_board_size_height_ / 2 + 1 < 10) { space -= 1; }
    oss << getXCoordString(space);

    for (int y = full_board_size_height_ - 1; y >= 0; --y) {
        oss << std::setw(space) << (y % 2 == 0 ? (std::to_string(y / 2 + 1)) : " ") << ' ';
        for (int x = 0; x < full_board_size_width_; ++x) {
            switch (board_[xyToPosition(x, y)]) {
                case Grid::kGridNoPlayer: oss << ' '; break;
                case Grid::kGridPlayer1: oss << getColorText("B", text_type, white, red); break;
                case Grid::kGridPlayer2: oss << getColorText("W", text_type, white, blue); break;
                case Grid::kGridNoLine: oss << (y % 2 == 0 ? ' ' : ' '); break;
                case Grid::kGridLine: oss << (y % 2 == 0 ? '-' : '|'); break;
                case Grid::kGridDots: oss << '*'; break;
                default: oss << 'E'; break;
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
            if (x != full_board_size_width_ - 1) { oss << ' '; }
        }
        oss << '\n';
    }
    return oss.str();
}

bool DotsAndBoxesEnv::shouldMark(const int pos) const
{
    // return true if the box can be marked at this position.
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

std::vector<float> DotsAndBoxesEnvLoader::getActionFeatures(const int pos, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    throw std::runtime_error{"DotsAndBoxesEnvLoader::getActionFeatures() is not implemented"};
}

} // namespace minizero::env::dotsandboxes
