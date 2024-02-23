#pragma once

#include "atari.h"
#include "configuration.h"
#include "paralleler.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace minizero::env::atari {

struct EnvInfo {
public:
    int seed_;
    std::vector<int> sgf_ids_;
    AtariEnv env_;
    AtariEnvLoader env_loader_; // longest
    std::vector<size_t> end_positions_;
};

class ObsRecoverThreadSharedData : public utils::BaseSharedData {
public:
    EnvInfo* getAvailableEnvInfoPtr();
    int getEnvIndex(const AtariEnvLoader& env_loader);
    EnvInfo* getInitEnvInfoPtr(const int seed);
    int getInitEnvInfoPtrIndex(const int seed);
    void setSgf(std::string file_path);
    void addEnvInfo(AtariEnvLoader env_loader, int line_id);
    void addEnvInfoToRemove(EnvInfo* env_info_ptr);
    void resetMember();
    std::pair<std::string, int> getNextEnvPair();

    std::ifstream original_file_;
    std::ofstream processed_file_;
    std::vector<std::string> sgfs_;
    std::vector<std::string>::iterator sgfs_it_;
    std::mutex mutex_;
    std::map<int, std::vector<EnvInfo*>> seed_env_info_;
    std::map<int, std::vector<EnvInfo*>>::iterator seed_env_info_it_;
    std::vector<EnvInfo*>::iterator env_info_it_;
    std::vector<EnvInfo*> env_info_to_remove_;
};

class ObsRecoverSlaveThread : public utils::BaseSlaveThread {
public:
    ObsRecoverSlaveThread(int id, std::shared_ptr<utils::BaseSharedData> shared_data)
        : BaseSlaveThread(id, shared_data) {}

    void initialize() override {}
    void runJob() override;
    bool addEnvironmentLoader();
    void handleEndPosition(std::string& sgf, EnvInfo* env_info_ptr);
    void recover();
    bool isDone() override { return false; }

    inline std::shared_ptr<ObsRecoverThreadSharedData> getSharedData() { return std::static_pointer_cast<ObsRecoverThreadSharedData>(shared_data_); }
};

class ObsRecover : public utils::BaseParalleler {
public:
    ObsRecover() {}

    void run(std::string& obs_file_path);
    void initialize() override;
    void summarize() override {}

    inline std::shared_ptr<ObsRecoverThreadSharedData> getSharedData() { return std::static_pointer_cast<ObsRecoverThreadSharedData>(shared_data_); }

protected:
    void removeEnvInfo(EnvInfo* env_info_ptr);
    std::vector<std::string> getAllSgfPath(const std::string& dir_path);
    void runSingleSgf(const std::string& path);

    void createSharedData() override { shared_data_ = std::make_shared<ObsRecoverThreadSharedData>(); }
    std::shared_ptr<utils::BaseSlaveThread> newSlaveThread(int id) override { return std::make_shared<ObsRecoverSlaveThread>(id, shared_data_); }
};

} // namespace minizero::env::atari
