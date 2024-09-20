#include "linesofaction.h"
#include "sgf_loader.h"
#include <algorithm>
#include <queue>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace minizero::env::linesofaction {

std::unordered_map<std::string, int> kLinesOfActionStrToIdx;
std::vector<int> kLinesOfActionIdxToFromIdx;
std::vector<int> kLinesOfActionIdxToDestIdx;
std::vector<std::string> kLinesOfActionIdxToStr;
std::vector<std::string> kLinesOfActionSquareToStr;

void initialize()
{
    // Generate the string for each position.
    for (int y = 0; y < kLinesOfActionBoardSize; ++y) {
        for (int x = 0; x < kLinesOfActionBoardSize; ++x) {
            char xx = x + 'a';
            char yy = y + '1';
            std::ostringstream oss;
            oss << xx << yy;
            kLinesOfActionSquareToStr.emplace_back(oss.str());
        }
    }

    // Now we generate the all possible move strings.
    std::vector<std::array<int, 2>> direct = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}, {1, 1}, {-1, 1}, {-1, -1}, {1, -1}};
    for (int y = 0; y < kLinesOfActionBoardSize; ++y) {
        for (int x = 0; x < kLinesOfActionBoardSize; ++x) {
            for (auto& dd : direct) {
                int xx = x + dd[0];
                int yy = y + dd[1];
                while (xx >= 0 && yy >= 0 && xx < kLinesOfActionBoardSize && yy < kLinesOfActionBoardSize) {
                    int from = x + kLinesOfActionBoardSize * y;
                    int to = xx + kLinesOfActionBoardSize * yy;
                    kLinesOfActionIdxToStr.emplace_back(kLinesOfActionSquareToStr[from] + kLinesOfActionSquareToStr[to]);
                    xx += dd[0];
                    yy += dd[1];
                }
            }
        }
    }

    // Order the possible move index.
    int size = kLinesOfActionIdxToStr.size();
    kLinesOfActionIdxToFromIdx.resize(size);
    kLinesOfActionIdxToDestIdx.resize(size);

    for (int idx = 0; idx < size; ++idx) {
        auto move = kLinesOfActionIdxToStr[idx];
        kLinesOfActionStrToIdx[move] = idx;

        int fx = static_cast<int>(move[0] - 'a');
        int fy = static_cast<int>(move[1] - '1');
        int dx = static_cast<int>(move[2] - 'a');
        int dy = static_cast<int>(move[3] - '1');
        kLinesOfActionIdxToFromIdx[idx] = fx + kLinesOfActionBoardSize * fy;
        kLinesOfActionIdxToDestIdx[idx] = dx + kLinesOfActionBoardSize * dy;
    }
}

void LinesOfActionEnv::reset()
{
    assert(board_size_ == kLinesOfActionBoardSize);
    turn_ = Player::kPlayer1;
    actions_.clear();
    bitboard_history_.clear();
    hashkey_history_.clear();

    int num_half_pieces = board_size_ - 2;
    LinesOfActionBitBoard p1_bitboard = 0ULL;
    LinesOfActionBitBoard p2_bitboard = 0ULL;

    int begin = 1;
    for (int pos = begin; pos < begin + num_half_pieces; ++pos) {
        p1_bitboard |= (1ULL << pos);
    }
    for (int i = 1; i < 1 + num_half_pieces; ++i) {
        int pos = i * board_size_;
        p2_bitboard |= (1ULL << pos);
        p2_bitboard |= (1ULL << (pos + board_size_ - 1));
    }
    begin = (board_size_ - 1) * board_size_ + 1;
    for (int pos = begin; pos < begin + num_half_pieces; ++pos) {
        p1_bitboard |= (1ULL << pos);
    }
    bitboard_.set(Player::kPlayer1, p1_bitboard);
    bitboard_.set(Player::kPlayer2, p2_bitboard);
    bitboard_history_.push_back(bitboard_);

    direction_.resize(8);
    direction_[0] = {1, 0};
    direction_[1] = {0, 1};
    direction_[2] = {-1, 0};
    direction_[3] = {0, -1};
    direction_[4] = {1, 1};
    direction_[5] = {1, -1};
    direction_[6] = {-1, -1};
    direction_[7] = {-1, 1};

    hash_key_ = computeHashKey();
    hashkey_history_.push_back(hash_key_);
}

