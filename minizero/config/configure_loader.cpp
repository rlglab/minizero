#include "configure_loader.h"
#include <algorithm>
#include <fstream>
#include <iostream>

namespace minizero::config {

template <>
bool SetParameter<bool>(bool& ref, const std::string& value)
{
    std::string tmp = value;
    transform(tmp.begin(), tmp.end(), tmp.begin(), ::toupper);
    if (tmp != "TRUE" && tmp != "1" && tmp != "FALSE" && tmp != "0") { return false; }

    ref = (tmp == "TRUE" || tmp == "1");
    return true;
}

template <>
bool SetParameter<std::string>(std::string& ref, const std::string& value)
{
    ref = value;
    return true;
}

template <>
std::string GetParameter<bool>(bool& ref)
{
    std::ostringstream oss;
    oss << (ref == true ? "true" : "false");
    return oss.str();
}

bool ConfigureLoader::LoadFromFile(std::string conf_file)
{
    if (conf_file.empty()) { return false; }

    std::string line;
    std::ifstream file(conf_file);
    if (file.fail()) {
        file.close();
        return false;
    }
    while (std::getline(file, line)) {
        if (!SetValue(line)) { return false; }
    }

    return true;
}

bool ConfigureLoader::LoadFromString(std::string conf_string)
{
    if (conf_string.empty()) { return false; }

    std::string line;
    std::istringstream iss(conf_string);
    while (std::getline(iss, line, ':')) {
        if (!SetValue(line)) { return false; }
    }

    return true;
}

std::string ConfigureLoader::ToString() const
{
    std::ostringstream oss;
    for (const auto& group_name : group_name_order_) {
        oss << "# " << group_name << std::endl;
        for (auto parameter : parameter_groups_.at(group_name)) { oss << parameter->ToString(); }
        oss << std::endl;
    }
    return oss.str();
}

void ConfigureLoader::Trim(std::string& s)
{
    if (s.empty()) { return; }
    s.erase(0, s.find_first_not_of(" "));
    s.erase(s.find_last_not_of(" ") + 1);
}

bool ConfigureLoader::SetValue(std::string line)
{
    if (line.empty() || line[0] == '#') { return true; }

    std::string key = line.substr(0, line.find("="));
    std::string value = line.substr(line.find("=") + 1);
    if (value.find("#") != std::string::npos) { value = value.substr(0, value.find("#")); }
    std::string group_name = line.substr(line.find("#") + 1);

    Trim(key);
    Trim(value);
    Trim(group_name);

    if (parameters_.count(key) == 0) {
        std::cerr << "Invalid key \"" + key + "\" and value \"" << value << "\"" << std::endl;
        return false;
    }

    if (!(*parameters_[key])(value)) {
        std::cerr << "Unsatisfiable value \"" + value + "\" for option \"" + key + "\"" << std::endl;
        return false;
    }

    return true;
}

} // namespace minizero::config