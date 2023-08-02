#pragma once

#include <algorithm>
#include <cassert>
#include <deque>
#include <utility>
#include <vector>

namespace minizero::utils {

template <typename Key, typename Value>
class VectorMap {
public:
    typedef std::pair<Key, Value> Item;

    VectorMap() {}
    VectorMap(const VectorMap& info) : info_(info.info_) {}
    VectorMap(VectorMap&& info) : info_(std::move(info.info_)) {}
    VectorMap(const std::vector<Item>& info)
    {
        for (const Item& item : info) { info_.emplace_back(item); }
    }
    VectorMap(std::vector<Item>&& info)
    {
        std::vector<Item> buff = std::move(info);
        for (Item& item : buff) { info_.emplace_back(std::move(item)); }
    }

public:
    operator std::vector<Item> &() { return info_; }
    operator const std::vector<Item> &() const { return info_; }
    VectorMap& operator=(const VectorMap& info)
    {
        info_ = info.info_;
        return *this;
    }
    VectorMap& operator=(VectorMap&& info)
    {
        info_ = std::move(info.info_);
        return *this;
    }
    Value& operator[](const Key& key)
    {
        auto it = find(key);
        return (it != end()) ? it->second : info_.emplace_back(key, Value()).second;
    }
    const Value& operator[](const Key& key) const
    {
        static Value npos;
        auto it = find(key);
        return (it != end()) ? it->second : npos;
    }

public:
    Value& at(const Key& key)
    {
        auto it = find(key);
        if (it != end()) { return it->second; }
        throw std::out_of_range("key not found");
    }
    const Value& at(const Key& key) const
    {
        auto it = find(key);
        if (it != end()) { return it->second; }
        throw std::out_of_range("key not found");
    }
    bool empty() const { return info_.empty(); }
    size_t size() const { return info_.size(); }
    void reserve(size_t n) { info_.reserve(n); }

    auto insert(const Item& p)
    {
        auto it = find(p.first);
        return std::pair(it == end() ? info_.insert(it, p) : it, it == end());
    }
    auto insert(Item&& p)
    {
        auto it = find(p.first);
        return std::pair(it == end() ? info_.insert(it, std::move(p)) : it, it == end());
    }
    auto erase(const Key& key)
    {
        auto it = find(key);
        return (it != end()) ? info_.erase(it) : end();
    }
    void clear() { info_.clear(); }

    auto find(const Key& key)
    {
        return std::find_if(begin(), end(), [&](const Item& p) { return p.first == key; });
    }
    auto find(const Key& key) const
    {
        return std::find_if(begin(), end(), [&](const Item& p) { return p.first == key; });
    }
    size_t count(const Key& key) const { return find(key) != end() ? 1 : 0; }
    bool contains(const Key& key) const { return count(key) > 0; }

public:
    auto begin() { return info_.begin(); }
    auto end() { return info_.end(); }
    auto begin() const { return info_.begin(); }
    auto end() const { return info_.end(); }

private:
    std::deque<Item> info_;
};

} // namespace minizero::utils
