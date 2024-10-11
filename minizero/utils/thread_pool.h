#pragma once

#include <algorithm>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>

namespace minizero::utils {

class ThreadPool {
public:
    /**
     * Starting a muti-thread task.
     *
     * @param work Callback function with two integer parameters `wid` and `tid`. User defined task.
     *     `wid`: unique work(task) id from 0 to `n_works` - 1.
     *     `tid`: unique thread id from 0 to `n_workers` - 1.
     * @param n_works n independent tasks to do.
     * @param n_workers n threads to use.
     * @return No return.
     */
    void start(std::function<void(int, int)> work, int n_works, int n_workers)
    {
        n_works_ = n_works;
        n_workers_ = n_workers;
        works_count_ = 0;
        workers_.clear();
        for (int i = 0; i < n_workers_; i++)
            workers_.push_back(std::thread(&ThreadPool::start_worker, this, work, i));
        for (auto& w : workers_)
            w.join();
    }

private:
    void start_worker(std::function<void(int, int)> work, int tid)
    {
        while (true) {
            int work_id = works_count_++;
            if (work_id >= n_works_) break;
            work(work_id, tid);
        }
    }

private:
    int n_works_;
    int n_workers_;
    std::atomic<int> works_count_;
    std::vector<std::thread> workers_;
};

} // namespace minizero::utils