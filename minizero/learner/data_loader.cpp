#include "data_loader.h"
#include "configuration.h"
#include "environment.h"
#include "random.h"
#include "rotation.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
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

void ReplayBuffer::addData(const EnvironmentLoader& env_loader)
{
    std::pair<int, int> data_range = env_loader.getDataRange();
    std::deque<float> position_priorities(data_range.second + 1, 0.0f);
    float max_priority = std::numeric_limits<float>::min();
    for (int i = data_range.first; i <= data_range.second; ++i) {
        float priority = env_loader.getPriority(i);
        max_priority = std::max(max_priority, priority);
        position_priorities[i] = priority;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // add new data to replay buffer
    num_data_ += (data_range.second - data_range.first + 1);
    position_priorities_.push_back(position_priorities);
    game_priorities_.push_back(max_priority);
    env_loaders_.push_back(env_loader);

    // remove old data if replay buffer is full
    const size_t replay_buffer_max_size = config::zero_replay_buffer * config::zero_num_games_per_iteration;
    while (position_priorities_.size() > replay_buffer_max_size) {
        data_range = env_loaders_.front().getDataRange();
        num_data_ -= (data_range.second - data_range.first + 1);
        position_priorities_.pop_front();
        game_priorities_.pop_front();
        env_loaders_.pop_front();
    }
}

std::pair<int, int> ReplayBuffer::sampleEnvAndPos()
{
    int env_id = sampleIndex(game_priorities_);
    int pos_id = sampleIndex(position_priorities_[env_id]);
    return {env_id, pos_id};
}

int ReplayBuffer::sampleIndex(const std::deque<float>& weight)
{
    std::discrete_distribution<> dis(weight.begin(), weight.end());
    return dis(Random::generator_);
}

float ReplayBuffer::getLossScale(const std::pair<int, int>& p)
{
    if (!config::learner_use_per) { return 1.0f; }

    // calculate importance sampling ratio
    int env_id = p.first, pos = p.second;
    float game_prob = game_priorities_[env_id] / game_priority_sum_;
    float pos_prob = position_priorities_[env_id][pos] / std::accumulate(position_priorities_[env_id].begin(), position_priorities_[env_id].end(), 0.0f);
    return 1.0f / (num_data_ * game_prob * pos_prob);
}

std::string DataLoaderSharedData::getNextEnvString()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::string env_string = "";
    if (!env_strings_.empty()) {
        env_string = env_strings_.front();
        env_strings_.pop_front();
    }
    return env_string;
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
    std::string env_string = getSharedData()->getNextEnvString();
    if (env_string.empty()) { return false; }

    EnvironmentLoader env_loader;
    if (env_loader.loadFromString(env_string)) { getSharedData()->replay_buffer_.addData(env_loader); }
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
    std::pair<int, int> p = getSharedData()->replay_buffer_.sampleEnvAndPos();
    int env_id = p.first, pos = p.second;

    // AlphaZero training data
    Data data;
    data.loss_scale_.push_back(getSharedData()->replay_buffer_.getLossScale(p));
    const EnvironmentLoader& env_loader = getSharedData()->replay_buffer_.env_loaders_[env_id];
    Rotation rotation = static_cast<Rotation>(Random::randInt() % static_cast<int>(Rotation::kRotateSize));
    data.features_ = env_loader.getFeatures(pos, rotation);
    data.policy_ = env_loader.getPolicy(pos, rotation);
    data.value_ = env_loader.getValue(pos);
    return data;
}

Data DataLoaderThread::sampleMuZeroTrainingData()
{
    // random pickup one position
    std::pair<int, int> p = getSharedData()->replay_buffer_.sampleEnvAndPos();
    int env_id = p.first, pos = p.second;

    // MuZero training data
    Data data;
    data.loss_scale_.push_back(getSharedData()->replay_buffer_.getLossScale(p));
    const EnvironmentLoader& env_loader = getSharedData()->replay_buffer_.env_loaders_[env_id];
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

    for (auto& t : slave_threads_) { t->start(); }
    for (auto& t : slave_threads_) { t->finish(); }
    getSharedData()->replay_buffer_.game_priority_sum_ = std::accumulate(getSharedData()->replay_buffer_.game_priorities_.begin(), getSharedData()->replay_buffer_.game_priorities_.end(), 0.0f);
}

void DataLoader::sampleData()
{
    getSharedData()->batch_index_ = 0;
    for (auto& t : slave_threads_) { t->start(); }
    for (auto& t : slave_threads_) { t->finish(); }
}

} // namespace minizero::learner
