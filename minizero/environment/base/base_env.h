#pragma once

#include "rotation.h"
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

protected:
    Player turn_;
    std::vector<Action> actions_;
};

template <class Action, class Env>
class BaseEnvLoader {
public:
    BaseEnvLoader() {}
    virtual ~BaseEnvLoader() = default;

    inline void reset()
    {
        content_ = "";
        tags_.clear();
        action_pairs_.clear();
        tags_.insert({"GM", getEnvName()});
        tags_.insert({"RE", "0"});
    }

    inline bool loadFromFile(const std::string& file_name)
    {
        std::ifstream fin(file_name.c_str());
        if (!fin) { return false; }

        std::stringstream buffer;
        buffer << fin.rdbuf();
        return loadFromString(buffer.str());
    }

    inline bool loadFromString(const std::string& content)
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

    inline void loadFromEnvironment(const Env& env, const std::vector<std::unordered_map<std::string, std::string>>& action_info_history = {})
    {
        reset();
        for (size_t i = 0; i < env.getActionHistory().size(); ++i) {
            std::string action_info = "";
            for (const auto& p : action_info_history[i]) { action_info += p.first + ":" + p.second + ";"; }
            addActionPair(env.getActionHistory()[i], action_info);
        }
        addTag("RE", std::to_string(env.getEvalScore()));
    }

    inline std::string toString() const
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

    std::vector<float> getPolicyDistribution(int id, utils::Rotation rotation = utils::Rotation::kRotationNone) const
    {
        assert(id < static_cast<int>(action_pairs_.size()));

        std::vector<float> policy(getPolicySize(), 0.0f);
        std::string action_info = action_pairs_[id].second;
        if (action_info.find("P:") == std::string::npos) {
            policy[getRotatePosition(action_pairs_[id].first.getActionID(), rotation)] = 1.0f;
        } else {
            size_t start = action_info.find("P:") + std::string("P:").size();
            size_t end = action_info.find(";", start);
            std::string distribution = action_info.substr(start, end - start);

            std::string tmp;
            float total = 0.0f;
            std::istringstream iss(distribution);
            while (std::getline(iss, tmp, ',')) {
                int position = getRotatePosition(std::stoi(tmp.substr(0, tmp.find(":"))), rotation);
                float count = std::stoi(tmp.substr(tmp.find(":") + 1));
                policy[position] = count;
                total += count;
            }
            for (auto& p : policy) { p /= total; }
        }
        return policy;
    }

    virtual int getPolicySize() const = 0;
    virtual int getRotatePosition(int position, utils::Rotation rotation) const = 0;
    virtual std::string getEnvName() const = 0;

    inline std::string getContent() const { return content_; }
    inline std::string getTag(const std::string& key) const { return tags_.count(key) ? tags_.at(key) : ""; }
    inline std::vector<std::pair<Action, std::string>>& getActionPairs() { return action_pairs_; }
    inline const std::vector<std::pair<Action, std::string>>& getActionPairs() const { return action_pairs_; }
    inline float getReturn() const { return std::stof(getTag("RE")); }
    inline void addActionPair(const Action& action, const std::string& action_info = "") { action_pairs_.push_back({action, action_info}); }
    inline void addTag(const std::string& key, const std::string& value) { tags_[key] = value; }

protected:
    std::string content_;
    std::unordered_map<std::string, std::string> tags_;
    std::vector<std::pair<Action, std::string>> action_pairs_;
};

} // namespace minizero::env
