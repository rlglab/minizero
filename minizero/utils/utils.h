#pragma once

#include <sstream>
#include <string>
#include <vector>

namespace minizero::utils {

std::vector<std::string> stringToVector(const std::string& s)
{
    std::string tmp;
    std::stringstream parser(s);
    std::vector<std::string> args;
    while (parser >> tmp) { args.push_back(tmp); }
    return args;
}

} // namespace minizero::utils