bool LinesOfActionEnv::act(const LinesOfActionAction& action)
{
    if (!isLegalActionInternal(action, false)) {
        // The act() would be used for the human inferface. The final score of
        // circular/repetition status is various but it rarely effect the final
        // result. We simply accept the circular/repetition action when playing
        // with human. However forbid it in self-play training.
        return false;
    }
    int from = action.getFromID();
    int dest = action.getDestID();

    LinesOfActionBitBoard& own_bitboard = bitboard_.get(turn_);
    LinesOfActionBitBoard& opp_bitboard = bitboard_.get(getNextPlayer(turn_, kLinesOfActionNumPlayer));

    own_bitboard ^= (1ULL << from);
    own_bitboard ^= (1ULL << dest);
    // remove the eaten piece
    if (opp_bitboard & (1ULL << dest)) { opp_bitboard ^= (1ULL << dest); }
    turn_ = action.nextPlayer();

    actions_.push_back(action);
    bitboard_history_.push_back(bitboard_);
    hash_key_ = computeHashKey();
    hashkey_history_.push_back(hash_key_);

    return true;
}

bool LinesOfActionEnv::act(const std::vector<std::string>& action_string_args)
{
    return act(LinesOfActionAction(action_string_args, board_size_));
}

std::vector<LinesOfActionAction> LinesOfActionEnv::getLegalActions() const
{
    std::vector<LinesOfActionAction> actions;
    int size = getPolicySize();
    for (int id = 0; id < size; ++id) {
        LinesOfActionAction action(id, turn_);
        if (isLegalAction(action)) {
            actions.push_back(action);
        }
    }
    return actions;
}

bool LinesOfActionEnv::isLegalAction(const LinesOfActionAction& action) const
{
    // Always forbid circular/repetition status in self-play training
    // and tree search.
    return isLegalActionInternal(action, true);
}

bool LinesOfActionEnv::isLegalActionInternal(const LinesOfActionAction& action, bool forbid_circular) const
{
    assert(action.getPlayer() == Player::kPlayer1 || action.getPlayer() == Player::kPlayer2);
    if (action.getActionID() < 0 || action.getActionID() >= static_cast<int>(kLinesOfActionIdxToStr.size())) {
        return false;
    }
    if (action.getPlayer() != turn_) { return false; }

    Player next_player = getNextPlayer(turn_, kLinesOfActionNumPlayer);
    int from = action.getFromID();
    int dest = action.getDestID();
    int fx = from % board_size_;
    int fy = from / board_size_;
    int dx = dest % board_size_;
    int dy = dest / board_size_;

    int dist = std::max(std::abs(dx - fx), std::abs(dy - fy));
    int xdir = (dx - fx) / dist;
    int ydir = (dy - fy) / dist;

    if (getPlayerAtBoardPos(from) != turn_) { return false; }
    if (getNumPiecesOnLine(fx, fy, xdir, ydir) != dist) { return false; }

    for (int i = 1; i < dist; ++i) {
        int xx = fx + i * xdir;
        int yy = fy + i * ydir;
        int currpos = xx + board_size_ * yy;
        // We can't not cross opp's piece.
        if (getPlayerAtBoardPos(currpos) == next_player) { return false; }
    }
    // We can't not eat own piece.
    if (getPlayerAtBoardPos(dest) == turn_) { return false; }
    if (forbid_circular && isCycleAction(action)) { return false; }

    return true;
}

bool LinesOfActionEnv::isTerminal() const
{
    assert(!getLegalActions().empty());
    bool end;
    whoConnectAll(end);
    return end;
}

float LinesOfActionEnv::getEvalScore(bool is_resign /* = false */) const
{
    assert(!getLegalActions().empty());

    Player result = Player::kPlayerNone;
    if (is_resign) {
        result = getNextPlayer(turn_, kLinesOfActionNumPlayer);
    } else {
        bool end;
        result = whoConnectAll(end);
    }

    switch (result) {
        case Player::kPlayer1: return 1.0f;
        case Player::kPlayer2: return -1.0f;
        default: return 0.0f;
    }
}

