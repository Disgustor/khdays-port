// Sprite-only minimap layer, drawn by the existing GPU quad renderer at the native layer's position.
#include "minimap.h"
#include "minimap_config.h"
#include "kh2_portrait.h"
#include "../gfx/kh2_sequence.h"
#include "khdays/assets/image.h"
#include "khdays/assets/dds.h"
extern "C" {
#include "elements.h"
#include "bg.h"
#include "parts.h"
#include "situation.h"
#include "../runtime/core/window.h"
#include "../runtime/core/rom.h"
#include "../runtime/core/session_log.h"
#include "../runtime/mods/textures.h"
#include "../runtime/menus/camp.h"
}
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>
namespace {
namespace cfg = khdays::ui::minimap;
namespace seq = khdays::assets::kh2;
cfg::Config config;
std::vector<unsigned> pixels;
struct Draw {
    std::string sprite;
    float x, y, angle = 0, scale = 1;
};
std::vector<Draw> draws;
std::array<KhdaysSequenceImage, KHDAYS_MINIMAP_BATCHES> programs{};
bool tried = false, loaded = false, recording = false, complete = false, overflow = false, owns = false;
unsigned owner = 0;
int atlas = -1, output = -1, output_scale = 0, layer = -1;
KhdaysSituationInputs context{};
std::filesystem::path path(const std::string &s)
{
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t *>(s.data()), s.size()));
}
unsigned read(unsigned p)
{
    unsigned v = 0;
    if (khdays_game_ram(p) && khdays_game_ram(p + 3))
        std::memcpy(&v, KHDAYS_W(p), 4);
    return v;
}
void fail(const char *why)
{
    loaded = owns = recording = complete = false;
    programs = {};
    khdays_log("minimap: %s; retaining native icons", why);
}
khdays::assets::DecodedTexture image(const std::filesystem::path &file)
{
    auto ext = file.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return ext == ".dds" ? khdays::assets::read_dds(file) : khdays::assets::read_image(file);
}
// Pack standalone overrides once, preserving their resolution and the shared GPU batch path.
void overrides(const std::filesystem::path &folder, khdays::assets::DecodedTexture &sheet)
{
    float factor = float(sheet.width) / config.width;
    for (auto &[name, s] : config.sprites) {
        s.x *= factor;
        s.y *= factor;
        s.width *= factor;
        s.height *= factor;
    }
    config.width = sheet.width;
    config.height = sheet.height;
    std::set<std::string> seen;
    auto append = [&](const cfg::Player &policy) {
        const auto &name = policy.sprite;
        if (!policy.enabled || name.empty() || config.sprites.contains(name) || !seen.insert(name).second)
            return;
        try {
            auto ext = path(name).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            if (ext != ".png" && ext != ".dds")
                throw std::runtime_error("expected an atlas sprite name or a PNG/DDS filename");
            auto icon = image(folder / path(name));
            int w = std::max(sheet.width, icon.width + 2), h = sheet.height + icon.height + 2;
            if (w > 16384 || h > 16384 || size_t(w) * h > 64 * 1024 * 1024)
                throw std::runtime_error("combined minimap atlas is too large");
            std::vector<std::uint8_t> rgba(size_t(w) * h * 4);
            for (int y = 0; y < sheet.height; ++y)
                std::memcpy(rgba.data() + size_t(y) * w * 4, sheet.rgba.data() + size_t(y) * sheet.width * 4,
                            size_t(sheet.width) * 4);
            for (int y = 0; y < icon.height; ++y)
                std::memcpy(rgba.data() + (size_t(y + sheet.height + 1) * w + 1) * 4,
                            icon.rgba.data() + size_t(y) * icon.width * 4, size_t(icon.width) * 4);
            cfg::Sprite s;
            s.x = 1;
            s.y = float(sheet.height + 1);
            s.width = float(icon.width);
            s.height = float(icon.height);
            float ratio = 8.f / std::max(icon.width, icon.height);
            s.draw_width = icon.width * ratio;
            s.draw_height = icon.height * ratio;
            s.pivot_x = s.draw_width / 2;
            s.pivot_y = s.draw_height / 2;
            config.sprites.emplace(name, s);
            sheet.width = config.width = w;
            sheet.height = config.height = h;
            sheet.rgba = std::move(rgba);
        } catch (const std::exception &e) {
            khdays_log("minimap: cannot load optional sprite %s: %s; retaining player fallback", name.c_str(),
                       e.what());
        }
    };
    for (const auto &p : config.players)
        append(p);
    for (const auto &[name, p] : config.characters)
        append(p);
}
void load()
{
    if (tried)
        return;
    tried = true;
    char found[2048];
    if (!khdays_mods_find("layouts/minimap/minimap.json", found, sizeof(found)))
        return;
    try {
        config = cfg::load_config(path(found));
        if (!config.enabled)
            return;
        auto folder = path(found).parent_path();
        auto texture = image(folder / path(config.texture));
        if (texture.width % config.width || texture.height % config.height ||
            texture.width / config.width != texture.height / config.height)
            throw std::runtime_error("sheet must be an equal integer multiple of logical_width and logical_height");
        overrides(folder, texture);
        pixels.resize(texture.rgba.size() / 4);
        std::memcpy(pixels.data(), texture.rgba.data(), texture.rgba.size());
        atlas = khdays_textures_add_info("minimap:atlas", texture.width, texture.height, pixels.data(),
                                         KHDAYS_REPLACEMENT_NO_MIPS);
        if (atlas < 0)
            throw std::runtime_error("could not register sprite sheet");
        loaded = true;
    } catch (const std::exception &e) {
        fail(e.what());
    }
}
bool ready()
{
    load();
    return loaded && khdays_textures_hd() && khdays_ui_mode(KHDAYS_UI_HUD) == KHDAYS_UI_ON;
}
void push(std::string sprite, float x, float y, float angle = 0, float scale = 1)
{
    if (!recording)
        return;
    if (auto found = config.sprites.find(sprite); found != config.sprites.end()) {
        const auto &s = found->second;
        float radius = std::hypot(std::max(std::abs(s.pivot_x), std::abs(s.draw_width - s.pivot_x)),
                                  std::max(std::abs(s.pivot_y), std::abs(s.draw_height - s.pivot_y))) *
                       s.scale * scale;
        if (x + s.offset_x + radius <= 0 || x + s.offset_x - radius >= 120 || y + s.offset_y + radius <= 0 ||
            y + s.offset_y - radius >= 80)
            return;
    }
    if (draws.size() >= KHDAYS_MINIMAP_BATCHES * KSI_MAX_QUADS) {
        overflow = true;
        return;
    }
    draws.push_back({std::move(sprite), x, y, angle, scale});
}
std::array<int, 2> cell(int x, int y)
{
    unsigned p = read(0x0207f638);
    return {((x + 0x800) >> 12) - int(read(p + 8)), ((y + 0x800) >> 12) - int(read(p + 12))};
}
std::string character(int player)
{
    unsigned panel = read(0x0207f614);
    if (!khdays_game_ram(panel))
        return {};
    for (unsigned i = 0; i < 4; ++i) {
        unsigned record = panel + 0x130 + 16 * i;
        int id = int(read(record)), icon = int(read(record + 4));
        if (id == player && icon >= 0 && icon < int(khdays::ui::kh2::portrait_names.size()))
            return khdays::ui::kh2::portrait_names[icon];
    }
    return {};
}
// Infer the dynamic buffer's placement from its actual BG map, not from a single-screen crop.
bool locate(float &x, float &y)
{
    for (int bg : {2, 3}) {
        KhdaysBgLayer l;
        if (!khdays_bg_text_layer(1, bg, &l) || l.colors256)
            continue;
        for (int cy = 0; cy < 24; ++cy)
            for (int cx = 0; cx < 32; ++cx) {
                unsigned e = khdays_bg_map_entry(1, &l, (l.hofs >> 3) + cx, (l.vofs >> 3) + cy);
                unsigned tile = e & 1023u;
                if ((e >> 12) != 13 || tile < 0x139 || tile >= 0x1cf)
                    continue;
                unsigned n = tile - 0x139;
                x = float(cx * 8 - (l.hofs & 7) - int(n % 15) * 8);
                y = float(cy * 8 - (l.vofs & 7) - int(n / 15) * 8);
                if (x < 80 || x > 128 || y < 32 || y > 80)
                    continue;
                layer = bg;
                return true;
            }
    }
    return false;
}
} // namespace
extern "C" void khdays_minimap_begin(void)
{
    recording = ready();
    draws.clear();
    complete = overflow = false;
    owner = read(0x0207f638);
}
extern "C" void khdays_minimap_end(void)
{
    complete = recording && !overflow;
    if (recording)
        khdays_ui_changed();
    recording = false;
}
extern "C" void khdays_minimap_icon(int kind, int x, int y)
{
    if (!recording || kind == 3 || kind == 7)
        return;
    auto c = cell(x, y);
    push("entity_" + std::to_string(kind), float(c[0]), float(c[1]));
}
extern "C" void khdays_minimap_player(int player, int x, int y, int heading)
{
    if (!recording || player < 0 || player > 3)
        return;
    auto c = cell(x, y);
    auto policy = config.players[player];
    auto custom = config.characters.find(character(player));
    if (custom != config.characters.end() && custom->second.enabled && !custom->second.sprite.empty() &&
        config.sprites.contains(custom->second.sprite))
        policy = custom->second;
    const int band = ((heading + 0x1000) / 0x2000) % 8;
    float angle = 0;
    std::string sprite = "player" + std::to_string(player + 1) + "_" + std::to_string(band);
    if (policy.enabled && (policy.smooth_rotation || !policy.sprite.empty())) {
        auto proposed = policy.sprite.empty() ? "player" + std::to_string(player + 1) + "_0" : policy.sprite;
        if (config.sprites.contains(proposed)) {
            sprite = proposed;
            angle = policy.smooth_rotation ? float(heading) * 360 / 65536.f : float(band) * 45;
        } else if (policy.smooth_rotation) {
            sprite = "player" + std::to_string(player + 1) + "_0";
            angle = float(heading) * 360 / 65536.f;
        }
    }
    if (policy.enabled)
        angle += policy.rotation_offset;
    push(sprite, float(c[0]), float(c[1]), angle, policy.enabled ? policy.scale : 1);
}
extern "C" void khdays_minimap_emblem(int player)
{
    if (player >= 0 && player < 4)
        push("emblem" + std::to_string(player + 1), 60, 40);
}
extern "C" void khdays_minimap_stamp(unsigned source, int x, int y)
{
    if (!recording)
        return;
    unsigned p = read(0x0207f638), chars = read(read(p + 0x18) + 0x14);
    unsigned offset = source - chars;
    char name[40];
    std::snprintf(name, sizeof(name), "marker_%03x", offset);
    push(name, float(x + 4), float(y + 4));
}
extern "C" void khdays_minimap_reset(void)
{
    draws.clear();
    recording = complete = owns = false;
    owner = 0;
    programs = {};
}
extern "C" void khdays_minimap_frame(unsigned frame)
{
    khdays_situation_take(&context, frame);
    programs = {};
    owns = false;
    if (ready() && complete)
        khdays_ui_changed();
}
extern "C" void khdays_minimap_emit(void)
{
    programs = {};
    owns = false;
    if (!ready() || !complete || owner != read(0x0207f638) || context.scene != 2 || !owner ||
        context.camp_screen != KHDAYS_CAMP_NONE || context.movie || context.tutorial ||
        (context.overlays & ((1ull << 8) | (1ull << 25) | (1ull << 26) | (1ull << 27))))
        return;
    float x, y;
    if (!locate(x, y))
        return;
    try {
        int resolution = khdays_ui_scale();
        if (output < 0 || output_scale != resolution) {
            khdays_ui_image_put(output);
            output = khdays_ui_image_get(120 * resolution, 80 * resolution);
            output_scale = resolution;
        }
        if (output < 0)
            throw std::runtime_error("could not allocate icon layer");
        std::vector<seq::SequenceQuad> quads;
        quads.reserve(draws.size());
        for (const auto &d : draws) {
            auto found = config.sprites.find(d.sprite);
            if (found == config.sprites.end())
                throw std::runtime_error("missing required sprite: " + d.sprite);
            const auto &s = found->second;
            float r = d.angle * 3.14159265358979323846f / 180, c = std::cos(r), sn = std::sin(r),
                  scale = s.scale * d.scale;
            seq::SequenceQuad q;
            for (unsigned v = 0; v < 4; ++v) {
                float lx = ((v & 1 ? s.draw_width : 0) - s.pivot_x) * scale,
                      ly = ((v & 2 ? s.draw_height : 0) - s.pivot_y) * scale;
                q.vertices[v] = {d.x + s.offset_x + lx * c + ly * sn, d.y + s.offset_y - lx * sn + ly * c,
                                 s.x + (v & 1 ? s.width : 0), s.y + (v & 2 ? s.height : 0), 0x80808080};
            }
            quads.push_back(q);
        }
        size_t batches = std::max(size_t(1), (quads.size() + KSI_MAX_QUADS - 1) / KSI_MAX_QUADS);
        for (size_t i = 0; i < batches; ++i) {
            auto &p = programs[i];
            auto *w = p.words;
            w[KSI_OUTPUT] = unsigned(khdays_ui_image_replacement(output));
            w[KSI_ATLAS] = unsigned(atlas);
            w[KSI_WIDTH] = 120 * resolution;
            w[KSI_HEIGHT] = 80 * resolution;
            w[KSI_LOGICAL_W] = config.width;
            w[KSI_LOGICAL_H] = config.height;
            w[KSI_CLEAR] = i == 0;
            w[KSI_FACE] = w[KSI_HD_FACE] = ~0u;
            w[KSI_ANCHORED] = 1;
            float anchor_x = x + 60, anchor_y = y + 40;
            std::memcpy(w + KSI_ANCHOR_X, &anchor_x, 4);
            std::memcpy(w + KSI_ANCHOR_Y, &anchor_y, 4);
            float factor = float(resolution);
            std::memcpy(w + KSI_SCALE, &factor, 4);
            size_t first = i * KSI_MAX_QUADS, n = std::min(size_t(KSI_MAX_QUADS), quads.size() - first);
            khdays::render::kh2::encode_sequence_quads(p, std::span<const seq::SequenceQuad>(quads).subspan(first, n));
        }
        KhdaysUiElement e{"hud.minimap_icons",
                          1,
                          KES_DIRECT | KES_FINE | KES_PICTURE | KES_NATIVE_FADE | unsigned(layer),
                          x,
                          y,
                          120,
                          80,
                          output};
        khdays_ui_emit(KHDAYS_UI_HUD, &e);
        owns = true;
    } catch (const std::exception &e) {
        fail(e.what());
    }
}
extern "C" int khdays_minimap_cell(int engine, int bg, unsigned entry)
{
    return owns && engine == 1 && bg == layer && (entry >> 12) == 13 && (entry & 1023u) >= 0x139 &&
           (entry & 1023u) < 0x1cf;
}
extern "C" void khdays_minimap_commands(KhdaysSequenceImage *out)
{
    std::copy(programs.begin(), programs.end(), out);
}
