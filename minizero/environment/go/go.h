#pragma once

#include "base_env.h"
#include "configuration.h"
#include "go_area.h"
#include "go_block.h"
#include "go_grid.h"
#include "go_unit.h"
#include "sgf_loader.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace minizero::env::go {

extern GoHashKey turn_hash_key;
extern std::vector<GoHashKey> empty_hash_key;
extern std::vector<GamePair<GoHashKey>> grids_hash_key;
extern std::vector<std::vector<GamePair<GoHashKey>>> sequence_hash_key;

void initialize();
GoHashKey getGoTurnHashKey();
GoHashKey getGoEmptyHashKey(int position);
GoHashKey getGoGridHashKey(int position, Player p);
GoHashKey getGoSequenceHashKey(int move, int position, Player p);

class GoAction : public BaseAction {
public:
    GoAction() : BaseAction() {}
    GoAction(int action_id, Player player) : BaseAction(action_id, player) {}
    GoAction(const std::vector<std::string>& action_string_args, int board_size);

    inline Player nextPlayer() const override { return getNextPlayer(player_, kGoNumPlayer); }
    inline std::string toConsoleString() const override { return minizero::utils::SGFLoader::actionIDToBoardCoordinateString(getActionID(), minizero::config::env_board_size); }
};

class GoEnv : public BaseEnv<GoAction> {
public:
    friend class GoBenson;

    GoEnv()
        : board_size_(minizero::config::env_board_size)
    {
        assert(board_size_ > 0 && board_size_ <= kMaxGoBoardSize);
        initialize();
        reset();
    }
    GoEnv(const GoEnv& env) { *this = env; }
    GoEnv& operator=(const GoEnv& env);

    void reset() override;
    bool act(const GoAction& action) override;
    bool act(const std::vector<std::string>& action_string_args) override;
    std::vector<GoAction> getLegalActions() const override;
    bool isLegalAction(const GoAction& action) const override;
    bool isTerminal() const override;
    float getReward() const override { return 0.0f; }
    float getEvalScore(bool is_resign = false) const override;
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const GoAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::string toString() const override;
    GoBitboard dilateBitboard(const GoBitboard& bitboard) const;

    inline std::string name() const override { return kGoName; }
    inline int getNumPlayer() const override { return kGoNumPlayer; }
    inline int getBoardSize() const { return board_size_; }
    inline float getKomi() const { return komi_; }
    inline GoHashKey getHashKey() const { return hash_key_; }
    inline const GoBitboard& getBoardMaskBitboard() const { return board_mask_bitboard_; }
    inline const GoBitboard& getFreeAreaIDBitBoard() const { return free_area_id_bitboard_; }
    inline const GoBitboard& getFreeBlockIDBitBoard() const { return free_block_id_bitboard_; }
    inline const GamePair<GoBitboard>& getStoneBitboard() const { return stone_bitboard_; }
    inline const GamePair<GoBitboard>& getBensonBitboard() const { return benson_bitboard_; }
    inline const GoGrid& getGrid(int id) const { return grids_[id]; }
    inline const GoArea& getArea(int id) const { return areas_[id]; }
    inline const GoBlock& getBlock(int id) const { return blocks_[id]; }
    inline bool isPassAction(const GoAction& action) const { return (action.getActionID() == board_size_ * board_size_); }
    inline const std::vector<GoHashKey>& getHashKeyHistory() const { return hashkey_history_; }
    inline const std::unordered_set<GoHashKey>& getHashTable() const { return hash_table_; }

protected:
    void initialize();
    GoBlock* newBlock();
    void removeBlock(GoBlock* block);
    void removeBlockFromBoard(GoBlock* block);
    GoBlock* combineBlocks(GoBlock* block1, GoBlock* block2);
    void updateArea(const GoAction& action);
    void addArea(Player player, const GoBitboard& area_bitboard);
    void removeArea(GoArea* area);
    GoArea* mergeArea(GoArea* area1, GoArea* area2);
    std::vector<GoBitboard> findAreas(const GoAction& action);
    void updateBenson(const GoAction& action);
    GoBitboard findBensonBitboard(GoBitboard block_bitboard) const;
    std::string getCoordinateString() const;
    GoBitboard floodFillBitBoard(int start_position, const GoBitboard& boundary_bitboard) const;
    GamePair<float> calculateTrompTaylorTerritory() const;

    // check data structure (for debugging)
    bool checkDataStructure() const;
    bool checkGridDataStructure() const;
    bool checkBlockDataStructure() const;
    bool checkAreaDataStructure() const;
    bool checkBensonDataStructure() const;

    int board_size_;
    float komi_;
    GoHashKey hash_key_;
    GoBitboard board_mask_bitboard_;
    GoBitboard board_left_boundary_bitboard_;
    GoBitboard board_right_boundary_bitboard_;
    GoBitboard free_area_id_bitboard_;
    GoBitboard free_block_id_bitboard_;
    GamePair<GoBitboard> stone_bitboard_;
    GamePair<GoBitboard> benson_bitboard_;

    std::vector<GoGrid> grids_;
    std::vector<GoArea> areas_;
    std::vector<GoBlock> blocks_;
    std::vector<GamePair<GoBitboard>> stone_bitboard_history_;
    std::vector<GoHashKey> hashkey_history_;
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
