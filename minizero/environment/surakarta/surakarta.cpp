#include "surakarta.h"
#include "color_message.h"
#include "random.h"
#include "sgf_loader.h"
#include <algorithm>
#include <cctype>
#include <string>

namespace minizero::env::surakarta {

using namespace minizero::utils;

int SurakartaAction::charToPos(char c) const
{
    c = std::toupper(c);
    if ('A' <= c && c <= 'Z' && c != 'I') {
        return c - 'A' - (c > 'I' ? 1 : 0);
    }
    return -1;
}

int SurakartaAction::stringToActionID(const std::vector<std::string>& action_string_args) const
{
    // c1 r1 c2 r2
    // example: 012345
    // example: a10b10

    std::string command_str = action_string_args.back();
    int dest_pos_idx = command_str.find_first_not_of("0123456789", 1);

    int r1_r = std::stoi(command_str.substr(1, dest_pos_idx));
    int r2_r = std::stoi(command_str.substr(dest_pos_idx + 1, (command_str.size() - (dest_pos_idx + 1))));
    int r1 = r1_r - 1;
    int r2 = r2_r - 1;
    int c1 = charToPos(command_str[0]);
    int c2 = charToPos(command_str[dest_pos_idx]);

    if (r1 < 0 || r2 < 0 || c1 < 0 || c2 < 0) {
        return -1; // Let it return illegal action error
    }
    if (r1 >= board_size_ || r2 >= board_size_ || c1 >= board_size_ || c2 >= board_size_) {
        return -1; // Let it return illegal action error
    }
    return coordinateToID(c1, r1, c2, r2);
}

std::string SurakartaAction::actionIDtoString(int action_id) const
{
    int pos = getFromPos(action_id);
    int row = pos / board_size_;
    int col = pos % board_size_;

    int dest_pos = getDestPos(action_id);
    int dest_row = dest_pos / board_size_;
    int dest_col = dest_pos % board_size_;

    char col_c = 'a' + col + ('a' + col >= 'i' ? 1 : 0);
    char dest_col_c = 'a' + dest_col + ('a' + dest_col >= 'i' ? 1 : 0);

    return std::string(1, col_c) + std::to_string(row + 1) + std::string(1, dest_col_c) + std::to_string(dest_row + 1);
}

int SurakartaAction::coordinateToID(int c1, int r1, int c2, int r2) const
{
    int pos = r1 * board_size_ + c1;
    int dest_pos = r2 * board_size_ + c2;
    return pos * (board_size_ * board_size_) + dest_pos;
}

void SurakartaEnv::reset()
{
    turn_ = Player::kPlayer1;
    actions_.clear();
    bitboard_history_.clear();
    bitboard_.reset();
    hashkey_history_.clear();

    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < board_size_; ++col) {
            bitboard_.get(Player::kPlayer1).set(row * board_size_ + col);
        }
    }
    for (int row = board_size_ - 1; row >= board_size_ - 2; --row) {
        for (int col = 0; col < board_size_; ++col) {
            bitboard_.get(Player::kPlayer2).set(row * board_size_ + col);
        }
    }
    bitboard_history_.push_back(bitboard_);
    no_capture_plies_ = 0;

    hash_key_ = computeHashKey();
    hashkey_history_.push_back(hash_key_);
}

bool SurakartaEnv::act(const SurakartaAction& action)
{
    if (!isLegalActionInternal(action, false)) {
        // The act() would be used for the human inferface. The final score of
        // circular/repetition status is various but it rarely effect the final
        // result. We simply accept the circular/repetition action when playing
        // with human. However forbid it in self-play training.
        return false;
    }

    actions_.push_back(action);
    int pos = action.getFromPos();
    int dest_pos = action.getDestPos();

    no_capture_plies_ += 1;
    bitboard_.get(action.getPlayer()).reset(pos);

    if (bitboard_.get(action.nextPlayer()).test(dest_pos)) {
        no_capture_plies_ = 0;
        bitboard_.get(action.nextPlayer()).reset(dest_pos);
    }
    bitboard_.get(action.getPlayer()).set(dest_pos);
    turn_ = action.nextPlayer();
    bitboard_history_.push_back(bitboard_);

    hash_key_ = computeHashKey();
    hashkey_history_.push_back(hash_key_);

    return true;
}

bool SurakartaEnv::act(const std::vector<std::string>& action_string_args)
{
    return act(SurakartaAction(action_string_args));
}

