#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace minizero::utils {

class SGFLoader {
public:
    bool loadFromFile(const std::string& file_name);
    bool loadFromString(const std::string& sgf_content);

    inline const std::string& getFileName() const { return file_name_; }
    inline const std::string& getSGFContent() const { return sgf_content_; }
    inline const std::unordered_map<std::string, std::string>& getTags() const { return tags_; }
    inline const std::vector<std::pair<std::vector<std::string>, std::string>>& getActions() const { return actions_; }

    static int boardCoordinateStringToActionID(const std::string& board_coordinate_string, int board_size);
    static std::string actionIDToBoardCoordinateString(int action_id, int board_size);
    static int sgfStringToActionID(const std::string& sgf_string, int board_size);
    static std::string actionIDToSGFString(int action_id, int board_size);

private:
    void reset();
    std::string trimSpace(const std::string& s) const;
    size_t calculateKeyValuePair(const std::string& s, size_t start_pos, std::pair<std::string, std::string>& key_value);

    std::string file_name_;
    std::string sgf_content_;
    std::unordered_map<std::string, std::string> tags_;
    std::vector<std::pair<std::vector<std::string>, std::string>> actions_;
};

} // namespace minizero::utils