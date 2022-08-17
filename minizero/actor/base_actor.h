#pragma once

#include "environment.h"
#include "network.h"
#include <vector>

namespace minizero::actor {

using namespace minizero;

class BaseActor {
public:
    BaseActor() {}
    virtual ~BaseActor() = default;

    virtual void reset();
    virtual void resetSearch();
    bool act(const Action& action, const std::string action_comment = "");
    bool act(const std::vector<std::string>& action_string_args, const std::string action_comment = "");
    std::string getRecord() const;

    inline bool isEnvTerminal() const { return env_.isTerminal(); }
    inline const float getEvalScore() const { return env_.getEvalScore(); }
    inline const Environment& getEnvironment() const { return env_; }
    inline const int getNNEvaluationBatchIndex() const { return nn_evaluation_batch_id_; }

    virtual Action think(bool with_play = false, bool display_board = false) = 0;
    virtual void beforeNNEvaluation() = 0;
    virtual void afterNNEvaluation(const std::shared_ptr<network::NetworkOutput>& network_output) = 0;
    virtual bool isSearchDone() const = 0;
    virtual Action getSearchAction() const = 0;
    virtual bool isResign() const = 0;
    virtual std::string getSearchInfo() const = 0;
    virtual std::string getActionComment() const = 0;
    virtual void setNetwork(const std::shared_ptr<network::Network>& network) = 0;

protected:
    int nn_evaluation_batch_id_;
    Environment env_;
    std::vector<std::string> action_comments_;
};

} // namespace minizero::actor