std::vector<SurakartaAction> SurakartaEnv::getLegalActions() const
{
    std::vector<SurakartaAction> actions;
    for (int pos = 0; pos < board_size_ * board_size_ * board_size_ * board_size_; ++pos) {
        SurakartaAction action(pos, turn_);
        if (!isLegalAction(action)) { continue; }
        actions.push_back(action);
    }
    return actions;
}

void SurakartaEnv::createTrajectories()
{
    const int red_coord_index = 2, green_coord_index = 1;
    redline_coord_ = {red_coord_index, board_size_ - red_coord_index - 1};
    greenline_coord_ = {green_coord_index, board_size_ - green_coord_index - 1};
    redline_trajectory_ = createSingleTrajectory(redline_coord_);
    greenline_trajectory_ = createSingleTrajectory(greenline_coord_);
}

std::vector<SurakartaEnv::Point> SurakartaEnv::createSingleTrajectory(std::vector<int> line_coordinates)
{
    assert(line_coordinates.size() == 2);

    // Create four ways for circuit trajectory. On the normal Surakarta game,
    // the circuit trajectory will be at the 2nd line and 3rd line. Here we take
    // take the 2nd line for example.
    std::vector<SurakartaEnv::Point> trajectory;
    int fix_y, fix_x;

    // start to go through trajectory at the a2 position, from left hand side
    // to right hand side
    //
    //     a b c d e f
    // 6 [             ]
    // 5 [             ]
    // 4 [             ]
    // 3 [             ]
    // 2 [ -------------->
    // 1 [             ]
    fix_y = line_coordinates[0];
    for (int i = 0; i < board_size_; ++i) {
        trajectory.push_back(Point(i, fix_y, Direction::kHorizontal));
    }
    trajectory.push_back(Point(-1, -1, Direction::kNone));

    // then from lower side to upper side at the e1 position
    //
    //             ^
    //     a b c d | f
    // 6 [         |   ]
    // 5 [         |   ]
    // 4 [         |   ]
    // 3 [         |   ]
    // 2 [         |   ]
    // 1 [         |   ]
    fix_x = line_coordinates[1];
    for (int i = 0; i < board_size_; ++i) {
        trajectory.push_back(Point(fix_x, i, Direction::kVertical));
    }
    trajectory.push_back(Point(-1, -1, Direction::kNone));

    // then from right hand side to left hand side at the f5 position
    //
    //       a b c d e f
    // 6   [             ]
    // 5 <-------------- ]
    // 4   [             ]
    // 3   [             ]
    // 2   [             ]
    // 1   [             ]
    fix_y = line_coordinates[1];
    for (int i = board_size_ - 1; i >= 0; --i) {
        trajectory.push_back(Point(i, fix_y, Direction::kHorizontal));
    }
    trajectory.push_back(Point(-1, -1, Direction::kNone));

    // then from upper side to lower side at the b6 position
    //
    //     a b c d e f
    // 6 [   |         ]
    // 5 [   |         ]
    // 4 [   |         ]
    // 3 [   |         ]
    // 2 [   |         ]
    // 1 [   |         ]
    //       v
    fix_x = line_coordinates[0];
    for (int i = board_size_ - 1; i >= 0; --i) {
        trajectory.push_back(Point(fix_x, i, Direction::kVertical));
    }
    trajectory.push_back(Point(-1, -1, Direction::kNone));

    return trajectory;
}

int SurakartaEnv::findStartTrajectoryIndex(Point target, std::vector<SurakartaEnv::Point> trajectory, bool same_direction) const
{
    size_t trajectory_size = trajectory.size();
    for (size_t idx = 0; idx < trajectory_size; ++idx) {
        if (!(trajectory[idx] == target)) { continue; }
        if (same_direction && !(trajectory[idx].dir == target.dir)) { continue; }
        return idx;
    }
    return -1;
}

std::vector<int> SurakartaEnv::findNeighbors(int x, int y, std::vector<SurakartaEnv::Point> trajectory, std::vector<int> colorline) const
{
    Point center_point(x, y);

    int cc0 = colorline[0];
    int cc1 = colorline[1];
    std::vector<SurakartaEnv::Point> cross_points = {Point(cc0, cc0), Point(cc0, cc1), Point(cc1, cc0), Point(cc1, cc1)};

    bool on_cross_point = false;
    for (auto cp : cross_points) {
        if (center_point == cp) {
            on_cross_point = true;
            break;
        }
    }

    if (on_cross_point) {
        Point up_point(x, y + 1, Direction::kVertical);
        Point down_point(x, y - 1, Direction::kVertical);
        Point right_point(x + 1, y, Direction::kHorizontal);
        Point left_point(x - 1, y, Direction::kHorizontal);

        int up_index = findStartTrajectoryIndex(up_point, trajectory, true);
        int down_index = findStartTrajectoryIndex(down_point, trajectory, true);
        int right_index = findStartTrajectoryIndex(right_point, trajectory, true);
        int left_index = findStartTrajectoryIndex(left_point, trajectory, true);

        return {up_index, down_index, right_index, left_index};
    } else {
        int point_index = findStartTrajectoryIndex(center_point, trajectory, false);
        int up_index = -1, down_index = -1, right_index = -1, left_index = -1;
        if (x == colorline[0] || x == colorline[1]) {
            up_index = point_index;
            down_index = point_index;
        }
        if (y == colorline[0] || y == colorline[1]) {
            right_index = point_index;
            left_index = point_index;
        }
        return {up_index, down_index, right_index, left_index};
    }
}

