#include "data_loader.h"
#include "configuration.h"
#include "environment.h"
#include "random.h"
#include "rotation.h"
#include <climits>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <utility>

namespace minizero::learner {

using namespace minizero;
using namespace minizero::utils;

DataLoader::DataLoader(std::string conf_file_name)
{
    env::setUpEnv();
    config::ConfigureLoader cl;
    config::setConfiguration(cl);
    cl.loadFromFile(conf_file_name);
}

void DataLoader::loadDataFromFile(const std::string& file_name)
{
    std::ifstream fin(file_name, std::ifstream::in);
    for (std::string content; std::getline(fin, content);) {
        EnvironmentLoader env_loader;
        if (!env_loader.loadFromString(content)) { continue; }

        std::pair<int, int> data_range = {0, static_cast<int>(env_loader.getActionPairs().size()) - 1};
        const std::string& dlen = env_loader.getTag("DLEN");
        if (!dlen.empty()) { data_range = {std::stoi(dlen), std::stoi(dlen.substr(dlen.find("-") + 1))}; }
        for (int i = data_range.first; i <= data_range.second; ++i) {
            priorities_.push_back(env_loader.getPriority(i));
            env_pos_index_.push_back({env_loaders_.size(), i});
        }
        env_loaders_.push_back(env_loader);
    }

    // create distribution
    distribution_ = std::discrete_distribution<>(priorities_.begin(), priorities_.end());
}

AlphaZeroData DataLoader::sampleAlphaZeroTrainingData()
{
    // random pickup one position
    const std::pair<int, int>& p = env_pos_index_[distribution_(Random::generator_)];
    int env_id = p.first, pos = p.second;

    // package AlphaZero training data
    AlphaZeroData data;
    Rotation rotation = static_cast<Rotation>(Random::randInt() % static_cast<int>(Rotation::kRotateSize));
    const EnvironmentLoader& env_loader = env_loaders_[env_id];
    data.features_ = env_loader.getFeatures(pos, rotation);
    data.policy_ = env_loader.getPolicy(pos, rotation);
    data.value_ = env_loader.getValue(pos);

    return data;
}

MuZeroData DataLoader::sampleMuZeroTrainingData(int unrolling_step)
{
    // random pickup one position
    const std::pair<int, int>& p = env_pos_index_[distribution_(Random::generator_)];
    int env_id = p.first, pos = p.second;
    while (pos + unrolling_step >= static_cast<int>(env_loaders_[env_id].getActionPairs().size())) {
        // random again until we can unroll all steps
        const std::pair<int, int>& p = env_pos_index_[distribution_(Random::generator_)];
        env_id = p.first, pos = p.second;
    }

    // package MuZero training data
    MuZeroData data;
    const EnvironmentLoader& env_loader = env_loaders_[env_id];
    Rotation rotation = static_cast<Rotation>(Random::randInt() % static_cast<int>(Rotation::kRotateSize));
    data.features_ = env_loader.getFeatures(pos, rotation);
    for (int step = 0; step <= unrolling_step; ++step) {
        // action features
        std::vector<float> action_features = env_loader.getActionFeatures(pos + step, rotation);
        if (step < unrolling_step) { data.action_features_.insert(data.action_features_.end(), action_features.begin(), action_features.end()); }

        // policy
        std::vector<float> policy = env_loader.getPolicy(pos + step, rotation);
        data.policy_.insert(data.policy_.end(), policy.begin(), policy.end());

        // value
        std::vector<float> value = env_loader.getValue(pos + step);
        data.value_.insert(data.value_.end(), value.begin(), value.end());

        // reward
        std::vector<float> reward = env_loader.getReward(pos + step);
        data.reward_.insert(data.reward_.end(), reward.begin(), reward.end());
    }
    return data;
}

} // namespace minizero::learner
