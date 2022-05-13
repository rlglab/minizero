# include "chess.h"
# include <iostream>
namespace minizero::env::chess{

ChessAction::ChessAction(Player player, int from, int to, Pieces move_p, Pieces taken_p, Pieces promote_p, int id){
    player_ = player;
    move_piece_ = move_p;
    taken_piece_ = taken_p;
    promotion_piece_ = promote_p;
    from_ = from;
    to_ = to;
    shortcastle_ = false;
    longcastle_ = false;
    enpassant_ = false;
    if(id == -1){
        action_id_ = convertToID();
    }else
        action_id_ = id;
    assert(action_id_ > 0);
}

ChessAction::ChessAction(const std::vector<std::string>& action_string_args)
{
    // TODO: implement chess action constructor
}



int ChessAction::convertToID(){
    int pos = from_; // 0-63
    int row = to_ / 8 - from_ / 8;
    int col = to_ % 8 - from_ % 8;

    if(move_piece_ == Pieces::knight){
        // std::cout << "move: knight\n";
        if(row == 2 && col == 1){
            return (pos * 73) + 57;    // NNE(57)
        }else if(row == 1 && col == 2){
            return (pos * 73) + 58;    // NEE(58)
        }else if(row == -1 && col == 2){
            return (pos * 73) + 59;    // SEE(59)
        }else if(row == -2 && col == 1){
            return (pos * 73) + 60;    // SSE(60)
        }else if(row == -2 && col == -1){
            return (pos * 73) + 61;    // SSW(61)
        }else if(row == -1 && col == -2){
            return (pos * 73) + 62;    // SWW(62)
        }else if(row == 1 && col == -2){
            return (pos * 73) + 63;    // NWW(63)
        }else if(row == 2 && col == -1){
            return (pos * 73) + 64;    // NNW(64)
        }  
    // 8 * (#_squares-1)[0-6] + dir[N NE E SE S SW W NW (1-8)]
    // N1 NE1 E1 SE1 S1 SW1 W1 NW1 1~8 | 8*0 + [1-8]
    // N2 NE2 E2 SE2 S2 SW2 W2 NW2 9~16 | 8*1 + [1-8]
    // N8 NE8 E8 SE8 S8 SW8 W8 NW8 49~56
    // N=1, NE=2, E=3, SE=4, S=5, SW=6, W=7, NW=8
    }else if(move_piece_ == Pieces::king){
        // std::cout << "move: king\n";
        if(col == 2){
            shortcastle_ = true;
            return (pos * 73) + 11;  // E-2: 8 * 1 + 3
        }else if(col == -2){
            longcastle_ = true;
            return (pos * 73) + 15;  // W-2: 8 * 1 + 7
        }           
        if(row == 1 && col == 0)
            return (pos * 73) + 1;  // N-1: 8 * 0 + 1
        if(row == 1 && col == 1)
            return (pos * 73) + 2;  // NE-1: 8 * 0 + 2
        if(row == 0 && col == 1)
            return (pos * 73) + 3;  // E-1: 8 * 0 + 3
        if(row == -1 && col == 1)
            return (pos * 73) + 4;  // SE-1: 8 * 0 + 4
        if(row == -1 && col == 0)
            return (pos * 73) + 5;  // S-1: 8 * 0 + 5
        if(row == -1 && col == -1)
            return (pos * 73) + 6;  // SW-1: 8 * 0 + 6
        if(row == 0 && col == -1)
            return (pos * 73) + 7;  // W-1: 8 * 0 + 7
        if(row == 1 && col == -1)
            return (pos * 73) + 8;  // NW-1: 8 * 0 + 8
    }else if(move_piece_ == Pieces::bishop || move_piece_ == Pieces::queen){
        // std::cout << "move: bishop/queen\n";
        if(row == col){
            if(row > 0)
                return (pos * 73) + (8 * (row - 1) + 2);       // row+ col+ NE-row
            if(row < 0)
                return (pos * 73) + (8 * (-1 * row - 1) + 6);  // row- col- SW-(-row)
        }else if(row == -1 * col){
            if(row > 0)
                return (pos * 73) + (8 * (row - 1) + 8);       // row+ col- NW-(row)
            if(row < 0)
                return (pos * 73) + (8 * (-1 * row - 1) + 4);  // row- col+ SE-(-row)
        }
    }
    if(move_piece_ == Pieces::rook || move_piece_ == Pieces::queen){
        // std::cout << "move: rook/queen\n";
        if(row == 0){
            if(col < 0)
                return (pos * 73) + (8 * (from_ - to_ - 1) + 7);   // W
            else
                return (pos * 73) + (8 * (to_ - from_ - 1) + 3);   // E    
        }else if(col == 0){
            if(row < 0)
                return (pos * 73) + (8 * (-1 * row - 1) + 5) ;   // S-(-row-1)
            else
                return (pos * 73) + (8 * (row - 1) + 1);   // N-(row-1)
        }
    }
    else if(move_piece_ == Pieces::pawn){
        // std::cout << "move: pawn\n";
        // N=1, NE=2, E=3, SE=4, S=5, SW=6, W=7, NW=8
        if(promotion_piece_ == Pieces::empty || promotion_piece_ == Pieces::queen){
            // N-1, N-2, S-1, S-2, NE-1, NW-1, SE-1, SW-1
            if(row == 1){
                if(col == -1)
                    return (pos * 73) + 8; // NW-1
                if(col == 1)
                    return (pos * 73) + 2; // NE-1
                if(col == 0)
                    return (pos * 73) + 1; // N-1
            }
            if(row == -1){
                if(col == -1)
                    return (pos * 73) + 6; // SW-1
                if(col == 1)
                    return (pos * 73) + 4; // SE-1
                if(col == 0)
                    return (pos * 73) + 5; // S-1
            }
            if(row == 2){
                if(col == 0)
                    return (pos * 73) + (8 + 1);   // N-2
            }
            if(row == -2){
                if(col == 0)
                    return (pos * 73) + (8 + 5);   //S-2
            }
        }else{
            // 65-73 [64 + 3 * dir + pro]
            // direction(left-0, forward-1, right-2)
            // promotion(Rook-1, Bishop-2, Knight-3) --> 6 - promotion_piece_(R-5, B-4, N-3)
            if((row == 1 && to_ / 8 == 7) || (row == -1 && to_ / 8 == 0)){
                if(col == -1)
                    return (pos * 73) + (64 + 0 + (6 - static_cast<int>(promotion_piece_))); // 65-67
                else if(col == 0)
                    return (pos * 73) + (64 + 3 + (6 - static_cast<int>(promotion_piece_))); // 68-70
                else if(col == 1)
                    return (pos * 73) + (64 + 6 * (6 - static_cast<int>(promotion_piece_))); // 71-73   
            }                     
        }
    }
    std::cout << "convertToID (from, to, actionId): " << from_ << ", " << to_ << " | ";
    std::cout << "direction (row, col): " << row << ", " << col << std::endl;
    return -1;
}

bool ChessEnv::squareIsAttack(Player ply, int position, bool check_kings_attack) const {
    
    int square_col = positionToCol(position); 
    int square_row = positionToRow(position);

    Player opponent;
    Bitboard opponent_pieces;  

    if(ply == Player::kPlayer1){
        opponent = Player::kPlayer2;
        opponent_pieces = ply2_pieces_;
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
}
