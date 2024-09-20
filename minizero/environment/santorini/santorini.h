#pragma once

#include "base_env.h"
#include "board.h"
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace minizero::env::santorini {

const std::string kSantoriniName = "santorini";
const int kSantoriniNumPlayer = 2;
const int kSantoriniBoardSize = 5;
const int kSantoriniLetterBoxSize = kSantoriniBoardSize + 2;
const int kSantoriniPolicySize = 1900;
const int kSantoriniHistoryLength = 8;
const int kSantoriniHistorySize = 6;

inline int positionToLetterBoxIdx(int pos) { return (pos / kSantoriniBoardSize + 1) * kSantoriniLetterBoxSize + pos % kSantoriniBoardSize + 1; }
inline int letterBoaxIdxToposition(int idx) { return (idx / kSantoriniLetterBoxSize - 1) * kSantoriniBoardSize + idx % kSantoriniLetterBoxSize - 1; }

class SantoriniAction : public BaseBoardAction<kSantoriniNumPlayer> {
public:
    SantoriniAction() : BaseBoardAction<kSantoriniNumPlayer>() {}
    SantoriniAction(int action_id, Player player);
    SantoriniAction(const std::vector<std::string>& action_string_args);
    std::string toConsoleString() const override;

    inline int getFrom() const { return from_; }
    inline int getTo() const { return to_; }
    inline int getBuild() const { return build_; }

private:
    int encodePlaced(int x, int y) const;
    std::string getSquareString(int pos) const;
    std::pair<int, int> decodePlaced(int z) const;
    void parseAction(int action_id);

    int from_;
    int to_;
    int build_;
};

class SantoriniEnv : public BaseBoardEnv<SantoriniAction> {
public:
    SantoriniEnv() { reset(); }

    void reset() override;
    bool act(const SantoriniAction& action) override;
    bool act(const std::vector<std::string>& action_string_args) override;
    std::vector<SantoriniAction> getLegalActions() const override;
    bool isLegalAction(const SantoriniAction& action) const override;
    bool isTerminal() const override;
    float getReward() const override { return 0.0f; }
    float getEvalScore(bool is_resign = false) const override;
    std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    std::vector<float> getActionFeatures(const SantoriniAction& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline int getNumInputChannels() const override { return 50; }
    inline int getPolicySize() const override { return kSantoriniPolicySize; }
    inline int getNumActionFeatureChannels() const override { return 0; }
    std::string toString() const override;
    inline std::string name() const override { return kSantoriniName; }
    inline int getNumPlayer() const override { return kSantoriniNumPlayer; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; };
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return action_id; };

private:
    Board board_;
    std::vector<Board> history_;
};

class SantoriniEnvLoader : public BaseBoardEnvLoader<SantoriniAction, SantoriniEnv> {
public:
    std::vector<float> getActionFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const override;
    inline std::vector<float> getValue(const int pos) const { return {getReturn()}; }
    inline std::string name() const override { return kSantoriniName; }
    inline int getPolicySize() const override { return kSantoriniPolicySize; }
    inline int getRotatePosition(int position, utils::Rotation rotation) const override { return position; }
    inline int getRotateAction(int action_id, utils::Rotation rotation) const override { return action_id; }
};

} // namespace minizero::env::santorini
