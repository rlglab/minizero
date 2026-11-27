#pragma once

#include "configuration.h"
#include "rotation.h"
#include "sgf_loader.h"
#include "utils.h"
#include "vector_map.h"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
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
    inline bool operator==(const GamePair<T>& rhs) const { return (black_ == rhs.black_ && white_ == rhs.white_); }

private:
    T black_;
    T white_;
};

template <class Action>
class BaseEnv {
public:
    /** Construct an environment. Derived constructors must initialize it to a usable state, usually by calling reset(). */
    BaseEnv() {}

    /** Allow an environment to be destroyed through a BaseEnv pointer. */
    virtual ~BaseEnv() = default;

    /**
     * Restore the initial game state.
     * Implementations must also reset the player color stored in turn_ and clear actions_, observations_, and other
     * episode state.
     */
    virtual void reset() = 0;

    /**
     * Apply an action to the current state.
     * A successful action should update the game state, action history, observation history, and turn as needed.
     *
     * @param action Action to apply.
     * @return true if the action was applied; otherwise false.
     */
    virtual bool act(const Action& action) = 0;

    /**
     * Parse and apply an action supplied by the console.
     * Each game defines its own argument format.
     *
     * @param action_string_args Game-specific action arguments supplied by the console.
     * @return true if the arguments describe a legal action and it was applied; otherwise false.
     */
    virtual bool act(const std::vector<std::string>& action_string_args) = 0;

    /**
     * Get all legal actions for the current turn.
     *
     * @return Legal actions in the current state.
     */
    virtual std::vector<Action> getLegalActions() const = 0;

    /**
     * Check whether an action is legal in the current state without modifying the environment.
     *
     * @param action Action to check.
     * @return true if the action is legal; otherwise false.
     */
    virtual bool isLegalAction(const Action& action) const = 0;

    /**
     * Check whether the current episode has ended.
     *
     * @return true if the episode has ended; otherwise false.
     */
    virtual bool isTerminal() const = 0;

    /**
     * Get the reward produced by the latest action.
     *
     * @return Reward produced by the latest action.
     */
    virtual float getReward() const = 0;

    /**
     * Get the current game result or score.
     * In two-player games, a positive value favors Player 1 and a negative value favors Player 2.
     *
     * @param is_resign Whether to score the current player as resigned. Some games may ignore this value.
     * @return Current game result or score.
     */
    virtual float getEvalScore(bool is_resign = false) const = 0;

    /**
     * Build the neural-network input for the current state.
     *
     * @param rotation Rotation to apply to the state.
     * @return Flattened input containing
     * getNumInputChannels() * getInputChannelHeight() * getInputChannelWidth() elements.
     */
    virtual std::vector<float> getFeatures(utils::Rotation rotation = utils::Rotation::kRotationNone) const = 0;

    /**
     * Build the MuZero input for an action.
     *
     * @param action Action to encode.
     * @param rotation Rotation to apply to the action.
     * @return Flattened input containing
     * getNumActionFeatureChannels() * getHiddenChannelHeight() * getHiddenChannelWidth() elements.
     */
    virtual std::vector<float> getActionFeatures(const Action& action, utils::Rotation rotation = utils::Rotation::kRotationNone) const = 0;

    /**
     * Get the number of channels returned by getFeatures().
     *
     * @return Number of observation channels.
     */
    virtual int getNumInputChannels() const = 0;

    /**
     * Get the number of channels returned by getActionFeatures().
     *
     * @return Number of action-feature channels, or 0 if action features are unused.
     */
    virtual int getNumActionFeatureChannels() const = 0;

    /**
     * Get the height of each input channel produced by getFeatures().
     *
     * @return Input-channel height.
     */
    virtual int getInputChannelHeight() const = 0;

    /**
     * Get the width of each input channel produced by getFeatures().
     *
     * @return Input-channel width.
     */
    virtual int getInputChannelWidth() const = 0;

