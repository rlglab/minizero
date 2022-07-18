#pragma once

#include "base_actor.h"
#include "network.h"
#include "paralleler.h"
#include <deque>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace minizero::actor {

class ThreadSharedData {
public:
    int getAvailableActorIndex();

    bool do_cpu_job_;
    int actor_index_;
    std::mutex mutex_;
    std::vector<std::shared_ptr<BaseActor>> actors_;
    std::vector<std::shared_ptr<network::Network>> networks_;
    std::vector<std::vector<std::shared_ptr<network::NetworkOutput>>> network_outputs_;
};

class SlaveThread : public utils::BaseSlaveThread<ThreadSharedData> {
public:
    SlaveThread(int id, ThreadSharedData& shared_data)
        : BaseSlaveThread(id, shared_data) {}

    void initialize() override;
    void runJob() override;
    inline bool isDone() override { return false; }

private:
    bool doCPUJob();
    void doGPUJob();
};

class ActorGroup : public utils::BaseParalleler<class ThreadSharedData, class SlaveThread> {
public:
    ActorGroup() {}

    void run() override;
    void initialize() override;
    void summarize() override {}

protected:
    void createNeuralNetworks();
    void createActors();
    void handleIO();
    void handleFinishedGame();
    void handleCommand();

    bool running_;
    std::deque<std::string> commands_;
    std::unordered_set<std::string> ignored_commands_;
};

} // namespace minizero::actor
