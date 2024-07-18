#pragma once
#include <algorithm>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace minizero::env::tetris {

class TetrisBoard {
public:
    static const int WIDTH = 10;
    static const int HEIGHT = 22;

    // Colors
    static constexpr const char* RESET = "\033[0m";
    static constexpr const char* COLOR_O = "\033[93m";
    static constexpr const char* COLOR_I = "\033[96m";
    static constexpr const char* COLOR_L = "\033[95m";
    static constexpr const char* COLOR_J = "\033[94m";
    static constexpr const char* COLOR_S = "\033[92m";
    static constexpr const char* COLOR_Z = "\033[91m";
    static constexpr const char* COLOR_T = "\033[97m";
    static constexpr const char* COLOR_CURRENT = "\033[1;93m";

    TetrisBoard() { clear(); }

    void clear()
    {
        board_.assign(HEIGHT, std::vector<int>(WIDTH, 0));
        clearCurrentPiece();
    }

    int get(int x, int y) const
    {
        return board_[y][x];
    }

    void set(int x, int y, int value)
    {
        board_[y][x] = value;
    }

    bool isOccupied(int x, int y) const
    {
        return board_[y][x] != 0;
    }
    bool isCurrentPiece(int x, int y) const
    {
        for (const auto& [dx, dy] : current_piece_) {
            if (x == current_piece_x_ + dx && y == current_piece_y_ + dy) {
                return true;
            }
        }
        return false;
    }

    bool isLineFull(int y) const
    {
        return std::all_of(board_[y].begin(), board_[y].end(), [](int cell) { return cell != 0; });
    }

    void clearLine(int y)
    {
        for (int i = y; i > 0; --i) {
            board_[i] = board_[i - 1];
        }
        board_[0] = std::vector<int>(WIDTH, 0);
    }

    bool isValidPosition(int x, int y) const
    {
        return x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT;
    }

    int clearFullLines()
    {
        int lines_cleared = 0;
        for (int y = HEIGHT - 1; y >= 0; --y) {
            if (isLineFull(y)) {
                clearLine(y);
                ++lines_cleared;
                ++y; // Recheck this line
            }
        }
        return lines_cleared;
    }

    bool canPlacePiece() const
    {
        for (const auto& [dx, dy] : current_piece_) {
            int x = current_piece_x_ + dx;
            int y = current_piece_y_ + dy;
            if (!isValidPosition(x, y) || isOccupied(x, y)) {
                return false;
            }
        }
        return true;
    }

    void placePiece()
    {
        for (const auto& [dx, dy] : current_piece_) {
            set(current_piece_x_ + dx, current_piece_y_ + dy, current_piece_type_);
        }
    }

    void setCurrentPiece(const std::vector<std::pair<int, int>>& piece, int x, int y, int piece_type)
    {
        current_piece_ = piece;
        current_piece_x_ = x;
        current_piece_y_ = y;
        current_piece_type_ = piece_type + 1;
    }

    bool moveLeft(int steps = 1)
    {
        auto original_piece_x_ = current_piece_x_;
        for (int i = 0; i < steps; ++i) {
            --current_piece_x_;
            if (!canPlacePiece()) {
                current_piece_x_ = original_piece_x_;
                return false;
            }
        }
        return true;
    }

    bool moveRight(int steps = 1)
    {
        auto original_piece_x_ = current_piece_x_;
        for (int i = 0; i < steps; ++i) {
            ++current_piece_x_;
            if (!canPlacePiece()) {
                current_piece_x_ = original_piece_x_;
                return false;
            }
        }
        return true;
    }

    bool rotate(int rotations = 1)
    {
        auto original_piece = current_piece_;
        for (int r = 0; r < rotations; ++r) {
            for (auto& [x, y] : current_piece_) {
                int temp = x;
                x = -y;
                y = temp;
            }
            if (!canPlacePiece()) {
                current_piece_ = original_piece;
                return false;
            }
        }
        return true;
    }

    bool drop()
    {
        while (fall()) {}
        return true;
    }

    bool down()
    {
        fall();
        return true;
    }

    bool fall()
    {
        ++current_piece_y_;
        if (!canPlacePiece()) {
            --current_piece_y_;
            return false;
        }
        return true;
    }

    bool isAtBottom() const
    {
        if (current_piece_.empty()) { return true; }
        TetrisBoard temp = *this;
        ++temp.current_piece_y_;
        return !temp.canPlacePiece();
    }

    void clearCurrentPiece()
    {
        current_piece_.clear();
        current_piece_x_ = 0;
        current_piece_y_ = 0;
        current_piece_type_ = 0;
    }

    bool isGameOver() const
    {
        for (int x = 0; x < WIDTH; ++x) {
            if (isOccupied(x, 1)) {
                return true;
            }
        }
        return false;
    }

    bool generateNewPiece(int piece_type)
    {
        static const std::vector<std::vector<std::pair<int, int>>> tetrominos = {
            {{-1, -1}, {0, -1}, {-1, 0}, {0, 0}}, // O
            {{-1, 0}, {0, 0}, {1, 0}, {2, 0}},    // I
            {{-1, 0}, {0, 0}, {1, -1}, {1, 0}},   // L
            {{-1, -1}, {-1, 0}, {0, 0}, {1, 0}},  // J
            {{-1, 0}, {0, 0}, {0, -1}, {1, -1}},  // S
            {{-1, -1}, {0, -1}, {0, 0}, {1, 0}},  // Z
            {{-1, 0}, {0, 0}, {1, 0}, {0, -1}}    // T
        };

        setCurrentPiece(tetrominos[piece_type], WIDTH / 2 - 1, 1, piece_type);
        return canPlacePiece();
    }

    bool isLegalAction(int rotation, int movement) const
    {
        TetrisBoard temp = *this;
        return temp.rotate(rotation) && (movement < 0 ? temp.moveLeft(-movement) : temp.moveRight(movement));
    }

    friend std::ostream& operator<<(std::ostream& out, const TetrisBoard& b)
    {
        out << "+--------------------+" << std::endl;
        for (int y = 0; y < HEIGHT; ++y) {
            out << "|";
            for (int x = 0; x < WIDTH; ++x) {
                if (b.isOccupied(x, y)) {
                    out << b.getColorForPiece(b.board_[y][x]) << "[]" << RESET;
                } else if (b.isCurrentPiece(x, y)) {
                    out << COLOR_CURRENT << "()" << RESET;
                } else {
                    out << "  ";
                }
            }
            out << "|" << std::endl;
        }
        out << "+--------------------+" << std::endl;
        return out;
    }

private:
    std::vector<std::vector<int>> board_;
    std::vector<std::pair<int, int>> current_piece_;
    int current_piece_x_, current_piece_y_;
    int current_piece_type_;
    static std::string getColorForPiece(int piece_type)
    {
        switch (piece_type) {
            case 1: return COLOR_O;
            case 2: return COLOR_I;
            case 3: return COLOR_L;
            case 4: return COLOR_J;
            case 5: return COLOR_S;
            case 6: return COLOR_Z;
            case 7: return COLOR_T;
            default: return RESET;
        }
    }
};

} // namespace minizero::env::tetris
