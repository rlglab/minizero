# include "chess.h"
# include <iostream>
namespace minizero::env::chess{

ChessAction::ChessAction(const std::vector<std::string>& action_string_args)
{
    // TODO: implement chess action constructor
}

void ChessEnv::setBoard (std::vector<std::pair<Player, Pieces>> board) {
    board_.resize(kChessBoardSize * kChessBoardSize);
    for(int i = 0; i < 64; i++){
        board_[i].first = board[i].first;
        board_[i].second = board[i].second;
    }
}

void ChessEnv::setBitboard (std::uint64_t rooks, std::uint64_t bishops, std::uint64_t knights, std::uint64_t pawns, std::uint64_t ply1, std::uint64_t ply2) {
    rooks_.setboard(rooks);
    bishops_.setboard(bishops);
    knights_.setboard(knights);
    pawns_.setboard(pawns);
    ply1_pieces_.setboard(ply1);
    ply2_pieces_.setboard(ply2);
}

void ChessEnv::setRemain(int ply1[], int ply2[]){
    for(int i = 0; i < 5; i++){
        ply1_remain_[i] = ply1[i];
        ply2_remain_[i] = ply2[i];
    }
}

void ChessEnv::setBoolean(bool ply1_k, bool ply1_kr, bool ply1_qr, bool ply1_check, bool ply1_insuff, bool ply1_00, bool ply1_000,
                          bool ply2_k, bool ply2_kr, bool ply2_qr, bool ply2_check, bool ply2_insuff, bool ply2_00, bool ply2_000){
    ply1_k_moved_ = ply1_k;
    ply1_kr_moved_ = ply1_kr;
    ply1_qr_moved_ = ply1_qr;
    ply1_ischecked_ = ply1_check;
    ply1_insuffi_ = ply1_insuff;
    ply1_can000_ = ply1_00;
    ply1_can000_ = ply1_000;
        
    ply2_k_moved_ = ply2_k;
    ply2_kr_moved_ = ply2_kr;
    ply2_qr_moved_ = ply2_qr;
    ply2_ischecked_ = ply2_check;
    ply2_insuffi_ = ply2_insuff;
    ply2_can00_ = ply2_00;
    ply2_can000_ = ply2_000;
}

void ChessEnv::setInt(int fifty, int three_rep, int ply1_k_pos, int ply2_k_pos){
    fifty_steps_ = fifty;
    repetitions_ = three_rep;
    ply1_king_pos_ = ply1_k_pos;
    ply2_king_pos_ = ply2_k_pos;
}

void ChessEnv::clearBitboardHistory(){
    pawns_history_.clear();
    knights_history_.clear();
    bishops_history_.clear();
    rooks_history_.clear();
    queens_history_.clear();
    king_history_.clear();
}



bool ChessEnv::squareIsAttacked(Player ply, int position, bool check_kings_attack) const {
    
    int square_col = positionToCol(position); 
    int square_row = positionToRow(position);

    Player opponent = Player::kPlayer2;
    Bitboard opponent_pieces = ply2_pieces_;

    if(ply == Player::kPlayer1){
        if(kBlackPawnAttacks[toBitBoardSquare(square_row, square_col)].intersects(opponent_pieces & pawns_)){
            return true;
        }
    }else if(ply == Player::kPlayer2){
        opponent = Player::kPlayer1;
        opponent_pieces = ply1_pieces_;
        if(kWhitePawnAttacks[toBitBoardSquare(square_row, square_col)].intersects(opponent_pieces & pawns_)){
            return true;
        }
    }

    
    if(kKnightAttacks[toBitBoardSquare(square_row, square_col)].intersects(opponent_pieces & knights_)){
        return true;
    }
    if(kBishopAttacks[toBitBoardSquare(square_row, square_col)].intersects(opponent_pieces & bishops_)){
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
    if(kRookAttacks[toBitBoardSquare(square_row, square_col)].intersects(opponent_pieces & rooks_)){
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
            if(board_[new_position].first == opponent && board_[new_position].second == Pieces::king)
                return true;
        }
    }
    return false;
}

void ChessEnv::checkBitboard() const {
    if(pawns_.countPawns() != (ply1_remain_[PAWN] + ply2_remain_[PAWN])){
        std::cout << "pawns.count: " << pawns_.countPawns() <<std::endl;
        std::cout << "remain pawns (w/b): " << ply1_remain_[PAWN] << ", " << ply2_remain_[PAWN] << std::endl;
        pawns_.showBitboard();
        std::cout << "\n-- Current Board --\n" << toString() << std::endl;
    }
    if(rooks_.count() != (ply1_remain_[ROOK] + ply2_remain_[ROOK] + ply1_remain_[QUEEN] + ply2_remain_[QUEEN])){
        std::cout << "rooks.count: " << rooks_.count() <<std::endl;
        std::cout << "remain rooks (w/b): " << ply1_remain_[ROOK] << ", " << ply2_remain_[ROOK] << std::endl;
        std::cout << "remain queens (w/b): " << ply1_remain_[QUEEN] << ", " << ply2_remain_[QUEEN] << std::endl;
        rooks_.showBitboard();
        std::cout << "\n-- Current Board --\n" << toString() << std::endl;
    }
    if(bishops_.count() != (ply1_remain_[BISHOP] + ply2_remain_[BISHOP] + ply1_remain_[QUEEN] + ply2_remain_[QUEEN])){
        std::cout << "bishops.count: " << bishops_.count() <<std::endl;
        std::cout << "remain bishops (w/b): " << ply1_remain_[BISHOP] << ", " << ply2_remain_[BISHOP] << std::endl;
        std::cout << "remain queens (w/b): " << ply1_remain_[QUEEN] << ", " << ply2_remain_[QUEEN] << std::endl;
        rooks_.showBitboard();
        std::cout << "\n-- Current Board --\n" << toString() << std::endl;
    }
    if(knights_.count() != (ply1_remain_[KNIGHT] + ply2_remain_[KNIGHT])){
        std::cout << "knights.count: " << knights_.count() <<std::endl;
        std::cout << "remain knights (w/b): " << ply1_remain_[KNIGHT] << ", " << ply2_remain_[KNIGHT] << std::endl;
        rooks_.showBitboard();
        std::cout << "\n-- Current Board --\n" << toString() << std::endl;
    }
    assert(pawns_.countPawns() == (ply1_remain_[PAWN] + ply2_remain_[PAWN]));
    assert(knights_.count() == (ply1_remain_[KNIGHT] + ply2_remain_[KNIGHT]));
    assert(bishops_.count() == (ply1_remain_[BISHOP] + ply1_remain_[QUEEN] + ply2_remain_[BISHOP] + ply2_remain_[QUEEN]));
    assert(rooks_.count() == (ply1_remain_[ROOK] + ply1_remain_[QUEEN] + ply2_remain_[ROOK] + ply2_remain_[QUEEN]));
}

}
