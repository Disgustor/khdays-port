// Ordered native sequence quads and depth masks into an existing UI image on the GPU.
ByteAddressBuffer commands : register(t0, space0);
RWByteAddressBuffer hd : register(u0, space1);
uint seq_word(uint program, uint at) { return commands.Load(at*4u); }
uint seq_hd_word(uint at) { return hd.Load(at); }
#include "kh2_sequence_sample.hlsli"
uint word(uint at) { return seq_word(0u,at); }
float number(uint at) { return asfloat(word(at)); }
float4 text_over(float4 colour, float2 at) {
    float2 face=at-float2(number(KSI_FACE_X),number(KSI_FACE_Y));
    bool text=word(KSI_TEXT_INSERT)!=0u;
    float scale=text?number(KSI_TEXT_SCALE):1.0f;
    if (text) {
        float2 clip=at-float2(number(KSI_TEXT_CLIP_X),number(KSI_TEXT_CLIP_Y));
        if (any(clip<0.0f) || clip.x>=number(KSI_TEXT_CLIP_W) || clip.y>=number(KSI_TEXT_CLIP_H)) return colour;
    }
    if (all(face>=0.0f) && face.x<number(KSI_FACE_W) && face.y<number(KSI_FACE_H)) {
        float4 src=seq_texel(word(KSI_FACE),(int2)floor(face/scale));
        if(text) src.a*=number(KSI_TEXT_ALPHA);
        colour=seq_over(colour,src);
        if (word(KSI_HD_FACE)!=0xffffffffu)
            colour=seq_over(colour,seq_texel(word(KSI_HD_FACE),(int2)floor(face/scale)));
    }
    return colour;
}
[numthreads(8,8,1)]
void cs_main(uint3 id : SV_DispatchThreadID) {
    uint width=word(KSI_WIDTH), height=word(KSI_HEIGHT);
    if (id.x>=width || id.y>=height) return;
    float2 at=(float2)id.xy+0.5f;
    uint depth=0u;
    float4 out_colour=0.0f;
    uint insert=word(KSI_TEXT_INSERT);
    for (uint q=0u; q<=word(KSI_COUNT); ++q) {
        if(insert==q+1u) out_colour=text_over(out_colour,at);
        if(q==word(KSI_COUNT)) break;
        float4 src; uint blend;
        if (seq_sample(0u,q,at,depth,src,blend)) out_colour=seq_blend(out_colour,src,blend);
    }
    if(insert==0u) out_colour=text_over(out_colour,at);
    uint first=hd.Load(word(KSI_OUTPUT)*16u);
    hd.Store((first+id.y*width+id.x)*4u,seq_pack(out_colour));
}
