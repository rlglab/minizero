#pragma once

#include "atari.h"
#include "configuration.h"
#include "paralleler.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace minizero::env::atari {

class ObsRemoverThreadSharedData : public utils::BaseSharedData {
public:
    std::string getAvailableSgfPath();

    std::vector<std::string> all_sgf_path_;
    std::vector<std::string>::iterator all_sgf_path_it_;
    std::mutex mutex_;
};

class ObsRemoverSlaveThread : public utils::BaseSlaveThread {
public:
    ObsRemoverSlaveThread(int id, std::shared_ptr<utils::BaseSharedData> shared_data)
        : BaseSlaveThread(id, shared_data) {}

    void initialize() override {}
    void runJob() override;
    bool removeSingleObs();

    bool isDone() override { return false; }

    inline std::shared_ptr<ObsRemoverThreadSharedData> getSharedData() { return std::static_pointer_cast<ObsRemoverThreadSharedData>(shared_data_); }
};

class ObsRemover : public utils::BaseParalleler {
public:
    ObsRemover() {}

    bool isProperGame(const std::string& path);
    void run(std::string& dir_path);
    void initialize() override;
    void summarize() override {}

    inline std::shared_ptr<ObsRemoverThreadSharedData> getSharedData() { return std::static_pointer_cast<ObsRemoverThreadSharedData>(shared_data_); }

protected:
    void createSharedData() override { shared_data_ = std::make_shared<ObsRemoverThreadSharedData>(); }
    std::shared_ptr<utils::BaseSlaveThread> newSlaveThread(int id) override { return std::make_shared<ObsRemoverSlaveThread>(id, shared_data_); }
};

} // namespace minizero::env::atari
