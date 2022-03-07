#pragma once
#include <random>

namespace minizero::utils {

class Random {
public:
    inline static void Seed(int seed) { generator_.seed(seed); }
    inline static int RandInt() { return int_distribution_(generator_); }
    inline static double RandReal(double range = 1.0f) { return real_distribution_(generator_) * range; }

    static thread_local std::mt19937 generator_;
    static thread_local std::uniform_int_distribution<int> int_distribution_;
    static thread_local std::uniform_real_distribution<double> real_distribution_;
};

} // namespace minizero::utils