#pragma once

#include "base_env.h"
#include "go_block.h"
#include "go_grid.h"
#include "go_unit.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace minizero::env::go {

extern GoHashKey turn_hash_key;
extern GoPair<std::vector<GoHashKey>> grids_hash_key;

void Initialize();
GoHashKey GetGoTurnHashKey();
GoHashKey GetGoGridHashKey(int position, Player p);

class GoAction : public BaseAction {
public:
    GoAction() : BaseAction() {}
    GoAction(int action_id, Player player) : BaseAction(action_id, player) {}
    GoAction(const std::vector<std::string>& action_string_args, int board_size);

    inline Player NextPlayer() const override { return GetNextPlayer(player_, kGoNumPlayer); }
};

class GoEnv : public BaseEnv<GoAction> {
public:
    friend class GoBenson;

    GoEnv(int board_size = kDefaultGoBoardSize)
        : board_size_(board_size)
    {
        assert(board_size_ > 0 && board_size_ <= kMaxGoBoardSize);
        Initialize();
        Reset();
    }
    GoEnv(const GoEnv& env);

    void Reset() override;
    void Reset(int board_size);
    bool Act(const GoAction& action) override;
    bool Act(const std::vector<std::string>& action_string_args) override;
    std::vector<GoAction> GetLegalActions() const override;
    bool IsLegalAction(const GoAction& action) const override;
    bool IsTerminal() const override;
    float GetEvalScore() const override;
    std::vector<float> GetFeatures() const override;
    std::string ToString() const override;

    inline std::string Name() const override { return kGoName; }
    inline float GetKomi() const { return komi_; }
    inline int GetBoardSize() const { return board_size_; }

private:
    void Initialize();
    GoBlock* NewBlock();
    void RemoveBlock(GoBlock* block);
    void RemoveBlockFromBoard(GoBlock* block);
    GoBlock* CombineBlocks(GoBlock* block1, GoBlock* block2);
    std::string GetCoordinateString() const;
    GoBitboard DilateBitboard(const GoBitboard& bitboard) const;
    GoPair<float> CalculateTrompTaylorTerritory() const;
    bool CheckDataStructure() const;

    inline bool IsPassAction(const GoAction& action) const { return (action.GetActionID() == board_size_ * board_size_); }

    int board_size_;
    float komi_;
    GoHashKey hash_key_;
    GoBitboard board_mask_bitboard_;
    GoBitboard board_left_boundary_bitboard_;
    GoBitboard board_right_boundary_bitboard_;
    GoBitboard free_block_id_bitboard_;
    GoPair<GoBitboard> stone_bitboard_;
    std::vector<GoGrid> grids_;
    std::vector<GoBlock> blocks_;
    std::vector<GoPair<GoBitboard>> stone_bitboard_history_;
    std::unordered_set<GoHashKey> hash_table_;
};

class GoEnvLoader : public BaseEnvLoader<GoAction, GoEnv> {
public:
    void LoadFromEnvironment(const GoEnv& env, const std::vector<std::string> action_distributions = {})
    {
        BaseEnvLoader<GoAction, GoEnv>::LoadFromEnvironment(env, action_distributions);
        AddTag("KM", std::to_string(env.GetKomi()));
        AddTag("SZ", std::to_string(env.GetBoardSize()));
    }

    inline std::vector<float> GetPolicyDistribution(int id) const
    {
        int board_size = std::stoi(GetTag("SZ"));
        std::vector<float> policy(board_size * board_size, 0.0f);
        SetPolicyDistribution(id, policy);
        return policy;
    }

    inline std::string GetEnvName() const { return kGoName; }
};

} // namespace minizero::env::go