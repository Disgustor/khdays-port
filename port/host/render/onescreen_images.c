/* Independent HUD images keep their screen shape's priority and dimming without inheriting its crop. */
#include "onescreen_images.h"
#include "../../runtime/display/frame_package.h"

#include <stdlib.h>
#include <string.h>

typedef struct Picture {
    KhdaysShape shape;
    int owner;
} Picture;

static Picture *pictures;
static KhdaysShape *placed;
static size_t picture_capacity, placed_capacity;

static int hud_shown(const KhdaysSituation *s)
{
    return (s->scene == KHDAYS_SIT_FIELD || s->scene == KHDAYS_SIT_FIELD_3D_BOTH || s->scene == KHDAYS_SIT_TUTORIAL) &&
           (s->states & (1u << KHDAYS_ST_HUD));
}

void khdays_onescreen_images(const KhdaysFramePackage *p, const KhdaysSituation *situation,
                             KhdaysComposition *composition)
{
    size_t maximum = 0, count = 0, out = 0;
    if (!composition->shape_count)
        return;
    for (int e = 0; e < 2; ++e)
        maximum += p->elements[e].count;
    if (!maximum)
        return;
    if (maximum > picture_capacity) {
        Picture *grown = realloc(pictures, maximum * sizeof(*pictures));
        if (!grown)
            return;
        pictures = grown;
        picture_capacity = maximum;
    }
    const size_t need = maximum + (size_t)composition->shape_count;
    if (need > placed_capacity) {
        KhdaysShape *grown = realloc(placed, need * sizeof(*placed));
        if (!grown)
            return;
        placed = grown;
        placed_capacity = need;
    }
    for (int e = 0; e < 2; ++e) {
        for (uint32_t i = 0; i < p->elements[e].count; ++i) {
            const uint32_t *r = p->elements[e].records + i * KE_WORDS;
            float x, y, w, h;
            if (!(r[KE_SLOT] & KES_PICTURE) || ((r[KE_SLOT] & KES_HUD) && !hud_shown(situation)))
                continue;
            memcpy(&x, r + KE_X, 4);
            memcpy(&y, r + KE_Y, 4);
            memcpy(&w, r + KE_W, 4);
            memcpy(&h, r + KE_H, 4);
            float anchor_x = (x + w < 256 ? x + w : 256) - 2;
            float anchor_y = (y + h < 192 ? y + h : 192) - 2;
            const unsigned *native = p->enemy_image.words;
            if (native[KSI_ANCHORED] && native[KSI_OUTPUT] == r[KE_IMAGE]) {
                memcpy(&anchor_x, native + KSI_ANCHOR_X, 4);
                memcpy(&anchor_y, native + KSI_ANCHOR_Y, 4);
            }
            const uint32_t lower = e != p->upper ? KOF_LOWER : 0;
            for (int s = 0; s < composition->shape_count; ++s) {
                const uint32_t *src = composition->shapes[s].words;
                float sx, sy, sw, sh, dx, dy, scale_x, scale_y;
                if ((src[KO_FLAGS] & (KOF_LOWER | KOF_3D | KOF_IMAGE)) != lower)
                    continue;
                memcpy(&sx, src + KO_SRC_X, 4);
                memcpy(&sy, src + KO_SRC_Y, 4);
                memcpy(&sw, src + KO_SRC_W, 4);
                memcpy(&sh, src + KO_SRC_H, 4);
                if (anchor_x < sx || anchor_x >= sx + sw || anchor_y < sy || anchor_y >= sy + sh)
                    continue;
                memcpy(&dx, src + KO_DST_X1, 4);
                memcpy(&dy, src + KO_DST_Y1, 4);
                memcpy(&scale_x, src + KO_SCALE_X, 4);
                memcpy(&scale_y, src + KO_SCALE_Y, 4);
                dx += (x - sx) * scale_x;
                dy += (y - sy) * scale_y;
                const float dw = w * scale_x, dh = h * scale_y;
                Picture *picture = &pictures[count++];
                picture->owner = s;
                uint32_t *dst = picture->shape.words;
                memset(dst, 0, KO_WORDS * sizeof(*dst));
                dst[KO_FLAGS] = KOF_IMAGE | lower | (src[KO_FLAGS] & KOF_DIM);
                dst[KO_KEYS] = r[KE_IMAGE];
                dst[KO_OPACITY] = src[KO_OPACITY];
                if (r[KE_SLOT] & KES_HUD) {
                    const unsigned layer = r[KE_SLOT] & KES_NUMBER_MASK;
                    const unsigned blend = p->desc2d[e][KD_BLEND], coef = p->desc2d[e][KD_COEF];
                    float alpha = 1;
                    /* Ov002_CommitDisplayState fades BG3 against BG0/backdrop with EVA and 16-EVA. */
                    if (((blend >> 16) & 3u) == 1u && (blend & (1u << layer)) && (blend & 0x2100u))
                        alpha = (float)((coef & 31u) > 16u ? 16u : coef & 31u) / 16.0f;
                    dst[KO_FLAGS] |= KOF_NATIVE_FADE;
                    dst[KO_IMAGE_GROUP] = 1u + (unsigned)e * 4u + layer;
                    memcpy(dst + KO_IMAGE_ALPHA, &alpha, 4);
                }
                float values[4] = {0, 0, (float)r[KE_IW], (float)r[KE_IH]};
                memcpy(dst + KO_SRC_X, values, sizeof(values));
                values[0] = dx;
                values[1] = dy;
                values[2] = dx + dw;
                values[3] = dy + dh;
                memcpy(dst + KO_DST_X1, values, sizeof(values));
                scale_x = dw / (float)r[KE_IW];
                scale_y = dh / (float)r[KE_IH];
                memcpy(dst + KO_SCALE_X, &scale_x, 4);
                memcpy(dst + KO_SCALE_Y, &scale_y, 4);
                break;
            }
        }
    }
    if (!count)
        return;
    for (int s = 0; s < composition->shape_count; ++s) {
        for (size_t i = 0; i < count; ++i)
            if (pictures[i].owner == s)
                placed[out++] = pictures[i].shape;
        placed[out++] = composition->shapes[s];
    }
    composition->shape_count = (int)out;
    composition->shapes = placed;
}
