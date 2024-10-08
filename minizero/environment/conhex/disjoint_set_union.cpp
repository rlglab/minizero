#include "disjoint_set_union.h"

namespace minizero::env::conhex {

using namespace minizero::utils;

DisjointSetUnion::DisjointSetUnion(int size)
{
    size_ = size;
    reset();
}

void DisjointSetUnion::reset()
{
    set_size_.resize(size_ + 4, 0); // +4 stands for top/left/right/bottom
    parent_.resize(size_ + 4);      // +4 stands for top/left/right/bottom

    // DSU reset
    for (int i = 0; i < size_ + 4; ++i) {
        parent_[i] = i;
        set_size_[i] = 0;
    }
}

int DisjointSetUnion::find(int index)
{
    if (parent_[index] == index) { return index; }
    return parent_[index] = find(parent_[index]);
}

void DisjointSetUnion::connect(int from_cell_id, int to_cell_id)
{
    // same as Union in DSU
    int fa = find(from_cell_id), fb = find(to_cell_id);
    if (fa == fb) { return; } // already same
    if (set_size_[fa] > set_size_[fb]) { std::swap(fa, fb); }
    parent_[fb] = fa;
    set_size_[fa] = set_size_[fb];
}

} // namespace minizero::env::conhex
