// gaug 76 supplies the picture, active text anchor, and complete pickup lifetime.
#include "kh2_obtained.h"
#include "kh2_hp_config.h"
#include "kh2_information.h"
#include "../gfx/kh2_sequence.h"
#include "../host/render/kh2_sequence_asset.h"
extern "C" {
#include "parts.h"
#include "situation.h"
#include "../runtime/menus/camp.h"
#include "../runtime/core/rom.h"
#include "../runtime/core/session_log.h"
#include "../runtime/mods/textures.h"
#include "../runtime/scripts/game_link.h"
#include "../host/screens/ui_text.h"
int khdays_settings_game_language(void);
}
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <optional>
#include <stdexcept>

namespace {
namespace seq = khdays::assets::kh2;
namespace render = khdays::render::kh2;
namespace ui = khdays::ui::kh2;
ui::ObtainedConfig config;
std::optional<render::SequenceAsset> asset;
std::vector<unsigned> atlas_pixels;
bool tried = false, allowed = false, pending = false, active = false, started = false, native = false;
unsigned previous = 0, revision = 0;
unsigned context_epoch = 0;
float age = 0, left = 0, top = 0;
float ink_left = 0, ink_right = 0;
int atlas = -1, image = -1, text = -1, width = 0, height = 0, image_scale = 0, text_w = 0, text_h = 0;
int frames = 0;
KhdaysUiElement anchor{};
KhdaysSequenceImage program{};
std::filesystem::path path(const std::string &s)
{
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t *>(s.data()), s.size()));
}
void disable(const char *why)
{
    asset.reset();
    pending = active = started = native = false;
    program = {};
    khdays_ui_changed();
    khdays_log("kh2 obtained: %s; keeping the Days notification", why);
}
void load()
{
    if (tried)
        return;
    tried = true;
    try {
        char relative[128], found[2048];
        const char *language = khdays_language_code(khdays_settings_game_language());
        std::snprintf(relative, sizeof(relative), "layouts/gauge/%s/gauge.json",
                      language && *language ? language : "en");
        if (!khdays_mods_find(relative, found, sizeof(found)) &&
            !khdays_mods_find("layouts/gauge/en/gauge.json", found, sizeof(found)))
            return;
        config = ui::load_obtained_config(path(found));
        if (!config.enabled)
            return;
        const auto folder = path(found).parent_path();
        asset = render::load_sequence_asset(folder / path(config.archive), config.entry, folder / path(config.texture));
        frames = seq::sequence_frame_count(asset->sequence, std::size_t(config.animation));
        if (frames <= 0 || frames > 4096)
            throw std::runtime_error("animation must contain 1 to 4096 frames");
        float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        bool has_anchor = false;
        for (int t = 0; t < frames; ++t) {
            const auto f = seq::evaluate_sequence(asset->sequence, std::size_t(config.animation), float(t));
            has_anchor |= f.active.has_value();
            if (f.quads.size() > KSI_MAX_QUADS)
                throw std::runtime_error("animation exceeds GPU quad capacity");
            for (const auto &q : f.quads)
                for (const auto &v : q.vertices) {
                    x0 = std::min(x0, (v.x - config.source_origin_x) * config.scale);
                    x1 = std::max(x1, (v.x - config.source_origin_x) * config.scale);
                    y0 = std::min(y0, (v.y - config.source_origin_y) * config.scale);
                    y1 = std::max(y1, (v.y - config.source_origin_y) * config.scale);
                }
        }
        if (!has_anchor)
            throw std::runtime_error("animation needs an active text anchor");
        left = std::floor(x0) - 2;
        top = std::floor(y0) - 2;
        const float w = std::ceil(x1) - left + 2, h = std::ceil(y1) - top + 2;
        if (!std::isfinite(w) || !std::isfinite(h) || w <= 0 || h <= 0 || w > 2048 || h > 512)
            throw std::runtime_error("animation exceeds supported canvas dimensions");
        width = int(w);
        height = int(h);
        atlas_pixels.resize(asset->texture.rgba.size() / 4);
        std::memcpy(atlas_pixels.data(), asset->texture.rgba.data(), asset->texture.rgba.size());
        atlas = khdays_textures_add_info("kh2:obtained:atlas", asset->texture.width, asset->texture.height,
                                         atlas_pixels.data(), KHDAYS_REPLACEMENT_NO_MIPS);
        if (atlas < 0)
            throw std::runtime_error("could not register obtained atlas");
        khdays_log("kh2 obtained: %s, animation %d", found, config.animation);
    } catch (const std::exception &e) {
        disable(e.what());
    }
}
} // namespace
extern "C" int khdays_ui_obtained_enabled(void)
{
    load();
    return asset && khdays_textures_hd() && khdays_ui_mode(KHDAYS_UI_TEXT) == KHDAYS_UI_ON;
}
extern "C" void khdays_ui_obtained_request(int announcement)
{
    if (!khdays_ui_obtained_enabled())
        return;
    context_epoch = khdays_ui_information_epoch();
    native = announcement != 0;
    if (announcement)
        pending = true;
    else
        pending = false;
    khdays_ui_changed();
}
extern "C" void khdays_ui_obtained_frame(unsigned frame)
{
    KhdaysSituationInputs context{};
    khdays_situation_take(&context, frame);
    const auto overlay = [&](int id) { return (context.overlays & (1ull << id)) != 0; };
    const bool event = context.event_status >= 0 && !(context.event_status & 8) &&
                       (khdays_situation_ran(&context, KHDAYS_UPD_Ov023_SceneRun) || context.pause_mode);
    allowed = context.scene == 2 && overlay(2) && !context.movie && !context.tutorial && !event &&
              context.camp_screen == KHDAYS_CAMP_NONE && !overlay(8) && !overlay(25) && !overlay(26) && !overlay(27);
    // The existing native-panel reset hook advances this epoch across map/HUD recreation.
    const unsigned epoch = khdays_ui_information_epoch();
    if (!allowed || !khdays_ui_obtained_enabled() || frame < previous || context_epoch != epoch) {
        if (active || pending || started)
            khdays_ui_changed();
        pending = active = started = native = false;
        program = {};
    }
    context_epoch = epoch;
    if (active && !khdays_link_paused() && frame >= previous)
        age += float(frame - previous) * config.frames_per_tick;
    previous = frame;
    if (active || pending)
        khdays_ui_changed();
}
extern "C" void khdays_ui_obtained_begin(void)
{
    program = {};
}
extern "C" int khdays_ui_obtained_text(const KhdaysUiElement *box, const unsigned *rgba, int w, int h, unsigned version)
{
    if (!allowed || !khdays_ui_obtained_enabled() || !box || !rgba || w <= 0 || h <= 0 || w > 4096 || h > 512)
        return 0;
    try {
        const int scale = khdays_ui_scale();
        if (text < 0 || w != text_w || h != text_h) {
            khdays_ui_image_put(text);
            text = khdays_ui_image_get(w, h);
            text_w = w;
            text_h = h;
            revision = version - 1;
        }
        if (image < 0 || scale != image_scale) {
            khdays_ui_image_put(image);
            image = khdays_ui_image_get(width * scale, height * scale);
            image_scale = scale;
        }
        if (text < 0 || image < 0)
            return 0;
        if (revision != version) {
            auto *out = khdays_ui_image_begin(text);
            if (!out)
                return 0;
            std::copy_n(rgba, std::size_t(w) * h, out);
            khdays_ui_image_end(text);
            revision = version;
            ink_left = float(w) / scale;
            ink_right = 0;
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                    if (rgba[std::size_t(y) * w + x] >> 24) {
                        ink_left = std::min(ink_left, float(x) / scale);
                        ink_right = std::max(ink_right, float(x + 1) / scale);
                    }
            if (ink_right == 0)
                ink_left = 0;
        }
        anchor = *box;
        if (pending) {
            age = 0;
            pending = false;
            active = started = true;
        }
        return khdays_ui_obtained_owns();
    } catch (const std::exception &e) {
        disable(e.what());
        return 0;
    }
}
extern "C" int khdays_ui_obtained_owns(void)
{
    return allowed && started && native && khdays_ui_obtained_enabled();
}
extern "C" void khdays_ui_obtained_end(void)
{
    if (!allowed || !active || !khdays_ui_obtained_enabled() || image < 0 || text < 0)
        return;
    try {
        if (age >= frames) {
            active = false;
            return;
        }
        const auto f = seq::evaluate_sequence(asset->sequence, std::size_t(config.animation), age);
        auto *w = program.words;
        const auto number = [w](unsigned i, float v) { std::memcpy(w + i, &v, 4); };
        w[KSI_OUTPUT] = unsigned(khdays_ui_image_replacement(image));
        w[KSI_ATLAS] = unsigned(atlas);
        w[KSI_LOGICAL_W] = unsigned(asset->logical_size[0]);
        w[KSI_LOGICAL_H] = unsigned(asset->logical_size[1]);
        w[KSI_WIDTH] = unsigned(width * image_scale);
        w[KSI_HEIGHT] = unsigned(height * image_scale);
        const float vx = (-left - config.source_origin_x * config.scale) * image_scale;
        const float vy = (-top - config.source_origin_y * config.scale) * image_scale;
        number(KSI_VIEW_X, vx);
        number(KSI_VIEW_Y, vy);
        number(KSI_SCALE, config.scale * image_scale);
        w[KSI_CLEAR] = w[KSI_ANCHORED] = 1;
        number(KSI_ANCHOR_X, anchor.x);
        number(KSI_ANCHOR_Y, anchor.y);
        render::encode_sequence_quads(program, f.quads);
        const auto &params = asset->sequence.clips[std::size_t(config.animation)].parameters;
        const auto a = f.active.value_or(seq::SequenceActive{});
        const float text_scale = config.text_scale * config.scale / 0.65f;
        // TREASURE_INFO::update selects ALIGN_CENTER when the clip's Ex parameter is one.
        const float align_x = params[4] == 1 ? (ink_left + ink_right) * .5f * text_scale * image_scale : 0;
        w[KSI_TEXT_INSERT] = unsigned(f.quads.size()) + 1;
        w[KSI_FACE] = f.active ? unsigned(khdays_ui_image_replacement(text)) : ~0u;
        w[KSI_HD_FACE] = ~0u;
        number(KSI_TEXT_SCALE, text_scale);
        number(KSI_FACE_X,
               vx + ((float(a.x) + float(params[0])) * config.scale + config.text_offset_x) * image_scale - align_x);
        number(KSI_FACE_Y, vy + ((float(a.y) + float(params[1])) * config.scale + config.text_offset_y) * image_scale -
                               3 * text_scale * image_scale);
        number(KSI_FACE_W, text_w * text_scale);
        number(KSI_FACE_H, text_h * text_scale);
        number(KSI_TEXT_CLIP_X, 0);
        number(KSI_TEXT_CLIP_Y, 0);
        number(KSI_TEXT_CLIP_W, float(width * image_scale));
        number(KSI_TEXT_CLIP_H, float(height * image_scale));
        number(KSI_TEXT_ALPHA, f.active ? std::clamp(float(a.color >> 24) / 128, 0.0f, 1.0f) : 0);
        auto e = anchor;
        e.id = "hud.obtained";
        e.image = image;
        e.slot = KES_DIRECT | KES_FINE | KES_PICTURE | KES_HUD | KES_NATIVE_FADE | 1;
        e.x += config.offset_x - 2;
        e.y += config.offset_y - 2;
        e.w = float(width);
        e.h = float(height);
        khdays_ui_emit_extended(KHDAYS_UI_TEXT, &e, &anchor);
    } catch (const std::exception &e) {
        disable(e.what());
    }
}
extern "C" void khdays_ui_obtained_commands(KhdaysSequenceImage *out)
{
    *out = program;
}
