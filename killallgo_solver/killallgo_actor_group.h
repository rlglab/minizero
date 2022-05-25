#pragma once

#include "killallgo_actor.h"
#include "paralleler.h"
#include "proof_cost_network.h"
#include <mutex>
#include <vector>

namespace solver {

class ThreadSharedData {
public:
    int getAvailableActorIndex();
    void outputRecord(const std::string& record);

    bool do_cpu_job_;
    int actor_index_;
    std::mutex mutex_;
    std::vector<std::shared_ptr<KillallGoActor>> actors_;
    std::vector<std::shared_ptr<ProofCostNetwork>> networks_;
    std::vector<std::vector<std::shared_ptr<minizero::network::NetworkOutput>>> network_outputs_;
};

class SlaveThread : public minizero::utils::BaseSlaveThread<ThreadSharedData> {
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

class KillallGoActorGroup : public minizero::utils::BaseParalleler<class ThreadSharedData, class SlaveThread> {
public:
    KillallGoActorGroup() {}

    void run();
    void initialize();
    void summarize() {}

protected:
    void createNeuralNetworks();
    void createActors();
};

} // namespace solver