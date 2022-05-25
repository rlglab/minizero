#pragma once

#include "environment.h"
#include "random.h"
#include <map>
#include <string>
#include <vector>

namespace solver {

class PCNData {
public:
    std::vector<float> features_;
    std::vector<float> policy_;
    std::vector<float> value_n_;
    std::vector<float> value_m_;
};

class DataLoader {
public:
    DataLoader(std::string conf_file_name);

    void loadDataFromFile(const std::string& file_name);
    PCNData getPCNTrainingData();

    inline void seed(int random_seed) { minizero::utils::Random::seed(random_seed); }
    inline int getDataSize() const { return env_loaders_.back().second; }

private:
    std::pair<int, int> getEnvIDAndPosition(int index) const;
    std::vector<float> getPolicyDistribution(const EnvironmentLoader& env_loader, int pos, minizero::utils::Rotation rotation = minizero::utils::Rotation::kRotationNone);
    void setPCNValue(std::vector<float>& values, float value);

    Environment env_;
    std::vector<std::pair<EnvironmentLoader, int>> env_loaders_;
};

} // namespace solver