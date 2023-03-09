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
    std::vector<float> value_;
};

class MuZeroData {
public:
    std::vector<float> features_;
    std::vector<float> action_features_;
    std::vector<float> policy_;
    std::vector<float> value_;
    std::vector<float> reward_;
};

class DataLoader {
public:
    DataLoader(std::string conf_file_name);

    void loadDataFromFile(const std::string& file_name);
    AlphaZeroData sampleAlphaZeroTrainingData();
    MuZeroData sampleMuZeroTrainingData(int unrolling_step);

    inline void seed(int random_seed) { utils::Random::seed(random_seed); }

protected:
    std::vector<float> priorities_;
    std::vector<EnvironmentLoader> env_loaders_;
    std::vector<std::pair<int, int>> env_pos_index_;
    std::discrete_distribution<> distribution_;
};

} // namespace minizero::learner