std::vector<float> LinesOfActionEnv::getFeatures(utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    /*
      22 channels:
         0~15. own/opponent position for last 8 turns
        16~19. number of pieces
           20. black turn
           21. white turn
    */
    int past_moves = std::min(8, static_cast<int>(bitboard_history_.size()));
    int spatial = board_size_ * board_size_;
    std::vector<float> features(getNumInputChannels() * spatial, 0.f);
    int last_idx = bitboard_history_.size() - 1;

    // 0 ~ 15
    for (int c = 0; c < 2 * past_moves; c += 2) {
        const LinesOfActionBitBoard& own_bitboard = bitboard_history_[last_idx - (c / 2)].get(turn_);
        const LinesOfActionBitBoard& opponent_bitboard = bitboard_history_[last_idx - (c / 2)].get(getNextPlayer(turn_, kLinesOfActionNumPlayer));
        for (int pos = 0; pos < spatial; ++pos) {
            int rotation_pos = getRotatePosition(pos, utils::reversed_rotation[static_cast<int>(rotation)]);
            LinesOfActionBitBoard pos_bitboard = 1ULL << rotation_pos;
            features[pos + c * spatial] = (own_bitboard & pos_bitboard ? 1.0f : 0.0f);
            features[pos + (c + 1) * spatial] = (opponent_bitboard & pos_bitboard ? 1.0f : 0.0f);
        }
    }

    // 16 ~ 19
    for (int pos = 0; pos < spatial; ++pos) {
        int rotation_pos = getRotatePosition(pos, utils::reversed_rotation[static_cast<int>(rotation)]);
        if (getPlayerAtBoardPos(rotation_pos) != Player::kPlayerNone) {
            features[pos + 16 * spatial] = getNumPiecesOnLine(rotation_pos, 0) / 8.f;
        }
        if (getPlayerAtBoardPos(rotation_pos) != Player::kPlayerNone) {
            features[pos + 17 * spatial] = getNumPiecesOnLine(rotation_pos, 1) / 8.f;
        }
        if (getPlayerAtBoardPos(rotation_pos) != Player::kPlayerNone) {
            features[pos + 18 * spatial] = getNumPiecesOnLine(rotation_pos, 4) / 8.f;
        }
        if (getPlayerAtBoardPos(rotation_pos) != Player::kPlayerNone) {
            features[pos + 19 * spatial] = getNumPiecesOnLine(rotation_pos, 5) / 8.f;
        }
    }

    // 20 ~ 21
    for (int pos = 0; pos < spatial; ++pos) {
        features[pos + 20 * spatial] = static_cast<float>(turn_ == Player::kPlayer1);
        features[pos + 21 * spatial] = static_cast<float>(turn_ == Player::kPlayer2);
    }
    return features;
}

std::vector<float> LinesOfActionEnv::getActionFeatures(const LinesOfActionAction& action, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    throw std::runtime_error{"LinesOfActionEnv::getActionFeatures() is not implemented"};
}

