#pragma once

#include "environment.h"
#include "random.h"
#include <map>
#include <string>
#include <vector>

namespace minizero::learner {

using namespace minizero;

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

private:
    std::pair<int, int> getEnvIDAndPosition(int index) const;

    Environment env_;
    std::vector<std::pair<EnvironmentLoader, int>> env_loaders_;
};

} // namespace minizero::learner