bool SurakartaEnv::runCircuit(int pos, int dest_pos, Player next_player, std::vector<SurakartaEnv::Point> trajectory, std::vector<int> colorline) const
{
    if (pos == dest_pos) { return false; }
    int y = pos / board_size_;
    int x = pos % board_size_;

    // Find four possible capture ways.
    std::vector<int> start_index = findNeighbors(x, y, trajectory, colorline);

    for (size_t i = 0; i < start_index.size(); ++i) {
        size_t j = 0;
        if (start_index[i] != -1) {
            int curr_index = start_index[i];
            int stride = 1; // positive offset
            Point curr_point = trajectory[curr_index];
            if ((curr_point.x == colorline[0] && i == 0) || (curr_point.x == colorline[1] && i == 1)) {
                stride = -1; // negative offset
            }
            if ((curr_point.y == colorline[0] && i == 3) || (curr_point.y == colorline[1] && i == 2)) {
                stride = -1; // negative offset
            }
            if (curr_point == Point(x, y)) {
                curr_index = curr_index + stride;
            }
            bool already_in_cycle = false;

            while (j < trajectory.size() - 1) {
                if (curr_index >= static_cast<int>(trajectory.size())) {
                    curr_index = 0;
                }
                if (curr_index < 0) {
                    curr_index = trajectory.size() - 1;
                }

                Point curr_p = trajectory[curr_index];
                if (curr_p == Point(-1, -1)) {
                    already_in_cycle = true;
                } else {
                    int tmp_pos = curr_p.y * board_size_ + curr_p.x;
                    if (tmp_pos == dest_pos && getPlayerAtBoardPos(tmp_pos) == next_player && already_in_cycle) {
                        return true;
                    } else if (getPlayerAtBoardPos(tmp_pos) != Player::kPlayerNone && curr_p != Point(x, y)) {
                        break;
                    }
                }
                curr_index = curr_index + stride;
                ++j;
            }
        }
    }
    return false;
}

bool SurakartaEnv::isLegalAction(const SurakartaAction& action) const
{
    // Always forbid circular/repetition status in self-play training and tree search.
    return isLegalActionInternal(action, true);
}

bool SurakartaEnv::isLegalActionInternal(const SurakartaAction& action, bool forbid_circular) const
{
    assert(action.getActionID() >= 0 && action.getActionID() < getPolicySize());
    int pos = action.getFromPos();
    int dest_pos = action.getDestPos();

    if (getPlayerAtBoardPos(pos) != action.getPlayer()) { return false; }
    if (action.getPlayer() != getTurn()) { return false; }

    // current pos x and y
    int y = pos / board_size_;
    int x = pos % board_size_;

    // dest pos x and y
    int dest_y = dest_pos / board_size_;
    int dest_x = dest_pos % board_size_;
    bool pos_in_line = false, dest_pos_in_line = false;

    // Check whether it is normal move. Notice normal move can not capture any piece.
    if ((dest_y == y && abs(x - dest_x) == 1) || (abs(y - dest_y) == 1 && dest_x == x) || (abs(y - dest_y) == 1 && abs(x - dest_x) == 1)) {
        if (getPlayerAtBoardPos(dest_pos) == Player::kPlayerNone) {
            return testCircular(action, forbid_circular);
        }
    }

    // Check whether the from position and dest position are both on the 'red' line.
    // If they are both on the 'red' line, we search circuit trajectory to find the possible capture way.
    for (size_t k = 0; k < redline_coord_.size(); ++k) {
        if (y == redline_coord_[k] || x == redline_coord_[k]) {
            pos_in_line = true;
        }
        if (dest_y == redline_coord_[k] || dest_x == redline_coord_[k]) {
            dest_pos_in_line = true;
        }
    }
    if (pos_in_line && dest_pos_in_line) {
        if (runCircuit(pos, dest_pos, action.nextPlayer(), redline_trajectory_, redline_coord_)) {
            return testCircular(action, forbid_circular);
        }
    }

    pos_in_line = false, dest_pos_in_line = false;

    // Check whether the from position and dest position are both on the 'green' line.
    // If they are both on the 'green' line, we search circuit trajectory to find the possible capture way.
    for (size_t k = 0; k < greenline_coord_.size(); ++k) {
        if (y == greenline_coord_[k] || x == greenline_coord_[k]) {
            pos_in_line = true;
        }
        if (dest_y == greenline_coord_[k] || dest_x == greenline_coord_[k]) {
            dest_pos_in_line = true;
        }
    }
    if (pos_in_line && dest_pos_in_line) {
        if (runCircuit(pos, dest_pos, action.nextPlayer(), greenline_trajectory_, greenline_coord_)) {
            return testCircular(action, forbid_circular);
        }
    }
    return false;
}

