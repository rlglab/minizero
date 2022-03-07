#pragma once

#include <cassert>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace minizero::env {

// we only support up to two players currently, if you want to use more than two players, please check the algorithm of MCTS
enum class Player {
    kPlayerNone = 0,
    kPlayer1 = 1,
    kPlayer2 = 2,
    kPlayerSize = 3
};

char PlayerToChar(Player p);
Player CharToPlayer(char c);
Player GetNextPlayer(Player player, int num_player);

class BaseAction {
public:
    BaseAction() : action_id_(-1), player_(Player::kPlayerNone) {}
    BaseAction(int action_id, Player player) : action_id_(action_id), player_(player) {}
    virtual ~BaseAction() = default;

    virtual Player NextPlayer() const = 0;

    inline int GetActionID() const { return action_id_; }
    inline Player GetPlayer() const { return player_; }

protected:
    int action_id_;
    Player player_;
};

template <class Action>
class BaseEnv {
public:
    BaseEnv() {}
    virtual ~BaseEnv() = default;

    virtual void Reset() = 0;
    virtual bool Act(const Action& action) = 0;
    virtual bool Act(const std::vector<std::string>& action_string_args) = 0;
    virtual std::vector<Action> GetLegalActions() const = 0;
    virtual bool IsLegalAction(const Action& action) const = 0;
    virtual bool IsTerminal() const = 0;
    virtual float GetEvalScore() const = 0;
    virtual std::vector<float> GetFeatures() const = 0;
    virtual std::string ToString() const = 0;
    virtual std::string Name() const = 0;

    inline Player GetTurn() const { return turn_; }
    inline const std::vector<Action>& GetActionHistory() const { return actions_; }

protected:
    Player turn_;
    std::vector<Action> actions_;
};

template <class Action, class Env>
class BaseEnvLoader {
public:
    BaseEnvLoader() {}
    virtual ~BaseEnvLoader() = default;

    inline void Reset()
    {
        tags_.clear();
        action_pairs_.clear();
        tags_.insert({"GM", GetEnvName()});
        tags_.insert({"RE", "0"});
    }

    inline bool LoadFromFile(const std::string& file_name)
    {
        std::ifstream fin(file_name.c_str());
        if (!fin) { return false; }

        std::stringstream buffer;
        buffer << fin.rdbuf();
        return LoadFromString(buffer.str());
    }

    inline bool LoadFromString(const std::string& content)
    {
        Reset();
        size_t index = 0;
        while (index < content.size()) {
            size_t left_bracket_pos = content.find('[', index);
            size_t right_bracket_pos = content.find(']', index);
            if (left_bracket_pos == std::string::npos || right_bracket_pos == std::string::npos) { return false; }

            std::string key = content.substr(index, left_bracket_pos - index);
            std::string value = content.substr(left_bracket_pos + 1, right_bracket_pos - left_bracket_pos - 1);
            if (key == "B" || key == "W") {
                Action action(std::stoi(value.substr(0, value.find('|'))), CharToPlayer(key[0]));
                std::string action_distribution = value.substr(value.find('|') + 1);
                AddActionPair(action, action_distribution);
            } else {
                AddTag(key, value);
            }
            index = right_bracket_pos + 1;
        }
        return true;
    }

    inline void LoadFromEnvironment(const Env& env, const std::vector<std::string> action_distributions = {})
    {
        Reset();
        for (size_t i = 0; i < env.GetActionHistory().size(); ++i) {
            std::string distribution = i < action_distributions.size() ? action_distributions[i] : "";
            AddActionPair(env.GetActionHistory()[i], distribution);
        }
        AddTag("RE", std::to_string(env.GetEvalScore()));
    }

    inline std::string ToString() const
    {
        std::ostringstream oss;
        for (const auto& t : tags_) { oss << t.first << "[" << t.second << "]"; }
        for (const auto& p : action_pairs_) {
            oss << PlayerToChar(p.first.GetPlayer())
                << "[" << p.first.GetActionID()
                << "|" << p.second << "]";
        }
        return oss.str();
    }

    virtual std::vector<float> GetPolicyDistribution(int id) const = 0;
    virtual std::string GetEnvName() const = 0;

    inline std::string GetTag(const std::string& key) const { return tags_.count(key) ? tags_.at(key) : ""; }
    inline std::vector<std::pair<Action, std::string>>& GetActionPairs() { return action_pairs_; }
    inline const std::vector<std::pair<Action, std::string>>& GetActionPairs() const { return action_pairs_; }
    inline float GetReturn() const { return std::stof(GetTag("RE")); }
    inline void AddActionPair(const Action& action, const std::string& action_distribution = "") { action_pairs_.push_back({action, action_distribution}); }
    inline void AddTag(const std::string& key, const std::string& value) { tags_[key] = value; }

protected:
    void SetPolicyDistribution(int id, std::vector<float>& policy) const
    {
        assert(id < static_cast<int>(action_pairs_.size()));
        std::string distribution = action_pairs_[id].second;
        if (distribution.empty()) {
            policy[action_pairs_[id].first.GetActionID()] = 1.0f;
        } else {
            float total = 0.0f;
            std::string tmp;
            std::istringstream iss(GetActionPairs()[id].second);
            while (std::getline(iss, tmp, ',')) {
                int position = std::stoi(tmp.substr(0, tmp.find(":")));
                float count = std::stoi(tmp.substr(tmp.find(":") + 1));
                policy[position] = count;
                total += count;
            }
            for (auto& p : policy) { p /= total; }
        }
    }

    std::unordered_map<std::string, std::string> tags_;
    std::vector<std::pair<Action, std::string>> action_pairs_;
};

} // namespace minizero::env
