#pragma once
#include <string>

namespace minizero::utils {

enum class TextType {
    kNormal,
    kBold,
    kUnderLine,
    kSize
};

enum class TextColor {
    kBlack,
    kRed,
    kGreen,
    kYellow,
    kBlue,
    kPurple,
    kCyan,
    kWhite,
    kSize
};

inline bool& isColorOutputEnabled()
{
    static bool enabled = true;
    return enabled;
}

inline void setColorOutputEnabled(bool enabled)
{
    isColorOutputEnabled() = enabled;
}

inline std::string getColorText(std::string text, TextType text_type, TextColor text_color, TextColor text_background)
{
    if (!isColorOutputEnabled()) { return text; }

    const int text_type_number[static_cast<int>(TextType::kSize)] = {0, 1, 4};
    return "\33[" + std::to_string(text_type_number[static_cast<int>(text_type)]) +
           ";3" + std::to_string(static_cast<int>(text_color)) +
           ";4" + std::to_string(static_cast<int>(text_background)) +
           "m" + text + "\33[0m";
}

} // namespace minizero::utils
