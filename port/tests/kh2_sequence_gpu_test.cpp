// The GPU sequence pass against the CPU reference, including large atlases and repeated draws.
#include "../gfx/kh2_sequence.h"
#include "../ui/kh2_hp_gauge.h"
#include "../ui/kh2_hp_config.h"
#include "../host/render/kh2_sequence_asset.h"
extern "C" {
#include "../host/render/kh2_sequence_gpu.h"
#include "../host/render/gpu_hd.h"
#include "../runtime/mods/textures.h"
}
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
namespace seq = khdays::assets::kh2;
namespace render = khdays::render::kh2;
namespace gauge = khdays::ui::kh2;
std::vector<std::uint32_t> pixels[4];
KhdaysTextureReplacement images[4]{};
std::uint32_t image_versions[4]{};
SDL_GPUDevice *device;
void require(bool ok)
{
    if (!ok)
        throw std::runtime_error(SDL_GetError());
}
void set_image(int id, int w, int h, const std::vector<std::uint32_t> &p)
{
    pixels[id] = p;
    images[id] = {w, h, pixels[id].data(), 0};
    ++image_versions[id];
}
void number(KhdaysSequenceImage &p, unsigned at, float f)
{
    std::memcpy(p.words + at, &f, 4);
}
void initialize(KhdaysSequenceImage &p, int w, int h, int lw, int lh, const render::SequenceView &v)
{
    p = {};
    p.words[KSI_OUTPUT] = 0;
    p.words[KSI_ATLAS] = 1;
    p.words[KSI_LOGICAL_W] = unsigned(lw);
    p.words[KSI_LOGICAL_H] = unsigned(lh);
    p.words[KSI_WIDTH] = unsigned(w);
    p.words[KSI_HEIGHT] = unsigned(h);
    number(p, KSI_VIEW_X, v.x);
    number(p, KSI_VIEW_Y, v.y);
    number(p, KSI_SCALE, v.scale);
    p.words[KSI_FACE] = 2;
    p.words[KSI_HD_FACE] = 0xffffffffu;
    set_image(0, w, h, std::vector<std::uint32_t>(std::size_t(w * h)));
    khdays_gpu_hd_close();
    require(khdays_gpu_hd_open(device));
}
void finish(SDL_GPUCommandBuffer *cmd)
{
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    require(fence && SDL_WaitForGPUFences(device, true, &fence, 1));
    SDL_ReleaseGPUFence(device, fence);
}
void draw(const KhdaysSequenceImage &p)
{
    auto *cmd = SDL_AcquireGPUCommandBuffer(device);
    require(cmd != nullptr);
    require(khdays_gpu_hd_sync(cmd));
    require(khdays_sequence_gpu_draw(device, cmd, &p));
    finish(cmd);
}
std::vector<std::uint32_t> read(unsigned offset, unsigned bytes)
{
    SDL_GPUTransferBufferCreateInfo info{};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    info.size = bytes;
    auto *transfer = SDL_CreateGPUTransferBuffer(device, &info);
    require(transfer != nullptr);
    auto *cmd = SDL_AcquireGPUCommandBuffer(device);
    require(cmd != nullptr);
    auto *copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUBufferRegion from{khdays_gpu_hd_buffer(), offset, bytes};
    SDL_GPUTransferBufferLocation to{transfer, 0};
    SDL_DownloadFromGPUBuffer(copy, &from, &to);
    SDL_EndGPUCopyPass(copy);
    finish(cmd);
    auto *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    require(mapped != nullptr);
    std::vector<std::uint32_t> out(bytes / 4);
    std::memcpy(out.data(), mapped, bytes);
    SDL_UnmapGPUTransferBuffer(device, transfer);
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return out;
}
void compare(const KhdaysSequenceImage &p, const std::vector<seq::SequenceQuad> &quads,
             const khdays::assets::DecodedTexture &atlas, int lw, int lh, const render::SequenceView &view)
{
    const int w = int(p.words[KSI_WIDTH]), h = int(p.words[KSI_HEIGHT]);
    std::vector<std::uint32_t> expected(std::size_t(w * h));
    render::draw_sequence(quads, atlas, lw, lh, expected, w, h, view);
    draw(p);
    const unsigned first = read(0, 16)[0];
    const auto got = read(first * 4u, unsigned(w * h * 4));
    int maximum = 0;
    std::size_t bad = 0;
    std::size_t first_bad = got.size();
    for (std::size_t i = 0; i < got.size(); ++i)
        for (unsigned s = 0; s < 32; s += 8) {
            // Subpixel filtering can retain RGB beneath rounded-to-zero alpha.
            if (s < 24 && (got[i] >> 24) == 0 && (expected[i] >> 24) == 0)
                continue;
            const int error = std::abs(int((got[i] >> s) & 255) - int((expected[i] >> s) & 255));
            maximum = std::max(maximum, error);
            bad += error > 2;
            if (error > 2 && first_bad == got.size())
                first_bad = i;
        }
    if (bad) {
        std::cerr << "first mismatch " << first_bad % std::size_t(w) << ',' << first_bad / std::size_t(w) << ": "
                  << std::hex << got[first_bad] << " expected " << expected[first_bad] << std::dec << '\n';
        throw std::runtime_error("GPU/reference mismatch: " + std::to_string(bad) + " channels, max " +
                                 std::to_string(maximum));
    }
}
void synthetic()
{
    khdays::assets::DecodedTexture atlas;
    atlas.width = atlas.height = 4;
    atlas.rgba.resize(64);
    for (unsigned i = 0; i < 16; ++i) {
        atlas.rgba[i * 4] = std::uint8_t(i * 13);
        atlas.rgba[i * 4 + 1] = std::uint8_t(255 - i * 11);
        atlas.rgba[i * 4 + 2] = std::uint8_t(i * 7);
        atlas.rgba[i * 4 + 3] = std::uint8_t(64 + i * 11);
    }
    std::vector<std::uint32_t> texels(16);
    std::memcpy(texels.data(), atlas.rgba.data(), 64);
    set_image(1, 4, 4, texels);
    set_image(2, 1, 1, {0});
    set_image(3, 1, 1, {0});
    seq::SequenceQuad q;
    q.vertices = {{{-16, -16, 0, 0}, {16, -16, 4, 0}, {-16, 16, 0, 4}, {16, 16, 4, 4}}};
    q.vertices[1].color = 0x80604080;
    q.vertices[2].color = 0x80408040;
    auto mask = q;
    mask.textured = false;
    mask.color_write = false;
    mask.depth_write = true;
    mask.depth = 200;
    mask.vertices[1].x = mask.vertices[3].x = 0;
    for (unsigned blend = 0; blend < 3; ++blend)
        for (bool filter : {false, true}) {
            auto foreground = q;
            foreground.depth_test = true;
            foreground.depth = 100;
            foreground.blend = blend;
            foreground.linear_filter = filter;
            std::vector<seq::SequenceQuad> quads{q, mask, foreground};
            KhdaysSequenceImage program{};
            render::SequenceView view{20, 20, 1};
            initialize(program, 40, 40, 4, 4, view);
            render::encode_sequence_quads(program, quads);
            compare(program, quads, atlas, 4, 4, view);
        }
    // An opaque face over the gauge must occlude it without a CPU copy.
    set_image(2, 1, 1, {0xff123456});
    KhdaysSequenceImage p{};
    initialize(p, 40, 40, 4, 4, {20, 20, 1});
    number(p, KSI_FACE_X, 20);
    number(p, KSI_FACE_Y, 20);
    number(p, KSI_FACE_W, 1);
    number(p, KSI_FACE_H, 1);
    render::encode_sequence_quads(p, std::span(&q, 1));
    draw(p);
    auto first = read(0, 16)[0];
    auto got = read(first * 4u, 40u * 40u * 4u);
    if (got[20 * 40 + 20] != 0xff123456)
        throw std::runtime_error("GPU gauge covered the face");
    set_image(3, 1, 1, {0x804080c0});
    p.words[KSI_HD_FACE] = 3;
    draw(p);
    got = read(first * 4u, 40u * 40u * 4u);
    const unsigned face = got[20 * 40 + 20];
    for (unsigned s = 0; s < 24; s += 8) {
        const float expected =
            float((0x804080c0u >> s) & 255) * (128.0f / 255) + float((0xff123456u >> s) & 255) * (127.0f / 255);
        if (std::abs(int((face >> s) & 255) - int(expected + 0.5f)) > 1)
            throw std::runtime_error("GPU HD face alpha mismatch");
    }
    std::cout << "GPU synthetic blending/filter/mask/face checks passed\n";
}
void large_atlas()
{
    // Synthetic high-resolution replacement: the same logical rectangle at eight times its atlas size.
    khdays::assets::DecodedTexture atlas;
    atlas.width = 4096;
    atlas.height = 2048;
    atlas.rgba.resize(std::size_t(atlas.width * atlas.height) * 4);
    std::vector<std::uint32_t> texels(std::size_t(atlas.width * atlas.height), 0xff40a080);
    std::memcpy(atlas.rgba.data(), texels.data(), atlas.rgba.size());
    set_image(1, atlas.width, atlas.height, texels);
    images[1].info = KHDAYS_REPLACEMENT_NO_MIPS;
    seq::SequenceQuad q;
    q.vertices = {{{-16, -16, 0, 0}, {16, -16, 512, 0}, {-16, 16, 0, 256}, {16, 16, 512, 256}}};
    KhdaysSequenceImage p{};
    initialize(p, 40, 40, 512, 256, {20, 20, 1});
    render::encode_sequence_quads(p, std::span(&q, 1));
    compare(p, {q}, atlas, 512, 256, {20, 20, 1});
    for (int i = 0; i < 100; ++i)
        draw(p);
    std::cout << "GPU 4096x2048 synthetic atlas checks passed\n";
}

