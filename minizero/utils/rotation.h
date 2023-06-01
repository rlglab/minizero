#pragma once

#include <cassert>
#include <string>
#include <vector>

namespace minizero::utils {

enum class Rotation {
    kRotationNone,
    kRotation90,
    kRotation180,
    kRotation270,
    kHorizontalRotation,
    kHorizontalRotation90,
    kHorizontalRotation180,
    kHorizontalRotation270,
    kRotateSize
};

const Rotation reversed_rotation[static_cast<int>(Rotation::kRotateSize)] = {
    Rotation::kRotationNone,
    Rotation::kRotation270,
    Rotation::kRotation180,
    Rotation::kRotation90,
    Rotation::kHorizontalRotation,
    Rotation::kHorizontalRotation90,
    Rotation::kHorizontalRotation180,
    Rotation::kHorizontalRotation270};

const std::string rotation_string[static_cast<int>(Rotation::kRotateSize)] = {
    "Rotation_None",
    "Rotation_90_Degree",
    "Rotation_180_Degree",
    "Rotation_270_Degree",
    "Horizontal_Rotation",
    "Horizontal_Rotation_90_Degree",
    "Horizontal_Rotation_180_Degree",
    "Horizontal_Rotation_270_Degree"};

inline std::string getRotationString(Rotation rotate) { return rotation_string[static_cast<int>(rotate)]; }

inline Rotation getRotationFromString(const std::string rotation_str)
{
    for (int i = 0; i < static_cast<int>(Rotation::kRotateSize); ++i) {
        if (rotation_str == rotation_string[i]) { return static_cast<Rotation>(i); }
    }
    return Rotation::kRotateSize;
}

inline int getPositionByRotating(Rotation rotation, int original_pos, int board_size)
{
    assert(original_pos >= 0 && original_pos <= board_size * board_size);
    if (original_pos == board_size * board_size) { return original_pos; }

    const float center = (board_size - 1) / 2.0;
    float x = original_pos % board_size - center;
    float y = original_pos / board_size - center;
    float rotation_x = x, rotation_y = y;
    switch (rotation) {
        case Rotation::kRotationNone:
            rotation_x = x, rotation_y = y;
            break;
        case Rotation::kRotation90:
            rotation_x = y, rotation_y = -x;
            break;
        case Rotation::kRotation180:
            rotation_x = -x, rotation_y = -y;
            break;
        case Rotation::kRotation270:
            rotation_x = -y, rotation_y = x;
            break;
        case Rotation::kHorizontalRotation:
            rotation_x = x, rotation_y = -y;
            break;
        case Rotation::kHorizontalRotation90:
            rotation_x = -y, rotation_y = -x;
            break;
        case Rotation::kHorizontalRotation180:
            rotation_x = -x, rotation_y = y;
            break;
        case Rotation::kHorizontalRotation270:
            rotation_x = y, rotation_y = x;
            break;
        default:
            assert(false);
            break;
    }

    int new_pos = (rotation_y + center) * board_size + (rotation_x + center);
    assert(new_pos >= 0 && new_pos < board_size * board_size);
    return new_pos;
}

} // namespace minizero::utils
