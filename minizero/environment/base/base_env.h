#pragma once

#include "configuration.h"
#include "rotation.h"
#include "sgf_loader.h"
#include "utils.h"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace minizero::env {

using namespace minizero;

// only support up to two players currently
enum class Player {
    kPlayerNone = 0,
    kPlayer1 = 1,
    kPlayer2 = 2,
    kPlayerSize = 3
};

char playerToChar(Player p);
Player charToPlayer(char c);
Player getNextPlayer(Player player, int num_player);
Player getPreviousPlayer(Player player, int num_player);

class BaseAction {
public:
    BaseAction() : action_id_(-1), player_(Player::kPlayerNone) {}
    BaseAction(int action_id, Player player) : action_id_(action_id), player_(player) {}
    virtual ~BaseAction() = default;

    virtual Player nextPlayer() const = 0;
    virtual std::string toConsoleString() const = 0;

    inline int getActionID() const { return action_id_; }
    inline Player getPlayer() const { return player_; }

protected:
    int action_id_;
    Player player_;
};

template <class T>
class GamePair {
public:
    GamePair() { reset(); }
    GamePair(T black, T white) : black_(black), white_(white) {}

    inline void reset() { black_ = white_ = T(); }
    inline T& get(Player p) { return (p == Player::kPlayer1 ? black_ : white_); }
    inline const T& get(Player p) const { return (p == Player::kPlayer1 ? black_ : white_); }
    inline void set(Player p, const T& value) { (p == Player::kPlayer1 ? black_ : white_) = value; }
    inline void set(const T& black, const T& white)
    {
        black_ = black;
        white_ = white;
    }
    inline bool operator==(const GamePair& rhs) { return (black_ == rhs.black_ && white_ == rhs.white_); }

private:
    T black_;
    T white_;
};

template <class Action>
class BaseEnv {
public:
    BaseEnv() {}
    virtual ~BaseEnv() = default;

    virtual void reset() = 0;
    virtual bool act(const Action& action) = 0;
    virtual bool act(const std::vector<std::string>& action_string_args) = 0;
    virtual std::vector<Action> getLegalActions() const = 0;
    virtual bool isLegalAction(const Action& action) const = 0;
    virtual bool isTerminal() const = 0;
    virtual float getReward() const = 0;
    virtual float getEvalScore(bool is_resign = false) const = 0;
    virtual std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const = 0;
    virtual std::vector<float> getActionFeatures(const Action& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const = 0;
    virtual std::string toString() const = 0;
    virtual std::string name() const = 0;
    virtual int getNumPlayer() const = 0;
    virtual void setTurn(Player p) { turn_ = p; }

    inline Player getTurn() const { return turn_; }
    inline const std::vector<Action>& getActionHistory() const { return actions_; }
    inline const std::vector<std::string>& getObservationHistory() const { return observations_; }

protected:
    Player turn_;
    std::vector<Action> actions_;
    std::vector<std::string> observations_;
};

template <class Action, class Env>
class BaseEnvLoader {
public:
    BaseEnvLoader() {}
    virtual ~BaseEnvLoader() = default;

    virtual void reset()
    {
        content_ = "";
        tags_.clear();
        action_pairs_.clear();
        tags_.insert({"GM", name()});
        tags_.insert({"RE", "0"});
    }

    virtual bool loadFromFile(const std::string& file_name)
    {
        std::ifstream fin(file_name.c_str());
        if (!fin) { return false; }

        std::stringstream buffer;
        buffer << fin.rdbuf();
        return loadFromString(buffer.str());
    }

    virtual bool loadFromString(const std::string& content)
    {
        reset();
        content_ = content;
        size_t index = content.find('(') + 1;
        while (index < content.size() && content[index] != ')') {
            size_t left_bracket_pos = content.find('[', index);
            size_t right_bracket_pos = content.find(']', index);
            if (left_bracket_pos == std::string::npos || right_bracket_pos == std::string::npos) { return false; }

            std::string key = content.substr(index, left_bracket_pos - index);
            std::string value = content.substr(left_bracket_pos + 1, right_bracket_pos - left_bracket_pos - 1);
            if (key == "B" || key == "W") {
                Action action(std::stoi(value.substr(0, value.find('|'))), charToPlayer(key[0]));
                std::string action_info = value.substr(value.find('|') + 1);
                addActionPair(action, action_info);
            } else {
                addTag(key, value);
            }
            index = right_bracket_pos + 1;
        }
        return true;
    }

    virtual void loadFromEnvironment(const Env& env, const std::vector<std::vector<std::pair<std::string, std::string>>>& action_info_history = {})
    {
        reset();
        for (size_t i = 0; i < env.getActionHistory().size(); ++i) {
            std::string action_info = "";
            if (action_info_history.size() > i) {
                for (const auto& p : action_info_history[i]) { action_info += p.first + ":" + p.second + ";"; }
            }
            addActionPair(env.getActionHistory()[i], action_info);
        }
        addTag("RE", std::to_string(env.getEvalScore()));

        // add observations
        std::string observations;
        for (const auto& obs : env.getObservationHistory()) { observations += obs; }
        addTag("OBS", utils::compressString(observations));
        assert(observations == utils::decompressString(getTag("OBS")));
    }

    virtual std::string toString() const
    {
        std::ostringstream oss;
        oss << "(";
        for (const auto& t : tags_) { oss << t.first << "[" << t.second << "]"; }
        for (const auto& p : action_pairs_) {
            oss << playerToChar(p.first.getPlayer())
                << "[" << p.first.getActionID()
                << "|" << p.second << "]";
        }
        oss << ")";
        return oss.str();
    }

