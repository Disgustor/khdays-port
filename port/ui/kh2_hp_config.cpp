// The mod gauge's flat JSON settings; no game or graphics-device dependencies.
#include "kh2_hp_config.h"
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <tuple>
#include <stdexcept>
#include <variant>

namespace khdays::ui::kh2 {
namespace {
using Value = std::variant<std::string, double, bool>;
using Object = std::map<std::string, Value>;
class Reader {
    std::string text;
    std::size_t at = 0;
    Object values;
    std::set<std::string> keys;
    [[noreturn]] void fail() const
    {
        throw std::runtime_error("gauge JSON: invalid flat JSON object at byte " + std::to_string(at));
    }
    char next()
    {
        while (at < text.size() && (text[at] == ' ' || text[at] == '\n' || text[at] == '\r' || text[at] == '\t'))
            ++at;
        return at < text.size() ? text[at] : '\0';
    }
    void take(char c)
    {
        if (next() != c)
            fail();
        ++at;
    }
    std::string string()
    {
        take('"');
        std::string out;
        while (at < text.size() && text[at] != '"') {
            char c = text[at++];
            if (static_cast<unsigned char>(c) < 32)
                fail();
            if (c == '\\') {
                if (at == text.size())
                    fail();
                c = text[at++];
                switch (c) {
                case '"':
                case '\\':
                case '/': break;
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                default: fail();
                }
            }
            out += c;
        }
        take('"');
        return out;
    }
    Value value()
    {
        if (next() == '"')
            return string();
        for (const auto &[word, boolean] : {std::pair{"true", true}, std::pair{"false", false}}) {
            const std::string token(word);
            if (text.compare(at, token.size(), token) == 0) {
                at += token.size();
                return boolean;
            }
        }
        const auto start = at;
        if (next() == '-')
            ++at;
        auto digit = [&] { return at < text.size() && text[at] >= '0' && text[at] <= '9'; };
        if (!digit())
            fail();
        if (text[at] == '0')
            ++at;
        else
            while (digit())
                ++at;
        if (at < text.size() && text[at] == '.') {
            ++at;
            if (!digit())
                fail();
            while (digit())
                ++at;
        }
        if (at < text.size() && (text[at] == 'e' || text[at] == 'E')) {
            ++at;
            if (at < text.size() && (text[at] == '+' || text[at] == '-'))
                ++at;
            if (!digit())
                fail();
            while (digit())
                ++at;
        }
        const double n = std::strtod(text.substr(start, at - start).c_str(), nullptr);
        if (!std::isfinite(n))
            fail();
        return n;
    }

