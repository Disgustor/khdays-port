// Logical sprite rectangles and optional player policies, independent of the game runtime.
#pragma once
#include <array>
#include <filesystem>
#include <map>
#include <string>
namespace khdays::ui::minimap {
struct Sprite {
    float x = 0, y = 0, width = 8, height = 8, draw_width = 8, draw_height = 8, pivot_x = 4, pivot_y = 4, scale = 1,
          offset_x = 0, offset_y = 0;
};
struct Player {
    bool enabled = false, smooth_rotation = false;
    std::string sprite;
    float rotation_offset = 0, scale = 1;
};
struct Config {
    bool enabled = false;
    std::string texture = "minimap.png";
    int width = 256, height = 128;
    std::map<std::string, Sprite> sprites;
    std::array<Player, 4> players;
    std::map<std::string, Player> characters;
};
Config load_config(const std::filesystem::path &path);
} // namespace khdays::ui::minimap
