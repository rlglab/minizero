#pragma once

#include <boost/thread.hpp>

namespace minizero::utils {

template <class SharedData>
class BaseSlaveThread {
public:
    BaseSlaveThread(int id, SharedData& shared_data)
        : id_(id),
          shared_data_(shared_data),
          start_barrier_(2),
          finish_barrier_(2) {}
    virtual ~BaseSlaveThread() = default;

    virtual void run()
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
    SharedData& shared_data_;
    boost::barrier start_barrier_;
    boost::barrier finish_barrier_;
};

template <class SharedData, class SlaveThread>
class BaseParalleler {
public:
    BaseParalleler() {}
    virtual ~BaseParalleler() = default;

    virtual void run()
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
        for (int id = 0; id < num_threads; ++id) {
            slave_threads_.emplace_back(std::make_shared<SlaveThread>(id, shared_data_));
            thread_groups_.create_thread(boost::bind(&SlaveThread::run, slave_threads_.back()));
        }
    }

    SharedData shared_data_;
    boost::thread_group thread_groups_;
    std::vector<std::shared_ptr<SlaveThread>> slave_threads_;
};

} // namespace minizero::utils