#pragma once

#include "environment.h"
#include "network.h"
#include "search.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace minizero::actor {

using namespace minizero;

class BaseActor {
public:
    BaseActor() {}
    virtual ~BaseActor() = default;

    virtual void reset();
    virtual void resetSearch();
    bool act(const Action& action, const std::unordered_map<std::string, std::string>& action_info = {});
    bool act(const std::vector<std::string>& action_string_args, const std::unordered_map<std::string, std::string>& action_info = {});
    virtual std::unordered_map<std::string, std::string> getActionInfo() const;
    virtual std::string getRecord(const std::unordered_map<std::string, std::string>& tags = {}) const;

    inline bool isEnvTerminal() const { return env_.isTerminal(); }
    inline const float getEvalScore() const { return env_.getEvalScore(); }
    inline Environment& getEnvironment() { return env_; }
    inline const Environment& getEnvironment() const { return env_; }
    inline const int getNNEvaluationBatchIndex() const { return nn_evaluation_batch_id_; }
    inline std::vector<std::unordered_map<std::string, std::string>>& getActionInfoHistory() { return action_info_history_; }
    inline const std::vector<std::unordered_map<std::string, std::string>>& getActionInfoHistory() const { return action_info_history_; }

    virtual Action think(bool with_play = false, bool display_board = false) = 0;
    virtual void beforeNNEvaluation() = 0;
    virtual void afterNNEvaluation(const std::shared_ptr<network::NetworkOutput>& network_output) = 0;
    virtual bool isSearchDone() const = 0;
    virtual Action getSearchAction() const = 0;
    virtual bool isResign() const = 0;
    virtual std::string getSearchInfo() const = 0;
    virtual void setNetwork(const std::shared_ptr<network::Network>& network) = 0;
    virtual std::shared_ptr<Search> createSearch() = 0;

protected:
    virtual std::string getMCTSPolicy() const = 0;
    virtual std::string getMCTSValue() const = 0;
    virtual std::string getEnvReward() const = 0;

    int nn_evaluation_batch_id_;
    Environment env_;
    std::shared_ptr<Search> search_;
    std::vector<std::unordered_map<std::string, std::string>> action_info_history_;
};

} // namespace minizero::actor
