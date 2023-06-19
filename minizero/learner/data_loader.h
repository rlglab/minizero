#pragma once

#include "environment.h"
#include "paralleler.h"
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace minizero::learner {

class BaseBatchDataPtr {
public:
    BaseBatchDataPtr() {}
    virtual ~BaseBatchDataPtr() = default;
};

class BatchDataPtr : public BaseBatchDataPtr {
public:
    BatchDataPtr() {}
    virtual ~BatchDataPtr() = default;

    float* features_;
    float* action_features_;
    float* policy_;
    float* value_;
    float* reward_;
    float* loss_scale_;
    int* sampled_index_;
};

class ReplayBuffer {
public:
    ReplayBuffer();

    std::mutex mutex_;
    int num_data_;
    float game_priority_sum_;
    std::deque<float> game_priorities_;
    std::deque<std::deque<float>> position_priorities_;
    std::deque<EnvironmentLoader> env_loaders_;

    void addData(const EnvironmentLoader& env_loader);
    std::pair<int, int> sampleEnvAndPos();
    int sampleIndex(const std::deque<float>& weight);
    float getLossScale(const std::pair<int, int>& p);
};

class DataLoaderSharedData : public utils::BaseSharedData {
public:
    std::string getNextEnvString();
    int getNextBatchIndex();

    virtual void createDataPtr() { data_ptr_ = std::make_shared<BatchDataPtr>(); }
    inline std::shared_ptr<BatchDataPtr> getDataPtr() { return std::static_pointer_cast<BatchDataPtr>(data_ptr_); }

    int batch_index_;
    ReplayBuffer replay_buffer_;
    std::mutex mutex_;
    std::deque<std::string> env_strings_;
    std::shared_ptr<BaseBatchDataPtr> data_ptr_;
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
    virtual bool sampleData();

    virtual void setAlphaZeroTrainingData(int batch_index);
    virtual void setMuZeroTrainingData(int batch_index);

    inline std::shared_ptr<DataLoaderSharedData> getSharedData() { return std::static_pointer_cast<DataLoaderSharedData>(shared_data_); }
};

class DataLoader : public utils::BaseParalleler {
public:
    DataLoader(const std::string& conf_file_name);

    void initialize() override;
    void summarize() override {}
    virtual void loadDataFromFile(const std::string& file_name);
    virtual void sampleData();
    virtual void updatePriority(int* sampled_index, float* batch_v_first, float* batch_v_last);

    void createSharedData() override { shared_data_ = std::make_shared<DataLoaderSharedData>(); }
    std::shared_ptr<utils::BaseSlaveThread> newSlaveThread(int id) override { return std::make_shared<DataLoaderThread>(id, shared_data_); }
    inline std::shared_ptr<DataLoaderSharedData> getSharedData() { return std::static_pointer_cast<DataLoaderSharedData>(shared_data_); }
};

} // namespace minizero::learner
