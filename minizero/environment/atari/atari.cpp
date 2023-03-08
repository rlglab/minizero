#include "atari.h"
#include <opencv2/opencv.hpp>
#include <utility>

namespace minizero::env::atari {

std::string getAtariActionName(int action_id)
{
    assert(action_id >= 0 && action_id < kAtariActionSize);
    std::string action_name = ale::action_to_string(ale::Action(action_id));
    return action_name.substr(action_name.find_last_of("_") + 1);
}

AtariEnv& AtariEnv::operator=(const AtariEnv& env)
{
    reset(env.seed_);
    for (const auto& action : env.getActionHistory()) { act(action); }
    return *this;
}

void AtariEnv::reset(int seed)
{
    turn_ = Player::kPlayer1;
    seed_ = seed;
    reward_ = 0;
    total_reward_ = 0;
    ale_.setInt("random_seed", seed_);
    ale_.setFloat("repeat_action_probability", kAtariRepeatActionProbability);
    ale_.loadROM(config::env_atari_rom_dir + "/" + config::env_atari_name + ".bin");
    ale_.reset_game();
    minimal_action_set_.clear();
    for (auto action_id : ale_.getMinimalActionSet()) { minimal_action_set_.insert(action_id); }
    actions_.clear();
    observations_.clear();
    observations_.push_back(getObservationString()); // initial observation
    feature_history_.clear();
    feature_history_.resize(kAtariFeatureHistorySize, std::vector<float>(3 * config::nn_input_channel_height * config::nn_input_channel_width, 0.0f));
    feature_history_.push_back(getObservation()); // initial screen
    feature_history_.pop_front();
    action_feature_history_.clear();
    action_feature_history_.resize(kAtariFeatureHistorySize, std::vector<float>(config::nn_input_channel_height * config::nn_input_channel_width, 0.0f));
}

bool AtariEnv::act(const AtariAction& action)
{
    assert(action.getPlayer() == Player::kPlayer1);
    assert(action.getActionID() >= 0 && action.getActionID() < kAtariActionSize);

    reward_ = 0;
    for (int i = 0; i < kAtariFrameSkip; ++i) { reward_ += ale_.act(ale::Action(action.getActionID())); }
    total_reward_ += reward_;
    actions_.push_back(action);
    observations_.push_back(getObservationString());
    // only keep recent 300 observations in atari games to save memory
    if (observations_.size() > 300) { observations_[observations_.size() - 300] = ""; }

    // action & observation history
    action_feature_history_.push_back(std::vector<float>(config::nn_input_channel_height * config::nn_input_channel_width, action.getActionID() * 1.0f / kAtariActionSize));
    action_feature_history_.pop_front();
    feature_history_.push_back(getObservation());
    feature_history_.pop_front();

    return true;
}

std::vector<AtariAction> AtariEnv::getLegalActions() const
{
    std::vector<AtariAction> legal_actions;
    for (int action_id = 0; action_id < kAtariActionSize; ++action_id) {
        AtariAction action(action_id, getTurn());
        if (isLegalAction(action)) { legal_actions.push_back(action); }
    }
    return legal_actions;
}

std::vector<float> AtariEnv::getFeatures(utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    std::vector<float> features;
    for (int i = 0; i < kAtariFeatureHistorySize; ++i) { // 1 for action; 3 for RGB, action first since the latest observation didn't have action yet
        features.insert(features.end(), action_feature_history_[i].begin(), action_feature_history_[i].end());
        features.insert(features.end(), feature_history_[i].begin(), feature_history_[i].end());
    }
    assert(static_cast<int>(features.size()) == kAtariFeatureHistorySize * 4 * config::nn_input_channel_height * config::nn_input_channel_width);
    return features;
}

std::vector<float> AtariEnv::getActionFeatures(const AtariAction& action, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    int hidden_size = config::nn_hidden_channel_height * config::nn_hidden_channel_width;
    std::vector<float> action_features(kAtariActionSize * hidden_size, 0.0f);
    std::fill(action_features.begin() + action.getActionID() * hidden_size, action_features.begin() + (action.getActionID() + 1) * hidden_size, 1.0f);
    return action_features;
}

std::vector<float> AtariEnv::getObservation(bool scale_01 /* = true */) const
{
    // get current screen rgb
    std::vector<unsigned char> screen_rgb;
    ale_.getScreenRGB(screen_rgb);

    // resize observation
    cv::Mat source_matrix(ale_.getScreen().height(), ale_.getScreen().width(), CV_8UC3, screen_rgb.data());
    cv::Mat reshape_matrix;
    cv::resize(source_matrix, reshape_matrix, cv::Size(kAtariResolution, kAtariResolution), 0, 0, cv::INTER_AREA);

    // change hwc to chw
    std::vector<float> observation(3 * kAtariResolution * kAtariResolution);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < kAtariResolution * kAtariResolution; ++j) {
            observation[i * kAtariResolution * kAtariResolution + j] = static_cast<float>(reshape_matrix.at<unsigned char>(j * 3 + i));
            if (scale_01) { observation[i * kAtariResolution * kAtariResolution + j] /= 255.0f; }
        }
    }
    return observation;
}

