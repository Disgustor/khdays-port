#pragma once
#include "kh2_hp_gauge.h"
#include "kh2_enemy_gauge.h"
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
struct EnemyGaugeConfig final {
    bool enabled = false;
    std::string archive = "zz0field.2dd", texture = "US_zz0field_2dd1.png", entry = "gaug";
    float offset_x = 0, offset_y = 0, scale = 0.65f, frames_per_tick = 1;
    EnemyGaugeStyle style;
};
struct ReticleConfig final {
    bool enabled = false;
    std::string archive = "zz0field.2dd", texture = "US_zz0field_2dd0.png", entry = "fiel";
    int animation = -1, end_animation = -1;
    float offset_x = 0, offset_y = 0, scale = 0.65f;
    float source_origin_x = 0, source_origin_y = 0, frames_per_tick = 1;
};
struct InformationConfig final {
    bool enabled = false;
    std::string archive = "zz0field.2dd", texture = "US_zz0field_2dd1.png", entry = "gaug";
    int intro = 0, loop = 1, reveal = 2, outro = 3;
    float scale = 0.65f, offset_x = 0, offset_y = 0;
    float source_origin_x = 0, source_origin_y = 15, frames_per_tick = 1;
    float text_offset_x = 0, text_offset_y = 0, text_scale = 1;
    float clip_width = 0, clip_height = 64, hold_frames = 300; // 0 width: use the PC sequence's Ex parameter
    float scroll_wait_frames = 180, scroll_speed = 1;
};
InformationConfig load_information_config(const std::filesystem::path &path);
struct ObtainedConfig final {
    bool enabled = false;
    std::string archive = "zz0field.2dd", texture = "US_zz0field_2dd1.png", entry = "gaug";
    int animation = 76;
    float offset_x = 0, offset_y = 0, scale = 0.65f, frames_per_tick = 1;
    float source_origin_x = 0, source_origin_y = 0;
    float text_offset_x = 0, text_offset_y = 0, text_scale = 1;
};
ObtainedConfig load_obtained_config(const std::filesystem::path &path);
struct MissionPanelConfig final {
    bool enabled = false;
    std::string archive = "mission.2dd", texture = "mission.png", entry = "miss";
    int reference = 0, intro = 1, outro = 2, track = 3, fill = 4, ready_fill = 5, full_fill = 6;
    int heading = 7, complete_heading = 8, full_marker = 9, minimum_marker = 10;
    int bead_empty = 11, bead_full = 12, unity = 13, rank = 14, chest = 15;
    float scale = .65f, offset_x = 0, offset_y = 0, source_origin_x = 0, source_origin_y = 15;
    float frames_per_tick = 1, text_scale = 1, counter_scale = 1, counters_scale = 1;
    float description_x = 30, description_y = 27, tally_x = 246, tally_y = 45;
    float counters_right = 260, counters_y = 27, icon_gap = 4, counter_gap = 6, counter_text_offset_y = 0;
    float beads_x = 30, beads_y = 45, bead_spacing = 16;
    float bar_start_x = 23, bar_end_x = 252, bar_y = 50;
    float heading_x = 23, heading_y = 40, full_marker_x = 254, full_marker_y = 50, minimum_marker_y = 50;
};
MissionPanelConfig load_mission_panel_config(const std::filesystem::path &path);
// Player settings at the root; shared party settings and per-slot overrides below party.
std::array<HpGaugeConfig, 4> load_hp_gauge_configs(const std::filesystem::path &path);
HpGaugeConfig load_hp_gauge_config(const std::filesystem::path &path);
EnemyGaugeConfig load_enemy_gauge_config(const std::filesystem::path &path);
// Soft lock, hard lock, interact, chain, error; each can fail independently.
std::array<ReticleConfig, 5> load_reticle_configs(const std::filesystem::path &path);
} // namespace khdays::ui::kh2
