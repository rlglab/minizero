#pragma once

#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace minizero::config {

// parameter setter
template <class T>
bool setParameter(T& ref, const std::string& value)
{
    std::istringstream iss(value);
    iss >> ref;
    return (iss && iss.rdbuf()->in_avail() == 0);
}
template <>
bool setParameter<bool>(bool& ref, const std::string& value);
template <>
bool setParameter<std::string>(std::string& ref, const std::string& value);

// parameter getter
template <class T>
std::string getParameter(T& ref)
{
    std::ostringstream oss;
    oss << ref;
    return oss.str();
}
template <>
std::string getParameter<bool>(bool& ref);

// parameter container
class BaseParameter {
public:
    virtual bool operator()(const std::string& value) = 0;
    virtual std::string toString() const = 0;
    virtual std::string getKey() const = 0;
    virtual ~BaseParameter() {}
};

template <class T, class Setter, class Getter>
class Parameter : public BaseParameter {
public:
    Parameter(const std::string key, T& ref, const std::string description, Setter setter, Getter getter)
        : key_(key), description_(description), ref_(ref), setter_(setter), getter_(getter)
    {
    }

    std::string toString() const override
    {
        std::ostringstream oss;
        if (description_.size() > 150) { oss << "# " << description_ << std::endl; }
        oss << key_ << "=" << getter_(ref_);
        if (!description_.empty() && description_.size() <= 150) { oss << " # " << description_; }
        oss << std::endl;
        return oss.str();
    }

    inline bool operator()(const std::string& value) override { return setter_(ref_, value); }
    inline std::string getKey() const override { return key_; }
    inline std::string getDescription() const { return description_; }

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

    virtual ~ConfigureLoader()
    {
        for (auto& group_name : group_name_order_) {
            for (auto parameter : parameter_groups_[group_name]) { delete parameter; }
            parameter_groups_[group_name].clear();
        }
    }

    template <class T, class Setter, class Getter>
    inline void addParameter(const std::string& key, T& value, const std::string description, const std::string& group_name, Setter setter, Getter getter)
    {
        if (parameter_groups_.count(group_name) == 0) { group_name_order_.push_back(group_name); }
        parameters_[key] = new Parameter<T, Setter, Getter>(key, value, description, setter, getter);
        parameter_groups_[group_name].push_back(parameters_[key]);
    }

    template <class T>
    inline void addParameter(const std::string& key, T& value, const std::string description, const std::string& group_name)
    {
        addParameter(key, value, description, group_name, setParameter<T>, getParameter<T>);
    }

    bool loadFromFile(std::string conf_file);
    bool loadFromString(std::string conf_string);
    std::string toString() const;
    std::string getConfig(std::string key) const;

private:
    void trim(std::string& s);
    bool setValue(std::string sLine);

    std::vector<std::string> group_name_order_;
    std::map<std::string, BaseParameter*> parameters_;
    std::map<std::string, std::vector<BaseParameter*>> parameter_groups_;
};

} // namespace minizero::config