bool SurakartaEnv::testCircular(const SurakartaAction& action, bool forbid_circular) const
{
    if (forbid_circular) {
        // Check whether the the board position after the action is already in the history.
        // We simply forbid it rather than finish the game due to adapt for different rules.
        return !isCircularAction(action);
    }
    return true;
}

bool SurakartaEnv::isTerminal() const
{
    if (no_capture_plies_ >= minizero::config::env_surakarta_no_capture_plies) {
        // Achive Fifty-move rule.
        return true;
    }

    int player1_piece_count = bitboard_.get(Player::kPlayer1).count();
    int player2_piece_count = bitboard_.get(Player::kPlayer2).count();
    if (player1_piece_count == 0 || player2_piece_count == 0) {
        // One side player has no piece. The game is over.
        return true;
    }
    return getLegalActions().empty();
}

float SurakartaEnv::getEvalScore(bool is_resign /* = false */) const
{
    Player result = (is_resign ? getNextPlayer(turn_, kSurakartaNumPlayer) : eval());
    switch (result) {
        case Player::kPlayer1: return 1.0f;
        case Player::kPlayer2: return -1.0f;
        default: return 0.0f;
    }
}

std::vector<float> SurakartaEnv::getFeatures(utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    /* 18 channels:
        0~15. own/opponent position
        16. black's turn
        17. white's turn
    */

    int past_moves = std::min(8, static_cast<int>(bitboard_history_.size()));
    int spatial = board_size_ * board_size_;
    std::vector<float> features(getNumInputChannels() * spatial, 0.f);
    int last_idx = bitboard_history_.size() - 1;

    // 0 ~ 15
    for (int c = 0; c < 2 * past_moves; c += 2) {
        const SurakartaBitboard& own_bitboard = bitboard_history_[last_idx - (c / 2)].get(turn_);
        const SurakartaBitboard& opp_bitboard = bitboard_history_[last_idx - (c / 2)].get(getNextPlayer(turn_, kSurakartaNumPlayer));
        for (int pos = 0; pos < spatial; ++pos) {
            int rotation_pos = getRotatePosition(pos, utils::reversed_rotation[static_cast<int>(rotation)]);
            features[pos + c * spatial] = (own_bitboard.test(rotation_pos) ? 1.0f : 0.0f);
            features[pos + (c + 1) * spatial] = (opp_bitboard.test(rotation_pos) ? 1.0f : 0.0f);
        }
    }

    // 16 ~ 17
    for (int pos = 0; pos < spatial; ++pos) {
        features[pos + 16 * spatial] = static_cast<float>(turn_ == Player::kPlayer1);
        features[pos + 17 * spatial] = static_cast<float>(turn_ == Player::kPlayer2);
    }
    return features;
}

std::vector<float> SurakartaEnv::getActionFeatures(const SurakartaAction& action, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    throw std::runtime_error{"SurakartaEnv::getActionFeatures() is not implemented"};
}

std::string SurakartaEnv::getCoordinateString() const
{
    std::ostringstream oss;
    oss << "  ";
    for (int i = 0; i < board_size_; ++i) {
        char c = 'A' + i + ('A' + i >= 'I' ? 1 : 0);
        oss << " " + std::string(1, c) + " ";
    }
    oss << "   ";
    return oss.str();
}

