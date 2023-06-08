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

bool SGFLoader::loadFromString(const std::string& content)
{
    reset();
    sgf_content_ = content;
    std::string key, value;
    int state = '(';
    bool accept_move = false;
    bool escape_next = false;
    int board_size = -1;
    for (char c : content) {
        switch (state) {
            case '(': // wait until record start
                if (!accept_move) {
                    accept_move = (c == '(');
                } else {
                    state = (c == ';') ? c : 'x';
                    accept_move = false;
                }
                break;
            case ';': // store key
                if (c == ';') {
                    accept_move = true;
                } else if (c == '[' || c == ')') {
                    state = c;
                } else if (std::isgraph(c)) {
                    key += c;
                }
                break;
            case '[': // store value
                if (c == '\\' && !escape_next) {
                    escape_next = true;
                } else if (c != ']' || escape_next) {
                    value += c;
                    escape_next = false;
                } else { // ready to store key-value pair
                    if (accept_move) {
                        if (board_size == -1) { return false; }
                        actions_.emplace_back().first = SGFAction(key, actionIDToBoardCoordinateString(sgfStringToActionID(value, board_size), board_size));
                        accept_move = false;
                    } else if (actions_.size()) {
                        actions_.back().second[key] = std::move(value);
                    } else {
                        if (key == "SZ") { board_size = std::stoi(value); }
                        tags_[key] = std::move(value);
                    }
                    key.clear();
                    value.clear();
                    state = ';';
                }
                break;
            case ')': // end of record, do nothing
                break;
        }
    }
    return state == ')';
}

void SGFLoader::reset()
{
    file_name_.clear();
    sgf_content_.clear();
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
    oss << static_cast<char>(x + 'A' + (x >= 8)) << y + 1;
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
    oss << static_cast<char>(x + 'a') << static_cast<char>(((board_size - 1) - y) + 'a');
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

} // namespace minizero::utils