    void object(const std::string &prefix, int depth)
    {
        if (depth > 4)
            fail();
        take('{');
        if (next() != '}')
            for (;;) {
                const auto key = string();
                if (key.empty() || key.find('.') != std::string::npos)
                    fail();
                const auto name = prefix + key;
                if (!keys.insert(name).second)
                    fail();
                take(':');
                if (next() == '{') {
                    const bool label = key == "player_icon" || key == "limit_text";
                    const bool allowed =
                        (prefix.empty() && (key == "party" || label)) ||
                        (prefix == "party." && (key == "slots" || label)) ||
                        (prefix == "party.slots." && (key == "1" || key == "2" || key == "3")) ||
                        ((prefix == "party.slots.1." || prefix == "party.slots.2." || prefix == "party.slots.3.") &&
                         label);
                    if (!allowed)
                        fail();
                    object(name + '.', depth + 1);
                } else
                    values.emplace(name, value());
                if (next() != ',')
                    break;
                ++at;
            }
        take('}');
    }

public:
    explicit Reader(std::string input) : text(std::move(input)) {}
    Object read()
    {
        object("", 0);
        if (next() != '\0' || at != text.size())
            fail();
        return values;
    }
};
template <class T> T get(Object &o, const char *key, T fallback)
{
    auto i = o.find(key);
    if (i == o.end())
        return fallback;
    const auto *v = std::get_if<T>(&i->second);
    if (!v)
        throw std::runtime_error(std::string("gauge JSON: wrong type for ") + key);
    T result = *v;
    o.erase(i);
    return result;
}
double number(Object &o, const char *key, double fallback, double lo, double hi, bool integer = false)
{
    const double n = get(o, key, fallback);
    if (n < lo || n > hi || (integer && n != std::floor(n)))
        throw std::runtime_error(std::string("gauge JSON: out of range ") + key);
    return n;
}
} // namespace

HpGaugeConfig parse_config(Object o, HpGaugeConfig c = {})
{
    number(o, "version", 1, 1, 1, true);
    c.enabled = get(o, "enabled", c.enabled);
    for (auto [key, field] : {std::pair{"archive", &c.archive}, {"texture", &c.texture}, {"entry", &c.entry}}) {
        *field = get(o, key, *field);
        if (field->empty() || field->find_first_of("/\\:") != std::string::npos || *field == "..")
            throw std::runtime_error(std::string("gauge JSON: use a file name for ") + key);
    }
    c.offset_x = float(number(o, "offset_x", c.offset_x, -256, 256));
    c.offset_y = float(number(o, "offset_y", c.offset_y, -256, 256));
    c.scale = float(number(o, "scale", c.scale, 0.05, 2));
    c.canvas_padding = float(number(o, "canvas_padding", 16, 0, 128));
    c.portrait_offset_x = float(number(o, "portrait_offset_x", 0, -128, 128));
    c.portrait_offset_y = float(number(o, "portrait_offset_y", 0, -128, 128));
    auto &s = c.style;
    s.geometry_scale = float(number(o, "geometry_scale", s.geometry_scale, 0.01, 4));
    s.source_origin_x = float(number(o, "source_origin_x", 0, -4096, 4096));
    s.source_origin_y = float(number(o, "source_origin_y", 0, -4096, 4096));
    s.play_intro = get(o, "play_intro", s.play_intro);
    s.center_override = int(number(o, "center_override", -1, -1, 65535, true));
    auto clip = [&](const char *key, std::size_t &v) { v = std::size_t(number(o, key, double(v), 0, 65535, true)); };
    clip("center_normal", s.center[0]);
    clip("center_alert", s.center[1]);
    clip("center_dead", s.center[2]);
    clip("center_damage_small", s.damage[0]);
    clip("center_damage_medium", s.damage[1]);
    clip("center_damage_large", s.damage[2]);
    clip("center_intro", s.intro);
    clip("center_fade", s.fade);
    clip("circle_normal", s.circle[0]);
    clip("circle_alert", s.circle[1]);
    clip("circle_dead", s.circle[2]);
    clip("bar_normal", s.bar[0]);
    clip("bar_alert", s.bar[1]);
    clip("bar_dead", s.bar[2]);
    clip("circle_damage", s.circle_damage);
    clip("bar_damage", s.bar_damage);
    clip("circle_fill", s.circle_fill);
    clip("bar_fill", s.bar_fill);
    clip("cap", s.cap);
    s.limit_circle = int(number(o, "limit_circle", -1, -1, 65535, true));
    s.limit_bar = int(number(o, "limit_bar", -1, -1, 65535, true));
    s.limit_marker = int(number(o, "limit_marker", -1, -1, 65535, true));
    for (auto [prefix, xy] : {std::pair{"center", &s.center_offset},
                              {"circle", &s.circle_offset},
                              {"bar", &s.bar_offset},
                              {"cap", &s.cap_offset},
                              {"limit_marker", &s.limit_marker_offset}}) {
        (*xy)[0] = float(number(o, (std::string(prefix) + "_offset_x").c_str(), 0, -128, 128));
        (*xy)[1] = float(number(o, (std::string(prefix) + "_offset_y").c_str(), 0, -128, 128));
    }
    for (auto [name, label] : {std::pair{"player_icon", &c.player_icon}, {"limit_text", &c.limit_text}}) {
        const auto prefix = std::string(name) + '.';
        auto num = [&](const char *key, double fallback, double lo, double hi, bool integer = false) {
            return number(o, (prefix + key).c_str(), fallback, lo, hi, integer);
        };
        label->enabled = get(o, (prefix + "enabled").c_str(), false);
        label->animation = int(num("animation", -1, -1, 65535, true));
        for (int i = 0; i < 4; ++i)
            label->animations[i] = int(num(("p" + std::to_string(i + 1)).c_str(), -1, -1, 65535, true));
        label->offset_x = float(num("offset_x", 0, -512, 512));
        label->offset_y = float(num("offset_y", 0, -512, 512));
        label->scale = float(num("scale", c.scale, 0.01, 4));
        label->source_origin_x = float(num("source_origin_x", 0, -4096, 4096));
        label->source_origin_y = float(num("source_origin_y", 0, -4096, 4096));
        label->frames_per_tick = float(num("frames_per_tick", 1, 0.01, 8));
        for (auto [key, field, fallback] :
             {std::tuple{"archive", &label->archive, &c.archive}, std::tuple{"texture", &label->texture, &c.texture},
              std::tuple{"entry", &label->entry, &c.entry}}) {
            *field = get(o, (prefix + key).c_str(), *fallback);
            if (field->empty() || field->find_first_of("/\\:") != std::string::npos || *field == "..")
                throw std::runtime_error("gauge JSON: label resources must be local file names");
        }
    }
    if (!o.empty())
        throw std::runtime_error("gauge JSON: unknown key " + o.begin()->first);
    return c;
}
std::array<HpGaugeConfig, 4> load_hp_gauge_configs(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file || file.tellg() < 0 || file.tellg() > 262144)
        throw std::runtime_error("gauge JSON: cannot read " + path.string());
    std::string text(static_cast<std::size_t>(file.tellg()), '\0');
    file.seekg(0);
    if (!file.read(text.data(), std::streamsize(text.size())))
        throw std::runtime_error("gauge JSON: incomplete file");
    auto all = Reader(std::move(text)).read();
    auto extract = [&](const std::string &prefix) {
        Object result;
        for (auto it = all.begin(); it != all.end();) {
            if (it->first.starts_with(prefix)) {
                result.emplace(it->first.substr(prefix.size()), it->second);
                it = all.erase(it);
            } else
                ++it;
        }
        return result;
    };
    std::array<Object, 3> overrides;
    for (int i = 0; i < 3; ++i)
        overrides[i] = extract("party.slots." + std::to_string(i + 1) + '.');
    auto party = extract("party.");
    std::array<HpGaugeConfig, 4> result;
    result[0] = parse_config(std::move(all));
    const bool present = !party.empty();
    for (int i = 0; i < 3; ++i) {
        auto selected = party;
        for (const auto &[key, value] : overrides[i]) {
            const bool offset = key == "offset_x" || key == "offset_y" || key == "player_icon.offset_x" ||
                                key == "player_icon.offset_y" || key == "limit_text.offset_x" ||
                                key == "limit_text.offset_y";
            const auto found = selected.find(key);
            const auto *extra = std::get_if<double>(&value);
            const auto *base = found != selected.end() ? std::get_if<double>(&found->second) : nullptr;
            if (offset && base && extra)
                selected.insert_or_assign(key, *base + *extra);
            else
                selected.insert_or_assign(key, value);
        }
        auto defaults = result[0];
        defaults.enabled = present;
        defaults.offset_x = defaults.offset_y = 0;
        defaults.style.geometry_scale = 23.0f / 45;
        defaults.style.center = {174, 169, 174};
        defaults.style.damage = {172, 171, 170};
        defaults.style.intro = 173;
        defaults.style.fade = 175;
        defaults.style.circle = {160, 161, 160};
        defaults.style.bar = {163, 164, 163};
        defaults.style.circle_damage = 165;
        defaults.style.bar_damage = 166;
        defaults.style.circle_fill = 167;
        defaults.style.bar_fill = 168;
        defaults.style.cap = 162;
        result[i + 1] = parse_config(std::move(selected), defaults);
        auto &style = result[i + 1].style;
        style.limit_circle = style.limit_bar = style.limit_marker = -1;
    }
    return result;
}
HpGaugeConfig load_hp_gauge_config(const std::filesystem::path &path)
{
    return load_hp_gauge_configs(path)[0];
}
} // namespace khdays::ui::kh2
