#pragma once

#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace minizero::config {

// parameter setter
template <class T>
bool SetParameter(T& ref, const std::string& value)
{
    std::istringstream iss(value);
    iss >> ref;
    return (iss && iss.rdbuf()->in_avail() == 0);
}
template <>
bool SetParameter<bool>(bool& ref, const std::string& value);
template <>
bool SetParameter<std::string>(std::string& ref, const std::string& value);

// parameter getter
template <class T>
std::string GetParameter(T& ref)
{
    std::ostringstream oss;
    oss << ref;
    return oss.str();
}
template <>
std::string GetParameter<bool>(bool& ref);

// parameter container
class BaseParameter {
public:
    virtual bool operator()(const std::string& value) = 0;
    virtual std::string ToString() const = 0;
    virtual ~BaseParameter() {}
};

template <class T, class Setter, class Getter>
class Parameter : public BaseParameter {
public:
    Parameter(const std::string key, T& ref, const std::string description, Setter setter, Getter getter)
        : key_(key), description_(description), ref_(ref), setter_(setter), getter_(getter)
    {
    }

    std::string ToString() const override
    {
        std::ostringstream oss;
        oss << key_ << "=" << getter_(ref_);
        if (!description_.empty()) { oss << " # " << description_; }
        oss << std::endl;
        return oss.str();
    }

    inline bool operator()(const std::string& value) override { return setter_(ref_, value); }
    inline std::string GetKey() const { return key_; }
    inline std::string GetDescription() const { return description_; }

private:
    std::string key_;
    std::string description_;

    T& ref_;
    Setter setter_;
    Getter getter_;
};

class ConfigureLoader {
public:
    ConfigureLoader() {}

    ~ConfigureLoader()
    {
        for (auto& group_name : group_name_order_) {
            for (auto parameter : parameter_groups_[group_name]) { delete parameter; }
            parameter_groups_[group_name].clear();
        }
        // for (size_t i = 0; i < parameters_.size(); ++i) { delete parameters_[i]; }
    }

    template <class T, class Setter, class Getter>
    inline void AddParameter(const std::string& key, T& value, const std::string description, const std::string& group_name, Setter setter, Getter getter)
    {
        if (parameter_groups_.count(group_name) == 0) { group_name_order_.push_back(group_name); }
        parameters_[key] = new Parameter<T, Setter, Getter>(key, value, description, setter, getter);
        parameter_groups_[group_name].push_back(parameters_[key]);
    }

    template <class T>
    inline void AddParameter(const std::string& key, T& value, const std::string description, const std::string& group_name)
    {
        AddParameter(key, value, description, group_name, SetParameter<T>, GetParameter<T>);
    }

    bool LoadFromFile(std::string conf_file);
    bool LoadFromString(std::string conf_string);
    std::string ToString() const;

private:
    void Trim(std::string& s);
    bool SetValue(std::string sLine);

    std::vector<std::string> group_name_order_;
    std::map<std::string, BaseParameter*> parameters_;
    std::map<std::string, std::vector<BaseParameter*>> parameter_groups_;
};

} // namespace minizero::config