#pragma once

#include <algorithm>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace minizero::utils {

inline std::vector<std::string> stringToVector(const std::string& s, const std::string& delim = " ", bool compress = true)
{
    std::vector<std::string> args;
    if (delim.size()) {
        args.reserve(std::count(s.begin(), s.end(), delim.front()));
        std::string::size_type pos = 0, end;
        while ((end = s.find(delim, pos)) != std::string::npos) {
            if (end > pos || !compress) { args.emplace_back(s.substr(pos, end - pos)); }
            pos = end + delim.size();
        }
        if (s.size() > pos || !compress) { args.emplace_back(s.substr(pos)); }
    } else {
        if (!compress) { args.emplace_back(); }
        for (char ch : s) { args.emplace_back(1, ch); }
        if (!compress) { args.emplace_back(); }
    }
    return args;
}

inline std::string compressToBinaryString(const std::string& s)
{
    if (s.empty()) { return s; }

    // use gzip to compress string
    std::stringstream compressed;
    boost::iostreams::filtering_streambuf<boost::iostreams::output> out;
    out.push(boost::iostreams::gzip_compressor());
    out.push(compressed);
    boost::iostreams::copy(boost::iostreams::basic_array_source<char>{s.data(), s.size()}, out);
    boost::iostreams::close(out);
    return compressed.str();
}

inline std::string binaryToHexString(const std::string& s)
{
    // encode binary string to hex string
    std::ostringstream oss;
    for (size_t i = 0; i < s.size(); ++i) {
        oss << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(static_cast<unsigned char>(s[i]));
    }
    return oss.str();
}

inline std::string hexToBinaryString(const std::string& s)
{
    assert(s.size() % 2 == 0);

    // decode hex string to binary string
    std::string decompressed_string;
    for (size_t i = 0; i < s.size(); i += 2) { decompressed_string += static_cast<unsigned char>(std::stoi(s.substr(i, 2), 0, 16)); }
    return decompressed_string;
}

inline std::string decompressBinaryString(const std::string& s)
{
    if (s.empty()) { return s; }

    // use gzip to decompress binary string
    boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
    in.push(boost::iostreams::gzip_decompressor());
    in.push(boost::iostreams::basic_array_source<char>(s.data(), s.size()));
    std::stringstream decompressed;
    boost::iostreams::copy(in, decompressed);
    boost::iostreams::close(in);
    return decompressed.str();
}

inline std::string compressString(const std::string& s)
{
    return binaryToHexString(compressToBinaryString(s));
}

inline std::string decompressString(const std::string& s)
{
    return decompressBinaryString(hexToBinaryString(s));
}

inline float transformValue(float value)
{
    // reference: Observe and Look Further: Achieving Consistent Performance on Atari, page 11
    const float epsilon = 0.001;
    const float sign_value = (value > 0.0f ? 1.0f : (value == 0.0f ? 0.0f : -1.0f));
    value = sign_value * (sqrt(fabs(value) + 1) - 1) + epsilon * value;
    return value;
}

inline float invertValue(float value)
{
    // reference: Observe and Look Further: Achieving Consistent Performance on Atari, page 11
    const float epsilon = 0.001;
    const float sign_value = (value > 0.0f ? 1.0f : (value == 0.0f ? 0.0f : -1.0f));
    return sign_value * (powf((sqrt(1 + 4 * epsilon * (fabs(value) + 1 + epsilon)) - 1) / (2 * epsilon), 2.0f) - 1);
}

template <typename T>
float stddev(const std::vector<T>& input)
{
    if (input.size() <= 1) { return 0.0f; }

    double mean = std::accumulate(input.begin(), input.end(), 0.0) / input.size();
    double variance = std::accumulate(input.begin(), input.end(), 0.0, [&](double sum, const T& value) { return sum + std::pow(static_cast<double>(value) - mean, 2); });
    return std::sqrt(variance / (input.size() - 1));
}

} // namespace minizero::utils