    /**
     * Get the height of MuZero's hidden state and action input.
     *
     * @return Hidden-state and action-input height.
     */
    virtual int getHiddenChannelHeight() const = 0;

    /**
     * Get the width of MuZero's hidden state and action input.
     *
     * @return Hidden-state and action-input width.
     */
    virtual int getHiddenChannelWidth() const = 0;

    /**
     * Get the number of outputs in the policy.
     * Valid action IDs range from 0 to getPolicySize() - 1. Special actions such as pass are included.
     *
     * @return Number of policy outputs.
     */
    virtual int getPolicySize() const = 0;

    /**
     * Get the number of outputs used to represent a value or reward.
     * Return 1 when a value or reward is stored as a single number.
     *
     * @return Number of value or reward outputs.
     */
    virtual int getDiscreteValueSize() const = 0;

    /**
     * Map a position to its index after rotation.
     *
     * @param position Position before rotation.
     * @param rotation Rotation to apply.
     * @return Position after rotation.
     */
    virtual int getRotatePosition(int position, utils::Rotation rotation) const = 0;

    /**
     * Map an action ID to the same action after rotation.
     *
     * @param action_id Action ID before rotation.
     * @param rotation Rotation to apply.
     * @return Equivalent action ID after rotation.
     */
    virtual int getRotateAction(int action_id, utils::Rotation rotation) const = 0;

    /**
     * Format the current state for console output and debugging.
     *
     * @return Human-readable representation of the current state.
     */
    virtual std::string toString() const = 0;

    /**
     * Get the environment name used by configuration and game records.
     *
     * @return Environment identifier.
     */
    virtual std::string name() const = 0;

    /**
     * Get the number of players in the environment.
     *
     * @return Number of players.
     */
    virtual int getNumPlayer() const = 0;

    /**
     * Set the player for the next turn without changing any other state.
     *
     * @param p Player to move next.
     */
    virtual void setTurn(Player p) { turn_ = p; }

    /**
     * Get the player whose action is expected next.
     *
     * @return Player whose action is expected next.
     */
    inline Player getTurn() const { return turn_; }

    /**
     * Get the actions applied since the last reset().
     *
     * @return Read-only action history.
     */
    inline const std::vector<Action>& getActionHistory() const { return actions_; }

    /**
     * Get the stored observations since the last reset().
     *
     * @return Read-only observation history.
     */
    inline const std::vector<std::string>& getObservationHistory() const { return observations_; }

    /**
     * Set game-specific configuration defaults before the configuration is loaded and an environment is created.
     * Every concrete environment must provide its own static method with this name.
     */
    static void setUpEnv();

protected:
    Player turn_;
    std::vector<Action> actions_;
    std::vector<std::string> observations_;
};

template <class Action, class Env>
class BaseEnvLoader {
public:
    /** Construct an empty environment loader. */
    BaseEnvLoader() {}

    /** Allow a loader to be destroyed through a BaseEnvLoader pointer. */
    virtual ~BaseEnvLoader() = default;

    /** Store game information as key-value pairs. */
    typedef minizero::utils::VectorMap<std::string, std::string> Tags;

    /** Store extra information for an action as key-value pairs. */
    typedef minizero::utils::VectorMap<std::string, std::string> ActionInfo;

public:
    /** Clear the loaded record and initialize the GM and RE tags. */
    virtual void reset()
    {
        sgf_content_.clear();
        tags_.clear();
        tags_.insert({"GM", name()});
        tags_.insert({"RE", "0"});
        action_pairs_.clear();
    }

    /**
     * Load and parse an SGF record from a file.
     *
     * @param file_name Path to the SGF record.
     * @return true if the file was opened and parsed successfully; otherwise false.
     */
    virtual bool loadFromFile(const std::string& file_name)
    {
        std::ifstream fin(file_name.c_str());
        if (!fin) { return false; }

        std::string line;
        std::string sgf_content;
        while (std::getline(fin, line)) {
            if (line.back() == '\r') { line.pop_back(); }
            if (line.empty()) { continue; }
            sgf_content += line;
        }
        return loadFromString(sgf_content);
    }

