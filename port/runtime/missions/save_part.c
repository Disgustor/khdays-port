/* The new missions' progress beside a save file (save_part.h). */
#include "save_part.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void khdays_save_part_free(KhdaysSavePart *kept)
{
    for (int i = 0; i < kept->kept_count; ++i) {
        free(kept->kept[i]);
    }
    free(kept->kept);
    memset(kept, 0, sizeof(*kept));
}

static void keep(KhdaysSavePart *kept, const char *line, size_t n)
{
    if (kept->kept_count == kept->kept_capacity) {
        const int grown = kept->kept_capacity == 0 ? 8 : kept->kept_capacity * 2;
        char **lines = realloc(kept->kept, (size_t)grown * sizeof(*lines));
        if (lines == NULL) {
            return;
        }
        kept->kept = lines;
        kept->kept_capacity = grown;
    }
    char *copy = malloc(n + 1);
    if (copy != NULL) {
        memcpy(copy, line, n);
        copy[n] = '\0';
        kept->kept[kept->kept_count++] = copy;
    }
}

void khdays_save_part_read(const char *text, size_t size, const KhdaysMissions *r, KhdaysFieldStore *s,
                           KhdaysSavePart *kept)
{
    khdays_field_store_clear(s);
    khdays_save_part_free(kept);
    for (size_t at = 0; at < size;) {
        const char *line = text + at;
        const char *eol = memchr(line, '\n', size - at);
        size_t n = eol != NULL ? (size_t)(eol - line) : size - at;
        at += n + (eol != NULL);
        while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == ' ')) {
            --n;
        }
        if (n == 0 || line[0] == '#') {
            continue;
        }
        char identity[512];
        const size_t id_length = strcspn(line, " ");
        if (id_length >= n || id_length >= sizeof(identity)) {
            continue; /* not a line this reads: dropped */
        }
        memcpy(identity, line, id_length);
        identity[id_length] = '\0';
        const int i = khdays_missions_index(r, identity);
        if (i < 0 || i >= s->count || r->list[i].number == 0) {
            keep(kept, line, n);
            continue;
        }
        char values[128];
        const size_t v = n - id_length < sizeof(values) ? n - id_length : sizeof(values) - 1;
        memcpy(values, line + id_length, v);
        values[v] = '\0';
        unsigned status, stage, transfer, badges;
        int best;
        if (sscanf(values, "%u %u %u %u %d", &status, &stage, &transfer, &badges, &best) == 5) {
            KhdaysMissionProgress *p = &s->progress[i];
            p->value[KHDAYS_RECORD_STATUS] = status & 7u;
            p->value[KHDAYS_RECORD_STAGE] = stage & 7u;
            p->value[KHDAYS_RECORD_TRANSFER] = transfer & 7u;
            p->value[KHDAYS_RECORD_BADGES] = badges & 15u;
            p->best = best;
        }
    }
}

static int has_progress(const KhdaysMissionProgress *p)
{
    for (int k = 0; k < KHDAYS_RECORDS; ++k) {
        if (p->value[k] != 0) {
            return 1;
        }
    }
    return p->best != -1;
}

char *khdays_save_part_text(const KhdaysMissions *r, const KhdaysFieldStore *s, const KhdaysSavePart *kept,
                            size_t *size)
{
    static const char head[] = "# khdays: the progress of the mods' missions for this save file\n";
    size_t room = sizeof(head);
    for (int i = 0; i < r->count && i < s->count; ++i) {
        room += strlen(r->list[i].identity) + 64;
    }
    for (int i = 0; i < kept->kept_count; ++i) {
        room += strlen(kept->kept[i]) + 1;
    }
    char *text = room >= sizeof(head) ? malloc(room) : NULL, *o = text;
    if (text == NULL) {
        return NULL;
    }
    memcpy(o, head, sizeof(head));
    o += sizeof(head) - 1;
    for (int i = 0; i < r->count && i < s->count; ++i) {
        const KhdaysMissionProgress *p = &s->progress[i];
        if (r->list[i].number == 0 || !has_progress(p)) {
            continue;
        }
        o += snprintf(o, room - (size_t)(o - text), "%s %u %u %u %u %d\n", r->list[i].identity,
                      p->value[KHDAYS_RECORD_STATUS], p->value[KHDAYS_RECORD_STAGE], p->value[KHDAYS_RECORD_TRANSFER],
                      p->value[KHDAYS_RECORD_BADGES], p->best);
    }
    for (int i = 0; i < kept->kept_count; ++i) {
        o += snprintf(o, room - (size_t)(o - text), "%s\n", kept->kept[i]);
    }
    *size = (size_t)(o - text);
    return text;
}