    virtual std::vector<float> getFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const
    {
        // a slow but naive method which simply replays the game again to get features
        Env env;
        for (int i = 0; i < std::min(pos, static_cast<int>(action_pairs_.size())); ++i) { env.act(action_pairs_[i].first); }
        return env.getFeatures(rotation);
    }

    virtual std::vector<float> getPolicy(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const
    {
        std::vector<float> policy(getPolicySize(), 0.0f);
        if (pos < static_cast<int>(action_pairs_.size())) {
            const std::string policy_distribution = extractActionInfo(action_pairs_[pos].second, "P:");
            if (policy_distribution.empty()) {
                policy[getRotatePosition(action_pairs_[pos].first.getActionID(), rotation)] = 1.0f;
            } else {
                std::string tmp;
                float total = 0.0f;
                std::istringstream iss(policy_distribution);
                while (std::getline(iss, tmp, ',')) {
                    int position = getRotatePosition(std::stoi(tmp.substr(0, tmp.find(":"))), rotation);
                    float count = std::stof(tmp.substr(tmp.find(":") + 1));
                    policy[position] = count;
                    total += count;
                }
                for (auto& p : policy) { p /= total; }
            }
        } else { // absorbing states
            std::fill(policy.begin(), policy.end(), 1.0f / getPolicySize());
        }
        return policy;
    }

    virtual std::pair<int, int> getDataRange() const
    {
        const std::string dlen = getTag("DLEN");
        if (dlen.empty()) {
            return {0, std::max(0, static_cast<int>(getActionPairs().size()) - 1)};
        } else {
            return {std::stoi(dlen), std::stoi(dlen.substr(dlen.find("-") + 1))};
        }
    }

    virtual std::vector<float> getValue(const int pos) const { return (pos < static_cast<int>(action_pairs_.size()) ? std::vector<float>{std::stof(extractActionInfo(action_pairs_[pos].second, "V:"))} : std::vector<float>{0.0f}); }
    virtual std::vector<float> getReward(const int pos) const { return (pos < static_cast<int>(action_pairs_.size()) ? std::vector<float>{std::stof(extractActionInfo(action_pairs_[pos].second, "R:"))} : std::vector<float>{0.0f}); }
    virtual float getPriority(const int pos) const { return 1.0f; }

    virtual std::vector<float> getActionFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const = 0;
    virtual std::string name() const = 0;
    virtual int getPolicySize() const = 0;
    virtual int getRotatePosition(int position, utils::Rotation rotation) const = 0;

    inline std::string getContent() const { return content_; }
    inline std::string getTag(const std::string& key) const { return tags_.count(key) ? tags_.at(key) : ""; }
    inline std::vector<std::pair<Action, std::string>>& getActionPairs() { return action_pairs_; }
    inline const std::vector<std::pair<Action, std::string>>& getActionPairs() const { return action_pairs_; }
    inline float getReturn() const { return std::stof(getTag("RE")); }
    inline void addActionPair(const Action& action, const std::string& action_info = "") { action_pairs_.push_back({action, action_info}); }
    inline void addTag(const std::string& key, const std::string& value) { tags_[key] = value; }

protected:
    inline std::string extractActionInfo(const std::string& action_info, const std::string& tag) const
    {
        if (action_info.find(tag) == std::string::npos) { return ""; }

        size_t start = action_info.find(tag) + tag.size();
        size_t end = action_info.find(";", start);
        return action_info.substr(start, end - start);
    }

    std::string content_;
    std::unordered_map<std::string, std::string> tags_;
    std::vector<std::pair<Action, std::string>> action_pairs_;
};

template <int kNumPlayer = 2>
class BaseBoardAction : public BaseAction {
public:
    BaseBoardAction() : BaseAction() {}
    BaseBoardAction(int action_id, Player player) : BaseAction(action_id, player) {}
    BaseBoardAction(const std::vector<std::string>& action_string_args, int board_size = minizero::config::env_board_size)
    {
        assert(action_string_args.size() == 2);
        assert(action_string_args[0].size() == 1);
        player_ = charToPlayer(action_string_args[0][0]);
        assert(static_cast<int>(player_) > 0 && static_cast<int>(player_) <= kNumPlayer); // assume kPlayer1 == 1, kPlayer2 == 2, ...
        action_id_ = minizero::utils::SGFLoader::boardCoordinateStringToActionID(action_string_args[1], board_size);
    }

    inline Player nextPlayer() const override { return getNextPlayer(getPlayer(), kNumPlayer); }
    inline std::string toConsoleString() const override { return minizero::utils::SGFLoader::actionIDToBoardCoordinateString(getActionID(), minizero::config::env_board_size); }
};

template <class Action>
class BaseBoardEnv : public BaseEnv<Action> {
public:
    BaseBoardEnv(int board_size = minizero::config::env_board_size) : board_size_(board_size) { assert(board_size_ > 0); }
    virtual ~BaseBoardEnv() = default;

    inline int getBoardSize() const { return board_size_; }

protected:
    int board_size_;
};

template <class Action, class Env>
class BaseBoardEnvLoader : public BaseEnvLoader<Action, Env> {
public:
    void loadFromEnvironment(const Env& env, const std::vector<std::vector<std::pair<std::string, std::string>>>& action_info_history = {}) override
    {
        BaseEnvLoader<Action, Env>::loadFromEnvironment(env, action_info_history);
        BaseEnvLoader<Action, Env>::addTag("SZ", std::to_string(env.getBoardSize()));
    }

    inline int getBoardSize() const
    {
        std::string size = BaseEnvLoader<Action, Env>::getTag("SZ");
        return size.size() ? std::stoi(size) : minizero::config::env_board_size;
    }
};

} // namespace minizero::env