std::string SurakartaEnv::toString() const
{
    int from_pos = -1, dest_pos = -1;
    if (!actions_.empty()) {
        from_pos = actions_[actions_.size() - 1].getFromPos();
        dest_pos = actions_[actions_.size() - 1].getDestPos();
    }

    std::ostringstream oss;
    oss << " " << getCoordinateString() << std::endl;
    for (int row = board_size_ - 1; row >= 0; --row) {
        oss << (row >= 9 ? "" : " ") << row + 1 << " ";
        for (int col = 0; col < board_size_; ++col) {
            int pos = row * board_size_ + col;
            TextColor bc = TextColor::kYellow;
            Player color = getPlayerAtBoardPos(pos);

            if (((row == redline_coord_[0] || row == redline_coord_[1]) && (col == greenline_coord_[0] || col == greenline_coord_[1])) ||
                ((col == redline_coord_[0] || col == redline_coord_[1]) && (row == greenline_coord_[0] || row == greenline_coord_[1]))) {
                bc = TextColor::kPurple;
            } else if (row == redline_coord_[0] || row == redline_coord_[1] || col == redline_coord_[0] || col == redline_coord_[1]) {
                bc = TextColor::kRed;
            } else if (row == greenline_coord_[0] || row == greenline_coord_[1] || col == greenline_coord_[0] || col == greenline_coord_[1]) {
                bc = TextColor::kGreen;
            }
            if (color == Player::kPlayer1) {
                oss << getColorText(" O", TextType::kNormal, TextColor::kBlack, bc);
            } else if (color == Player::kPlayer2) {
                oss << getColorText(" X", TextType::kNormal, TextColor::kWhite, bc);
            } else {
                oss << getColorText(" .", TextType::kNormal, TextColor::kBlack, bc);
            }
            oss << getColorText((from_pos == pos || dest_pos == pos ? "<" : " "), TextType::kBold, TextColor::kBlack, bc);
        }
        oss << (row >= 9 ? "" : " ") << row + 1 << std::endl;
    }
    oss << " " << getCoordinateString() << std::endl;
    return oss.str();
}

bool SurakartaEnv::isCircularAction(const SurakartaAction& action) const
{
    // Return true if the board position after the action is already in the history.
    GamePair<SurakartaBitboard> bitboard = bitboard_;
    int pos = action.getFromPos();
    int dest_pos = action.getDestPos();
    bitboard.get(action.getPlayer()).reset(pos);
    bitboard.get(action.nextPlayer()).reset(dest_pos);
    bitboard.get(action.getPlayer()).set(dest_pos);

    SurakartaHashKey hash_key = computeHashKey(bitboard, action.nextPlayer());

    return find(hashkey_history_.rbegin(), hashkey_history_.rend(), hash_key) != hashkey_history_.rend();
}

Player SurakartaEnv::eval() const
{
    // The player who has the most pieces is winner.
    int player1_piece_count = bitboard_.get(Player::kPlayer1).count();
    int player2_piece_count = bitboard_.get(Player::kPlayer2).count();
    if (player1_piece_count > player2_piece_count) {
        return Player::kPlayer1;
    } else if (player2_piece_count > player1_piece_count) {
        return Player::kPlayer2;
    } else {
        return Player::kPlayerNone;
    }
}

Player SurakartaEnv::getPlayerAtBoardPos(int position) const
{
    if (bitboard_.get(Player::kPlayer1).test(position)) {
        return Player::kPlayer1;
    } else if (bitboard_.get(Player::kPlayer2).test(position)) {
        return Player::kPlayer2;
    }
    return Player::kPlayerNone;
}

SurakartaHashKey SurakartaEnv::computeHashKey() const
{
    return computeHashKey(bitboard_, turn_);
}

SurakartaHashKey SurakartaEnv::computeHashKey(const GamePair<SurakartaBitboard>& bitboard, Player turn) const
{
    auto hashCat = [](std::uint64_t hash, std::uint64_t x) -> std::uint64_t {
        x = 0xfad0d7f2fbb059f1ULL * (x + 0xbaad41cdcb839961ULL) + 0x7acec0050bf82f43ULL * ((x >> 31) + 0xd571b3a92b1b2755ULL);
        hash ^= 0x299799adf0d95defULL + x + (hash << 6) + (hash >> 2);
        return hash;
    };

    std::hash<SurakartaBitboard> hash_fn;
    std::uint64_t play1_hash = hash_fn(bitboard.get(Player::kPlayer1));
    std::uint64_t play2_hash = hash_fn(bitboard.get(Player::kPlayer2));
    std::uint64_t color_hash = turn == Player::kPlayer1 ? 0x1234567887564321ULL : 0;
    std::uint64_t hash = hashCat(play1_hash, play2_hash) ^ color_hash;
    return hash;
}

std::vector<float> SurakartaEnvLoader::getActionFeatures(const int pos, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    throw std::runtime_error{"SurakartaEnvLoader::getActionFeatures() is not implemented"};
}

} // namespace minizero::env::surakarta
