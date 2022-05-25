#pragma once

#include "base_actor.h"
#include "network.h"
#include "paralleler.h"
#include <mutex>
#include <vector>

namespace minizero::actor {

class ThreadSharedData {
public:
    int getAvailableActorIndex();
    void outputRecord(const std::string& record);

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

    void initialize();
    void runJob();
    inline bool isDone() { return false; }

private:
    bool doCPUJob();
    void doGPUJob();
};

class ActorGroup : public utils::BaseParalleler<class ThreadSharedData, class SlaveThread> {
public:
    ActorGroup() {}

    void run();
    void initialize();
    void summarize() {}

protected:
    void createNeuralNetworks();
    void createActors();
};

} // namespace minizero::actor