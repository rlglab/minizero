#include "data_loader.h"
#include "../killallgo_configuration.h"
#include "environment.h"
#include "rotation.h"
#include <climits>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace solver {

using namespace minizero;
using namespace minizero::utils;

DataLoader::DataLoader(std::string conf_file_name)
{
    env::setUpEnv();
    config::ConfigureLoader cl;
    solver::setConfiguration(cl);
    cl.loadFromFile(conf_file_name);
}

void DataLoader::loadDataFromFile(const std::string& file_name)
{
    std::ifstream fin(file_name, std::ifstream::in);
    for (std::string content; std::getline(fin, content);) {
        EnvironmentLoader env_loader;
        if (!env_loader.loadFromString(content)) { continue; }
        int total_length = env_loaders_.empty() ? 0 : env_loaders_.back().second;
        env_loaders_.push_back({env_loader, total_length + env_loader.getActionPairs().size()});
    }
}

PCNData DataLoader::getPCNTrainingData()
{
    // random pickup one position
    std::pair<int, int> p = getEnvIDAndPosition(Random::randInt() % getDataSize());
    int env_id = p.first, pos = p.second;

    // replay the game until to the selected position
    const EnvironmentLoader& env_loader = env_loaders_[env_id].first;
    env_.reset();
    for (int i = 0; i < pos; ++i) { env_.act(env_loader.getActionPairs()[i].first); }

    // calculate training data
    PCNData data;
    Rotation rotation = static_cast<Rotation>(Random::randInt() % static_cast<int>(Rotation::kRotateSize));
    data.features_ = env_.getFeatures(rotation);
    data.policy_ = getPolicyDistribution(env_loader, pos, rotation);

    // calculate pcn value
    float value_n = (env_loader.getReturn() == -1 ? 0 : solver::nn_value_size - 1);
    float value_m = (env_loader.getReturn() == 1 ? 0 : solver::nn_value_size - 1);
    for (int i = env_loader.getActionPairs().size() - 1; i >= pos; --i) {
        const Action& action = env_loader.getActionPairs()[i].first;
        if (action.getPlayer() == env::Player::kPlayer1) {
            value_n += log10(50);
        } else if (action.getPlayer() == env::Player::kPlayer2) {
            value_m += log10(50);
        }
    }
    setPCNValue(data.value_n_, fmax(0, fmin(solver::nn_value_size - 1, value_n)));
    setPCNValue(data.value_m_, fmax(0, fmin(solver::nn_value_size - 1, value_m)));

    return data;
}

std::pair<int, int> DataLoader::getEnvIDAndPosition(int index) const
{
    int left = 0, right = env_loaders_.size();
    index %= env_loaders_.back().second;

    while (left < right) {
        int mid = left + (right - left) / 2;
        if (index >= env_loaders_[mid].second) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }

    return {left, (left == 0 ? index : index - env_loaders_[left - 1].second)};
}

std::vector<float> DataLoader::getPolicyDistribution(const EnvironmentLoader& env_loader, int pos, utils::Rotation rotation /*= utils::Rotation::kRotationNone*/)
{
    std::vector<float> policy(env_loader.getPolicySize(), 0.0f);
    const std::string& distribution = env_loader.getActionPairs()[pos].second;
    if (distribution.empty()) {
        const Action& action = env_loader.getActionPairs()[pos].first;
        policy[env_loader.getRotatePosition(action.getActionID(), rotation)] = 1.0f;
    } else {
        float sum = 0.0f;
        std::string tmp;
        std::istringstream iss(distribution);
        while (std::getline(iss, tmp, ',')) {
            int position = env_loader.getRotatePosition(std::stoi(tmp.substr(0, tmp.find(":"))), rotation);
            float count = std::stof(tmp.substr(tmp.find(":") + 1));
            policy[position] = count;
            sum += count;
        }
        for (auto& p : policy) { p /= sum; }
    }
    return policy;
}

void DataLoader::setPCNValue(std::vector<float>& values, float value)
{
    values.clear();
    values.resize(solver::nn_value_size, 0.0f);

    int value_floor = floor(value);
    int value_ceil = ceil(value);
    if (value_floor == value_ceil) {
        values[value_floor] = 1.0f;
    } else {
        values[value_floor] = value_ceil - value;
        values[value_ceil] = value - value_floor;
    }
}

} // namespace solver