// Move the original BG labels with their HUD slot, or draw an opt-in SEQD label.
#include "kh2_hud_labels.h"
#include "kh2_hp_config.h"
#include "kh2_hp_hud.h"
#include "kh2_party_hud.h"
#include "kh2_portrait.h"
#include "../host/render/kh2_sequence_asset.h"
#include "../gfx/kh2_sequence.h"
extern "C" {
#include "parts.h"
#include "bg.h"
#include "../runtime/mods/textures.h"
#include "../runtime/scripts/game_link.h"
#include "../runtime/core/session_log.h"
#include "../runtime/core/rom.h"
#include "../host/screens/ui_text.h"
int khdays_settings_game_language(void);
}
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <map>
#include <memory>
#include <optional>
#include <limits>

namespace {
namespace gauge = khdays::ui::kh2;
namespace seq = khdays::assets::kh2;
namespace render = khdays::render::kh2;
struct Resource {
    render::SequenceAsset asset;
    std::vector<std::uint32_t> pixels;
    int atlas = -1;
};
struct Label {
    gauge::GaugeLabelConfig config;
    std::shared_ptr<Resource> resource;
    seq::SequencePlayer player;
    bool present = false, shown = false, was = false;
    unsigned drawn_frame = ~0u;
    int number = 0, image = -1, width = 0, height = 0, scale = 0;
    KhdaysUiElement box{}, anchor{}, element{};
    KhdaysSequenceImage program{};
    std::vector<KhdaysUiElement> native;
};
std::array<Label, 5> labels;
std::array<gauge::HpGaugeConfig, 4> gauges;
bool tried = false, loaded = false, reference = true;
unsigned previous_frame = 0;
float elapsed = 0;
constexpr const char *ids[]{"hud.player_icon", "hud.party1.icon", "hud.party2.icon", "hud.party3.icon",
                            "hud.limit_text"};
std::filesystem::path path(std::string_view s)
{
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t *>(s.data()), s.size()));
}
int number_tile(unsigned tile)
{
    if (tile >= 1 && tile <= 8)
        return int((tile - 1) / 2);
    if (tile >= 39 && tile <= 46)
        return int((tile - 39) / 2);
    if (tile == 173)
        return 0;
    if (tile == 174)
        return 1;
    if (tile == 141)
        return 2;
    if (tile == 142)
        return 3;
    return -1;
}
bool label_tile(int index, unsigned entry)
{
    const unsigned t = entry & 1023;
    return index == 4 ? t == 0x180 || t == 0x181 || t == 0x1a0 || t == 0x1a1 : number_tile(t) >= 0 || t == 172;
}
void load()
{
    if (tried)
        return;
    tried = true;
    try {
        char name[128], found[2048];
        const char *language = khdays_language_code(khdays_settings_game_language());
        std::snprintf(name, sizeof(name), "layouts/gauge/%s/gauge.json", language && *language ? language : "en");
        if (!khdays_mods_find(name, found, sizeof(found)) &&
            !khdays_mods_find("layouts/gauge/en/gauge.json", found, sizeof(found)))
            return;
        const auto configs = gauge::load_hp_gauge_configs(path(found));
        gauges = configs;
        std::map<std::string, std::shared_ptr<Resource>> resources;
        for (int i = 0; i < 5; ++i) {
            auto &s = labels[i];
            s.config = i == 4 ? configs[0].limit_text : configs[i].player_icon;
            const auto &c = s.config;
            if (!c.enabled)
                continue;
            if (c.animation < 0 && std::all_of(c.animations.begin(), c.animations.end(), [](int n) { return n < 0; }))
                continue;
            try {
                const auto key = c.archive + '\n' + c.texture + '\n' + c.entry;
                auto &r = resources[key];
                if (!r) {
                    r = std::make_shared<Resource>();
                    const auto folder = path(found).parent_path();
                    r->asset = render::load_sequence_asset(folder / path(c.archive), c.entry, folder / path(c.texture));
                    r->pixels.resize(r->asset.texture.rgba.size() / 4);
                    std::memcpy(r->pixels.data(), r->asset.texture.rgba.data(), r->asset.texture.rgba.size());
                    const auto atlas_name = "kh2:label:" + std::to_string(i);
                    r->atlas =
                        khdays_textures_add_info(atlas_name.c_str(), r->asset.texture.width, r->asset.texture.height,
                                                 r->pixels.data(), KHDAYS_REPLACEMENT_NO_MIPS);
                    if (r->atlas < 0)
                        throw std::runtime_error("could not allocate label atlas");
                }
                if (r->atlas < 0)
                    throw std::runtime_error("could not allocate label atlas");
                for (int clip : {c.animation, c.animations[0], c.animations[1], c.animations[2], c.animations[3]})
                    if (clip >= 0 && std::size_t(clip) >= r->asset.sequence.clips.size())
                        throw std::runtime_error("label animation is outside the archive");
                s.resource = r;
            } catch (const std::exception &e) {
                khdays_log("kh2 label: %s; keeping the native label", e.what());
            }
        }
        loaded = true;
    } catch (const std::exception &e) {
        khdays_log("kh2 labels: %s", e.what());
    }
}
KhdaysHudParent parent(int index, const KhdaysUiElement &anchor)
{
    KhdaysHudParent p{anchor.x + anchor.w * 0.5f, anchor.y + anchor.h * 0.5f, 1, 1, 1};
    khdays_ui_kh2_hp_parent(index == 4 ? 0 : index, &p);
    return p;
}
std::array<float, 2> position(int index, const KhdaysUiElement &box)
{
    const auto &s = labels[index];
    const auto p = parent(index, s.anchor);
    float y = box.y + p.y - (s.anchor.y + s.anchor.h * 0.5f);
    if (index >= 1 && index <= 3 && khdays_ui_kh2_party_ready(index)) {
        const auto &g = gauges[index];
        // Use the player's 37-unit bar attachment, reduced with the party artwork.
        y = p.y + 37 * g.style.geometry_scale * g.scale - box.h;
    }
    return {box.x + p.x - (s.anchor.x + s.anchor.w * 0.5f) + s.config.offset_x, y + s.config.offset_y};
}
bool draw(Label &s, int index)
{
    if (!s.resource)
        return false;
    const int clip =
        index < 4 && s.config.animations[s.number] >= 0 ? s.config.animations[s.number] : s.config.animation;
    if (clip < 0)
        return false;
    const auto &a = s.resource->asset;
    s.player.select(a.sequence, std::size_t(clip), !s.was);
    if (s.was && s.drawn_frame != previous_frame)
        s.player.advance(a.sequence, elapsed * s.config.frames_per_tick);
    s.drawn_frame = previous_frame;
    const auto p = parent(index, s.anchor);
    seq::SequencePose pose;
    pose.alpha = p.alpha;
    auto frame = seq::evaluate_sequence(a.sequence, s.player.clip(), s.player.frame(), pose);
    float x0 = std::numeric_limits<float>::infinity(), y0 = x0, x1 = -x0, y1 = -x0;
    for (auto &q : frame.quads)
        for (auto &v : q.vertices) {
            v.x = (v.x - s.config.source_origin_x) * s.config.scale * p.scale_x;
            v.y = (v.y - s.config.source_origin_y) * s.config.scale * p.scale_y;
            x0 = std::min(x0, v.x);
            y0 = std::min(y0, v.y);
            x1 = std::max(x1, v.x);
            y1 = std::max(y1, v.y);
        }
    if (frame.quads.empty())
        x0 = y0 = x1 = y1 = 0;
    x0 = std::floor(x0 - 4);
    y0 = std::floor(y0 - 4);
    const float w = std::ceil(x1 + 4 - x0), h = std::ceil(y1 + 4 - y0);
    if (!std::isfinite(w) || !std::isfinite(h) || w > 512 || h > 512)
        throw std::runtime_error("label exceeds its 512-dot canvas");
    const int scale = khdays_ui_scale();
    if (s.image < 0 || s.scale != scale || w > s.width || h > s.height) {
        khdays_ui_image_put(s.image);
        s.width = std::max(s.width, int(w));
        s.height = std::max(s.height, int(h));
        s.scale = scale;
        s.image = khdays_ui_image_get(s.width * scale, s.height * scale);
    }
    if (s.image < 0)
        return false;
    const render::SequenceView view{-x0 * scale, -y0 * scale, float(scale)};
    if (reference) {
        std::vector<std::uint32_t> pixels(std::size_t(s.width * scale) * std::size_t(s.height * scale));
        render::draw_sequence(frame.quads, a.texture, a.logical_size[0], a.logical_size[1], pixels, s.width * scale,
                              s.height * scale, view);
        auto *out = khdays_ui_image_begin(s.image);
        if (!out)
            return false;
        std::copy(pixels.begin(), pixels.end(), out);
        khdays_ui_image_end(s.image);
    } else {
        auto *words = s.program.words;
        auto number = [words](unsigned at, float value) { std::memcpy(words + at, &value, 4); };
        words[KSI_OUTPUT] = unsigned(khdays_ui_image_replacement(s.image));
        words[KSI_ATLAS] = unsigned(s.resource->atlas);
        words[KSI_LOGICAL_W] = unsigned(a.logical_size[0]);
        words[KSI_LOGICAL_H] = unsigned(a.logical_size[1]);
        words[KSI_WIDTH] = unsigned(s.width * scale);
        words[KSI_HEIGHT] = unsigned(s.height * scale);
        number(KSI_VIEW_X, view.x);
        number(KSI_VIEW_Y, view.y);
        number(KSI_SCALE, view.scale);
        words[KSI_CLEAR] = 1;
        render::encode_sequence_quads(s.program, frame.quads);
    }
    s.element = s.box;
    s.element.id = ids[index];
    s.element.slot = KES_DIRECT | KES_PICTURE | 3;
    s.element.image = s.image;
    const auto at = position(index, s.box);
    s.element.x = at[0] + s.box.w * 0.5f + x0;
    s.element.y = at[1] + s.box.h * 0.5f + y0;
    s.element.w = float(s.width);
    s.element.h = float(s.height);
    return true;
}
} // namespace