std::string AtariEnv::getObservationString() const
{
    std::string obs_string;
    std::vector<float> observation = getObservation(false);
    for (const auto& o : observation) {
        assert(o >= 0 && o < 256);
        obs_string += static_cast<char>(o);
    }
    return obs_string;
}

void AtariEnvLoader::reset()
{
    BaseEnvLoader::reset();
    observations_.clear();
}

bool AtariEnvLoader::loadFromString(const std::string& content)
{
    bool success = BaseEnvLoader::loadFromString(content);
    addObservations(getTag("OBS"));
    return success;
}

void AtariEnvLoader::loadFromEnvironment(const AtariEnv& env, const std::vector<std::unordered_map<std::string, std::string>>& action_info_history /* = {} */)
{
    BaseEnvLoader<AtariAction, AtariEnv>::loadFromEnvironment(env, action_info_history);
    addTag("SD", std::to_string(env.getSeed()));
}

std::vector<float> AtariEnvLoader::getFeatures(const int pos, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    std::pair<int, int> data_range = {-1, -1};
    const std::string& dlen = getTag("DLEN");
    if (!dlen.empty()) { data_range = {std::stoi(dlen), std::stoi(dlen.substr(dlen.find("-") + 1))}; }
    if (pos < data_range.first || pos > data_range.second) { return getFeaturesByReplay(pos, rotation); }

    std::vector<float> features;
    int start = pos - kAtariFeatureHistorySize + 1, end = pos;
    for (int i = start; i <= end; ++i) { // 1 for action; 3 for RGB, action first since the latest observation didn't have action yet
        std::vector<float> action_features(config::nn_input_channel_height * config::nn_input_channel_width, ((i - 1 >= 0) ? action_pairs_[i - 1].first.getActionID() * 1.0f / kAtariActionSize : 0.0f));
        features.insert(features.end(), action_features.begin(), action_features.end());
        if (i >= 0) {
            for (const auto& o : observations_[i]) { features.push_back(static_cast<unsigned int>(static_cast<unsigned char>(o)) / 255.0f); }
        } else {
            std::vector<float> f(3 * kAtariResolution * kAtariResolution, 0.0f);
            features.insert(features.end(), f.begin(), f.end());
        }
    }
    return features;
}

std::vector<float> AtariEnvLoader::getActionFeatures(const int pos, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    assert(pos < static_cast<int>(action_pairs_.size()));
    const AtariAction& action = action_pairs_[pos].first;
    int hidden_size = config::nn_hidden_channel_height * config::nn_hidden_channel_width;
    std::vector<float> action_features(kAtariActionSize * hidden_size, 0.0f);
    std::fill(action_features.begin() + action.getActionID() * hidden_size, action_features.begin() + (action.getActionID() + 1) * hidden_size, 1.0f);
    return action_features;
}

float AtariEnvLoader::getValue(const int pos) const
{
    assert(pos < static_cast<int>(action_pairs_.size()));

    // calculate n-step return
    int n_step = std::min(pos + config::learner_n_step_return, static_cast<int>(action_pairs_.size()));
    float value = 0.0f;
    for (int step = 0; step < n_step; ++step) {
        float reward = ((step == n_step - 1) ? BaseEnvLoader::getValue(step) : getReward(step));
        value += std::pow(config::actor_mcts_reward_discount, step) * reward;
    }
    return value;
}

void AtariEnvLoader::addObservations(const std::string& compressed_obs)
{
    observations_.resize(action_pairs_.size() + 1, "");
    if (compressed_obs.empty()) { return; }

    int obs_length = 3 * kAtariResolution * kAtariResolution;
    std::string observations_str = utils::decompressString(compressed_obs);
    assert(observations_str.size() % obs_length == 0);

    int index = observations_.size();
    for (size_t end = observations_str.size(); end > 0; end -= obs_length) { observations_[--index] = observations_str.substr(end - obs_length, obs_length); }
    assert(index >= 0);
}

std::vector<float> AtariEnvLoader::getFeaturesByReplay(const int pos, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    assert(pos <= static_cast<int>(action_pairs_.size()));

    AtariEnv env;
    env.reset(std::stoi(getTag("SD")));
    for (int i = 0; i < pos; ++i) { env.act(action_pairs_[i].first); }
    return env.getFeatures(rotation);
}

} // namespace minizero::env::atari
