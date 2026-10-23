#pragma once
#include "kh2_hp_gauge.h"
#include <filesystem>
#include <string>

namespace khdays::ui::kh2 {
struct HpGaugeConfig final {
    std::string archive = "zz0field.2dd", texture = "US_zz0field_2dd1.png", entry = "gaug";
    bool enabled = true;
    float offset_x = 0, offset_y = 0, scale = 0.65f, canvas_padding = 16;
    float portrait_offset_x = 0, portrait_offset_y = 0;
    HpGaugeStyle style;
};
// A deliberately flat JSON object; unknown or mistyped settings are errors.
HpGaugeConfig load_hp_gauge_config(const std::filesystem::path &path);
} // namespace khdays::ui::kh2
