#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace minizero::utils {

class SGFLoader {
public:
    bool LoadFromFile(const std::string& file_name);
    bool LoadFromString(const std::string& sgf_content);

    inline const std::string& GetFileName() const { return file_name_; }
    inline const std::string& GetSGFContent() const { return sgf_content_; }
    inline const std::unordered_map<std::string, std::string>& GetTags() const { return tags_; }
    inline const std::vector<std::pair<std::vector<std::string>, std::string>>& GetActions() const { return actions_; }

    static int BoardCoordinateStringToActionID(const std::string& board_coordinate_string, int board_size);
    static std::string ActionIDToBoardCoordinateString(int action_id, int board_size);
    static int SGFStringToActionID(const std::string& sgf_string, int board_size);
    static std::string ActionIDToSGFString(int action_id, int board_size);

private:
    void Reset();
    std::string TrimSpace(const std::string& s) const;
    size_t CalculateKeyValuePair(const std::string& s, size_t start_pos, std::pair<std::string, std::string>& key_value);

    std::string file_name_;
    std::string sgf_content_;
    std::unordered_map<std::string, std::string> tags_;
    std::vector<std::pair<std::vector<std::string>, std::string>> actions_;
};

} // namespace minizero::utils