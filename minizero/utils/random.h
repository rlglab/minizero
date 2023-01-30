#pragma once

#include <limits>
#include <random>
#include <vector>

namespace minizero::utils {

class Random {
public:
    static inline void seed(int seed) { generator_.seed(seed); }
    static inline int randInt() { return int_distribution_(generator_); }
    static inline double randReal(double range = 1.0f) { return real_distribution_(generator_) * range; }

    static inline std::vector<float> randDirichlet(float alpha, int size)
    {
        std::vector<float> dirichlet;
        std::gamma_distribution<float> gamma_distribution(alpha);
        for (int i = 0; i < size; ++i) { dirichlet.emplace_back(gamma_distribution(generator_)); }
        float sum = std::accumulate(dirichlet.begin(), dirichlet.end(), 0.0f);
        if (sum < std::numeric_limits<float>::min()) { return dirichlet; }
        for (int i = 0; i < size; ++i) { dirichlet[i] /= sum; }
        return dirichlet;
    }

    static inline std::vector<float> randGumbel(int size)
    {
        std::extreme_value_distribution<float> gumbel_distribution(0.0, 1.0);
        std::vector<float> gumbel;
        for (int i = 0; i < size; ++i) {
            float value = gumbel_distribution(generator_);
            while (std::isinf(value)) { value = gumbel_distribution(generator_); }
            gumbel.emplace_back(value);
        }
        return gumbel;
    }

    static thread_local std::mt19937 generator_;
    static thread_local std::uniform_int_distribution<int> int_distribution_;
    static thread_local std::uniform_real_distribution<double> real_distribution_;
};

} // namespace minizero::utils