void multiple_images()
{
    set_image(1, 1, 1, {0xffffffff});
    for (int id : {0, 2, 3})
        set_image(id, 16, 16, std::vector<std::uint32_t>(256, 0xffffffff));
    khdays_gpu_hd_close();
    require(khdays_gpu_hd_open(device));
    KhdaysSequenceImage programs[3]{};
    seq::SequenceQuad quad;
    quad.vertices = {{{0, 0, 0, 0}, {16, 0, 1, 0}, {0, 16, 0, 1}, {16, 16, 1, 1}}};
    for (unsigned i = 0; i < 3; ++i) {
        auto &p = programs[i];
        p.words[KSI_OUTPUT] = i == 0 ? 0 : i + 1;
        p.words[KSI_ATLAS] = 1;
        p.words[KSI_LOGICAL_W] = p.words[KSI_LOGICAL_H] = 1;
        p.words[KSI_WIDTH] = p.words[KSI_HEIGHT] = 16;
        p.words[KSI_CLEAR] = 1;
        number(p, KSI_SCALE, 1);
        for (auto &v : quad.vertices)
            v.color = i == 0 ? 0x80000080 : 0x80008000;
        if (i < 2)
            render::encode_sequence_quads(p, std::span(&quad, 1));
    }
    auto *cmd = SDL_AcquireGPUCommandBuffer(device);
    require(cmd && khdays_gpu_hd_sync(cmd));
    for (const auto &p : programs)
        require(khdays_sequence_gpu_draw(device, cmd, &p));
    finish(cmd);
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned target = programs[i].words[KSI_OUTPUT];
        const unsigned first = read(target * 16, 16)[0];
        const auto result = read(first * 4, 256 * 4);
        const unsigned expected = i == 0 ? 0xff0000ff : i == 1 ? 0xff00ff00 : 0;
        if (std::any_of(result.begin(), result.end(), [&](unsigned c) { return c != expected; }))
            throw std::runtime_error("multiple sequence passes mixed their commands or retained a blank frame");
    }
    std::cout << "GPU multiple-image command cycling and transparent-frame clearing passed\n";
}

