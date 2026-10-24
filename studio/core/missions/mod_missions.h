// A mod's new missions -- missions/missions.txt and each missions/<key>/ -- read by the port's own reader
// (port/runtime/missions/registry.c): made from a game mission, their settings changed line by line, checked.
// docs/STUDIO.md, "New missions".
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace studio {

// A setting of a mission's block: its name and whether it takes a language (name, objective, description).
struct MissionSetting {
    const char* name;
    bool language;
    const char* help;
};
const std::vector<MissionSetting>& mission_settings();

// The game's five languages as the files name them: en fr de it es.
const std::vector<std::string>& mission_languages();

// What a mission's block says, as the port reads it: its texts by language (empty: the English one) and the
// rest as written; empty when the mod has no such mission (or it does not read: *why).
struct ModMission {
    std::string key;
    std::vector<std::string> name, objective, description;  // by mission_languages()
    int category = -1, room = 0, level = 0, time_ms = 0, template_file = 0;
    std::string world, unlock;
    bool own_file = false;  // missions/<key>/mission.p2, else its template's file as it is
};
std::vector<ModMission> read_mod_missions(const std::filesystem::path& mod_dir, std::vector<std::string>* problems);

// A new mission `key` in the mod, made from game mission `template_file` (its file copied as
// missions/<key>/mission.p2), played in `world`'s room `room`: its block written into missions.txt with the
// key as its name and objective, both read back before it reports. False with the reason.
bool new_mod_mission(const std::filesystem::path& nitrofs, const std::filesystem::path& mod_dir, const std::string& key,
                     int template_file, const std::string& world, int room, std::string* why);

// Setting `name` (`language` for a text, else empty) of mission `key` made `value` (texts: a line break as
// written); the file read back by the port's reader and left as it was when the mission would not read.
bool set_mod_mission(const std::filesystem::path& mod_dir, const std::string& key, const std::string& name,
                     const std::string& language, const std::string& value, std::string* why);

// Every mission of the mod checked as the port takes them: the descriptions, each one's file (its own or its
// template's), its world and room. One line each, problems first said "problem:". True with none.
bool check_mod_missions(const std::filesystem::path& nitrofs, const std::filesystem::path& mod_dir,
                        std::vector<std::string>* lines);

}  // namespace studio