extern "C" void khdays_ui_kh2_labels_frame(unsigned frame)
{
    load();
    elapsed = frame >= previous_frame && !khdays_link_paused() ? float(frame - previous_frame) : 0;
    previous_frame = frame;
    if (elapsed > 0 && std::any_of(labels.begin(), labels.end(), [](const auto &s) { return bool(s.resource); }))
        khdays_ui_changed();
}
extern "C" void khdays_ui_kh2_labels_reference(int cpu)
{
    reference = cpu != 0;
    for (auto &s : labels) {
        s.native.clear();
        s.was = s.present;
        s.present = s.shown = false;
        s.program.words[KSI_COUNT] = s.program.words[KSI_CLEAR] = 0;
    }
}
extern "C" void khdays_ui_kh2_labels_prepare(void)
{
    if (!loaded || khdays_ui_mode(KHDAYS_UI_HUD) != KHDAYS_UI_ON)
        return;
    KhdaysBgLayer l;
    if (!khdays_bg_text_layer(0, 3, &l) || l.colors256)
        return;
    std::array<std::array<int, 4>, 5> bounds;
    for (auto &b : bounds)
        b = {256, 192, 0, 0};
    std::array<bool, 5> active{}, has_anchor{};
    std::array<std::array<int, 4>, 3> faces;
    for (auto &b : faces)
        b = {256, 192, 0, 0};
    for (int y = 0; y < 25; ++y)
        for (int x = 0; x < 33; ++x) {
            const unsigned e = khdays_bg_map_entry(0, &l, ((l.hofs >> 3) + x) % l.wt, ((l.vofs >> 3) + y) % l.ht);
            const int slot = gauge::portrait_tile_slot((e & 1023) * 32);
            if (slot < 1 || slot > 3)
                continue;
            const char *origin = khdays_bg_origin(0, l.chars + (e & 1023) * 32);
            if (!origin || !std::strstr(origin, "UI/btl/") || !std::strstr(origin, "main.p2"))
                continue;
            auto &b = faces[std::size_t(slot - 1)];
            const int px = x * 8 - (l.hofs & 7), py = y * 8 - (l.vofs & 7);
            b[0] = std::min(b[0], px);
            b[1] = std::min(b[1], py);
            b[2] = std::max(b[2], px + 8);
            b[3] = std::max(b[3], py + 8);
        }
    for (int i = 0; i < 5; ++i) {
        auto &s = labels[i];
        const int slot = i == 4 ? 0 : i;
        s.anchor = {"hud.portrait", 0, 3, 206, 136, 48, 48, -1};
        if (slot > 0 && !khdays_ui_kh2_party_box(slot, &s.anchor)) {
            const auto &b = faces[std::size_t(slot - 1)];
            if (b[2] <= b[0] || b[3] <= b[1])
                continue;
            s.anchor = {"hud.portrait", 0, 3, float(b[0]), float(b[1]), float(b[2] - b[0]), float(b[3] - b[1]), -1};
        }
        has_anchor[i] = true;
        active[i] = (slot == 0 ? khdays_ui_kh2_hp_ready() : khdays_ui_kh2_party_ready(slot)) || bool(s.resource);
        active[i] = active[i] || s.config.offset_x != 0 || s.config.offset_y != 0;
    }
    struct Row {
        std::array<int, 4> bounds{256, 192, 0, 0};
        int number = -1;
    };
    std::map<int, Row, std::greater<int>> rows;
    auto include = [](auto &b, int x, int y) {
        b[0] = std::min(b[0], x);
        b[1] = std::min(b[1], y);
        b[2] = std::max(b[2], x + 8);
        b[3] = std::max(b[3], y + 8);
    };
    for (int y = 0; y < 25; ++y)
        for (int x = 0; x < 33; ++x) {
            const int cx = ((l.hofs >> 3) + x) % l.wt, cy = ((l.vofs >> 3) + y) % l.ht;
            const unsigned e = khdays_bg_map_entry(0, &l, cx, cy);
            const char *origin = khdays_bg_origin(0, l.chars + (e & 1023) * 32);
            if (!origin || !std::strstr(origin, "UI/btl/") || !std::strstr(origin, "main.p2") ||
                !khdays_bg_cell_shows(0, &l, e))
                continue;
            const int px = x * 8 - (l.hofs & 7), py = y * 8 - (l.vofs & 7);
            if (active[4] && label_tile(4, e))
                include(bounds[4], px, py);
            if (px < 176 || !label_tile(0, e))
                continue;
            auto &row = rows[py];
            include(row.bounds, px, py);
            const int n = number_tile(e & 1023);
            if (n >= 0)
                row.number = n;
        }
    int slot = 1;
    for (const auto &[y, row] : rows) {
        if (row.number < 0)
            continue;
        if (y + 4 >= labels[0].anchor.y + 16) {
            if (active[0]) {
                bounds[0] = row.bounds;
                labels[0].number = row.number;
            }
            continue;
        }
        while (slot < 4 && !has_anchor[slot])
            ++slot;
        if (slot >= 4)
            break;
        if (active[slot]) {
            bounds[slot] = row.bounds;
            labels[slot].number = row.number;
        }
        ++slot;
    }
    for (int i = 0; i < 5; ++i) {
        auto &s = labels[i];
        const auto &b = bounds[i];
        s.present = b[2] > b[0] && b[3] > b[1];
        if (!s.present)
            continue;
        s.box = {ids[i], 0, 3, float(b[0]), float(b[1]), float(b[2] - b[0]), float(b[3] - b[1]), -1};
        try {
            s.shown = draw(s, i);
        } catch (const std::exception &e) {
            khdays_log("kh2 label: %s; keeping the native label", e.what());
            s.resource.reset();
            s.program.words[KSI_COUNT] = s.program.words[KSI_CLEAR] = 0;
        }
    }
}
extern "C" int khdays_ui_kh2_label_box(int index, KhdaysUiElement *box)
{
    if (index < 0 || index >= 5 || !labels[index].present)
        return 0;
    *box = labels[index].box;
    return 1;
}
extern "C" int khdays_ui_kh2_label_cell(int index, unsigned entry)
{
    return label_tile(index, entry);
}
extern "C" int khdays_ui_kh2_label_native(int index, const KhdaysUiElement *element)
{
    auto &s = labels[index];
    if (!s.present)
        return 0;
    if (!s.shown) {
        auto e = *element;
        const auto at = position(index, *element);
        e.x = at[0];
        e.y = at[1];
        s.native.push_back(e);
    }
    return 1;
}
extern "C" void khdays_ui_kh2_labels_emit(void)
{
    for (const auto &s : labels) {
        for (const auto &e : s.native)
            khdays_ui_emit_extended(KHDAYS_UI_HUD, &e, &s.anchor);
        if (s.shown)
            khdays_ui_emit_extended(KHDAYS_UI_HUD, &s.element, &s.anchor);
    }
}
extern "C" void khdays_ui_kh2_labels_commands(KhdaysSequenceImage out[5])
{
    for (int i = 0; i < 5; ++i)
        out[i] = labels[i].program;
}