void private_portraits(const std::filesystem::path &folder)
{
    for (const auto *name : {"P_EX110.a.us", "P_EX020.a.us"}) {
        const auto a =
            render::load_sequence_asset(folder / name, "face", "face", folder / (std::string(name) + "_face.dds"));
        std::vector<std::uint32_t> texels(a.texture.rgba.size() / 4);
        std::memcpy(texels.data(), a.texture.rgba.data(), a.texture.rgba.size());
        set_image(1, a.texture.width, a.texture.height, texels);
        images[1].info = KHDAYS_REPLACEMENT_NO_MIPS;
        for (unsigned clip = 0; clip < 4; ++clip)
            for (float frame : {0.0f, 15.0f, 29.0f, 30.0f, 59.0f}) {
                const auto evaluated = seq::evaluate_sequence(a.sequence, clip, frame);
                KhdaysSequenceImage p{};
                const render::SequenceView view{128, 128, 2};
                initialize(p, 256, 256, a.logical_size[0], a.logical_size[1], view);
                render::encode_sequence_quads(p, evaluated.quads);
                compare(p, evaluated.quads, a.texture, a.logical_size[0], a.logical_size[1], view);
            }
        std::cout << name << ": all four face states match the GPU/reference renderer\n";
    }
}
void private_assets(const char *archive, const char *texture, const gauge::HpGaugeStyle &style = {})
{
    auto asset = render::load_sequence_asset(archive, "gaug", texture);
    std::vector<std::uint32_t> texels(asset.texture.rgba.size() / 4);
    std::memcpy(texels.data(), asset.texture.rgba.data(), asset.texture.rgba.size());
    set_image(1, asset.texture.width, asset.texture.height, texels);
    images[1].info = KHDAYS_REPLACEMENT_NO_MIPS;
    for (float hp : {62.0f, 124.0f}) {
        gauge::HpGaugeSample sample;
        sample.hp = sample.max_hp = sample.previous_hp = hp;
        const auto frame = gauge::evaluate_hp_gauge(asset.sequence, sample, style);
        const render::SequenceView view{560, 256, 5.2f};
        KhdaysSequenceImage p{};
        initialize(p, 832, 512, 512, 256, view);
        render::encode_sequence_quads(p, frame.quads);
        compare(p, frame.quads, asset.texture, 512, 256, view);
    }
    for (float hp : {0.0f, 31.0f, 124.0f, 186.0f, 248.0f, 372.0f}) {
        gauge::HpGaugeSample sample;
        sample.hp = hp;
        sample.max_hp = sample.previous_hp = 372;
        sample.damage_frame = 30;
        sample.limit_hp = hp <= 186 ? 124.0f : 248.0f;
        auto frame = gauge::evaluate_hp_gauge(asset.sequence, sample, style);
        render::SequenceView view{155, 32, 0.65f};
        KhdaysSequenceImage p{};
        initialize(p, 197, 64, 512, 256, view);
        render::encode_sequence_quads(p, frame.quads);
        compare(p, frame.quads, asset.texture, 512, 256, view);
    }
    for (int scale : {1, 4, 8}) {
        gauge::HpGaugeSample sample;
        sample.hp = 248;
        sample.max_hp = sample.previous_hp = 372;
        sample.damage_frame = 30;
        sample.limit_hp = 248;
        sample.limit_marker_hp = 310;
        auto frame = gauge::evaluate_hp_gauge(asset.sequence, sample, style);
        KhdaysSequenceImage p{};
        initialize(p, 197 * scale, 64 * scale, 512, 256, {155.0f * scale, 32.0f * scale, 0.65f * scale});
        render::encode_sequence_quads(p, frame.quads);
        for (int i = 0; i < 3; ++i)
            draw(p);
        std::vector<double> timings;
        for (int i = 0; i < 100; ++i) {
            const auto start = std::chrono::steady_clock::now();
            draw(p);
            timings.push_back(
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
        }
        std::sort(timings.begin(), timings.end());
        std::cout << "gaug GPU " << scale << "x, " << asset.texture.width << "x" << asset.texture.height
                  << " atlas: median " << timings[50] << " ms, p95 " << timings[95] << " ms, max " << timings.back()
                  << " ms (includes submission/wait)\n";
    }
    if (style.limit_marker >= 0)
        for (float marker : {0.0f, 62.0f, 124.0f, 185.9f, 186.0f, 186.1f, 248.0f, 372.0f}) {
            gauge::HpGaugeSample sample;
            sample.hp = 31;
            sample.max_hp = sample.previous_hp = 372;
            sample.limit_hp = 124;
            sample.limit_marker_hp = marker;
            const auto frame = gauge::evaluate_hp_gauge(asset.sequence, sample, style);
            const render::SequenceView view{560, 256, 5.2f};
            KhdaysSequenceImage p{};
            initialize(p, 832, 512, 512, 256, view);
            render::encode_sequence_quads(p, frame.quads);
            try {
                compare(p, frame.quads, asset.texture, 512, 256, view);
            } catch (const std::exception &e) {
                throw std::runtime_error("divider at " + std::to_string(marker) + " HP: " + e.what());
            }
        }
}

void private_enemy(const char *archive, const char *texture)
{
    const auto a = render::load_sequence_asset(archive, "gaug", texture);
    std::vector<std::uint32_t> texels(a.texture.rgba.size() / 4);
    std::memcpy(texels.data(), a.texture.rgba.data(), a.texture.rgba.size());
    set_image(1, a.texture.width, a.texture.height, texels);
    images[1].info = KHDAYS_REPLACEMENT_NO_MIPS;
    for (int resolution : {1, 4, 8}) {
        KhdaysSequenceImage p{};
        for (int capacity : {250, 750})
            for (int hp : {0, 1, capacity - 1, capacity, capacity + 1, capacity * 3 / 2})
                for (float damage : {0.0f, 10.0f, 60.0f, 75.0f, 91.0f, 100.0f}) {
                    const auto frame = gauge::evaluate_enemy_gauge(
                        a.sequence, {hp, capacity * 3 / 2, capacity * 3 / 2, capacity, 15, damage});
                    const render::SequenceView view{132.0f * resolution, 4.0f * resolution, 0.65f * resolution};
                    initialize(p, 135 * resolution, 14 * resolution, a.logical_size[0], a.logical_size[1], view);
                    p.words[KSI_FACE] = 0xffffffffu;
                    render::encode_sequence_quads(p, frame.quads);
                    compare(p, frame.quads, a.texture, a.logical_size[0], a.logical_size[1], view);
                }
        for (int i = 0; i < 3; ++i)
            draw(p);
        std::vector<double> timings;
        for (int i = 0; i < 100; ++i) {
            const auto start = std::chrono::steady_clock::now();
            draw(p);
            timings.push_back(
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
        }
        std::sort(timings.begin(), timings.end());
        std::cout << "Enemy GPU/reference matched at " << resolution << "x; median " << timings[50] << " ms, p95 "
                  << timings[95] << " ms (includes submission/wait)\n";
    }
}
} // namespace
extern "C" {
int khdays_textures_replacements(void)
{
    return 4;
}
const KhdaysTextureReplacement *khdays_textures_replacement(int i)
{
    return i >= 0 && i < 4 ? &images[i] : nullptr;
}
uint32_t khdays_textures_version(int i)
{
    return i == 1 ? 0 : image_versions[i];
}
void khdays_textures_read_begin(int) {}
void khdays_textures_read_end(int) {}
}
int main(int argc, char **argv)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
        return 77;
    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL,
                                 true, nullptr);
    if (!device) {
        std::cerr << SDL_GetError() << '\n';
        SDL_Quit();
        return 77;
    }
    int result = 0;
    try {
        require(khdays_gpu_hd_open(device));
        synthetic();
        large_atlas();
        multiple_images();
        if (argc == 4 && std::string(argv[1]) == "--enemy")
            private_enemy(argv[2], argv[3]);
        else if (argc == 3 && std::string(argv[1]) == "--portraits")
            private_portraits(argv[2]);
        else if (argc >= 3)
            private_assets(argv[1], argv[2],
                           argc >= 4
                               ? gauge::load_hp_gauge_configs(argv[3]).at(argc >= 5 ? std::stoul(argv[4]) : 0).style
                               : gauge::HpGaugeStyle{});
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        result = 1;
    }
    khdays_sequence_gpu_close();
    khdays_gpu_hd_close();
    SDL_DestroyGPUDevice(device);
    SDL_Quit();
    return result;
}
