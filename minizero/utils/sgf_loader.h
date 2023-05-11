#pragma once

#include "vector_map.h"
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace minizero::utils {

class SGFLoader {
public:
    class Action : public std::vector<std::string> {
    public:
        Action(const std::string& player, const std::string& move)
        {
            push_back(player);
            push_back(move);
        }
        Action(const std::vector<std::string>& action) : std::vector<std::string>(action) {}
        Action() {}
    };

    typedef minizero::utils::VectorMap<std::string, std::string> Info;

public:
    virtual bool loadFromFile(const std::string& file_name);
    virtual bool loadFromString(const std::string& sgf_content);

    inline const std::string& getFileName() const { return file_name_; }
    inline const std::string& getSGFContent() const { return sgf_content_; }
    inline const Info& getTags() const { return tags_; }
    inline const std::vector<std::pair<Action, Info>>& getActions() const { return actions_; }

    static int boardCoordinateStringToActionID(const std::string& board_coordinate_string, int board_size);
    static std::string actionIDToBoardCoordinateString(int action_id, int board_size);
    static int sgfStringToActionID(const std::string& sgf_string, int board_size);
    static std::string actionIDToSGFString(int action_id, int board_size);

protected:
    virtual void reset();
    std::string trimSpace(const std::string& s) const;

    std::string file_name_;
    std::string sgf_content_;
    Info tags_;
    std::vector<std::pair<Action, Info>> actions_;
};

} // namespace minizero::utils
