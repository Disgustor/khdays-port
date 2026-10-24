/* The new missions' progress (field_store.h). */
#include "field_store.h"

#include <stdlib.h>
#include <string.h>

/* The game's progress fields of mission n, each at its block's start + its width x n (Ov005_CommitMissionResults,
 * Ov005_CommitMenuRewards, Ov008_IsEncounterEligible). */
#define STATUS_FIELDS 0x28e4u   /* 1 seen, 2 cleared, 3 complete */
#define STAGE_FIELDS 0x2a4cu    /* its crown in the list */
#define TRANSFER_FIELDS 0x2ca4u /* its rank in Mission Mode (a row whose reward kind is 8) */
#define BADGE_FIELDS 0x92bu     /* a bit for each kind of its list rows (1 << (kind - 1)): set, that row is open */

static const struct {
    unsigned start, width;
} records[KHDAYS_RECORDS] = {{STATUS_FIELDS, 3u}, {STAGE_FIELDS, 3u}, {TRANSFER_FIELDS, 3u}, {BADGE_FIELDS, 4u}};

unsigned khdays_mission_field(int number, int record, unsigned *bits)
{
    *bits = records[record].width;
    return records[record].start + records[record].width * (unsigned)number;
}

/* A placed bit: its mission's index, its record and its place in the record's bits. */
#define PLACE(index, record, bit) ((unsigned)(index) << 8 | (unsigned)(record) << 4 | (unsigned)(bit))
#define PLACE_INDEX(p) ((int)((p) >> 8))
#define PLACE_RECORD(p) ((int)((p) >> 4 & 0xfu))
#define PLACE_BIT(p) ((unsigned)((p) & 0xfu))

static unsigned slot_of(unsigned key, unsigned capacity)
{
    return (key * 2654435761u) & (capacity - 1u);
}

/* The place of bit `bit`, or 0 with *found 0. */
static unsigned find(const KhdaysFieldStore *s, unsigned bit, int *found)
{
    const unsigned key = bit + 1u;
    *found = 0;
    if (s->capacity == 0) {
        return 0;
    }
    for (unsigned i = slot_of(key, s->capacity);; i = (i + 1u) & (s->capacity - 1u)) {
        if (s->keys[i] == 0) {
            return 0;
        }
        if (s->keys[i] == key) {
            *found = 1;
            return s->places[i];
        }
    }
}

static void insert(KhdaysFieldStore *s, unsigned key, unsigned place)
{
    unsigned i = slot_of(key, s->capacity);
    while (s->keys[i] != 0) {
        i = (i + 1u) & (s->capacity - 1u);
    }
    s->keys[i] = key;
    s->places[i] = place;
    ++s->used;
}

/* Room for `more` bits, the table kept at most half full. */
static int room(KhdaysFieldStore *s, unsigned more)
{
    unsigned capacity = s->capacity == 0 ? 64u : s->capacity;
    while ((s->used + more) * 2u > capacity) {
        capacity *= 2u;
    }
    if (capacity == s->capacity) {
        return 1;
    }
    unsigned *keys = calloc(capacity, sizeof(*keys)), *places = calloc(capacity, sizeof(*places));
    if (keys == NULL || places == NULL) {
        free(keys);
        free(places);
        return 0;
    }
    unsigned *old_keys = s->keys, *old_places = s->places;
    const unsigned old_capacity = s->capacity;
    s->keys = keys;
    s->places = places;
    s->capacity = capacity;
    s->used = 0;
    for (unsigned i = 0; i < old_capacity; ++i) {
        if (old_keys[i] != 0) {
            insert(s, old_keys[i], old_places[i]);
        }
    }
    free(old_keys);
    free(old_places);
    return 1;
}

void khdays_field_store_free(KhdaysFieldStore *s)
{
    free(s->progress);
    free(s->keys);
    free(s->places);
    memset(s, 0, sizeof(*s));
}

int khdays_field_store_fits(const KhdaysFieldStore *s, int number)
{
    for (int r = 0; r < KHDAYS_RECORDS; ++r) {
        unsigned bits;
        const unsigned start = khdays_mission_field(number, r, &bits);
        if (start < KHDAYS_FLAG_BITS) {
            return 0;
        }
        for (int other = 0; other < r; ++other) { /* nor its own records meet */
            unsigned other_bits;
            const unsigned other_start = khdays_mission_field(number, other, &other_bits);
            if (start < other_start + other_bits && other_start < start + bits) {
                return 0;
            }
        }
        for (unsigned b = 0; b < bits; ++b) {
            int taken;
            (void)find(s, start + b, &taken);
            if (taken) {
                return 0;
            }
        }
    }
    return 1;
}

int khdays_field_store_place(KhdaysFieldStore *s, int index, int number)
{
    if (index < 0 || !khdays_field_store_fits(s, number) || !room(s, 16u)) {
        return 0;
    }
    if (index >= s->count) {
        const int count = index + 1;
        KhdaysMissionProgress *p = realloc(s->progress, (size_t)count * sizeof(*p));
        if (p == NULL) {
            return 0;
        }
        for (int i = s->count; i < count; ++i) {
            memset(&p[i], 0, sizeof(p[i]));
            p[i].best = -1;
        }
        s->progress = p;
        s->count = count;
    }
    for (int r = 0; r < KHDAYS_RECORDS; ++r) {
        unsigned bits;
        const unsigned start = khdays_mission_field(number, r, &bits);
        for (unsigned b = 0; b < bits; ++b) {
            insert(s, start + b + 1u, PLACE(index, r, b));
        }
    }
    return 1;
}

void khdays_field_store_clear(KhdaysFieldStore *s)
{
    for (int i = 0; i < s->count; ++i) {
        memset(&s->progress[i], 0, sizeof(s->progress[i]));
        s->progress[i].best = -1;
    }
}

/* Whose bit `bit` is: its mission's index, its record and its mask in the record's value; 0 when none's. */
static int placed(const KhdaysFieldStore *s, unsigned bit, int *index, int *record, unsigned *mask)
{
    int found;
    const unsigned p = find(s, bit, &found);
    unsigned width;
    *index = PLACE_INDEX(p);
    *record = PLACE_RECORD(p);
    if (!found || *index >= s->count || *record >= KHDAYS_RECORDS) {
        return 0;
    }
    (void)khdays_mission_field(0, *record, &width);
    *mask = 1u << (width - 1u - PLACE_BIT(p));
    return 1;
}

int khdays_field_store_get(const KhdaysFieldStore *s, unsigned field, unsigned bits, unsigned *value)
{
    unsigned v = 0;
    if (bits == 0 || bits > 32u || s->capacity == 0) {
        return 0;
    }
    for (unsigned b = 0; b < bits; ++b) {
        int index, record;
        unsigned mask;
        if (!placed(s, field + b, &index, &record, &mask)) {
            return 0;
        }
        v |= (s->progress[index].value[record] & mask ? 1u : 0u) << (bits - 1u - b);
    }
    *value = v;
    return 1;
}

int khdays_field_store_set(KhdaysFieldStore *s, unsigned field, unsigned bits, unsigned value)
{
    unsigned now;
    if (!khdays_field_store_get(s, field, bits, &now)) {
        return 0; /* (every bit a new mission's, or none written) */
    }
    for (unsigned b = 0; b < bits; ++b) {
        int index, record;
        unsigned mask;
        if (placed(s, field + b, &index, &record, &mask)) {
            unsigned *v = &s->progress[index].value[record];
            *v = (value >> (bits - 1u - b) & 1u) ? *v | mask : *v & ~mask;
        }
    }
    return 1;
}
