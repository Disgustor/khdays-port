// The mod gauge's flat JSON settings; no game or graphics-device dependencies.
#include "kh2_hp_config.h"
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <stdexcept>
#include <variant>

namespace khdays::ui::kh2 {
namespace {
using Value = std::variant<std::string, double, bool>;
using Object = std::map<std::string, Value>;
class Reader {
    std::string text;
    std::size_t at = 0;
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

public:
    explicit Reader(std::string input) : text(std::move(input)) {}
    Object read()
    {
        Object out;
        take('{');
        if (next() != '}')
            for (;;) {
                const auto key = string();
                take(':');
                if (!out.emplace(key, value()).second)
                    throw std::runtime_error("gauge JSON: duplicate key " + key);
                if (next() != ',')
                    break;
                ++at;
            }
        take('}');
        if (next() != '\0' || at != text.size())
            fail();
        return out;
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

HpGaugeConfig load_hp_gauge_config(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file || file.tellg() < 0 || file.tellg() > 65536)
        throw std::runtime_error("gauge JSON: cannot read " + path.string());
    std::string text(static_cast<std::size_t>(file.tellg()), '\0');
    file.seekg(0);
    if (!file.read(text.data(), std::streamsize(text.size())))
        throw std::runtime_error("gauge JSON: incomplete file");
    auto o = Reader(std::move(text)).read();
    HpGaugeConfig c;
    number(o, "version", 1, 1, 1, true);
    c.enabled = get(o, "enabled", c.enabled);
    for (auto [key, field] : {std::pair{"archive", &c.archive}, {"texture", &c.texture}, {"entry", &c.entry}}) {
        *field = get(o, key, *field);
        if (field->empty() || field->find_first_of("/\\:") != std::string::npos || *field == "..")
            throw std::runtime_error(std::string("gauge JSON: use a file name for ") + key);
    }
    c.offset_x = float(number(o, "offset_x", 0, -256, 256));
    c.offset_y = float(number(o, "offset_y", 0, -256, 256));
    c.scale = float(number(o, "scale", 0.65, 0.05, 2));
    c.canvas_padding = float(number(o, "canvas_padding", 16, 0, 128));
    c.portrait_offset_x = float(number(o, "portrait_offset_x", 0, -128, 128));
    c.portrait_offset_y = float(number(o, "portrait_offset_y", 0, -128, 128));
    auto &s = c.style;
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
    if (!o.empty())
        throw std::runtime_error("gauge JSON: unknown key " + o.begin()->first);
    return c;
}
} // namespace khdays::ui::kh2
