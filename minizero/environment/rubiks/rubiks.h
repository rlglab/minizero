#pragma once

#include "base_env.h"
#include "configuration.h"
#include "random.h"
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace minizero::env::rubiks {

const std::string kRubiksName = "rubiks";
const int kRubiksNumPlayer = 1;
const int kMaxRubiksBoardSize = 3;
const int kMaxRotateNum = 30;

const int kCubeFace = 6;

/**
 *    Colors for each of the six faces of the cube:
 *    W(White), O(Orange), G(Green), R(Red), Y(Yellow), B(Blue)
 *
 *            ______
 *           |      |
 *           | 0: W |
 *     ______|______|______ ______
 *    |      |      |      |      |
 *    | 1: O | 2: G | 3: R | 4: Y |
 *    |______|______|______|______|
 *           |      |
 *           | 5: B |
 *           |______|
 *      
 */
const std::string kCubeColorOrder = "WOGRYB";

/**
 *    Notation for six faces of the cube:
 *    U(Up), L(Left), F(Front), R(Right), B(Back), D(Down)
 *
 *            ______
 *           |      |
 *           | 0: U |
 *     ______|______|______ ______
 *    |      |      |      |      |
 *    | 1: L | 2: F | 3: R | 4: B |
 *    |______|______|______|______|
 *           |      |
 *           | 5: D |
 *           |______|
 *
 *
 *    The kCubeRotateSide defines both the four faces of the cube
 *    and the stickers on those faces that will be affected
 *    when a specific rotation is applied.
 *
 *    e.g. "Front" rotation for 3*3 cube,
 *    swap the corresponding sticker:
 *
 *            ______                           ______                           ______                   
 *           |      |                         |      |                         |      |               
 *           |      |                         |      |                         |      |                 
 *     ______|!!@@##|______ ______      ______|!!@@##|______ ______      ______|______|______ ______    
 *    |    ##|      |      |      |    |      |      |!!    |      |    |      |      |!!    |      |   
 *    |    @@|  F   |      |      |    |      |  F   |@@    |      |    |      |  F   |@@    |      | 
 *    |____!!|______|______|______|    |______|______|##____|______|    |______|______|##____|______|  
 *           |      |                         |      |                         |##@@!!|                 
 *           |      |                         |      |                         |      |                
 *           |______|                         |______|                         |______|              
 *
 *
 *    The four numbers' meanings:
 *    1st number: The face need to be rotated (0~5)
 *    2nd~4th number: 
 *    - 000:  x := ly,             y := bs
 *    - 001:  x := ly,             y := board_size-bs
 *    - 010:  x := board_size-ly,  y := bs
 *    - 011:  x := board_size-ly,  y := bs
 *    - 100:  x := bs,             y := ly
 *    - 101:  x := board_size-bs,  y := ly
 *    - 110:  x := bs,             y := board_size-ly
 *    - 111:  x := board_size-bs,  y := board_size-ly
 *
 *    e.g. For a clockwise rotation 
 *    rotate()
 *      for i from 0 to 2:
 *        for ly from 0 to layer:
 *          for bs from 0 to board_size:
 *            swap(board[face_0][x_0][y_0], swap[face_1][x_1][y_1])
 */
const std::vector<std::vector<std::vector<int>>> kCubeRotateSide = {
    {
        {1, 0, 0, 0}, {2, 0, 0, 0}, {3, 0, 0, 0}, {4, 0, 0, 0} // Up
    },
    {
        {0, 1, 0, 1}, {4, 1, 1, 0}, {5, 1, 0, 1}, {2, 1, 0, 1} // Left
    },
    {
        {0, 0, 1, 1}, {1, 1, 1, 0}, {5, 0, 0, 0}, {3, 1, 0, 1} // Front
    },
    {
        {0, 1, 1, 0}, {2, 1, 1, 0}, {5, 1, 1, 0}, {4, 1, 0, 1} // Right
    },
    {
        {0, 0, 0, 0}, {3, 1, 1, 0}, {5, 0, 1, 1}, {1, 1, 0, 1} // Back
    },
    {
        {1, 0, 1, 1}, {4, 0, 1, 1}, {3, 0, 1, 1}, {2, 0, 1, 1} // Down
    }};

const std::string kRubiksActionName[] = {
    "up", "left", "front", "right", "back", "down",
    "up_p", "left_p", "front_p", "right_p", "back_p", "down_p"};

class RubiksAction : public BaseAction {
public:
    RubiksAction() : BaseAction() {}
    RubiksAction(int action_id, Player player) : BaseAction(action_id, player) {}
    RubiksAction(const std::vector<std::string>& action_string_args) : BaseAction() {} // TODO

    inline Player nextPlayer() const override { return Player::kPlayer1; }
    inline std::string toConsoleString() const { return kRubiksActionName[action_id_ % 12] + "_" + std::to_string(action_id_ / 12 + 1); }
};

class RubiksEnv : public BaseBoardEnv<RubiksAction> {
public:
    RubiksEnv()
    {
        assert(getBoardSize() <= kMaxRubiksBoardSize);
        reset();
    }

    void reset() override { reset(utils::Random::randInt(), minizero::config::env_rubiks_scramble_rotate); }
    void reset(int seed, int scramble);
    bool act(const RubiksAction& action) override;
    bool act(const std::vector<std::string>& action_string_args) override;
    std::vector<RubiksAction> getLegalActions() const override;
    bool isLegalAction(const RubiksAction& action) const override;
    bool isTerminal() const override;
    float getReward() const override { return 0.0f; }

    float getEvalScore(bool is_resign = false) const override;
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const RubiksAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::string toString() const override;
    inline std::string name() const override { return kRubiksName + std::to_string(getBoardSize()) + "x" + std::to_string(getBoardSize()); }
    inline int getNumPlayer() const override { return kRubiksNumPlayer; }

    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return utils::getPositionByRotating(utils::Rotation::kRotationNone, position, getBoardSize()); };
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return getRotatePosition(action_id, utils::Rotation::kRotationNone); };

    inline int getSeed() const { return seed_; }
    inline int getScramble() const { return scramble_; }

