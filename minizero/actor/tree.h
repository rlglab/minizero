#pragma once

#include <string>
#include <vector>

namespace minizero::actor {

template <class ExtraData>
class TreeExtraData {
public:
    TreeExtraData() { reset(); }

    inline void reset() { extra_data_.clear(); }
    inline int store(const ExtraData& data)
    {
        int index = extra_data_.size();
        extra_data_.push_back(data);
        return index;
    }
    inline const ExtraData& getExtraData(int index) const
    {
        assert(index >= 0 && index < size());
        return extra_data_[index];
    }
    inline int size() const { return extra_data_.size(); }

private:
    std::vector<ExtraData> extra_data_;
};

class BaseTreeNode {
public:
    BaseTreeNode() {}
    virtual ~BaseTreeNode() = default;

    virtual void reset() = 0;
    virtual std::string toString() const = 0;

    inline bool isLeaf() const { return (num_children_ == 0); }
    inline void setNumChildren(int num_children) { num_children_ = num_children; }
    inline int getNumChildren() const { return num_children_; }

protected:
    int num_children_;
};

template <class TreeNode>
class Tree {
public:
    Tree(long long node_size)
    {
        assert(node_size >= 0);
        nodes_.resize(1 + node_size);
    }

    inline void reset()
    {
        current_node_size_ = 1;
        getRootNode()->reset();
    }

    inline TreeNode* allocateNodes(int size)
    {
        assert(current_node_size_ + size <= static_cast<int>(nodes_.size()));
        TreeNode* node = &nodes_[current_node_size_];
        current_node_size_ += size;
        return node;
    }

    inline TreeNode* getRootNode() { return &nodes_[0]; }
    inline const TreeNode* getRootNode() const { return &nodes_[0]; }

protected:
    long long current_node_size_;
    std::vector<TreeNode> nodes_;
};

} // namespace minizero::actor