#include "board.h"
#include "color_message.h"
#include <iostream>

namespace minizero::env::santorini {

using namespace minizero::utils;
using namespace std;

Board::Board()
{
    plane_[0] = Bitboard::kValidSpace;
    for (int i = 1; i < 5; ++i) {
        plane_[i] = Bitboard();
    }
    for (auto& p : player_) {
        for (auto& it : p) {
            it = Bitboard();
        }
    }
    setPlayer(0);
    setPlayer(1);
}

Board::Board(const Board& board)
{
    for (int i = 0; i < 5; ++i) {
        plane_[i] = board.plane_[i];
    }
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            player_[i][j] = board.player_[i][j];
        }
    }
}

vector<pair<int, int>> Board::getLegalMove(int p_id) const
{
    vector<pair<int, int>> ret;
    for (auto& p : player_[p_id]) {
        auto from = p.toList()[0];
        auto avail = Bitboard();
        for (int i = 0; i < 4; ++i) {
            if (plane_[i] & p) {
                for (int j = 0; j <= i + 1; ++j) {
                    avail |= plane_[j];
                }
                break;
            }
        }

        // All possibel legal moves on the current position.
        Bitboard movable = Bitboard::getNeighbor(from) & avail;

        // Piece can not overlap any other piece.
        for (auto& e : player_[p_id ^ 1]) {
            movable &= (~e);
        }
        for (auto& e : player_[p_id]) {
            movable &= (~e);
        }

        // Push the legal moves into list.
        for (auto& to : movable.toList()) {
            ret.push_back(make_pair(from, to));
        }
    }
    return ret;
}

vector<int> Board::getLegalBuild(int idx) const
{
    Bitboard avail = Bitboard();
    for (int i = 0; i < 4; ++i) {
        avail |= plane_[i];
    }

    // All possibel legal moves on the current position.
    Bitboard buildable = Bitboard::getNeighbor(idx) & avail;
    for (auto& p : player_) {
        for (auto& e : p) {
            // We can not set the buidling on the pieces.
            buildable &= (~e);
        }
    }
    return buildable.toList();
}

std::pair<int, int> Board::getPlayerIdx(int p_id) const
{
    return std::make_pair(player_[p_id][0].toList()[0], player_[p_id][1].toList()[0]);
}

const std::array<Bitboard, 5>& Board::getPlanes() const
{
    return plane_;
}

bool Board::isTerminal(int p_id) const
{
    if (player_[0][0] == 1 || player_[0][1] == 1 || player_[1][0] == 1 || player_[1][1] == 1) {
        // Someone doesn't play the first move yet.
        return false;
    }
    if (checkWin(p_id) || checkWin(p_id ^ 1)) { return true; }
    if (getLegalMove(p_id).empty()) { return true; }

    return false;
}
bool Board::checkWin(int p_id) const
{
    // Return true if current ID's player win the game. The first player
    // moving his piece to the 3rd lever building is the winner.
    if (((player_[p_id][0] | player_[p_id][1]) & plane_[3]) != 0) {
        return true;
    }
    return false;
}

string Board::toConsole() const
{
    static vector<pair<TextColor, TextColor>> palette = {
        make_pair(TextColor::kBlack, TextColor::kWhite),
        make_pair(TextColor::kBlack, TextColor::kCyan),
        make_pair(TextColor::kBlack, TextColor::kBlue),
        make_pair(TextColor::kBlack, TextColor::kPurple),
        make_pair(TextColor::kBlack, TextColor::kRed)};

    string ret = "";
    ret += " ";
    for (int i = 1; i < 6; ++i) {
        ret += "  ";
        ret += static_cast<char>('A' + i - 1);
    }
    ret += "\n";
    for (int i = 5; i >= 1; --i) {
        ret += std::to_string(i);
        ret += " ";
        for (int j = 1; j < 6; ++j) {
            Bitboard now(1UL << (i * 7 + j));
            bool hasPiece = false;
            for (auto& p : player_[0]) {
                if (p & now) {
                    ret += getColorText("P", TextType::kBold, TextColor::kBlack, TextColor::kYellow);
                    hasPiece = true;
                    break;
                }
            }
            for (auto& p : player_[1]) {
                if (p & now) {
                    ret += getColorText("P", TextType::kBold, TextColor::kBlack, TextColor::kGreen);
                    hasPiece = true;
                    break;
                }
            }

            for (int i = 0; i < 5; ++i) {
                if (now & plane_[i]) {
                    if (!hasPiece) {
                        ret += getColorText(" ", TextType::kNormal, palette[i].first, palette[i].second);
                    }
                    ret += getColorText(to_string(i), TextType::kNormal, palette[i].first, palette[i].second);
                    break;
                }
            }
            ret += " ";
        }
        ret += std::to_string(i);
        ret += "\n";
    }
    ret += " ";
    for (int i = 1; i < 6; ++i) {
        ret += "  ";
        ret += static_cast<char>('A' + i - 1);
    }
    ret += "\n";
    return ret;
}

bool Board::setPlayer(int p_id, int idx_1, int idx_2)
{
    player_[p_id][0] = Bitboard(1UL << idx_1);
    player_[p_id][1] = Bitboard(1UL << idx_2);
    return true;
}

bool Board::movePiece(int from, int to)
{
    Bitboard from_board(1UL << from);
    Bitboard to_board(1UL << to);
    return movePiece(from_board, to_board);
}

bool Board::movePiece(Bitboard from, Bitboard to)
{
    for (auto& p : player_) {
        for (auto& piece : p) {
            if ((piece & from) != 0) {
                piece = to;
                return (piece & Bitboard(Bitboard::kValidSpace)) != 0;
            }
        }
    }
    return false;
}

bool Board::buildTower(int idx)
{
    Bitboard b(1UL << idx);
    for (int i = 0; i < 4; ++i) {
        if (plane_[i] & b) {
            plane_[i] ^= b;
            plane_[i + 1] ^= b;
            return true;
        }
    }
    return false;
}

} // namespace minizero::env::santorini
