#pragma once

#include <cassert>
#include <string>
#include <vector>

namespace minizero::actor {

template <class Data>
class TreeData {
public:
    TreeData() { reset(); }

    inline void reset() { data_.clear(); }
    inline int store(const Data& data)
    {
        int index = data_.size();
        data_.push_back(data);
        return index;
    }
    inline const Data& getData(int index) const
    {
        assert(index >= 0 && index < size());
        return data_[index];
    }
    inline int size() const { return data_.size(); }

private:
    std::vector<Data> data_;
};

class TreeNode {
public:
    TreeNode() {}
    virtual ~TreeNode() = default;

    virtual void reset() = 0;
    virtual std::string toString() const = 0;
    virtual bool displayInTreeLog() const { return true; }

    inline bool isLeaf() const { return (num_children_ == 0); }
    inline void setAction(Action action) { action_ = action; }
    inline void setNumChildren(int num_children) { num_children_ = num_children; }
    inline void setFirstChild(TreeNode* first_child) { first_child_ = first_child; }
    inline Action getAction() const { return action_; }
    inline int getNumChildren() const { return num_children_; }
    inline TreeNode* getFirstChild() const { return first_child_; }

protected:
    Action action_;
    int num_children_;
    TreeNode* first_child_;
};

class Tree {
public:
    Tree(long long tree_node_size)
        : tree_node_size_(tree_node_size),
          nodes_(nullptr)
    {
        assert(tree_node_size >= 0);
    }

    ~Tree()
    {
        if (nodes_) { delete[] nodes_; }
    }

    inline void reset()
    {
        if (!nodes_) { nodes_ = createTreeNodes(1 + tree_node_size_); }
        current_node_size_ = 1;
        getRootNode()->reset();
    }

    inline TreeNode* allocateNodes(int size)
    {
        assert(current_node_size_ + size <= tree_node_size_);
        TreeNode* node = getNodeIndex(current_node_size_);
        current_node_size_ += size;
        return node;
    }

    std::string toString(const std::string& env_string)
    {
        assert(!env_string.empty() && env_string.back() == ')');
        std::ostringstream oss;
        TreeNode* pRoot = getRootNode();
        std::string env_prefix = env_string.substr(0, env_string.size() - 1);
        oss << env_prefix << "C[" << pRoot->toString() << "]" << getTreeInfo_r(pRoot) << ")";
        return oss.str();
    }

    std::string getTreeInfo_r(const TreeNode* pNode) const
    {
        std::ostringstream oss;

        int numChildren = 0;
        TreeNode* pChild = pNode->getFirstChild();
        for (int i = 0; i < pNode->getNumChildren(); ++i, ++pChild) {
            if (pChild->isLeaf()) { continue; }
            ++numChildren;
        }

        pChild = pNode->getFirstChild();
        for (int i = 0; i < pNode->getNumChildren(); ++i, ++pChild) {
            if (!pChild->displayInTreeLog()) { continue; }
            if (numChildren > 1) { oss << "("; }
            oss << playerToChar(pChild->getAction().getPlayer())
                << "[" << pChild->getAction().getActionID() << "]"
                << "C[" << pChild->toString() << "]" << getTreeInfo_r(pChild);
            if (numChildren > 1) { oss << ")"; }
        }
        return oss.str();
    }

    inline TreeNode* getRootNode() { return &nodes_[0]; }
    inline const TreeNode* getRootNode() const { return &nodes_[0]; }

protected:
    virtual TreeNode* createTreeNodes(long long tree_node_size) = 0;
    virtual TreeNode* getNodeIndex(int index) = 0;

    long long tree_node_size_;
    long long current_node_size_;
    TreeNode* nodes_;
};

} // namespace minizero::actor