    /**
     * Parse an SGF record from a string and replace the currently loaded record.
     * Information before the first action is stored as game information. Information after an action is stored with
     * that action.
     *
     * @param content SGF record to parse.
     * @return true if the parser reaches a closing parenthesis; otherwise false.
     */
    virtual bool loadFromString(const std::string& content)
    {
        reset();
        sgf_content_ = content;
        std::string key, value;
        int state = '(';
        bool accept_move = false;
        bool escape_next = false;
        int board_size = minizero::config::env_board_size;
        for (char c : content) {
            switch (state) {
                case '(': // wait until record start
                    if (!accept_move) {
                        accept_move = (c == '(');
                    } else {
                        state = (c == ';') ? c : 'x';
                        accept_move = false;
                    }
                    break;
                case ';': // store key
                    if (c == ';') {
                        accept_move = true;
                    } else if (c == '[' || c == ')') {
                        state = c;
                    } else if (std::isgraph(c)) {
                        key += c;
                    }
                    break;
                case '[': // store value
                    if (c == '\\' && !escape_next) {
                        escape_next = true;
                    } else if (c != ']' || escape_next) {
                        value += c;
                        escape_next = false;
                    } else { // ready to store key-value pair
                        if (accept_move) {
                            int action_id = value.size() && std::isdigit(value[0]) ? std::stoi(value) : minizero::utils::SGFLoader::sgfStringToActionID(value, board_size);
                            action_pairs_.emplace_back().first = Action(action_id, charToPlayer(key[0]));
                            accept_move = false;
                        } else if (action_pairs_.size()) {
                            action_pairs_.back().second[key] = std::move(value);
                        } else {
                            if (key == "SZ") { board_size = std::stoi(value); }
                            tags_[key] = std::move(value);
                        }
                        key.clear();
                        value.clear();
                        state = ';';
                    }
                    break;
                case ')': // end of record, do nothing
                    break;
            }
        }
        return state == ')';
    }

    /**
     * Build a record from an environment and replace the currently loaded record.
     * Each entry in action_info_history is stored with the action at the same position. The game result and observation
     * history are also stored.
     *
     * @param env Environment to record.
     * @param action_info_history Extra information for each action.
     */
    virtual void loadFromEnvironment(const Env& env, const std::vector<std::vector<std::pair<std::string, std::string>>>& action_info_history = {})
    {
        reset();
        for (size_t i = 0; i < env.getActionHistory().size(); ++i) {
            addActionPair(env.getActionHistory()[i], action_info_history.size() > i ? ActionInfo(action_info_history[i]) : ActionInfo());
        }
        addTag("RE", std::to_string(env.getEvalScore()));

        // add observations
        std::string observations;
        for (const auto& obs : env.getObservationHistory()) { observations += obs; }
        addTag("OBS", utils::compressString(observations));
        assert(observations == utils::decompressString(getTag("OBS")));
    }

    /**
     * Convert the loaded record to SGF using numeric action IDs.
     *
     * @return SGF representation of the loaded record.
     */
    virtual std::string toString() const
    {
        std::ostringstream oss;
        oss << "(;";
        for (const auto& t : tags_) { oss << t.first << "[" << escapeSGFString(t.second) << "]"; }
        for (const auto& p : action_pairs_) {
            oss << ";" << playerToChar(p.first.getPlayer()) << "[" << p.first.getActionID() << "]";
            for (const auto& info : p.second) { oss << info.first << "[" << escapeSGFString(info.second) << "]"; }
        }
        oss << ")";
        return oss.str();
    }

