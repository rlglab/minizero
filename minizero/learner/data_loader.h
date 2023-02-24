#pragma once

#include "environment.h"
#include "random.h"
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace minizero::learner {

class AlphaZeroData {
public:
    std::vector<float> features_;
    std::vector<float> policy_;
    float value_;
};

class MuZeroData {
public:
    std::vector<float> features_;
    std::vector<float> action_features_;
    std::vector<float> policy_;
    float value_;
};

class DataLoader {
public:
    DataLoader(std::string conf_file_name);

    void loadDataFromFile(const std::string& file_name);
    AlphaZeroData getAlphaZeroTrainingData();
    MuZeroData getMuZeroTrainingData(int unrolling_step);

    inline void seed(int random_seed) { utils::Random::seed(random_seed); }
    inline int getDataSize() const { return env_loaders_.back().second; }

protected:
    std::pair<int, int> getEnvIDAndPosition(int index) const;

    std::vector<std::pair<EnvironmentLoader, int>> env_loaders_;
};

} // namespace minizero::learner
