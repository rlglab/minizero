#pragma once

#include "environment.h"
#include <map>
#include <string>
#include <vector>

namespace minizero::learner {

class DataLoader {
public:
    DataLoader();

    void LoadDataFromDirectory(const std::string& directory_name);
    void LoadDataFromFile(const std::string& file_name);
    void CalculateFeaturesAndLabel(int index);

    inline std::vector<float> GetFeatures() const { return features_; }
    inline std::vector<float> GetPolicy() const { return policy_; }
    inline float GetValue() const { return value_; }
    inline int GetDataSize() const { return env_loaders_.back().second; }

private:
    std::pair<int, int> GetEnvIDAndPosition(int index) const;

    std::vector<float> features_;
    std::vector<float> policy_;
    float value_;
    Environment env_;
    std::vector<std::pair<EnvironmentLoader, int>> env_loaders_;
};

} // namespace minizero::learner