std::string LinesOfActionEnv::toString() const
{
    std::ostringstream oss;
    int from = -1, dest = -1;
    if (!actions_.empty()) {
        from = actions_.rbegin()->getFromID();
        dest = actions_.rbegin()->getDestID();
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
            Player p = getPlayerAtBoardPos(pos);
            if (p == Player::kPlayerNone) {
                oss << (pos == from ? '#' : '.');
            } else if (p == Player::kPlayer1) {
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

bool LinesOfActionEnv::isOnBoard(int x, int y) const
{
    return x >= 0 && y >= 0 && x < board_size_ && y < board_size_;
}

int LinesOfActionEnv::getNumPiecesOnLine(int pos, int k) const
{
    int x = pos % board_size_;
    int y = pos / board_size_;
    int dx = direction_[k][0];
    int dy = direction_[k][1];
    return getNumPiecesOnLine(x, y, dx, dy);
}

int LinesOfActionEnv::getNumPiecesOnLine(int x, int y, int dx, int dy) const
{
    // Will return how many pieces (black and white) on the line.
    // The dx and dy are the shift direction.
    int pos = x + board_size_ * y;
    int xx = x;
    int yy = y;
    int cnt = 0;

    // Here is original position.
    if (getPlayerAtBoardPos(pos) != Player::kPlayerNone) { cnt += 1; }

    // Search positive direction way.
    xx = x + dx;
    yy = y + dy;
    while (isOnBoard(xx, yy)) {
        pos = xx + board_size_ * yy;
        if (getPlayerAtBoardPos(pos) != Player::kPlayerNone) { cnt += 1; }
        xx += dx;
        yy += dy;
    }

    // Search negative direction way.
    xx = x - dx;
    yy = y - dy;
    while (isOnBoard(xx, yy)) {
        pos = xx + board_size_ * yy;
        if (getPlayerAtBoardPos(pos) != Player::kPlayerNone) { cnt += 1; }
        xx -= dx;
        yy -= dy;
    }

    return cnt;
}

bool LinesOfActionEnv::searchConnection(Player p) const
{
    // Return true if all pieces are  connected vertically, horizontally or diagonally
    // (8-connectivity). Please see this website
    // https://en.wikipedia.org/wiki/Lines_of_Action
    std::vector<bool> marked(board_size_ * board_size_, false);
    std::queue<int> que;
    int conut = 0;

    for (int pos = 0; pos < board_size_ * board_size_; ++pos) {
        if (getPlayerAtBoardPos(pos) == p) {
            conut++;
        }
    }

    for (int pos = 0; pos < board_size_ * board_size_; ++pos) {
        if (getPlayerAtBoardPos(pos) == p) {
            marked[pos] = true;
            que.push(pos);
            break;
        }
    }

    int reachable = que.size();
    while (!que.empty()) {
        int pos = que.front();
        que.pop();

        int x = pos % board_size_;
        int y = pos / board_size_;

        for (int k = 0; k < 8; ++k) {
            int xx = x + direction_[k][0];
            int yy = y + direction_[k][1];
            if (isOnBoard(xx, yy)) {
                int ppos = xx + board_size_ * yy;
                if (!marked[ppos] && getPlayerAtBoardPos(ppos) == p) {
                    marked[ppos] = true;
                    que.push(ppos);
                    ++reachable;
                }
            }
        }
    }
    return reachable == conut;
}

Player LinesOfActionEnv::whoConnectAll(bool& end) const
{
    bool p1_connection = searchConnection(Player::kPlayer1);
    bool p2_connection = searchConnection(Player::kPlayer2);
    end = p1_connection || p2_connection;

    if (end) {
        // draw
        if (p1_connection && p2_connection) { return Player::kPlayerNone; }
        // play1 won
        if (p1_connection) { return Player::kPlayer1; }
        // play2 won
        if (p2_connection) { return Player::kPlayer2; }
    }
    return Player::kPlayerNone;
}

LinesOfActionHashKey LinesOfActionEnv::computeHashKey() const
{
    return computeHashKey(bitboard_, turn_);
}

LinesOfActionHashKey LinesOfActionEnv::computeHashKey(const GamePair<LinesOfActionHashKey>& bitboard, Player turn) const
{
    auto hashCat = [](std::uint64_t hash, std::uint64_t x) -> std::uint64_t {
        x = 0xfad0d7f2fbb059f1ULL * (x + 0xbaad41cdcb839961ULL) + 0x7acec0050bf82f43ULL * ((x >> 31) + 0xd571b3a92b1b2755ULL);
        hash ^= 0x299799adf0d95defULL + x + (hash << 6) + (hash >> 2);
        return hash;
    };

    std::uint64_t play1_hash = bitboard.get(Player::kPlayer1);
    std::uint64_t play2_hash = bitboard.get(Player::kPlayer2);
    std::uint64_t color_hash = turn == Player::kPlayer1 ? 0x1234567887564321ULL : 0;
    std::uint64_t hash = hashCat(play1_hash, play2_hash) ^ color_hash;
    return hash;
}

bool LinesOfActionEnv::isCycleAction(const LinesOfActionAction& action) const
{
    // assume the action is legal
    GamePair<LinesOfActionHashKey> bitboard = bitboard_;
    int from = action.getFromID();
    int dest = action.getDestID();
    bitboard.get(action.getPlayer()) ^= (1ULL << from);
    if (bitboard.get(action.nextPlayer()) & (1ULL << dest)) {
        bitboard.get(action.nextPlayer()) ^= (1ULL << dest);
    }
    bitboard.get(action.getPlayer()) ^= (1ULL << dest);

    LinesOfActionHashKey hash_key = computeHashKey(bitboard, action.nextPlayer());
    return find(hashkey_history_.rbegin(), hashkey_history_.rend(), hash_key) != hashkey_history_.rend();
}

Player LinesOfActionEnv::getPlayerAtBoardPos(int pos) const
{
    for (Player p : {Player::kPlayer1, Player::kPlayer2}) {
        if (bitboard_.get(p) & (1ULL << pos)) { return p; }
    }
    return Player::kPlayerNone;
}

std::vector<float> LinesOfActionEnvLoader::getActionFeatures(const int pos, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    throw std::runtime_error{"LinesOfActionEnvLoader::getActionFeatures() is not implemented"};
}

} // namespace minizero::env::linesofaction
