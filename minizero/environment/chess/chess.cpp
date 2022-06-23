# include "chess.h"
# include <iostream>
namespace minizero::env::chess{
std::uint64_t hash_key[NUM_OF_PIECES][NUM_OF_SQUARE];
void initialize() {
    std::mt19937_64 generator;
    generator.seed(0);

    for(int i = 0; i < NUM_OF_PIECES; i++)
        for(int j = 0; j < NUM_OF_SQUARE; j++)
            hash_key[i][j] = generator();
}
ChessAction::ChessAction(const std::vector<std::string>& action_string_args){
    // TODO: implement chess action constructor
}
std::string ChessEnv::toString() const {
    // TODO
    std::ostringstream oss;
    oss << "    a   b   c   d   e   f   g   h" << std::endl;
    for (int row = kChessBoardSize - 1; row >= 0; --row) {
        oss << row + 1 << " ";
        for (int col = 0; col < kChessBoardSize; ++col) {
            int id = row * kChessBoardSize + col;
            if (board_[id].first == Player::kPlayerNone) {
                oss << " .. ";
            } else{
                if (board_[id].first == Player::kPlayer1)
                    oss<< " w";
                else if (board_[id].first == Player::kPlayer2)
                    oss<< " b";
                switch (board_[id].second){
                    case Pieces::pawn:
                        oss << "p";
                        break;
                    case Pieces::knight:
                        oss << "N";
                        break;
                    case Pieces::bishop:
                        oss << "B";
                        break;
                    case Pieces::rook:
                        oss << "R";
                        break;
                    case Pieces::queen:
                        oss << "Q";
                        break;
                    case Pieces::king:
                        oss << "K";
                        break;
                    default:
                        oss << "-";
                        break;
                }
                oss << " ";
            }
        }
        oss << " " << row + 1 << std::endl;
    }
    oss << "    a   b   c   d   e   f   g   h" << std::endl;
    return oss.str(); 
}
void ChessEnv::reset() {
    // TODO
    turn_ = Player::kPlayer1;
    actions_.clear();
    setRemain();
        
    board_.resize(kChessBoardSize * kChessBoardSize);
        
    for(int i = 0; i < 16; i++){
        board_[i].first = Player::kPlayer1;
        if(i >= 8)
            board_[i].second = Pieces::pawn;
        else if(i == 0 || i == 7)
            board_[i].second = Pieces::rook;
        else if(i == 1 || i == 6)
            board_[i].second = Pieces::knight;
        else if(i == 2 || i == 5)
            board_[i].second = Pieces::bishop;
        else if(i == 3)
            board_[i].second = Pieces::queen;
        else if(i == 4)
            board_[i].second = Pieces::king;
    }
    for(int i=16; i<48; i++){
        board_[i].first = Player::kPlayerNone;
        board_[i].second = Pieces::empty;
    }
    for(int i = 48; i < 64; i++){
        board_[i].first = Player::kPlayer2;
        if (i < 56)
            board_[i].second = Pieces::pawn;
        else if(i == 56 || i == 63)
            board_[i].second = Pieces::rook;
        else if(i == 57 || i == 62)
            board_[i].second = Pieces::knight;
        else if(i == 58 || i == 61)
            board_[i].second = Pieces::bishop;
        else if(i == 59)
            board_[i].second = Pieces::queen;
        else if(i == 60)
            board_[i].second = Pieces::king;
    }

    setInt();
    setChessPair();
    bitboard_.reset();
    history_.clear();
    setHash();
}
void ChessEnv::setHash() {
    hash_table_1_.clear();
    hash_table_2_.clear();

    hash_value_ = hash_key[ROOK][0] ^ hash_key[KNIGHT][1] ^ hash_key[BISHOP][2] ^ hash_key[QUEEN][3];
    hash_value_ ^= hash_key[ROOK][7] ^ hash_key[KNIGHT][6] ^ hash_key[BISHOP][5] ^ hash_key[KING][4];
    for(int i = 8; i < 16; i++)
        hash_value_ ^= hash_key[PAWN][i];

    for(int i = 48; i < 56; i++)
        hash_value_ ^= hash_key[PAWN + 6][i];
    hash_value_ ^= hash_key[ROOK + 6][56] ^ hash_key[KNIGHT + 6][57] ^ hash_key[BISHOP + 6][58] ^ hash_key[QUEEN + 6][59];
    hash_value_ ^= hash_key[ROOK + 6][63] ^ hash_key[KNIGHT + 6][62] ^ hash_key[BISHOP + 6][61] ^ hash_key[KING + 6][60];
        
    hash_table_1_.insert(hash_value_);
}
int ChessEnv::storeHashTable(std::uint64_t newhash) {
    // std::cout<<"store: "<< newhash << std::endl;
    if(hash_table_1_.count(newhash) == 0){
        hash_table_1_.insert(newhash);
        return 1;
    }else if(hash_table_2_.count(newhash) == 0){
        hash_table_2_.insert(newhash);
        return 2;
    }
    return 3;
}
std::uint64_t ChessEnv::updateHashValue (Player turn, int move_from, int move_to, 
                                Pieces moved, Pieces taken, Pieces promote_to, 
                                bool is00, bool is000, bool isenpassant) {
         
    int from = move_from;
    int to = move_to;
        
    Pieces move = moved;
    Pieces take = taken;
    Pieces promote = promote_to;
        
    bool is_00 = is00;
    bool is_000 = is000;
    bool enpassant = isenpassant;
        
    int turn_id = turn == Player::kPlayer1 ? 0 : 6;
        
    if (is_00) {
        hash_value_ ^= hash_key[KING + turn_id][from] ^ hash_key[KING + turn_id][to];
        hash_value_ ^= hash_key[ROOK + turn_id][to + 1] ^ hash_key[ROOK + turn_id][to - 1];
        return hash_value_;
    }

    if (is_000) {
        hash_value_ ^= hash_key[KING + turn_id][from] ^ hash_key[KING + turn_id][to];
        hash_value_ ^= hash_key[ROOK + turn_id][to - 2] ^ hash_key[ROOK + turn_id][to + 1];
        return hash_value_;
    }

    if (enpassant) {
        hash_value_ ^= hash_key[PAWN + turn_id][from] ^ hash_key[PAWN + turn_id][to];
        hash_value_ ^= hash_key[PAWN + 6 - turn_id][rowColToPosition(positionToRow(from), positionToCol(to))];
        return hash_value_;
    }

    if (move == Pieces::pawn && promote == Pieces::empty) {
        hash_value_ ^= hash_key[PAWN + turn_id][from] ^ hash_key[PAWN + turn_id][to];
    }else if (move == Pieces::knight) {
        hash_value_ ^= hash_key[KNIGHT + turn_id][from] ^ hash_key[KNIGHT + turn_id][to];
    }else if (move == Pieces::bishop) {
        hash_value_ ^= hash_key[BISHOP + turn_id][from] ^ hash_key[BISHOP + turn_id][to];
    }else if (move == Pieces::rook) {
        hash_value_ ^= hash_key[ROOK + turn_id][from] ^ hash_key[ROOK + turn_id][to];
    }else if (move == Pieces::queen) {
        hash_value_ ^= hash_key[QUEEN + turn_id][from] ^ hash_key[QUEEN + turn_id][to];
    }else if (move == Pieces::king) {
        hash_value_ ^= hash_key[KING + turn_id][from] ^ hash_key[KING + turn_id][to];
    }

    if (take == Pieces::pawn) {
        hash_value_ ^= hash_key[PAWN + 6 - turn_id][to];
    }else if (take == Pieces::knight) {
        hash_value_ ^= hash_key[KNIGHT + 6 - turn_id][to];
    }else if (take == Pieces::bishop) {
        hash_value_ ^= hash_key[BISHOP + 6 - turn_id][to];
    }else if (take == Pieces::rook) {
        hash_value_ ^= hash_key[ROOK + 6 - turn_id][to];
    }else if (take == Pieces::queen) {
        hash_value_ ^= hash_key[QUEEN + 6 - turn_id][to];
    }

    if (promote == Pieces::knight) {
        if(turn_id == 0) 
            hash_value_ ^= hash_key[PAWN + turn_id][from] ^ hash_key[KNIGHT + turn_id][to];
        else
            hash_value_ ^= hash_key[PAWN + turn_id][from] ^ hash_key[KNIGHT + turn_id][to];
    }else if (promote == Pieces::bishop) {
        if(turn_id == 0) 
            hash_value_ ^= hash_key[PAWN + turn_id][from] ^ hash_key[BISHOP + turn_id][to];
        else
            hash_value_ ^= hash_key[PAWN + turn_id][from] ^ hash_key[BISHOP + turn_id][to];
    }else if (promote == Pieces::rook) {
        if(turn_id == 0) 
            hash_value_ ^= hash_key[PAWN + turn_id][from] ^ hash_key[ROOK + turn_id][to];
        else
            hash_value_ ^= hash_key[PAWN + turn_id][from] ^ hash_key[ROOK + turn_id][to];
    }else if (promote == Pieces::queen) {
        if(turn_id == 0) 
            hash_value_ ^= hash_key[PAWN + turn_id][from] ^ hash_key[QUEEN + turn_id][to];
        else
            hash_value_ ^= hash_key[PAWN + turn_id][from] ^ hash_key[QUEEN + turn_id][to];
    }
    return hash_value_;
}
void ChessEnv::setRemain(){
    int remain[5] = {8, 2, 2, 2, 1};
    for(int i = 0; i < 5; i++){
        ply1_remain_[i] = remain[i];
        ply2_remain_[i] = remain[i];
    }
}
void ChessEnv::setChessPair(){
    king_pos_.set(4, 60);
    king_moved_.set(false, false);
    ischecked_.set(false, false);
    insuffi_.set(false, false);
    can00_.set(false, false);
    can000_.set(false, false);
    qrook_moved_.set(false, false);
    krook_moved_.set(false, false);
}
void ChessEnv::setInt(){
    fifty_steps_ = 0;
    repetitions_ = 0;
}
int ChessEnv::rowColToPosition(int row, int col) const { 
    assert(!rowOutOfBoard(row) && !colOutOfBoard(col));
    return row * 8 + col; 
}
bool ChessEnv::squareIsAttacked(Player ply, int position, bool check_kings_attack) const {
    
    int square_col = positionToCol(position); 
    int square_row = positionToRow(position);

    Player opponent = Player::kPlayer2;
    Bitboard opponent_pieces = bitboard_.black_;

    if(ply == Player::kPlayer1){
        if(kBlackPawnAttacks[toBitBoardSquare(square_row, square_col)].intersects(opponent_pieces & bitboard_.pawns_)){
            return true;
        }
    }else if(ply == Player::kPlayer2){
        opponent = Player::kPlayer1;
        opponent_pieces = bitboard_.white_;
        if(kWhitePawnAttacks[toBitBoardSquare(square_row, square_col)].intersects(opponent_pieces & bitboard_.pawns_)){
            return true;
        }
    }

    
    if(kKnightAttacks[toBitBoardSquare(square_row, square_col)].intersects(opponent_pieces & bitboard_.knights_)){
        return true;
    }
    if(kBishopAttacks[toBitBoardSquare(square_row, square_col)].intersects(opponent_pieces & bitboard_.bishops_)){
        for(const auto& direction : kBishopDirections){
            int row = square_row;
            int col = square_col;
            while(true){
                row += direction.first;
                col += direction.second;
                if(outOfBoard(row, col))
                    break;
                int new_position = rowColToPosition(row, col);
                if(board_[new_position].first == opponent && (board_[new_position].second == Pieces::bishop || board_[new_position].second == Pieces::queen))
                    return true;
                if(board_[new_position].second != Pieces::empty)
                    break;
            }
        }
    }
    if(kRookAttacks[toBitBoardSquare(square_row, square_col)].intersects(opponent_pieces & bitboard_.rooks_)){
        for(const auto& direction : kRookDirections){
            int row = square_row;
            int col = square_col;
            while(true){
                row += direction.first;
                col += direction.second;
                if(outOfBoard(row, col))
                    break;
                int new_position = rowColToPosition(row, col);
                if(board_[new_position].first == opponent && (board_[new_position].second == Pieces::rook || board_[new_position].second == Pieces::queen))
                    return true;
                if(board_[new_position].second != Pieces::empty)
                    break;
            }
        }
    }
    if(check_kings_attack){
        for(const auto& move : kKingMoves){
            int row = square_row + move.first;
            int col = square_col + move.second;
            if(outOfBoard(row, col))
                continue;
            int new_position = rowColToPosition(row, col);
            if(king_pos_.get(opponent) == new_position)
                return true;
        }
    }
    return false;
}
bool ChessEnv::PlyIsCheck(Player ply) const {
    assert (ply != Player::kPlayerNone);
    return squareIsAttacked(ply, king_pos_.get(ply), false);
}
bool ChessEnv::checkBitboard() const {
    if(bitboard_.pawns_.countPawns() != (ply1_remain_[PAWN] + ply2_remain_[PAWN])){
        std::cout << "pawns.count: " << bitboard_.pawns_.countPawns() <<std::endl;
        std::cout << "remain pawns (w/b): " << ply1_remain_[PAWN] << ", " << ply2_remain_[PAWN] << std::endl;
        bitboard_.pawns_.showBitboard();
        std::cout << "\n-- Current Board --\n" << toString() << std::endl;
        return false;
    }
    if(bitboard_.rooks_.count() != (ply1_remain_[ROOK] + ply2_remain_[ROOK] + ply1_remain_[QUEEN] + ply2_remain_[QUEEN])){
        std::cout << "rooks.count: " << bitboard_.rooks_.count() <<std::endl;
        std::cout << "remain rooks (w/b): " << ply1_remain_[ROOK] << ", " << ply2_remain_[ROOK] << std::endl;
        std::cout << "remain queens (w/b): " << ply1_remain_[QUEEN] << ", " << ply2_remain_[QUEEN] << std::endl;
        bitboard_.rooks_.showBitboard();
        std::cout << "\n-- Current Board --\n" << toString() << std::endl;
        return false;
    }
    if(bitboard_.bishops_.count() != (ply1_remain_[BISHOP] + ply2_remain_[BISHOP] + ply1_remain_[QUEEN] + ply2_remain_[QUEEN])){
        std::cout << "bishops.count: " << bitboard_.bishops_.count() <<std::endl;
        std::cout << "remain bishops (w/b): " << ply1_remain_[BISHOP] << ", " << ply2_remain_[BISHOP] << std::endl;
        std::cout << "remain queens (w/b): " << ply1_remain_[QUEEN] << ", " << ply2_remain_[QUEEN] << std::endl;
        bitboard_.rooks_.showBitboard();
        std::cout << "\n-- Current Board --\n" << toString() << std::endl;
        return false;
    }
    if(bitboard_.knights_.count() != (ply1_remain_[KNIGHT] + ply2_remain_[KNIGHT])){
        std::cout << "knights.count: " << bitboard_.knights_.count() <<std::endl;
        std::cout << "remain knights (w/b): " << ply1_remain_[KNIGHT] << ", " << ply2_remain_[KNIGHT] << std::endl;
        bitboard_.rooks_.showBitboard();
        std::cout << "\n-- Current Board --\n" << toString() << std::endl;
        return false;
    }
    return true;
}
void ChessEnv::showRemain() const {
    std::cout << "White: ";
    for(int i = 0; i < 5 ; i ++){
        std::cout << ply1_remain_[i] << ' ';
    }
    std::cout << "\nBlack: ";
    for(int i = 0; i < 5 ; i ++){
        std::cout << ply2_remain_[i] << ' ';
    }
    std::cout << std::endl;
}
int ChessEnv::getDirection(int dir) const{
    if(dir == 0) return 0;
    if(dir < 0)
        return -1;
    return 1;
}
bool ChessEnv::isTerminal() const {
    Player who = eval();
    if(who != Player::kPlayerNone) {
        return true;
    } else {        
        int fifty_steps =  fifty_steps_;
        if (fifty_steps % 2) {
            fifty_steps = (fifty_steps + 1) / 2;
        } else {
            fifty_steps = (fifty_steps) / 2;
        }
            
        if(fifty_steps >= 50 || repetitions_ >= 3 || (insuffi_.get(Player::kPlayer1) && insuffi_.get(Player::kPlayer2))){
            /* std::cout << "no progress count: " << fifty_steps << std::endl;
            std::cout << "repetitions: " << repetitions_ << std::endl;
            if(insuffi_.get(Player::kPlayer1) && insuffi_.get(Player::kPlayer2))
                std::cout << "insuffi. materials: True\n";
            else
                std::cout << "insuffi. materials: False\n";*/
            return true;
        }else{
            std::vector<ChessAction> actions = getLegalActions();
            if(actions.size() == 0) return true;
        }
    }
    return false;
}
void ChessEnv::handle00(bool is00, int from) {
    if (!is00) return;
    board_[from + 1].first = turn_;
    board_[from + 1].second = Pieces::rook;
    board_[from + 3].first = Player::kPlayerNone;
    board_[from + 3].second = Pieces::empty;
    if(turn_ == Player::kPlayer1) {
        bitboard_.white_.set(toBitBoardSquare(from + 1)); // to 1 (R)
        bitboard_.white_.reset(toBitBoardSquare(from + 3)); // to 0
        bitboard_.rooks_.set(toBitBoardSquare(from + 1));
        bitboard_.rooks_.reset(toBitBoardSquare(from + 3));
    }else {
        bitboard_.black_.set(toBitBoardSquare(from + 1)); // to 1 (R)
        bitboard_.black_.reset(toBitBoardSquare(from + 3)); // to 0
        bitboard_.rooks_.set(toBitBoardSquare(from + 1));
        bitboard_.rooks_.reset(toBitBoardSquare(from + 3));
    }
}
void ChessEnv::handle000(bool is000, int from) {
    if(!is000) return;
    board_[from - 1].first = turn_;
    board_[from - 1].second = Pieces::rook;
    board_[from - 4].first = Player::kPlayerNone;
    board_[from - 4].second = Pieces::empty;
    if(turn_ == Player::kPlayer1) {
        bitboard_.white_.set(toBitBoardSquare(from - 1));
        bitboard_.white_.reset(toBitBoardSquare(from - 4));
        bitboard_.rooks_.set(toBitBoardSquare(from - 1));
        bitboard_.rooks_.reset(toBitBoardSquare(from - 4));
    }else {
        bitboard_.black_.set(toBitBoardSquare(from - 1));
        bitboard_.black_.reset(toBitBoardSquare(from - 4));
        bitboard_.rooks_.set(toBitBoardSquare(from - 1));
        bitboard_.rooks_.reset(toBitBoardSquare(from - 4));
    }
}
void ChessEnv::handleEnPassant(int from, int to) {
    board_[rowColToPosition(positionToRow(from), positionToCol(to))].first = Player::kPlayerNone;
    board_[rowColToPosition(positionToRow(from), positionToCol(to))].second = Pieces::empty; 
    bitboard_.pawns_.reset(toBitBoardSquare(positionToRow(from), positionToCol(to)));
    if(turn_ == Player::kPlayer1) {
        assert(positionToRow(from) == 4);
        bitboard_.black_.reset(toBitBoardSquare(4, positionToCol(to)));
    }else {
        assert(positionToRow(from) == 3);
        bitboard_.white_.reset(toBitBoardSquare(3, positionToCol(to)));
    }
}
void ChessEnv::handlePawnMove(Pieces promote, int from, int to) {
    bitboard_.pawns_.reset(toBitBoardSquare(from));
    if(promote == Pieces::empty){
        bitboard_.pawns_.set(toBitBoardSquare(to));
    }else{
        if (promote == Pieces::knight) {
            bitboard_.knights_.set(toBitBoardSquare(to));
            if (turn_ == Player::kPlayer1) {
                ply1_remain_[PAWN] -= 1;
                ply1_remain_[KNIGHT] += 1;
            } else {
                ply2_remain_[PAWN] -= 1;
                ply2_remain_[KNIGHT] += 1;
            }
        } else {
            if (promote == Pieces::bishop || promote == Pieces::queen) {
                bitboard_.bishops_.set(toBitBoardSquare(to));
                if (promote == Pieces::bishop) {
                    if(turn_ == Player::kPlayer1){
                        ply1_remain_[PAWN] -= 1;
                        ply1_remain_[BISHOP] += 1;
                    }else{
                        ply2_remain_[PAWN] -= 1;
                        ply2_remain_[BISHOP] += 1;
                    }
                }
            }
            if (promote == Pieces::rook || promote == Pieces::queen){
                bitboard_.rooks_.set(toBitBoardSquare(to));
                if (promote == Pieces::rook) {
                    if(turn_ == Player::kPlayer1){
                        ply1_remain_[PAWN] -= 1;
                        ply1_remain_[ROOK] += 1;
                    }else{
                        ply2_remain_[PAWN] -= 1;
                        ply2_remain_[ROOK] += 1;
                    }
                } else {
                    if(turn_ == Player::kPlayer1){
                        ply1_remain_[PAWN] -= 1;
                        ply1_remain_[QUEEN] += 1;
                    }else{
                        ply2_remain_[PAWN] -= 1;
                        ply2_remain_[QUEEN] += 1;
                    }
                }
            } 
        }
    }
    setEnPassantFlag(from, to);
}
void ChessEnv::handleBishopMove(int from, int to) {
    bitboard_.bishops_.reset(toBitBoardSquare(from));
    bitboard_.bishops_.set(toBitBoardSquare(to));
}
void ChessEnv::handleRookMove(int from, int to) {
    bitboard_.rooks_.reset(toBitBoardSquare(from));
    bitboard_.rooks_.set(toBitBoardSquare(to));
}
void ChessEnv::handleKnightMove(int from, int to) {
    bitboard_.knights_.reset(toBitBoardSquare(from));
    bitboard_.knights_.set(toBitBoardSquare(to));
}
void ChessEnv::handleQueenMove(int from, int to) {
    handleBishopMove(from, to);
    handleRookMove(from, to);
}
void ChessEnv::handleKingMove(int to) {
    king_moved_.set(turn_, true);
    king_pos_.set(turn_, to);
}
void ChessEnv::handleMyRookIsMoved(Player turn, Pieces move, int from, int krook_pos, int qrook_pos) {
    if (!krook_moved_.get(turn) && move == Pieces::rook && from == krook_pos)
        krook_moved_.set(turn, true);
    if (!qrook_moved_.get(turn) && move == Pieces::rook && from == qrook_pos)
        qrook_moved_.set(turn, true);
}
void ChessEnv::handleEnemyRookIsMoved(Player opponent, int to, int krook_pos, int qrook_pos) {
    bitboard_.rooks_.reset(toBitBoardSquare(to));
    if (!krook_moved_.get(opponent) && to == krook_pos)
        krook_moved_.set(opponent, true);
    if (!qrook_moved_.get(opponent) && to == qrook_pos)
        qrook_moved_.set(opponent, true);
}
void ChessEnv::handleNotRookCapture(Pieces take, int to) {
    if (take == Pieces::pawn) {
        bitboard_.pawns_.reset(toBitBoardSquare(to));
    }else if (take == Pieces::bishop) {
        bitboard_.bishops_.reset(toBitBoardSquare(to));
    } else if (take == Pieces::queen) {
        bitboard_.bishops_.reset(toBitBoardSquare(to));
        bitboard_.rooks_.reset(toBitBoardSquare(to));
    } else if (take == Pieces::knight) {
        bitboard_.knights_.reset(toBitBoardSquare(to));
    }
}
void ChessEnv::updateBoard(int from, int to, Pieces promote) {
    board_[to].first = turn_;
    board_[to].second = promote == Pieces::empty ? board_[from].second : promote;
    board_[from].first = Player::kPlayerNone;
    board_[from].second = Pieces::empty;
}
void ChessEnv::update50Steps(Pieces take, Pieces move) {
    if (take == Pieces::empty && move != Pieces::pawn) {
        fifty_steps_++;
    }else{
        fifty_steps_ = 0;
    }
}
void ChessEnv::setEnPassantFlag(int from, int to) {
    int delta_row = positionToRow(to) - positionToRow(from);
    if (delta_row == 2)
        bitboard_.pawns_.set(toBitBoardSquare(7, positionToCol(to))); // ply2 can enpassant
    else if (delta_row == -2) {
        bitboard_.pawns_.set(toBitBoardSquare(0, positionToCol(to)));
    }
}
int ChessEnv::getToFromID(int move_dir, int from) {
    if (move_dir <= 64) {
        int new_row = positionToRow(from) + kActionIdDirections64[move_dir - 1].second;
        int new_col = positionToCol(from) + kActionIdDirections64[move_dir - 1].first; 
        return rowColToPosition(new_row, new_col);
    } else {
        int new_row = (turn_ == Player::kPlayer1) ? 7 : 0;
        int new_col = positionToCol(from) + kActionIdDirections73[move_dir - 65].first;
        return rowColToPosition(new_row, new_col);
    }
}
Pieces ChessEnv::getPromoteFromID(int move_dir, int to, Pieces move) {
    if (move == Pieces::pawn) {
        if(move_dir <= 56) {
            if (positionToRow(to) == 7 || positionToRow(to) == 0)
                return Pieces::queen;
        }else {
            return kActionIdDirections73[move_dir - 65].second;
        }
    }
    return Pieces::empty;
}
bool ChessEnv::canPlayer00(Player player, int next_square1, int next_square2) {
    return !(ischecked_.get(player) || king_moved_.get(player) || krook_moved_.get(player) || 
      board_[next_square1].second != Pieces::empty || board_[next_square2].second != Pieces::empty || 
      squareIsAttacked(player, next_square1, true) || squareIsAttacked(player, next_square2, true));
}
bool ChessEnv::canPlayer000(Player player, int next_square1, int next_square2) {
    return !(ischecked_.get(player) || king_moved_.get(player) || qrook_moved_.get(player) || 
      board_[next_square1].second != Pieces::empty || board_[next_square2].second != Pieces::empty || 
      squareIsAttacked(player, next_square1, true) || squareIsAttacked(player, next_square2, true));
}
bool ChessEnv::whiteInsuffi() {
    return !(ply1_remain_[PAWN] > 0 || ply1_remain_[ROOK] > 0 || ply1_remain_[QUEEN] > 0 || 
             ply1_remain_[BISHOP] >= 2 || ((ply1_remain_[BISHOP] > 0 && ply1_remain_[KNIGHT]  > 0)) ||
             ply1_remain_[KNIGHT] >= 2);
}
bool ChessEnv::blackInsuffi() {
    return !(ply2_remain_[PAWN] > 0 || ply2_remain_[ROOK] > 0 || ply2_remain_[QUEEN] > 0 || 
             ply2_remain_[BISHOP] >= 2 || ((ply2_remain_[BISHOP] > 0 && ply2_remain_[KNIGHT]  > 0)) ||
             ply2_remain_[KNIGHT] >= 2);
}
bool ChessEnv::isLegalAction(const ChessAction& action) const {
    int action_id = action.getActionID();
    Player action_turn = action.getPlayer();
    int id = action_id % 73 + 1; // 1-73
    int from = action_id / 73;
    int to = from;
    Pieces promote = Pieces::empty;
    if (id <= 64) {
        int new_row = positionToRow(from) + kActionIdDirections64[id - 1].second;
        int new_col = positionToCol(from) + kActionIdDirections64[id - 1].first; 
        if(outOfBoard(new_row, new_col)) return false;
        to = rowColToPosition(new_row, new_col);
    } else {
        int new_row = (action_turn == Player::kPlayer1) ? 7 : 0;
        int new_col = positionToCol(from) + kActionIdDirections73[id - 65].first;
        if(outOfBoard(new_row, new_col)) return false;
        to = rowColToPosition(new_row, new_col);
        promote = kActionIdDirections73[id - 65].second;
    }
    
    if(board_[from].first != turn_) return false;
    int row = positionToRow(from), col = positionToCol(from);      
    Pieces move = board_[from].second;
    Pieces take = board_[to].second; 
    if(move == Pieces::empty || turn_ == board_[to].first) return false;
    bool is_00 = (move == Pieces::king && id == ID00);
    bool is_000 = (move == Pieces::king && id == ID000);
    bool isenpassant = (move == Pieces::pawn && positionToCol(from) != positionToCol(to) && take == Pieces::empty);
    if(move != Pieces::pawn && (promote != Pieces::empty || id > 64)) return false;   
    if(move != Pieces::knight && (id <= 64 && id > 56)) return false;
    if (move == Pieces::pawn) {
        if (id <= 8) {
            if(id == ID_E1 || id == ID_W1) return false;
            if (turn_ == Player::kPlayer1) {
                if(kActionIdDirections64[id - 1].second < 0) return false;
                if(row == 6) promote = Pieces::queen;
            } else {
                if(kActionIdDirections64[id - 1].second > 0) return false;
                if(row == 1) promote = Pieces::queen;
            }
                    
            if (kActionIdDirections64[id - 1].first == 0) {   // move forward
                if(take != Pieces::empty) return false;
            } else {  // take
                if (isenpassant) {
                    if(turn_ == Player::kPlayer1){
                        if(!bitboard_.pawns_.get(toBitBoardSquare(0, positionToCol(to))) || row != 4)
                            return false;
                    } else if(!bitboard_.pawns_.get(toBitBoardSquare(7, positionToCol(to))) || row != 3)
                        return false;
                    if(board_[to].first != Player::kPlayerNone) return false;
                    if(board_[rowColToPosition(row, positionToCol(to))].first == turn_ || board_[rowColToPosition(row, positionToCol(to))].second != Pieces::pawn)
                        return false;
                }
            }
        } else if (id == ID_N2 || id == ID_S2) { // 8: {0, 2}  12: {0, -2}
            if (turn_ == Player::kPlayer1) {
                if(row != 1 || id != ID_N2) return false;
                if(board_[from + 8].second != Pieces::empty || board_[to].second != Pieces::empty)
                    return false;
            } else {
                if(row != 6 || id != ID_S2) return false;
                if(board_[from - 8].second != Pieces::empty || board_[to].second != Pieces::empty)
                    return false;
            }
        }else if (id >= 65) { // underpromotion       
            if((turn_ == Player::kPlayer1 && row != 6) || (turn_ == Player::kPlayer2 && row != 1))
                return false;
            int direction = kActionIdDirections73[id - 65].first;
            if (direction == 0 && take != Pieces::empty) return false;
            if (direction != 0 && take == Pieces::empty) return false;
        } else {
            return false;
        }
    } else if (move == Pieces::knight) {
        if(!(id <= 64 && id > 56)) return false;
    } else if (move == Pieces::bishop || move == Pieces::rook || move == Pieces::queen) {
        if(id > 56) return false;
        if((move == Pieces::rook && id % 2 == 0) || (move == Pieces::bishop && id % 2 == 1))
            return false;
        int dir_row = kActionIdDirections64[id - 1].second;
        int dir_col = kActionIdDirections64[id - 1].first;
        int num_of_square = (dir_row == 0) ? dir_col : dir_row;
        dir_row = getDirection(dir_row);  
        dir_col = getDirection(dir_col);         
        num_of_square = (num_of_square < 0) ? -1 * num_of_square : num_of_square;
        for (int i = 1; i < num_of_square; i++) {
            if(board_[rowColToPosition(row + dir_row * i, col + dir_col * i)].second != Pieces::empty)
                return false;
        }
    } else if (move == Pieces::king) {
        if(!is_00 && !is_000) {
            if(id > 8 || squareIsAttacked(turn_, to, true)) return false;   
        }else if ((is_00 && !can00_.get(turn_)) || (is_000 && !can000_.get(turn_))){
            return false;
        }
    }
    assert(take != Pieces::king);
    ChessEnv AfterAction(*this);
    AfterAction.fakeAct(from, to, move, take, promote, is_00, is_000, isenpassant);
    return !AfterAction.PlyIsCheck(this->turn_);
}
std::vector<ChessAction> ChessEnv::getLegalActions() const {
    // TODO
    std::vector<ChessAction> actions;
    for (int pos = 0; pos < kChessBoardSize * kChessBoardSize; pos++) {
        for (int id = 0; id < 73; id++) {
            int action_id = pos * 73 + id;
            ChessAction action(action_id, turn_); 
            if (isLegalAction(action)) {
                actions.push_back(action);
            }
        }
    }   
    return actions;
}
bool ChessEnv::act(const std::vector<std::string>& action_string_args) {
    // TODO
    assert(action_string_args.size() == 2);
    assert(action_string_args[0].size() == 1 && (charToPlayer(action_string_args[0][0]) == Player::kPlayer1 || charToPlayer(action_string_args[0][0]) == Player::kPlayer2));
    turn_ = charToPlayer(action_string_args[0][0]);
    std::string move_str = action_string_args[1];
    int i;
    for(i = 0; i < 1858; i++){
        if(move_str == kAllMoves[i])
            break;
    }
    if(i == 1858)
        return false;
    Pieces move, taken;
    // promote: q n b r
    int from_col = move_str[0] - 'a';
    int from_row = move_str[1] - '1';
    int to_col = move_str[2] - 'a';
    int to_row = move_str[3] - '1';
    int dir_row = to_row - from_row;
    int dir_col = to_col - from_col;
    int from = rowColToPosition(from_row, from_col);
    int to = rowColToPosition(to_row, to_col);
    int id = 0;
    move = board_[from].second;
    taken = board_[to].second;

    if(move_str.size() == 5){
        if(move_str[4] == 'q') {
            if(dir_col != 0 && (taken == Pieces::empty || board_[to].first == turn_)) return false;
            if (turn_ == Player::kPlayer1) {
                if(from_row != 6 || dir_row != 1) return false;
                if (dir_col == -1) {
                    id = from * 73 + 8;
                } else if (dir_col == 0) {
                    if (taken != Pieces::empty) return false;
                    id = from * 73 + 1;
                } else if (dir_col == 1) {
                    id = from * 73 + 2;
                }
            } else {
                if (from_row != 1 || dir_row != -1) return false;
                if (dir_col == -1) {
                    id = from * 73 + 6;
                } else if (dir_col == 0) {
                    if (taken != Pieces::empty) return false;
                    id = from * 73 + 5;
                } else if (dir_col == 1) {
                    id = from * 73 + 4;
                } else {
                    return false;
                }
            }
        } else if(move_str[4] == 'r') {
            if (dir_col == -1) {
                id = from * 73 + 65;
            } else if (dir_col == 0) {
                id = from * 73 + 68;
            } else if (dir_col == 1) {
                id = from * 73 + 71;
            } else {
                return false;
            }
        } else if(move_str[4] == 'b') {
            if (dir_col == -1) {
                id = from * 73 + 66;
            } else if (dir_col == 0) {
                id = from * 73 + 69;
            } else if (dir_col == 1) {
                id = from * 73 + 72;
            } else {
                return false;
            }
        } else if(move_str[4] == 'n') {
            if (dir_col == -1) {
                id = from * 73 + 67;
            } else if (dir_col == 0) {
                id = from * 73 + 70;
            } else if (dir_col == 1) {
                id = from * 73 + 73;
            } else {
                return false;
            }
        }
        ChessAction action(id - 1, turn_);
        if(!isLegalAction(action)) return false;
        return act(action);  
    }

    if(move == Pieces::king){
        if((turn_ == Player::kPlayer1 && move_str == "e1g1") || (turn_ == Player::kPlayer2 && move_str == "e8g8")) {
            id = from * 73 + 11;
        } else if((turn_ == Player::kPlayer1 && move_str == "e1c1") || (turn_ == Player::kPlayer2 && move_str == "e8c8")) {
            id = from * 73 + 15;
        } else {
            if (dir_col == 0) {
                if (dir_row == 1) id = from * 73 + 1;
                else if ( dir_row == -1) id = from * 73 + 5;
            } else if (dir_col == 1) {
                if (dir_row == 1) id = from * 73 + 2;
                else if (dir_row == 0) id = from * 73 + 3;
                else if (dir_row == -1) id = from * 73 + 4;
            } else if (dir_col == -1) {
                if (dir_row == 1) id = from * 73 + 7;
                else if (dir_row == 0) id = from * 73 + 6;
                else if (dir_row == -1) id = from * 73 + 5;
            } else {
                return false;
            }
        }
    }else if(move == Pieces::pawn){
        if(turn_ == Player::kPlayer1){
            if(dir_row <= 0)
                return false;
            if(dir_col != 0 && taken == Pieces::empty){
                if(!(from_row == 4 && board_[rowColToPosition(from_row, to_col)].first == Player::kPlayer2 && board_[rowColToPosition(from_row, to_col)].second == Pieces::pawn)){
                    return false;
                }
            }
            if (dir_row == 2) {
                if (dir_col != 0) return false;
                id = from * 73 + 9;
            } else if (dir_row == 1) {
                if (dir_col == -1) id = from * 73 + 8;
                else if (dir_col == 0) id = from * 73 + 1;
                else if (dir_col == 1) id = from * 73 + 2;
                else return false;
            }
        }else{
            if(dir_row >= 0)
                return false;
            if(dir_col != 0 && taken == Pieces::empty){
                if(!(from_row == 3 && board_[rowColToPosition(from_row, to_col)].first == Player::kPlayer1 && board_[rowColToPosition(from_row, to_col)].second == Pieces::pawn)){
                    return false;
                }
            }
            if (dir_row == -2) {
                if (dir_col != 0) return false;
                id = from * 73 + 13;
            } else if (dir_row == -1) {
                if (dir_col == -1) id = from * 73 + 6;
                else if (dir_col == 0) id = from * 73 + 5;
                else if (dir_col == 1) id = from * 73 + 4;
                else return false;
            }
        }
    } else if (move == Pieces::knight) {
        if (dir_col == -1) {
            if (dir_row == 2) id = from * 73 + 64;
            else if ( dir_row == -2) id = from * 73 + 61;
        } else if (dir_col == 1) {
            if (dir_row == 2) id = from * 73 + 57;
            else if ( dir_row == -2) id = from * 73 + 60;
        } else if (dir_col == -2) {
            if (dir_row == 1) id = from * 73 + 63;
            else if ( dir_row == -1) id = from * 73 + 62;
        } else if (dir_col == 2) {
            if (dir_row == 1) id = from * 73 + 58;
            else if ( dir_row == -1) id = from * 73 + 59;
        } else {
            return false;
        }
    } else {
        if (dir_col == 0) {
            if (dir_row > 0) id = from * 73 + (dir_row - 1) * 8 + 1;
            else id = from * 73 + (-1 * dir_row - 1) * 8 + 5;
        } else if (dir_row == 0) {
            if (dir_col > 0) id = from * 73 + (dir_col - 1) * 8 + 3;
            else id = from * 73 + (-1 * dir_col - 1) * 8 + 7;
        } else {
            if (dir_col > 0 && dir_row > 0) {
                if (dir_col != dir_row) return false;
                id = from * 73 + (dir_col - 1) * 8 + 2;
            }else if ((dir_col > 0 && dir_row < 0)) {
                if (dir_col != -1 * dir_row) return false;
                id = from * 73 + (dir_col - 1) * 8 + 4;
            }else if ((dir_col < 0 && dir_row < 0)) {
                if (dir_col != dir_row) return false;
                id = from * 73 + (-1 * dir_col - 1) * 8 + 6;
            }else { //left-up
                if (dir_col != -1 * dir_row) return false;
                id = from * 73 + (dir_row - 1) * 8 + 8;
            }
        }
    }
    if (id == 0) { return false; }
    ChessAction action(id - 1, turn_);
    // ChessAction action(action_string_args);
    if (!isLegalAction(action)) { return false; }
    return act(action);
} 
bool ChessEnv::act(const ChessAction& action) {
    int action_id = action.getActionID(); // pos * 73 + dir 1-4672 pos=0, 73 -> 73
    int from = action_id / 73, move_dir = action_id % 73 + 1;
    int to = getToFromID(move_dir, from);
    Pieces move = board_[from].second, take = board_[to].second;
    Pieces promote = getPromoteFromID(move_dir, to, move);
    updateBoard(from, to, promote);
    bool is00 = (move == Pieces::king && move_dir == ID00);
    bool is000 = (move == Pieces::king && move_dir == ID000);
    bool is_enpassant = (move == Pieces::pawn && positionToCol(from) != positionToCol(to) && take == Pieces::empty);
    handle00(is00, from);
    handle000(is000, from);

    if (is_enpassant) {
        take = Pieces::pawn;
        handleEnPassant(from, to);
    } else if(take != Pieces::empty){
        if (turn_ == Player::kPlayer1){
            bitboard_.black_.reset(toBitBoardSquare(to)); 
        } else {
            bitboard_.white_.reset(toBitBoardSquare(to));
        } 
        if (take == Pieces::pawn)
            bitboard_.pawns_.reset(toBitBoardSquare(to));
    }
    update50Steps(take, move);
    if (turn_ == Player::kPlayer1) {
        bitboard_.white_.reset(toBitBoardSquare(from)); // to 0
        bitboard_.white_.set(toBitBoardSquare(to)); // to 1 (K)
        for (int i = 0; i < kChessBoardSize; i++)
            bitboard_.pawns_.reset(toBitBoardSquare(0, i));
        handleMyRookIsMoved(Player::kPlayer1, move, from, 7, 0);
        if (take == Pieces::rook){
            ply2_remain_[ROOK] -= 1;
            handleEnemyRookIsMoved(Player::kPlayer2, to, 63, 56);
        }
    } else {
        bitboard_.black_.reset(toBitBoardSquare(from)); // to 0
        bitboard_.black_.set(toBitBoardSquare(to)); // to 1 (K)    
        for (int i = 0; i < kChessBoardSize; i++)
            bitboard_.pawns_.reset(toBitBoardSquare(7, i));
        handleMyRookIsMoved(Player::kPlayer2, move, from, 63, 56);
        if (take == Pieces::rook){
            ply1_remain_[ROOK] -= 1;
            handleEnemyRookIsMoved(Player::kPlayer1, to, 7, 0);
        }
    }

    if (take == Pieces::pawn) {
        if(turn_ == Player::kPlayer2)
            ply1_remain_[PAWN] -= 1;    // PAWN -> pieces::pawn
        else
            ply2_remain_[PAWN] -= 1;
    } else if (take == Pieces::bishop || take == Pieces::queen) {
        bitboard_.bishops_.reset(toBitBoardSquare(to));
        if(take == Pieces::bishop){
            if(turn_ == Player::kPlayer2)
                ply1_remain_[BISHOP] -= 1;
            else
                ply2_remain_[BISHOP] -= 1;
        }else{
            bitboard_.rooks_.reset(toBitBoardSquare(to));
            if(turn_ == Player::kPlayer2)
                ply1_remain_[QUEEN] -= 1;
            else
                ply2_remain_[QUEEN] -= 1;
        }
    } else if (take == Pieces::knight) {
        bitboard_.knights_.reset(toBitBoardSquare(to));
        if(turn_ == Player::kPlayer2)
            ply1_remain_[KNIGHT] -= 1;
        else
            ply2_remain_[KNIGHT] -= 1;
    }

    if (move == Pieces::pawn) {
        handlePawnMove(promote, from, to);
    }else if (move == Pieces::bishop) {
        handleBishopMove(from, to);
    }else if (move == Pieces::knight) {
        handleKnightMove(from, to);
    }else if (move == Pieces::king) {
        handleKingMove(to);
    } else if (move == Pieces::rook) {
        handleRookMove(from, to);
    } else {
        handleQueenMove(from, to);
    }
    insuffi_.set(whiteInsuffi(), blackInsuffi());
    assert(take != Pieces::king && !PlyIsCheck(turn_) && checkBitboard());
    ischecked_.set(PlyIsCheck(Player::kPlayer1), PlyIsCheck(Player::kPlayer2));
    can00_.set(canPlayer00(Player::kPlayer1, 5, 6), canPlayer00(Player::kPlayer2, 61, 62));
    can000_.set(canPlayer000(Player::kPlayer1, 2, 3), canPlayer000(Player::kPlayer2, 58, 59));
    int repeat = storeHashTable(updateHashValue(turn_, from, to, move, take, promote, is00, is000, is_enpassant));
    repetitions_ = (repetitions_ < repeat) ? repeat : repetitions_;
    history_.update(bitboard_, king_pos_);    
    actions_.push_back(action);
    turn_ = action.nextPlayer();
    return true;
}
void ChessEnv::fakeAct(int from, int to, Pieces move, Pieces take, Pieces promote, bool is00, bool is000, bool isenpassant) {   
    updateBoard(from, to, promote);  
    handle00(is00, from);
    handle000(is000, from);
    if (isenpassant) {
        handleEnPassant(from, to);
    }else if(take != Pieces::empty) {
        if (turn_ == Player::kPlayer1)
            bitboard_.black_.reset(toBitBoardSquare(to));
        else 
            bitboard_.white_.reset(toBitBoardSquare(to));
    }
  
    if (turn_ == Player::kPlayer1) {
        bitboard_.white_.reset(toBitBoardSquare(from)); // to 0
        bitboard_.white_.set(toBitBoardSquare(to)); // to 1 (K)
        handleMyRookIsMoved(Player::kPlayer1, move, from, 7, 0);
        if (take == Pieces::rook){
            handleEnemyRookIsMoved(Player::kPlayer2, to, 63, 56);
            bitboard_.rooks_.reset(toBitBoardSquare(to));
        } else {
            handleNotRookCapture(take, to);
        }
    } else {
        bitboard_.black_.reset(toBitBoardSquare(from)); // to 0
        bitboard_.black_.set(toBitBoardSquare(to)); // to 1 (K)
        handleMyRookIsMoved(Player::kPlayer2, move, from, 63, 56);
        if (take == Pieces::rook){
            handleEnemyRookIsMoved(Player::kPlayer1, to, 7, 0);
            bitboard_.rooks_.reset(toBitBoardSquare(to));
        } else {
            handleNotRookCapture(take, to);
        } 
    }
    
    if (move == Pieces::pawn) {
        bitboard_.pawns_.reset(toBitBoardSquare(from));
        if (promote == Pieces::empty)
            bitboard_.pawns_.set(toBitBoardSquare(to));
        else if (promote == Pieces::bishop || promote == Pieces::queen) {
            bitboard_.bishops_.set(toBitBoardSquare(to));
        }
        if (promote == Pieces::rook || promote == Pieces::queen){
            bitboard_.rooks_.set(toBitBoardSquare(to));
        }else if (promote == Pieces::knight) {
            bitboard_.knights_.set(toBitBoardSquare(to));
        }
        setEnPassantFlag(from, to);
    }else if (move == Pieces::queen) {
        handleQueenMove(from, to);
    }else if (move == Pieces::bishop) {
        handleBishopMove(from, to);
    }else if (move == Pieces::knight) {
        handleKnightMove(from, to);
    }else if (move == Pieces::rook) {
        handleRookMove(from, to);
    }else if (move == Pieces::king) {
        handleKingMove(to);
    }
}
Player ChessEnv::eval() const {
    // TODO
    if(turn_ == Player::kPlayer1 && ischecked_.get(Player::kPlayer1)){
        std::vector<ChessAction> actions = getLegalActions();
        if(actions.size() == 0) {
            // std::cout << "result: 0-1\n";
            return Player::kPlayer2;
        }
    }
    if(turn_ == Player::kPlayer2 &&ischecked_.get(Player::kPlayer2)){
        std::vector<ChessAction> actions = getLegalActions();
        if(actions.size() == 0){
            // std::cout << "result: 1-0\n";
            return Player::kPlayer1;
        }
    }
    return Player::kPlayerNone;
}
}
