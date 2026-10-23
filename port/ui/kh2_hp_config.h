#pragma once
#include "kh2_hp_gauge.h"
#include <filesystem>
#include <string>

namespace khdays::ui::kh2 {
struct GaugeLabelConfig final {
    bool enabled = false;
    std::string archive, texture, entry;
    int animation = -1;
    std::array<int, 4> animations{-1, -1, -1, -1};
    float offset_x = 0, offset_y = 0, scale = 0.65f;
    float source_origin_x = 0, source_origin_y = 0, frames_per_tick = 1;
};
struct HpGaugeConfig final {
    std::string archive = "zz0field.2dd", texture = "US_zz0field_2dd1.png", entry = "gaug";
    bool enabled = true;
    float offset_x = 0, offset_y = 0, scale = 0.65f, canvas_padding = 16;
    float portrait_offset_x = 0, portrait_offset_y = 0;
    HpGaugeStyle style;
    GaugeLabelConfig player_icon, limit_text;
};
// Player settings at the root; shared party settings and per-slot overrides below party.
std::array<HpGaugeConfig, 4> load_hp_gauge_configs(const std::filesystem::path &path);
HpGaugeConfig load_hp_gauge_config(const std::filesystem::path &path);
} // namespace khdays::ui::kh2
