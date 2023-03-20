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

    std::lock_guard<std::mutex> lock(getSharedData()->mutex_);
    for (int i = data_range.first; i <= data_range.second; ++i) {
        getSharedData()->priorities_.push_back(env_loader.getPriority(i));
        getSharedData()->env_pos_index_.push_back({getSharedData()->env_loaders_.size(), i});
    }
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
    int sampled_index = getSharedData()->distribution_(Random::generator_);
    const std::pair<int, int>& p = getSharedData()->env_pos_index_[sampled_index];
    int env_id = p.first, pos = p.second;

    // AlphaZero training data
    Data data;
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
    int sampled_index = getSharedData()->distribution_(Random::generator_);
    const std::pair<int, int>& p = getSharedData()->env_pos_index_[sampled_index];
    int env_id = p.first, pos = p.second;
    while (pos + config::learner_muzero_unrolling_step >= static_cast<int>(getSharedData()->env_loaders_[env_id].getActionPairs().size())) {
        // random again until we can unroll all steps
        sampled_index = getSharedData()->distribution_(Random::generator_);
        const std::pair<int, int>& p = getSharedData()->env_pos_index_[sampled_index];
        env_id = p.first, pos = p.second;
    }

    // MuZero training data
    Data data;
    const EnvironmentLoader& env_loader = getSharedData()->env_loaders_[env_id];
    Rotation rotation = static_cast<Rotation>(Random::randInt() % static_cast<int>(Rotation::kRotateSize));
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

    // create distribution
    getSharedData()->distribution_ = std::discrete_distribution<>(getSharedData()->priorities_.begin(), getSharedData()->priorities_.end());
}

void DataLoader::sampleData()
{
    getSharedData()->batch_index_ = 0;
    for (auto& t : slave_threads_) { t->start(); }
    for (auto& t : slave_threads_) { t->finish(); }
}

} // namespace minizero::learner