private:
    void transpose(int face);
    void rotate(int face, int layer, bool prime);
    bool checkSolved() const;

    /**
     *    e.g. 3*3 cube: board_[6][3][3]
     *   
     *            ______
     *           |      |
     *           |  0   |
     *     ______|______|______ ______
     *    |      |      |      |      |
     *    |  1   |  2   |  3   |  4   |
     *    |______|______|______|______|
     *           |      |
     *           |  5   |
     *           |______|
     *
     */
    std::vector<std::vector<std::vector<char>>> board_;

    std::mt19937 random_;
    int seed_;
    int scramble_;
};

class RubiksEnvLoader : public BaseBoardEnvLoader<RubiksAction, RubiksEnv> {
public:
    void loadFromEnvironment(const RubiksEnv& env, const std::vector<std::vector<std::pair<std::string, std::string>>>& action_info_history = {}) override
    {
        BaseBoardEnvLoader<RubiksAction, RubiksEnv>::loadFromEnvironment(env, action_info_history);
        addTag("SD", std::to_string(env.getSeed()));
        addTag("SC", std::to_string(env.getScramble()));
    }

    inline int getSeed() const { return std::stoi(BaseBoardEnvLoader<RubiksAction, RubiksEnv>::getTag("SD")); }
    inline int getScramble() const { return std::stoi(BaseBoardEnvLoader<RubiksAction, RubiksEnv>::getTag("SC")); }

    std::vector<float> getFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline std::vector<float> getValue(const int pos) const { return {getReturn()}; }
    inline std::string name() const override { return kRubiksName + std::to_string(getBoardSize()) + "x" + std::to_string(getBoardSize()); }
    inline int getPolicySize() const override { return getBoardSize() / 2 * 12; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return utils::getPositionByRotating(utils::Rotation::kRotationNone, position, getBoardSize()); }
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return getRotatePosition(action_id, utils::Rotation::kRotationNone); }
};

} // namespace minizero::env::rubiks
