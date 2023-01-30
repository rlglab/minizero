#include "sgf_loader.h"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <sstream>

namespace minizero::utils {

bool SGFLoader::loadFromFile(const std::string& file_name)
{
    std::ifstream fin(file_name.c_str());
    if (!fin) { return false; }

    std::string line;
    std::string sgf_content;
    while (std::getline(fin, line)) {
        if (line.back() == '\r') { line.pop_back(); }
        if (line.empty()) { continue; }
        sgf_content += line;
    }
    return loadFromString(sgf_content);
}

bool SGFLoader::loadFromString(const std::string& sgf_content)
{
    reset();
    sgf_content_ = trimSpace(sgf_content);
    for (size_t index = 0; index < sgf_content_.size() && sgf_content_[index] != ')';) {
        std::pair<std::string, std::string> key_value;
        if ((index = calculateKeyValuePair(sgf_content_, index, key_value)) == std::string::npos) { return false; }
        if (key_value.first == "B" || key_value.first == "W") {
            int board_size = tags_.count("SZ") ? std::stoi(tags_["SZ"]) : -1;
            if (board_size == -1) { return false; }
            std::vector<std::string> args{key_value.first, actionIDToBoardCoordinateString(sgfStringToActionID(key_value.second, board_size), board_size)};
            std::string comment;
            if (index < sgf_content_.size() && sgf_content_[index] == 'C') {
                if ((index = calculateKeyValuePair(sgf_content_, index, key_value)) == std::string::npos) { return false; }
                comment = key_value.second;
            }
            actions_.push_back({args, comment});
        } else {
            tags_[key_value.first] = key_value.second;
        }
    }
    return true;
}

void SGFLoader::reset()
{
    file_name_ = "";
    sgf_content_ = "";
    tags_.clear();
    actions_.clear();
}

int SGFLoader::boardCoordinateStringToActionID(const std::string& board_coordinate_string, int board_size)
{
    std::string tmp = board_coordinate_string;
    std::transform(tmp.begin(), tmp.end(), tmp.begin(), ::toupper);
    if (tmp == "PASS") { return board_size * board_size; }

    if (board_coordinate_string.size() < 2) { return -1; }
    int x = std::toupper(board_coordinate_string[0]) - 'A' + (std::toupper(board_coordinate_string[0]) > 'I' ? -1 : 0);
    int y = atoi(board_coordinate_string.substr(1).c_str()) - 1;
    return y * board_size + x;
}

std::string SGFLoader::actionIDToBoardCoordinateString(int action_id, int board_size)
{
    assert(action_id >= 0 && action_id <= board_size * board_size);

    if (action_id == board_size * board_size) { return "PASS"; }
    int x = action_id % board_size;
    int y = action_id / board_size;
    std::ostringstream oss;
    oss << (char)(x + 'A' + (x >= 8)) << y + 1;
    return oss.str();
}

int SGFLoader::sgfStringToActionID(const std::string& sgf_string, int board_size)
{
    if (sgf_string.size() != 2) { return board_size * board_size; }
    int x = std::toupper(sgf_string[0]) - 'A';
    int y = (board_size - 1) - (std::toupper(sgf_string[1]) - 'A');
    return y * board_size + x;
}

std::string SGFLoader::actionIDToSGFString(int action_id, int board_size)
{
    assert(action_id >= 0 && action_id <= board_size * board_size);

    if (action_id == board_size * board_size) { return ""; }
    int x = action_id % board_size;
    int y = action_id / board_size;
    std::ostringstream oss;
    oss << (char)(x + 'a') << (char)(((board_size - 1) - y) + 'a');
    return oss.str();
}

std::string SGFLoader::trimSpace(const std::string& s) const
{
    bool skip = false;
    std::string new_s;
    for (const auto& c : s) {
        skip = (c == '[' ? true : (c == ']' ? false : skip));
        if (skip || c != ' ') { new_s += c; }
    }
    return new_s;
}

size_t SGFLoader::calculateKeyValuePair(const std::string& s, size_t start_pos, std::pair<std::string, std::string>& key_value)
{
    key_value = {"", ""};
    bool is_key = true;
    for (; start_pos < s.size(); ++start_pos) {
        if (is_key) {
            if (isalpha(s[start_pos])) {
                key_value.first += s[start_pos];
            } else if (s[start_pos] == '[') {
                if (key_value.first == "") { return std::string::npos; }
                is_key = false;
            }
        } else {
            if (s[start_pos] == ']') {
                if (s[start_pos - 1] == '\\') {
                    key_value.second += s[start_pos];
                } else {
                    return ++start_pos;
                }
            } else if (s[start_pos] != '\\') {
                key_value.second += s[start_pos];
            }
        }
    }
    return std::string::npos;
}

} // namespace minizero::utils
