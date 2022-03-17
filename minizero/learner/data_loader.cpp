#include "data_loader.h"
#include "environment.h"
#include <climits>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace minizero::learner {

DataLoader::DataLoader()
{
    minizero::env::SetUpEnv();
}

void DataLoader::LoadDataFromDirectory(const std::string& directory_name)
{
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory_name)) {
        LoadDataFromFile(entry.path());
    }
}

void DataLoader::LoadDataFromFile(const std::string& file_name)
{
    std::ifstream fin(file_name, std::ifstream::in);
    for (std::string content; std::getline(fin, content);) {
        EnvironmentLoader env_loader;
        if (!env_loader.LoadFromString(content)) { continue; }
        int total_length = env_loaders_.empty() ? 0 : env_loaders_.back().second;
        env_loaders_.push_back({env_loader, total_length + env_loader.GetActionPairs().size()});
    }
}

void DataLoader::CalculateFeaturesAndLabel(int index)
{
    std::pair<int, int> p = GetEnvIDAndPosition(index);
    int env_id = p.first, pos = p.second;

    const EnvironmentLoader& env_loader = env_loaders_[env_id].first;
    env_.Reset();
    for (int i = 0; i < pos; ++i) { env_.Act(env_loader.GetActionPairs()[i].first); }

    features_ = env_.GetFeatures();
    policy_ = env_loader.GetPolicyDistribution(pos);
    value_ = env_loader.GetReturn();
}

std::pair<int, int> DataLoader::GetEnvIDAndPosition(int index) const
{
    int left = 0, right = env_loaders_.size();
    int random_pos = index % env_loaders_.back().second;

    while (left < right) {
        int mid = left + (right - left) / 2;
        if (random_pos >= env_loaders_[mid].second) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }

    return {left, (left == 0 ? random_pos : random_pos - env_loaders_[left - 1].second)};
}

} // namespace minizero::learner