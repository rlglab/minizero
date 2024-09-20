#include "breakthrough.h"
#include <algorithm>
#include <stdexcept>

namespace minizero::env::breakthrough {

std::vector<int> kBreakthroughPolicySize;
std::vector<std::string> kBreakthroughIdxToStr;
std::vector<int> kBreakthroughIdxToFromIdx;
std::vector<int> kBreakthroughIdxToDestIdx;
std::vector<std::string> kBreakthroughSquareToStr;
std::unordered_map<std::string, int> kBreakthroughStrToIdx;

void initialize()
{
    std::vector<std::string> remaining_moves;
    kBreakthroughPolicySize = std::vector<int>(kBreakthroughMaxBoardSize + 1, 0);

    // Generate the string for each position.
    for (int y = 0; y < kBreakthroughMaxBoardSize; ++y) {
        for (int x = 0; x < kBreakthroughMaxBoardSize; ++x) {
            char xx = x + 'a';
            char yy = y + '1';
            std::ostringstream oss;
            oss << xx << yy;
            kBreakthroughSquareToStr.emplace_back(oss.str());
        }
    }

    std::vector<std::array<int, 2>> direct = {{-1, 1}, {0, 1}, {1, 1}, {-1, -1}, {0, -1}, {1, -1}};

    // Now we generate the move index table for each move and adapt to differenet
    // board size without initializing it again. First we compute all possible
    // legal moves.
    for (int y = 0; y < kBreakthroughMaxBoardSize; ++y) {
        for (int x = 0; x < kBreakthroughMaxBoardSize; ++x) {
            for (auto& dd : direct) {
                int xx = x + dd[0];
                int yy = y + dd[1];
                if (xx >= 0 && yy >= 0 && xx < kBreakthroughMaxBoardSize && yy < kBreakthroughMaxBoardSize) {
                    int from = x + kBreakthroughMaxBoardSize * y;
                    int dest = xx + kBreakthroughMaxBoardSize * yy;
                    remaining_moves.emplace_back(kBreakthroughSquareToStr[from] + kBreakthroughSquareToStr[dest]);
                }
            }
        }
    }

    // Find 'all' possibel legal moves for current board size, then saving the
    // number of legal moves in the table.
    for (int curr_bsize = kBreakthroughMinBoardSize; curr_bsize <= kBreakthroughMaxBoardSize; ++curr_bsize) {
        for (int y = 0; y < curr_bsize; ++y) {
            for (int x = 0; x < curr_bsize; ++x) {
                for (auto& dd : direct) {
                    int xx = x + dd[0];
                    int yy = y + dd[1];
                    if (xx >= 0 && yy >= 0 && xx < curr_bsize && yy < curr_bsize) {
                        int from = x + kBreakthroughMaxBoardSize * y;
                        int dest = xx + kBreakthroughMaxBoardSize * yy;
                        std::string move = kBreakthroughSquareToStr[from] + kBreakthroughSquareToStr[dest];
                        auto it = std::find(std::begin(remaining_moves), std::end(remaining_moves), move);
                        if (it != std::end(remaining_moves)) {
                            remaining_moves.erase(it);
                            // Order the move index.
                            kBreakthroughIdxToStr.emplace_back(move);
                        }
                    }
                }
            }
        }
        // Save number of legal moves for current board size.
        kBreakthroughPolicySize[curr_bsize] = kBreakthroughIdxToStr.size();
    }
    // result should be...
    // board size 5: 104 legal moves
    // board size 6: 160 legal moves
    // board size 7: 228 legal moves
    // board size 8: 308 legal moves

    int size = kBreakthroughIdxToStr.size();
    kBreakthroughIdxToFromIdx.resize(size);
    kBreakthroughIdxToDestIdx.resize(size);

    // Separate from index and destination index for each move.
    // For example, the target move is 'a1b2'.
    // The from index is 'a1' and the destination index is 'b2'.
    for (int idx = 0; idx < size; ++idx) {
        auto move = kBreakthroughIdxToStr[idx];
        kBreakthroughStrToIdx[move] = idx;

        int fx = static_cast<int>(move[0] - 'a');
        int fy = static_cast<int>(move[1] - '1');
        int dx = static_cast<int>(move[2] - 'a');
        int dy = static_cast<int>(move[3] - '1');
        kBreakthroughIdxToFromIdx[idx] = fx + (fy << 8);
        kBreakthroughIdxToDestIdx[idx] = dx + (dy << 8);
    }
}

int BreakthroughAction::getFromID(int board_size) const
{
    int move = kBreakthroughIdxToFromIdx[action_id_];
    int x = move & 0xff;
    int y = (move >> 8) & 0xff;
    return x + board_size * y;
}

int BreakthroughAction::getDestID(int board_size) const
{
    int move = kBreakthroughIdxToDestIdx[action_id_];
    int x = move & 0xff;
    int y = (move >> 8) & 0xff;
    return x + board_size * y;
}

void BreakthroughEnv::reset()
{
    assert(board_size_ <= kBreakthroughMaxBoardSize);
    assert(board_size_ >= kBreakthroughMinBoardSize);
    turn_ = Player::kPlayer1;
    actions_.clear();

    // here is the chess board and rank
    //
    //     a b c d e f g h
    // 8 [ reverse rank 1  ]
    // 7 [ reverse rank 2  ]
    // 6 [      ...        ]
    // 5 [      ...        ]
    // 4 [      ...        ]
    // 3 [      ...        ]
    // 2 [     rank 2      ]
    // 1 [     rank 1      ]
    bitboard_rank1_ = 1ULL;
    for (int i = 1; i < board_size_; ++i) { bitboard_rank1_ |= (1ULL << i); }

    bitboard_rank2_ = bitboard_rank1_ << board_size_;
    bitboard_reverse_rank2_ = bitboard_rank1_ << ((board_size_ - 2) * board_size_);
    bitboard_reverse_rank1_ = bitboard_rank1_ << ((board_size_ - 1) * board_size_);

    bitboard_.set(Player::kPlayer1, bitboard_rank1_ | bitboard_rank2_);
    bitboard_.set(Player::kPlayer2, bitboard_reverse_rank1_ | bitboard_reverse_rank2_);
    bitboard_history_.clear();
    bitboard_history_.emplace_back(bitboard_);
}

bool BreakthroughEnv::act(const BreakthroughAction& action)
{
    if (!isLegalAction(action)) { return false; }
    actions_.push_back(action);
    int from = action.getFromID(board_size_);
    int dest = action.getDestID(board_size_);

    BreakthroughBitBoard& own_bitboard = bitboard_.get(turn_);
    BreakthroughBitBoard& opp_bitboard = bitboard_.get(getNextPlayer(turn_, kBreakthroughNumPlayer));
    own_bitboard ^= (1ULL << from);
    own_bitboard ^= (1ULL << dest);

    // remove the eaten piece
    if (opp_bitboard & (1ULL << dest)) { opp_bitboard ^= (1ULL << dest); }
    turn_ = action.nextPlayer();

    assert(own_bitboard & (1ULL << from) == 0ULL);
    assert(own_bitboard & opp_bitboard == 0ULL);
    bitboard_history_.emplace_back(bitboard_);

    return true;
}

bool BreakthroughEnv::act(const std::vector<std::string>& action_string_args)
{
    return act(BreakthroughAction(action_string_args, board_size_));
}

bool BreakthroughEnv::isLegalAction(const BreakthroughAction& action) const
{
    assert(action.getPlayer() == Player::kPlayer1 || action.getPlayer() == Player::kPlayer2);
    if (action.getActionID() < 0 || action.getActionID() >= static_cast<int>(kBreakthroughIdxToStr.size())) {
        return false;
    }
    int from = action.getFromID(board_size_);
    int dest = action.getDestID(board_size_);

    Player from_color = getPlayerAtBoardPos(from);
    Player dest_color = getPlayerAtBoardPos(dest);

    // illegal if it is not own piece
    if (from_color != turn_) { return false; }

    int fx = from % board_size_;
    int fy = from / board_size_;
    int dx = dest % board_size_;
    int dy = dest / board_size_;

    // illegal if the direction is incorrect
    if (dy - fy != (from_color == Player::kPlayer1 ? 1 : -1)) { return false; }

    // illegal if we eat own piece
    if (dest_color == turn_) { return false; }

    // illegal if we eat opp's piece with incorrect direction
    if (dest_color != Player::kPlayerNone && dx == fx) { return false; }

    return true;
}

std::vector<BreakthroughAction> BreakthroughEnv::getLegalActions() const
{
    std::vector<BreakthroughAction> actions;
    int size = kBreakthroughIdxToStr.size();
    for (int action_id = 0; action_id < size; ++action_id) {
        BreakthroughAction action(action_id, turn_);
        if (isLegalAction(action)) { actions.emplace_back(action); }
    }
    return actions;
}

bool BreakthroughEnv::isTerminal() const
{
    // zero means draw or no result
    return getEvalScore(false) != 0.0f;
}

float BreakthroughEnv::getEvalScore(bool is_resign) const
{
    Player winner = Player::kPlayerNone;

    // lose the game if all pieces are eaten
    if (!bitboard_.get(Player::kPlayer1)) {
        winner = Player::kPlayer2;
    } else if (!bitboard_.get(Player::kPlayer2)) {
        winner = Player::kPlayer1;
    }

    // win the game if own piece reach the last line
    if (bitboard_.get(Player::kPlayer1) & bitboard_reverse_rank1_) {
        winner = Player::kPlayer1;
    } else if (bitboard_.get(Player::kPlayer2) & bitboard_rank1_) {
        winner = Player::kPlayer2;
    }
    Player result = is_resign ? getNextPlayer(turn_, kBreakthroughNumPlayer) : winner;
    switch (result) {
        case Player::kPlayer1: return 1.0f;
        case Player::kPlayer2: return -1.0f;
        default: break;
    }
    return 0.0f;
}

bool BreakthroughEnv::isThreatPosition(Player color, int position) const
{
    Player opp = getNextPlayer(color, kBreakthroughNumPlayer);

    BreakthroughBitBoard own_bitboard = bitboard_.get(color);
    BreakthroughBitBoard opp_bitboard = bitboard_.get(opp);
    if (!(own_bitboard & (1ULL << position))) { return false; }

    int x = position % board_size_;
    int y = position / board_size_;
    int dy = color == Player::kPlayer1 ? 1 : -1;

    for (int dx : {1, -1}) {
        int xx = x + dx;
        int yy = y + dy;
        position = xx + board_size_ * yy;
        if (opp_bitboard & (1ULL << position)) { return true; }
    }
    return false;
}

BreakthroughBitBoard BreakthroughEnv::getThreatSpace(Player color) const
{
    BreakthroughBitBoard threat_bitboard = 0ULL;
    for (int pos = 0; pos < board_size_ * board_size_; ++pos) {
        if (isThreatPosition(color, pos)) {
            threat_bitboard |= (1ULL << pos);
        }
    }
    return threat_bitboard;
}

std::vector<float> BreakthroughEnv::getFeatures(utils::Rotation rotation) const
{
    /*
      18 channels:
         0~15. own/opponent position for last 8 turns
        16~17. threat
           18. black turn
           19. white turn
    */
    int past_moves = std::min(8, static_cast<int>(bitboard_history_.size()));
    int spatial = board_size_ * board_size_;
    std::vector<float> features(getNumInputChannels() * spatial, 0.f);
    int last_idx = bitboard_history_.size() - 1;

    // 0 ~ 15
    for (int c = 0; c < 2 * past_moves; c += 2) {
        const BreakthroughBitBoard& own_bitboard = bitboard_history_[last_idx - (c / 2)].get(turn_);
        const BreakthroughBitBoard& opponent_bitboard = bitboard_history_[last_idx - (c / 2)].get(getNextPlayer(turn_, kBreakthroughNumPlayer));
        for (int pos = 0; pos < spatial; ++pos) {
            int rotation_pos = getRotatePosition(pos, utils::reversed_rotation[static_cast<int>(rotation)]);
            BreakthroughBitBoard pos_bitboard = 1ULL << rotation_pos;
            features[pos + c * spatial] = (own_bitboard & pos_bitboard ? 1.0f : 0.0f);
            features[pos + (c + 1) * spatial] = (opponent_bitboard & pos_bitboard ? 1.0f : 0.0f);
        }
    }

    // 16 ~ 17
    BreakthroughBitBoard own_threat_bitboard = getThreatSpace(turn_);
    BreakthroughBitBoard opponent_threat_bitboard = getThreatSpace(getNextPlayer(turn_, kBreakthroughNumPlayer));
    for (int pos = 0; pos < spatial; ++pos) {
        int rotation_pos = getRotatePosition(pos, utils::reversed_rotation[static_cast<int>(rotation)]);
        BreakthroughBitBoard pos_bitboard = 1ULL << rotation_pos;
        features[pos + 16 * spatial] = (own_threat_bitboard & pos_bitboard ? 1.0f : 0.0f);
        features[pos + 17 * spatial] = (opponent_threat_bitboard & pos_bitboard ? 1.0f : 0.0f);
    }

    // 18 ~ 19
    for (int pos = 0; pos < spatial; ++pos) {
        features[pos + 18 * spatial] = static_cast<float>(turn_ == Player::kPlayer1);
        features[pos + 19 * spatial] = static_cast<float>(turn_ == Player::kPlayer2);
    }
    return features;
}

std::vector<float> BreakthroughEnv::getActionFeatures(const BreakthroughAction& action, utils::Rotation rotation) const
{
    throw std::runtime_error{"BreakthroughEnv::getActionFeatures() is not implemented"};
}

std::string BreakthroughEnv::toString() const
{
    std::ostringstream oss;
    int from = -1, dest = -1;
    if (!actions_.empty()) {
        from = actions_.rbegin()->getFromID(board_size_);
        dest = actions_.rbegin()->getDestID(board_size_);
    }

    oss << " ";
    for (int x = 0; x < board_size_; ++x) {
        oss << "  " << char('A' + x);
    }
    oss << std::endl;
    for (int y = board_size_ - 1; y >= 0; --y) {
        oss << y + 1 << " ";
        for (int x = 0; x < board_size_; ++x) {
            int pos = y * board_size_ + x;
            oss << (pos == dest ? '[' : ' ');
            if (getPlayerAtBoardPos(pos) == Player::kPlayerNone) {
                oss << (pos == from ? '#' : '.');
            } else if (getPlayerAtBoardPos(pos) == Player::kPlayer1) {
                oss << "X";
            } else {
                oss << "O";
            }
            oss << (pos == dest ? ']' : ' ');
        }
        oss << " " << y + 1 << std::endl;
    }
    oss << " ";
    for (int x = 0; x < board_size_; ++x) {
        oss << "  " << char('A' + x);
    }
    oss << std::endl;
    return oss.str();
}

Player BreakthroughEnv::getPlayerAtBoardPos(int pos) const
{
    for (Player p : {Player::kPlayer1, Player::kPlayer2}) {
        if (bitboard_.get(p) & (1ULL << pos)) { return p; }
    }
    return Player::kPlayerNone;
}

std::vector<float> BreakthroughEnvLoader::getActionFeatures(const int pos, utils::Rotation rotation) const
{
    throw std::runtime_error{"BreakthroughEnvLoader::getActionFeatures() is not implemented"};
}

} // namespace minizero::env::breakthrough
