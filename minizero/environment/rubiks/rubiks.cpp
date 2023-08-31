#include "rubiks.h"
#include "color_message.h"
#include "random.h"
#include "sgf_loader.h"
#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>

namespace minizero::env::rubiks {

using namespace minizero::utils;

void RubiksEnv::reset(int seed, int scramble)
{
    turn_ = Player::kPlayer1;
    random_.seed(seed_ = seed);
    board_.resize(kCubeFace);
    scramble_ = scramble;
    for (int face = 0; face < kCubeFace; face++) {
        board_[face].resize(board_size_);
        for (int row = 0; row < board_size_; row++) {
            board_[face][row].resize(board_size_);
        }
    }
    for (int face = 0; face < kCubeFace; face++) {
        for (int row = 0; row < board_size_; row++) {
            for (int col = 0; col < board_size_; col++) {
                board_[face][row][col] = kCubeColorOrder[face];
            }
        }
    }
    while (scramble--) {
        act(RubiksAction(std::uniform_int_distribution<int>(0, board_size_ / 2 * 12 - 1)(random_), turn_));
    }
    actions_.clear();
}

bool RubiksEnv::act(const RubiksAction& action)
{
    actions_.push_back(action);
    int id = action.getActionID();
    int face = id % 6;
    int layer = id / 12 + 1;
    bool prime = (id % 12) >= 6;
    rotate(face, layer, prime);
    return true;
}

bool RubiksEnv::act(const std::vector<std::string>& action_string_args)
{
    return act(RubiksAction(action_string_args));
}

std::vector<RubiksAction> RubiksEnv::getLegalActions() const
{
    std::vector<RubiksAction> actions;
    for (int i = 0; i < board_size_ / 2 * 12; i++) {
        RubiksAction action(i, turn_);
        if (!isLegalAction(action)) { continue; }
        actions.push_back(action);
    }
    return actions;
}

bool RubiksEnv::isLegalAction(const RubiksAction& action) const
{
    return true;
}

bool RubiksEnv::isTerminal() const
{
    return (checkSolved() || static_cast<int>(actions_.size()) >= kMaxRotateNum);
}

float RubiksEnv::getEvalScore(bool is_resign /*= false*/) const
{
    return (checkSolved() ? 1.0f : -1.0f);
}

bool RubiksEnv::checkSolved() const
{
    for (int face = 0; face < kCubeFace; face++) {
        for (int row = 0; row < board_size_; row++) {
            for (int col = 0; col < board_size_; col++) {
                if (board_[face][row][col] != kCubeColorOrder[face]) {
                    return false;
                }
            }
        }
    }
    return true;
}

std::vector<float> RubiksEnv::getFeatures(utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    std::vector<float> features;
    for (int color = 0; color < kCubeFace; ++color) {
        for (int face = 0; face < kCubeFace; face++) {
            for (int row = 0; row < board_size_; row++) {
                for (int col = 0; col < board_size_; col++) {
                    features.push_back(board_[face][row][col] == kCubeColorOrder[color] ? 1.0f : 0.0f);
                }
            }
        }
    }
    return features;
}

std::vector<float> RubiksEnv::getActionFeatures(const RubiksAction& action, utils::Rotation rotation /*= utils::Rotation::kRotationNone*/) const
{
    // TODO
    return {};
}

void RubiksEnv::transpose(int face)
{
    for (int row = 0; row < board_size_; row++) {
        for (int col = row + 1; col < board_size_; col++) {
            std::swap(board_[face][row][col], board_[face][col][row]);
        }
    }
}

void RubiksEnv::rotate(int face, int layer, bool prime)
{
    const std::vector<std::vector<int>>& sides = kCubeRotateSide[face];
    if (prime) {
        transpose(face);
        for (int i = 2; i >= 0; i--) {
            for (int ly = 0; ly < layer; ly++) {
                for (int bs = 0; bs < board_size_; bs++) {
                    int ax = sides[i][1] ? (sides[i][3] ? board_size_ - bs - 1 : bs) : (sides[i][2] ? board_size_ - ly - 1 : ly);
                    int ay = sides[i][1] ? (sides[i][2] ? board_size_ - ly - 1 : ly) : (sides[i][3] ? board_size_ - bs - 1 : bs);
                    int bx = sides[i + 1][1] ? (sides[i + 1][3] ? board_size_ - bs - 1 : bs) : (sides[i + 1][2] ? board_size_ - ly - 1 : ly);
                    int by = sides[i + 1][1] ? (sides[i + 1][2] ? board_size_ - ly - 1 : ly) : (sides[i + 1][3] ? board_size_ - bs - 1 : bs);
                    std::swap(board_[sides[i][0]][ax][ay], board_[sides[i + 1][0]][bx][by]);
                }
            }
        }
    }
    for (int i = 0; i < board_size_ / 2; i++) {
        for (int j = 0; j < board_size_; j++) {
            std::swap(board_[face][i][j], board_[face][board_size_ - i - 1][j]);
        }
    }
    if (!prime) {
        transpose(face);
        for (int i = 1; i < 4; i++) {
            for (int ly = 0; ly < layer; ly++) {
                for (int bs = 0; bs < board_size_; bs++) {
                    int ax = sides[i][1] ? (sides[i][3] ? board_size_ - bs - 1 : bs) : (sides[i][2] ? board_size_ - ly - 1 : ly);
                    int ay = sides[i][1] ? (sides[i][2] ? board_size_ - ly - 1 : ly) : (sides[i][3] ? board_size_ - bs - 1 : bs);
                    int bx = sides[i - 1][1] ? (sides[i - 1][3] ? board_size_ - bs - 1 : bs) : (sides[i - 1][2] ? board_size_ - ly - 1 : ly);
                    int by = sides[i - 1][1] ? (sides[i - 1][2] ? board_size_ - ly - 1 : ly) : (sides[i - 1][3] ? board_size_ - bs - 1 : bs);
                    std::swap(board_[sides[i][0]][ax][ay], board_[sides[i - 1][0]][bx][by]);
                }
            }
        }
    }
}

std::string RubiksEnv::toString() const
{
    std::ostringstream oss;
    std::unordered_map<char, std::string> color_code_to_rgb({{'G', "\033[48;2;0;155;72m"}, {'W', "\033[48;2;255;255;255m"}, {'R', "\033[48;2;183;18;52m"}, {'Y', "\033[48;2;255;213;0m"}, {'B', "\033[48;2;0;70;173m"}, {'O', "\033[48;2;255;88;0m"}});
    for (int row = 0; row < board_size_; row++) {
        for (int col = 0; col < board_size_; col++) oss << "  ";
        for (int col = 0; col < board_size_; col++) {
            oss << color_code_to_rgb[board_[0][row][col]] + "  \033[m";
        }
        oss << std::endl;
    }
    for (int row = 0; row < board_size_; row++) {
        for (int col = 0; col < board_size_ * 4; col++) {
            oss << color_code_to_rgb[board_[col / board_size_ + 1][row][col % board_size_]] + "  \033[m";
        }
        oss << std::endl;
    }
    for (int row = 0; row < board_size_; row++) {
        for (int col = 0; col < board_size_; col++) oss << "  ";
        for (int col = 0; col < board_size_; col++) {
            oss << color_code_to_rgb[board_[5][row][col]] + "  \033[m";
        }
        oss << std::endl;
    }
    return oss.str();
}

std::vector<float> RubiksEnvLoader::getFeatures(const int pos, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    RubiksEnv env;
    env.reset(getSeed(), getScramble());
    for (int i = 0; i < std::min(pos, static_cast<int>(action_pairs_.size())); ++i) { env.act(action_pairs_[i].first); }
    return env.getFeatures(rotation);
}

std::vector<float> RubiksEnvLoader::getActionFeatures(const int pos, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    // TODO
    return {};
}

} // namespace minizero::env::rubiks
