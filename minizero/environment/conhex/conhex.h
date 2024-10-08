#pragma once

#include "base_env.h"
#include "configuration.h"
#include "conhex_graph.h"
#include <bitset>
#include <set>
#include <string>
#include <vector>

namespace minizero::env::conhex {

const std::string kConHexName = "conhex";
const int kConHexNumPlayer = 2;

typedef BaseBoardAction<kConHexNumPlayer> ConHexAction;

class ConHexEnv : public BaseBoardEnv<ConHexAction> {
public:
    ConHexEnv();

    void reset() override;
    bool act(const ConHexAction& action) override;
    bool act(const std::vector<std::string>& action_string_args) override;
    std::vector<ConHexAction> getLegalActions() const override;
    bool isLegalAction(const ConHexAction& action) const override;
    bool isTerminal() const override;
    float getReward() const override { return 0.0f; }
    float getEvalScore(bool is_resign = false) const override;
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const ConHexAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline int getNumInputChannels() const override { return 6; }
    inline int getPolicySize() const override { return getBoardSize() * getBoardSize(); }
    std::string toString() const override;
    inline std::string name() const override { return kConHexName; }
    inline int getNumPlayer() const override { return kConHexNumPlayer; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; }
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return action_id; }

private:
    bool isPlaceable(int table_id) const;

    ConHexGraph conhex_graph_;
    ConHexBitboard invalid_actions_;
};

class ConHexEnvLoader : public BaseBoardEnvLoader<ConHexAction, ConHexEnv> {
public:
    std::vector<float> getActionFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline std::vector<float> getValue(const int pos) const { return {getReturn()}; }
    inline std::string name() const override { return kConHexName; }
    inline int getPolicySize() const override { return getBoardSize() * getBoardSize(); }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; }
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return action_id; }
};

} // namespace minizero::env::conhex
