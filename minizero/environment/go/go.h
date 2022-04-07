#pragma once

#include "base_env.h"
#include "configuration.h"
#include "go_block.h"
#include "go_grid.h"
#include "go_unit.h"
#include "sgf_loader.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace minizero::env::go {

extern GoHashKey turn_hash_key;
extern GoPair<std::vector<GoHashKey>> grids_hash_key;

void initialize();
GoHashKey getGoTurnHashKey();
GoHashKey getGoGridHashKey(int position, Player p);

class GoAction : public BaseAction {
public:
    GoAction() : BaseAction() {}
    GoAction(int action_id, Player player) : BaseAction(action_id, player) {}
    GoAction(const std::vector<std::string>& action_string_args, int board_size);

    inline Player nextPlayer() const override { return getNextPlayer(player_, kGoNumPlayer); }
    inline std::string toConsoleString() const override { return minizero::utils::SGFLoader::actionIDToBoardCoordinateString(getActionID(), minizero::config::env_go_board_size); }
};

class GoEnv : public BaseEnv<GoAction> {
public:
    friend class GoBenson;

    GoEnv(int board_size = minizero::config::env_go_board_size)
        : board_size_(board_size)
    {
        assert(board_size_ > 0 && board_size_ <= kMaxGoBoardSize);
        initialize();
        reset();
    }
    GoEnv(const GoEnv& env);

    void reset() override;
    bool act(const GoAction& action) override;
    bool act(const std::vector<std::string>& action_string_args) override;
    std::vector<GoAction> getLegalActions() const override;
    virtual bool isLegalAction(const GoAction& action) const override;
    bool isTerminal() const override;
    float getEvalScore(bool is_resign = false) const override;
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const GoAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::string toString() const override;

    inline std::string name() const override { return kGoName; }
    inline float getKomi() const { return komi_; }
    inline int getBoardSize() const { return board_size_; }

protected:
    void initialize();
    GoBlock* newBlock();
    void removeBlock(GoBlock* block);
    void removeBlockFromBoard(GoBlock* block);
    GoBlock* combineBlocks(GoBlock* block1, GoBlock* block2);
    std::string getCoordinateString() const;
    GoBitboard dilateBitboard(const GoBitboard& bitboard) const;
    GoPair<float> calculateTrompTaylorTerritory() const;
    bool checkDataStructure() const;

    inline bool isPassAction(const GoAction& action) const { return (action.getActionID() == board_size_ * board_size_); }

    int board_size_;
    float komi_;
    GoHashKey hash_key_;
    GoBitboard board_mask_bitboard_;
    GoBitboard board_left_boundary_bitboard_;
    GoBitboard board_right_boundary_bitboard_;
    GoBitboard free_block_id_bitboard_;
    GoPair<GoBitboard> stone_bitboard_;
    GoPair<GoBitboard> benson_bitboard;
    GoPair<GoBitboard>* benson_bitboard_p = &benson_bitboard;

    std::vector<GoGrid> grids_;
    std::vector<GoBlock> blocks_;
    std::vector<GoPair<GoBitboard>> stone_bitboard_history_;
    std::unordered_set<GoHashKey> hash_table_;
};

class GoEnvLoader : public BaseEnvLoader<GoAction, GoEnv> {
public:
    void loadFromEnvironment(const GoEnv& env, const std::vector<std::string> action_distributions = {})
    {
        BaseEnvLoader<GoAction, GoEnv>::loadFromEnvironment(env, action_distributions);
        addTag("KM", std::to_string(env.getKomi()));
        addTag("SZ", std::to_string(env.getBoardSize()));
    }

    inline int getBoardSize() const { return std::stoi(getTag("SZ")); }
    inline int getPolicySize() const override { return getBoardSize() * getBoardSize() + 1; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return getPositionByRotating(rotation, position, getBoardSize()); }
    inline std::string getEnvName() const { return kGoName; }
};

} // namespace minizero::env::go