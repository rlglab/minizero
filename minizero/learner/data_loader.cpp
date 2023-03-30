#include "data_loader.h"
#include "configuration.h"
#include "environment.h"
#include "random.h"
#include "rotation.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <utility>

namespace minizero::learner {

using namespace minizero;
using namespace minizero::utils;

void DataPtr::copyData(int batch_index, const Data& data)
{
    std::copy(data.features_.begin(), data.features_.end(), features_ + data.features_.size() * batch_index);
    std::copy(data.action_features_.begin(), data.action_features_.end(), action_features_ + data.action_features_.size() * batch_index);
    std::copy(data.policy_.begin(), data.policy_.end(), policy_ + data.policy_.size() * batch_index);
    std::copy(data.value_.begin(), data.value_.end(), value_ + data.value_.size() * batch_index);
    std::copy(data.reward_.begin(), data.reward_.end(), reward_ + data.reward_.size() * batch_index);
    std::copy(data.loss_scale_.begin(), data.loss_scale_.end(), loss_scale_ + data.loss_scale_.size() * batch_index);
}

void DataLoaderSharedData::clear()
{
    env_index_ = 0;
    env_strings_.clear();
    batch_index_ = 0;
    num_data_ = 0;
    game_priority_sum_ = 0.0f;
    game_priorities_.clear();
    position_priorities_.clear();
    env_loaders_.clear();
}

int DataLoaderSharedData::getNextEnvIndex()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return (env_index_ < static_cast<int>(env_strings_.size()) ? env_index_++ : env_strings_.size());
}

int DataLoaderSharedData::getNextBatchIndex()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return (batch_index_ < config::learner_batch_size ? batch_index_++ : config::learner_batch_size);
}

std::pair<int, int> DataLoaderSharedData::sampleEnvAndPos()
{
    int env_id = sampleIndex(game_priorities_);
    int pos_id = sampleIndex(position_priorities_[env_id]);
    return {env_id, pos_id};
}

int DataLoaderSharedData::sampleIndex(const std::vector<float>& weight)
{
    std::discrete_distribution<> dis(weight.begin(), weight.end());
    return dis(Random::generator_);
}

void DataLoaderThread::initialize()
{
    int seed = config::program_auto_seed ? std::random_device()() : config::program_seed + id_;
    Random::seed(seed);
}

void DataLoaderThread::runJob()
{
    if (!getSharedData()->env_strings_.empty()) {
        while (addEnvironmentLoader()) {}
    } else {
        while (sampleData()) {}
    }
}

bool DataLoaderThread::addEnvironmentLoader()
{
    int env_index = getSharedData()->getNextEnvIndex();
    if (env_index >= static_cast<int>(getSharedData()->env_strings_.size())) { return false; }

    EnvironmentLoader env_loader;
    const std::string& content = getSharedData()->env_strings_[env_index];
    if (!env_loader.loadFromString(content)) { return true; }

    std::pair<int, int> data_range = {0, static_cast<int>(env_loader.getActionPairs().size()) - 1};
    const std::string& dlen = env_loader.getTag("DLEN");
    if (!dlen.empty()) { data_range = {std::stoi(dlen), std::stoi(dlen.substr(dlen.find("-") + 1))}; }

    std::vector<float> position_priorities(data_range.second + 1, 0.0f);
    for (int i = data_range.first; i <= data_range.second; ++i) { position_priorities[i] = env_loader.getPriority(i); }

    std::lock_guard<std::mutex> lock(getSharedData()->mutex_);
    getSharedData()->position_priorities_.push_back(position_priorities);
    getSharedData()->num_data_ += data_range.second - data_range.first + 1;
    getSharedData()->game_priorities_.push_back(*std::max_element(position_priorities.begin(), position_priorities.end()));
    getSharedData()->game_priority_sum_ += getSharedData()->game_priorities_.back();
    getSharedData()->env_loaders_.push_back(env_loader);
    return true;
}

