#pragma once

#include <cassert>
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
    virtual bool displayInTreeLog() const { return true; }

    inline bool isLeaf() const { return (num_children_ == 0); }
    inline void setAction(Action action) { action_ = action; }
    inline void setNumChildren(int num_children) { num_children_ = num_children; }
    inline void setFirstChild(BaseTreeNode* first_child) { first_child_ = first_child; }
    inline Action getAction() const { return action_; }
    inline int getNumChildren() const { return num_children_; }
    inline BaseTreeNode* getFirstChild() const { return first_child_; }

protected:
    Action action_;
    int num_children_;
    BaseTreeNode* first_child_;
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
            if (pChild->getCount() == 0) { continue; }
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
    long long current_node_size_;
    std::vector<TreeNode> nodes_;
};

} // namespace minizero::actor