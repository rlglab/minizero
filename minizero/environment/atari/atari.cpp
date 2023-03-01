#include "atari.h"
#include <opencv2/opencv.hpp>

namespace minizero::env::atari {

std::string getAtariActionName(int action_id)
{
    assert(action.getActionID() >= 0 && action.getActionID() < kAtariActionSize);
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
    for (size_t i = 0; i < kAtariFeatureHistorySize; ++i) { // 1 for action; 3 for RGB, action first since the latest observation didn't have action yet
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

} // namespace minizero::env::atari
