// Ordered native sequence quads and depth masks into an existing UI image on the GPU.
#define KSI_SHADER
#include "../gfx/sequence_image.h"
ByteAddressBuffer commands : register(t0, space0);
RWByteAddressBuffer hd : register(u0, space1);
uint word(uint at) { return commands.Load(at * 4u); }
float number(uint at) { return asfloat(word(at)); }
float4 unpack(uint t) { return float4(t & 255u, (t >> 8) & 255u, (t >> 16) & 255u, t >> 24); }
uint pack(float4 c) {
    uint4 p = (uint4)(clamp(c, 0.0f, 255.0f) + 0.5f);
    return p.x | (p.y << 8) | (p.z << 16) | (p.w << 24);
}
float4 texel(uint image, int2 p) {
    uint first = hd.Load(image * 16u), w = hd.Load(image * 16u + 4u), h = hd.Load(image * 16u + 8u);
    p = clamp(p, int2(0, 0), int2(w, h) - 1);
    return unpack(hd.Load((first + (uint)p.y * w + (uint)p.x) * 4u));
}
float4 sample_at(uint image, float2 uv, bool filtered) {
    const float2 size = float2(hd.Load(image * 16u + 4u), hd.Load(image * 16u + 8u));
    uv = clamp(uv, 0.0f, size);
    if (!filtered) return texel(image, (int2)floor(uv));
    uv -= 0.5f;
    int2 p = (int2)floor(uv);
    float2 f = uv - (float2)p;
    return lerp(lerp(texel(image,p),texel(image,p+int2(1,0)),f.x),
                lerp(texel(image,p+int2(0,1)),texel(image,p+1),f.x),f.y);
}
float4 over(float4 dst, float4 src) {
    float a = src.a / 255.0f, b = dst.a / 255.0f * (1.0f-a), total = a+b;
    return total == 0.0f ? 0.0f : float4((src.rgb*a+dst.rgb*b)/total,total*255.0f);
}
[numthreads(8,8,1)]
void cs_main(uint3 id : SV_DispatchThreadID) {
    uint width = word(KSI_WIDTH), height = word(KSI_HEIGHT);
    if (id.x >= width || id.y >= height) return;
    float2 at = (float2)id.xy + 0.5f;
    float2 view = float2(number(KSI_VIEW_X), number(KSI_VIEW_Y));
    float scale = number(KSI_SCALE);
    uint atlas = word(KSI_ATLAS), depth = 0u;
    float2 ratio = float2(hd.Load(atlas*16u+4u),hd.Load(atlas*16u+8u)) /
                   float2(word(KSI_LOGICAL_W),word(KSI_LOGICAL_H));
    float4 out_colour = 0.0f;
    for (uint q = 0u; q < word(KSI_COUNT); ++q) {
        uint base = KSI_HEADER + q*KSI_QUAD_WORDS;
        // Keep shared mask edges identical to the CPU's separate float operations.
        precise float2 p0 = asfloat(commands.Load2(base*4u))*scale+view;
        precise float2 a = (asfloat(commands.Load2((base+5u)*4u))*scale+view)-p0;
        precise float2 b = (asfloat(commands.Load2((base+10u)*4u))*scale+view)-p0;
        precise float det = a.x*b.y-a.y*b.x;
        if (abs(det) < 1e-8f) continue;
        precise float2 delta = at-p0;
        precise float s = (delta.x*b.y-delta.y*b.x)/det, t = (a.x*delta.y-a.y*delta.x)/det;
        if (s < 0.0f || s >= 1.0f || t < 0.0f || t >= 1.0f) continue;
        uint flags = word(base+KSI_FLAGS), z = word(base+KSI_DEPTH);
        if ((flags & 1u) != 0u && z < depth) continue;
        if ((flags & 2u) != 0u) depth = z;
        if ((flags & 4u) == 0u) continue;
        float4 c0=unpack(word(base+4u)), c1=unpack(word(base+9u));
        float4 c2=unpack(word(base+14u)), c3=unpack(word(base+19u));
        float4 tint = s+t <= 1.0f ? c0+(c1-c0)*s+(c2-c0)*t : c3+(c2-c3)*(1.0f-s)+(c1-c3)*(1.0f-t);
        float4 src = 255.0f;
        if ((flags & 8u) != 0u) {
            float2 uv0=asfloat(commands.Load2((base+2u)*4u));
            float2 uv1=asfloat(commands.Load2((base+7u)*4u)), uv2=asfloat(commands.Load2((base+12u)*4u));
            src = sample_at(atlas,(uv0+(uv1-uv0)*s+(uv2-uv0)*t)*ratio,(flags&16u)!=0u);
        }
        src = clamp(src*tint/128.0f,0.0f,255.0f);
        uint blend = word(base+KSI_BLEND);
        float4 result = over(out_colour,src);
        if (blend == 1u) result.rgb = out_colour.rgb+src.rgb*src.a/255.0f;
        else if (blend == 2u) result.rgb = out_colour.rgb-src.rgb*src.a/255.0f;
        out_colour = unpack(pack(result));
    }
    float2 face = at-float2(number(KSI_FACE_X),number(KSI_FACE_Y));
    if (all(face >= 0.0f) && face.x < number(KSI_FACE_W) && face.y < number(KSI_FACE_H)) {
        out_colour = over(out_colour,texel(word(KSI_FACE),(int2)floor(face)));
        if (word(KSI_HD_FACE) != 0xffffffffu)
            out_colour = over(out_colour,texel(word(KSI_HD_FACE),(int2)floor(face)));
    }
    uint first = hd.Load(word(KSI_OUTPUT)*16u);
    hd.Store((first+id.y*width+id.x)*4u,pack(out_colour));
}