bool DataLoaderThread::sampleData()
{
    int batch_index = getSharedData()->getNextBatchIndex();
    if (batch_index >= config::learner_batch_size) { return false; }

    if (config::nn_type_name == "alphazero") {
        getSharedData()->data_ptr_.copyData(batch_index, sampleAlphaZeroTrainingData());
    } else if (config::nn_type_name == "muzero") {
        getSharedData()->data_ptr_.copyData(batch_index, sampleMuZeroTrainingData());
    } else {
        return false; // should not be here
    }

    return true;
}

Data DataLoaderThread::sampleAlphaZeroTrainingData()
{
    // random pickup one position
    std::pair<int, int> p = getSharedData()->sampleEnvAndPos();
    int env_id = p.first, pos = p.second;

    // AlphaZero training data
    Data data;
    data.loss_scale_.push_back(1.0f);
    const EnvironmentLoader& env_loader = getSharedData()->env_loaders_[env_id];
    Rotation rotation = static_cast<Rotation>(Random::randInt() % static_cast<int>(Rotation::kRotateSize));
    data.features_ = env_loader.getFeatures(pos, rotation);
    data.policy_ = env_loader.getPolicy(pos, rotation);
    data.value_ = env_loader.getValue(pos);
    return data;
}

Data DataLoaderThread::sampleMuZeroTrainingData()
{
    // random pickup one position
    std::pair<int, int> p = getSharedData()->sampleEnvAndPos();
    int env_id = p.first, pos = p.second;

    // MuZero training data
    Data data;
    const EnvironmentLoader& env_loader = getSharedData()->env_loaders_[env_id];
    Rotation rotation = static_cast<Rotation>(Random::randInt() % static_cast<int>(Rotation::kRotateSize));
    if (env_loader.name().find("atari") != std::string::npos) { // importance sampling for atari games
        float game_prob = getSharedData()->game_priorities_[env_id] / getSharedData()->game_priority_sum_;
        float pos_prob = getSharedData()->position_priorities_[env_id][pos] / std::accumulate(getSharedData()->position_priorities_[env_id].begin(), getSharedData()->position_priorities_[env_id].end(), 0.0f);
        data.loss_scale_.push_back(1.0f / (getSharedData()->num_data_ * game_prob * pos_prob));
    } else {
        data.loss_scale_.push_back(1.0f);
    }
    data.features_ = env_loader.getFeatures(pos, rotation);
    for (int step = 0; step <= config::learner_muzero_unrolling_step; ++step) {
        // action features
        std::vector<float> action_features = env_loader.getActionFeatures(pos + step, rotation);
        if (step < config::learner_muzero_unrolling_step) { data.action_features_.insert(data.action_features_.end(), action_features.begin(), action_features.end()); }

        // policy
        std::vector<float> policy = env_loader.getPolicy(pos + step, rotation);
        data.policy_.insert(data.policy_.end(), policy.begin(), policy.end());

        // value
        std::vector<float> value = env_loader.getValue(pos + step);
        data.value_.insert(data.value_.end(), value.begin(), value.end());

        // reward
        std::vector<float> reward = env_loader.getReward(pos + step);
        if (step < config::learner_muzero_unrolling_step) { data.reward_.insert(data.reward_.end(), reward.begin(), reward.end()); }
    }
    return data;
}

DataLoader::DataLoader(const std::string& conf_file_name)
{
    env::setUpEnv();
    config::ConfigureLoader cl;
    config::setConfiguration(cl);
    cl.loadFromFile(conf_file_name);
    initialize();
}

void DataLoader::initialize()
{
    createSlaveThreads(config::learner_num_thread);
}

void DataLoader::loadDataFromFile(const std::string& file_name)
{
    std::ifstream fin(file_name, std::ifstream::in);
    for (std::string content; std::getline(fin, content);) { getSharedData()->env_strings_.push_back(content); }

    getSharedData()->env_index_ = 0;
    for (auto& t : slave_threads_) { t->start(); }
    for (auto& t : slave_threads_) { t->finish(); }
    getSharedData()->env_strings_.clear();
}

void DataLoader::sampleData()
{
    getSharedData()->batch_index_ = 0;
    for (auto& t : slave_threads_) { t->start(); }
    for (auto& t : slave_threads_) { t->finish(); }
}

} // namespace minizero::learner
