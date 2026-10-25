// Synthetic pickup playback: text follows the active track, including its fade and one-shot lifetime.
#include "../ui/kh2_obtained.h"
#include "../ui/kh2_hp_config.h"
#include "../host/render/kh2_sequence_asset.h"
#include "khdays/assets/kh2_bar.h"
extern "C" {
#include "../ui/situation.h"
#include "../ui/parts.h"
#include "../runtime/menus/camp.h"
}
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {
std::filesystem::path settings;
bool missing = false, paused = false, hd = true;
int uploads = 0, scale = 2;
unsigned frame = 10;
unsigned caption_epoch = 1;
KhdaysSituationInputs context = [] {
    KhdaysSituationInputs c{};
    c.scene = 2;
    c.overlays = 1ull << 2;
    c.camp_screen = KHDAYS_CAMP_NONE;
    c.event_status = -1;
    return c;
}();
std::vector<std::vector<unsigned>> images;
KhdaysSequenceImage program{};
KhdaysUiElement submitted{}, anchor{};
std::optional<khdays::assets::kh2::SequenceData> reference_data;
void expect(bool ok, const char *why)
{
    if (!ok)
        throw std::runtime_error(why);
}
float number(unsigned i)
{
    float v;
    std::memcpy(&v, program.words + i, 4);
    return v;
}
void step(bool text = true, unsigned revision = 1)
{
    khdays_ui_obtained_frame(frame++);
    khdays_ui_obtained_begin();
    if (text) {
        const KhdaysUiElement e{"hud.obtained", 0, 1, 0, 56, 80, 24, -1};
        std::vector<unsigned> rgba(64 * scale * 16 * scale, 0xffffffffu);
        khdays_ui_obtained_text(&e, rgba.data(), 64 * scale, 16 * scale, revision);
    }
    khdays_ui_obtained_end();
    khdays_ui_obtained_commands(&program);
}
void test()
{
    khdays_ui_obtained_request(1);
    step();
    expect(khdays_ui_obtained_owns() && program.words[KSI_COUNT] == 1, "pickup did not start");
    expect(number(KSI_TEXT_ALPHA) == 0 && program.words[KSI_TEXT_INSERT] == 2, "text did not follow intro alpha/order");
    expect(submitted.y == 54 && anchor.y == 56 && (submitted.slot & KES_PICTURE), "native pickup anchor lost");
    const float initial_y = number(KSI_FACE_Y);
    const int allocated = int(images.size()), uploaded = uploads;
    step();
    expect(number(KSI_FACE_Y) < initial_y && number(KSI_TEXT_ALPHA) > 0, "text did not move/fade with the sequence");
    paused = true;
    step();
    const auto frozen = program;
    step();
    expect(!std::memcmp(&frozen, &program, sizeof(program)), "paused pickup animation advanced");
    paused = false;
    for (int i = 0; i < 14; ++i)
        step(false);
    expect(!program.words[KSI_CLEAR] && khdays_ui_obtained_owns(), "one-shot looped or native art reappeared");
    expect(int(images.size()) == allocated && uploads == uploaded, "warm playback allocated or uploaded text");
    khdays_ui_obtained_request(1);
    step();
    expect(program.words[KSI_COUNT] && number(KSI_TEXT_ALPHA) == 0, "repeated item did not restart");
    expect(uploads == uploaded, "identical item needlessly uploaded text again");
    step(true, 2);
    expect(uploads == uploaded + 1, "changed item text was not refreshed");
    khdays_ui_obtained_request(0);
    expect(!khdays_ui_obtained_owns(), "normal selection remained hidden");
    step(false);
    expect(program.words[KSI_CLEAR], "native disappearance cut off the sequence's own exit");
    context.camp_screen = KHDAYS_CAMP_IN_MENU;
    step(false);
    expect(!program.words[KSI_CLEAR] && !khdays_ui_obtained_owns(), "pickup leaked into camp");
    context.camp_screen = KHDAYS_CAMP_NONE;
    step(false);
    expect(!program.words[KSI_CLEAR], "old pickup resurrected after leaving a menu");
    khdays_ui_obtained_request(1);
    step();
    ++caption_epoch;
    step(false);
    expect(!program.words[KSI_CLEAR] && !khdays_ui_obtained_owns(), "pickup survived native panel recreation");
    khdays_ui_obtained_request(1);
    step();
    hd = false;
    expect(!khdays_ui_obtained_enabled() && !khdays_ui_obtained_owns(), "HD-disabled fallback was suppressed");
}
void reference(const char *filename)
{
    namespace seq = khdays::assets::kh2;
    std::ifstream file(filename, std::ios::binary);
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)), {});
    for (const auto &e : seq::bar_entries(bytes.data(), bytes.size()))
        if (e.type == 25 && e.name == "gaug" && e.size)
            reference_data = seq::read_sequence(bytes.data() + e.offset, e.size);
    expect(reference_data.has_value(), "missing reference gaug");
    const auto &d = *reference_data;
    expect(seq::evaluate_sequence(d, 76, 27).active->x == 0 && seq::evaluate_sequence(d, 76, 27).active->y == 0,
           "obtained active origin was not centered");
    expect(seq::evaluate_sequence(d, 76, 8).active->y == 0 && seq::evaluate_sequence(d, 76, 12).active->y == -4,
           "original pickup entrance used a full-turn sway instead of a half-turn");
    khdays_ui_obtained_request(1);
    const int count = seq::sequence_frame_count(d, 76);
    expect(count == 107, "unexpected original obtained lifetime");
    for (int t = 0; t < count; ++t) {
        step(t == 0);
        const auto f = seq::evaluate_sequence(d, 76, float(t));
        expect(program.words[KSI_COUNT] == f.quads.size(), "runtime lost obtained art");
        expect(f.active.has_value(), "reference frame lost its text parent");
        const float x = number(KSI_FACE_X) - number(KSI_VIEW_X) + (d.clips[76].parameters[4] == 1 ? 32 * scale : 0);
        const float y = number(KSI_FACE_Y) - number(KSI_VIEW_Y) + 3 * scale;
        expect(std::abs(x - (f.active->x + d.clips[76].parameters[0]) * .65f * scale) < .001f &&
                   std::abs(y - (f.active->y + d.clips[76].parameters[1]) * .65f * scale) < .001f,
               "text did not follow original active translation");
        expect(std::abs(number(KSI_TEXT_ALPHA) - std::clamp(float(f.active->color >> 24) / 128, 0.0f, 1.0f)) < .001f,
               "text did not follow original active alpha");
    }
    step(false);
    expect(!program.words[KSI_CLEAR], "obtained text outlived animation 76");
}
} // namespace
namespace khdays::render::kh2 {
SequenceAsset load_sequence_asset(const std::filesystem::path &, const std::string &, const std::filesystem::path &)
{
    if (missing)
        throw std::runtime_error("missing fixture");
    SequenceAsset a;
    a.logical_size = {512, 256};
    a.texture.width = a.texture.height = 1;
    a.texture.rgba = {255, 255, 255, 255};
    auto &d = a.sequence;
    if (reference_data) {
        d = *reference_data;
        return a;
    }
    assets::kh2::SequencePart p;
    p.uv = {0, 0, 1, 1};
    p.colors.fill(0x80808080);
    d.parts.push_back(p);
    d.groups.push_back({{-84, -6, 182, 21}, 0});
    d.sprites.push_back({0, 1});
    assets::kh2::SequenceAnimation anim;
    anim.flags = 1 | 2 | (1u << 4) | (1u << 5) | (1u << 6);
    anim.time = {0, 9};
    anim.offset_y = {40, 0};
    anim.colors = {0x00808080, 0x80808080};
    d.animations.push_back(anim);
    d.clips.resize(77);
    d.clips[76].animations = {0, 1};
    d.clips[76].loop = 1;
    d.clips[76].restart = 150;
    return a;
}
} // namespace khdays::render::kh2
extern "C" {
unsigned khdays_ui_information_epoch(void)
{
    return caption_epoch;
}
int khdays_settings_game_language(void)
{
    return 0;
}
const char *khdays_language_code(int)
{
    return "en";
}
int khdays_mods_find(const char *, char *out, int size)
{
    std::snprintf(out, std::size_t(size), "%s", settings.string().c_str());
    return 1;
}
int khdays_textures_hd(void)
{
    return hd;
}
int khdays_ui_mode(int)
{
    return KHDAYS_UI_ON;
}
void khdays_ui_changed(void) {}
void khdays_log(const char *, ...) {}
int khdays_link_paused(void)
{
    return paused;
}
void khdays_situation_take(KhdaysSituationInputs *out, unsigned)
{
    *out = context;
}
int khdays_ui_scale(void)
{
    return scale;
}
int khdays_ui_image_get(int w, int h)
{
    images.emplace_back(std::size_t(w) * h);
    return int(images.size()) - 1;
}
void khdays_ui_image_put(int) {}
unsigned *khdays_ui_image_begin(int image)
{
    return images.at(std::size_t(image)).data();
}
void khdays_ui_image_end(int)
{
    ++uploads;
}
int khdays_ui_image_replacement(int image)
{
    return image;
}
int khdays_textures_add_info(const char *, int, int, const unsigned *, unsigned)
{
    return 99;
}
void khdays_ui_emit_extended(int, const KhdaysUiElement *e, const KhdaysUiElement *box)
{
    submitted = *e;
    anchor = *box;
}
}
int main(int argc, char **argv)
{
    try {
        const bool legacy = argc > 1 && std::string(argv[1]) == "--legacy";
        missing = argc > 1 && std::string(argv[1]) == "--missing";
        settings = std::filesystem::temp_directory_path() /
                   (std::string("khdays-obtained") + (argc > 1 ? argv[1] : "") + ".json");
        std::ofstream(settings) << (legacy ? "{}" : R"({"obtained":{"enabled":true}})");
        if (argc == 3 && std::string(argv[1]) == "--reference")
            reference(argv[2]);
        else if (missing || legacy) {
            khdays_ui_obtained_request(1);
            step();
            expect(!khdays_ui_obtained_enabled() && !program.words[KSI_CLEAR],
                   "missing/legacy replacement broke fallback");
        } else
            test();
        std::filesystem::remove(settings);
        std::cout << "Obtained animation, cached text, parent alpha and native fallback passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
