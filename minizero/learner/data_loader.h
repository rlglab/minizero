#pragma once

#include "environment.h"
#include "paralleler.h"
#include "random.h"
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace minizero::learner {

class Data {
public:
    void clear();
    void concatData(const Data& data);

    std::vector<float> features_;
    std::vector<float> action_features_;
    std::vector<float> policy_;
    std::vector<float> value_;
    std::vector<float> reward_;
};

class DataLoaderSharedData : public utils::BaseSharedData {
public:
    int getNextEnvIndex();

    int env_index_;
    std::mutex mutex_;
    std::vector<std::string> env_strings_;

    Data data_;
    int batch_size_;
    std::vector<float> priorities_;
    std::vector<EnvironmentLoader> env_loaders_;
    std::vector<std::pair<int, int>> env_pos_index_;
    std::discrete_distribution<> distribution_;
};

class DataLoaderThread : public utils::BaseSlaveThread {
public:
    DataLoaderThread(int id, std::shared_ptr<utils::BaseSharedData> shared_data)
        : BaseSlaveThread(id, shared_data) {}

    void initialize() override;
    void runJob() override;
    bool isDone() override { return false; }

protected:
    virtual bool addEnvironmentLoader();
    bool sampleAlphaZeroTrainingData();
    bool sampleMuZeroTrainingData();

    inline std::shared_ptr<DataLoaderSharedData> getSharedData() { return std::static_pointer_cast<DataLoaderSharedData>(shared_data_); }
};

class DataLoader : public utils::BaseParalleler {
public:
    DataLoader(const std::string& conf_file_name);

    void initialize() override;
    void summarize() override {}
    virtual void loadDataFromFile(const std::string& file_name);
    virtual Data sampleData();

    void createSharedData() override { shared_data_ = std::make_shared<DataLoaderSharedData>(); }
    std::shared_ptr<utils::BaseSlaveThread> newSlaveThread(int id) override { return std::make_shared<DataLoaderThread>(id, shared_data_); }
    inline std::shared_ptr<DataLoaderSharedData> getSharedData() { return std::static_pointer_cast<DataLoaderSharedData>(shared_data_); }
};

} // namespace minizero::learner
