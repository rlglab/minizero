#pragma once

#include "base_env.h"
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace minizero::env::conhex {

class DisjointSetUnion {
public:
    DisjointSetUnion(int size);
    int find(int index);                            // DSU
    void connect(int from_cell_id, int to_cell_id); // DSU
    void reset();

private:
    std::vector<int> parent_;
    std::vector<int> set_size_;
    int size_;
};

} // namespace minizero::env::conhex
