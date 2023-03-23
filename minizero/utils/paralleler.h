#pragma once

#include <boost/thread.hpp>
#include <memory>
#include <vector>

namespace minizero::utils {

class BaseSharedData {
public:
    BaseSharedData() {}
    virtual ~BaseSharedData() = default;
};

class BaseSlaveThread {
public:
    BaseSlaveThread(int id, std::shared_ptr<BaseSharedData> shared_data)
        : id_(id),
          shared_data_(shared_data),
          start_barrier_(2),
          finish_barrier_(2) {}
    virtual ~BaseSlaveThread() = default;

    void run()
    {
        initialize();
        while (!isDone()) {
            start_barrier_.wait();
            runJob();
            finish_barrier_.wait();
        }
    }

    virtual void initialize() = 0;
    virtual void runJob() = 0;
    virtual bool isDone() = 0;

    inline void start() { start_barrier_.wait(); }
    inline void finish() { finish_barrier_.wait(); }

protected:
    int id_;
    std::shared_ptr<BaseSharedData> shared_data_;
    boost::barrier start_barrier_;
    boost::barrier finish_barrier_;
};

class BaseParalleler {
public:
    BaseParalleler() {}

    virtual ~BaseParalleler()
    {
        thread_groups_.interrupt_all();
        thread_groups_.join_all();
    }

    void run()
    {
        initialize();
        for (auto& t : slave_threads_) { t->start(); }
        for (auto& t : slave_threads_) { t->finish(); }
        summarize();
    }

    virtual void initialize() = 0;
    virtual void summarize() = 0;

protected:
    void createSlaveThreads(int num_threads)
    {
        createSharedData();
        for (int id = 0; id < num_threads; ++id) {
            slave_threads_.emplace_back(newSlaveThread(id));
            thread_groups_.create_thread(boost::bind(&BaseSlaveThread::run, slave_threads_.back()));
        }
    }

    virtual void createSharedData() = 0;
    virtual std::shared_ptr<BaseSlaveThread> newSlaveThread(int id) = 0;

    boost::thread_group thread_groups_;
    std::shared_ptr<BaseSharedData> shared_data_;
    std::vector<std::shared_ptr<BaseSlaveThread>> slave_threads_;
};

} // namespace minizero::utils