    /**
     * Get the features for the state before the action at pos.
     * The default implementation rebuilds the state by replaying the earlier actions.
     *
     * @param pos Position in the action history.
     * @param rotation Rotation to apply.
     * @return Features for the state at pos.
     */
    virtual std::vector<float> getFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const
    {
        // a slow but naive method which simply replays the game again to get features
        Env env;
        for (int i = 0; i < std::min(pos, static_cast<int>(action_pairs_.size())); ++i) { env.act(action_pairs_[i].first); }
        return env.getFeatures(rotation);
    }

    /**
     * Get the policy for the action at pos.
     * Use the policy stored in P when available. Otherwise, use the recorded action. For positions after the final
     * action, give every action the same probability.
     *
     * @param pos Position in the action history.
     * @param rotation Rotation to apply.
     * @return Policy for the action at pos.
     */
    virtual std::vector<float> getPolicy(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const
    {
        std::vector<float> policy(getPolicySize(), 0.0f);
        if (pos < static_cast<int>(action_pairs_.size())) {
            const std::string policy_distribution = action_pairs_[pos].second["P"];
            if (policy_distribution.empty()) {
                policy[getRotateAction(action_pairs_[pos].first.getActionID(), rotation)] = 1.0f;
            } else {
                std::string tmp;
                float total = 0.0f;
                std::istringstream iss(policy_distribution);
                while (std::getline(iss, tmp, ',')) {
                    int position = getRotateAction(std::stoi(tmp.substr(0, tmp.find(":"))), rotation);
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

    /**
     * Get the first and last positions used for training.
     * Use the DLEN value when available. Otherwise, calculate the range from the action history.
     *
     * @return First and last training positions, including both endpoints.
     */
    virtual std::pair<int, int> getDataRange() const
    {
        const std::string dlen = getTag("DLEN");
        if (dlen.empty()) {
            return {0, std::max(0, static_cast<int>(getActionPairs().size()) - 1)};
        } else {
            return {std::stoi(dlen), std::stoi(dlen.substr(dlen.find("-") + 1))};
        }
    }

    /**
     * Get the value stored in V at pos.
     *
     * @param pos Position in the action history.
     * @return Stored value, or zero for a position after the final action.
     */
    virtual std::vector<float> getValue(const int pos) const { return (pos < static_cast<int>(action_pairs_.size()) ? std::vector<float>{std::stof(action_pairs_[pos].second["V"])} : std::vector<float>{0.0f}); }

    /**
     * Get the reward stored in R at pos.
     *
     * @param pos Position in the action history.
     * @return Stored reward, or zero for a position after the final action.
     */
    virtual std::vector<float> getReward(const int pos) const { return (pos < static_cast<int>(action_pairs_.size()) ? std::vector<float>{std::stof(action_pairs_[pos].second["R"])} : std::vector<float>{0.0f}); }

    /**
     * Add or replace information for the action at pos.
     *
     * @param pos Position of the action to update.
     * @param tag Information name.
     * @param value Information value.
     * @return true if an action exists at pos; otherwise false.
     */
    virtual bool setActionPairInfo(const int pos, const std::string& tag, const std::string value)
    {
        if (pos >= static_cast<int>(action_pairs_.size())) { return false; }
        action_pairs_[pos].second[tag] = value;
        return true;
    }
    /**
     * Get the sampling priority at pos.
     * The default implementation assigns equal priority to every position.
     *
     * @param pos Position in the action history.
     * @return Sampling priority at pos.
     */
    virtual float getPriority(const int pos) const { return 1.0f; }

    /**
     * Get the action features at pos.
     *
     * @param pos Position in the action history.
     * @param rotation Rotation to apply.
     * @return Action features at pos.
     */
    virtual std::vector<float> getActionFeatures(const int pos, utils::Rotation rotation = utils::Rotation::kRotationNone) const = 0;

    /**
     * Get the environment name stored in GM.
     *
     * @return Environment identifier.
     */
    virtual std::string name() const = 0;

    /**
     * Get the size of each policy.
     *
     * @return Policy size.
     */
    virtual int getPolicySize() const = 0;

    /**
     * Map a position to its index after rotation.
     *
     * @param position Position before rotation.
     * @param rotation Rotation to apply.
     * @return Position after rotation.
     */
    virtual int getRotatePosition(int position, utils::Rotation rotation) const = 0;

    /**
     * Map an action ID to the same action after rotation.
     *
     * @param action_id Action ID before rotation.
     * @param rotation Rotation to apply.
     * @return Equivalent action ID after rotation.
     */
    virtual int getRotateAction(int action_id, utils::Rotation rotation) const = 0;

    /**
     * Get game information by name.
     *
     * @param key Information name.
     * @return Information value, or an empty string if the name does not exist.
     */
    virtual inline std::string getTag(const std::string& key) const { return tags_.count(key) ? tags_.at(key) : ""; }

    /**
     * Add or replace game information.
     *
     * @param key Information name.
     * @param value Information value.
     */
    virtual inline void addTag(const std::string& key, const std::string& value) { tags_[key] = value; }

    /**
     * Get the original SGF string parsed by loadFromString(), either directly or through loadFromFile().
     *
     * @return Original SGF string.
     */
    inline std::string getSGFContent() const { return sgf_content_; }

    /**
     * Get the recorded actions and their extra information for editing.
     *
     * @return Editable action and information pairs.
     */
    inline std::vector<std::pair<Action, ActionInfo>>& getActionPairs() { return action_pairs_; }

    /**
     * Get the recorded actions and their extra information.
     *
     * @return Read-only action and information pairs.
     */
    inline const std::vector<std::pair<Action, ActionInfo>>& getActionPairs() const { return action_pairs_; }

    /**
     * Add an action and its extra information to the end of the record.
     *
     * @param action Action to add.
     * @param action_info Extra information for the action.
     */
    inline void addActionPair(const Action& action, const ActionInfo& action_info = {}) { action_pairs_.emplace_back(action, action_info); }

    /**
     * Get the game result stored in the RE property.
     *
     * @return Game result as a floating-point value.
     */
    inline float getReturn() const { return std::stof(getTag("RE")); }

protected:
    /**
     * Add backslashes before special characters in an SGF value.
     *
     * @param str SGF value to escape.
     * @return Escaped SGF value.
     */
    std::string escapeSGFString(const std::string& str) const
    {
        std::string special = "()[]\\";
        std::string escaped;
        escaped.reserve(str.size() + 10);
        for (char c : str) {
            if (special.find(c) != std::string::npos) escaped += '\\';
            escaped += c;
        }
        return escaped;
    }

protected:
    std::string sgf_content_;
    Tags tags_;
    std::vector<std::pair<Action, ActionInfo>> action_pairs_;
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
    inline int getNumActionFeatureChannels() const override { return 1; }
    inline int getInputChannelHeight() const override { return getBoardSize(); }
    inline int getInputChannelWidth() const override { return getBoardSize(); }
    inline int getHiddenChannelHeight() const override { return getBoardSize(); }
    inline int getHiddenChannelWidth() const override { return getBoardSize(); }
    inline int getDiscreteValueSize() const override { return 1; }

protected:
    int board_size_;
};

template <class Action, class Env>
class BaseBoardEnvLoader : public BaseEnvLoader<Action, Env> {
public:
    BaseBoardEnvLoader() : board_size_(minizero::config::env_board_size) {}
    virtual ~BaseBoardEnvLoader() = default;

    void loadFromEnvironment(const Env& env, const std::vector<std::vector<std::pair<std::string, std::string>>>& action_info_history = {}) override
    {
        BaseEnvLoader<Action, Env>::loadFromEnvironment(env, action_info_history);
        addTag("SZ", std::to_string(env.getBoardSize()));
    }

    inline void addTag(const std::string& key, const std::string& value) override
    {
        BaseEnvLoader<Action, Env>::addTag(key, value);
        if (key == "SZ") { board_size_ = std::stoi(value); }
    }

    inline int getBoardSize() const { return board_size_; }

protected:
    int board_size_;
};

} // namespace minizero::env
