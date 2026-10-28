/* The mods' characters (runtime/characters/): descriptions read and refused, their identities and numbers --
 * on made-up descriptions only. */
#include "../runtime/characters/registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond);                                                 \
            ++failures;                                                                                                \
        }                                                                                                              \
    } while (0)

/* The reports a reading gave: their lines and the last reason. */
typedef struct Reports {
    int count, lines[16];
    char last[300];
} Reports;

static void reported(const char *mod, const char *key, int line, const char *why, void *context)
{
    Reports *r = context;
    (void)mod;
    (void)key;
    if (r->count < 16) {
        r->lines[r->count] = line;
    }
    ++r->count;
    snprintf(r->last, sizeof(r->last), "%s", why);
}

static int read_text(KhdaysCharacters *r, const char *mod, const char *key, const char *text, Reports *reports)
{
    return khdays_characters_read(r, mod, mod, key, text, strlen(text), reported, reports);
}

static const char block[] = "\xef\xbb\xbf# a made-up character\n"
                            "[character]\n"
                            "format = 1\n"
                            "key = block\n"
                            "base = Roxas          # plays as him\n"
                            "name = Block\n"
                            "name es = Bloque\n"
                            "weapon = Stick\n"
                            "face = face.png\n"
                            "locked = base\n"
                            "gauges = 1 2 3 4 5 6 7 8 9\n"
                            "\n"
                            "[model]\n"
                            "file = model/block.glb\n"
                            "skeleton = paired\n";

static void a_description_read(void)
{
    KhdaysCharacters r;
    Reports reports;
    memset(&r, 0, sizeof(r));
    memset(&reports, 0, sizeof(reports));
    CHECK(read_text(&r, "test.characters", "block", block, &reports) == 1);
    CHECK(reports.count == 0);
    CHECK(r.count == 1);
    const KhdaysCharacter *c = &r.list[0];
    CHECK(strcmp(c->identity, "test.characters/block") == 0);
    CHECK(strcmp(c->key, "block") == 0);
    CHECK(c->base == 0);
    CHECK(strcmp(khdays_characters_name(c, 0), "Block") == 0);  /* en: the one without a language */
    CHECK(strcmp(khdays_characters_name(c, 4), "Bloque") == 0); /* es */
    CHECK(strcmp(khdays_characters_weapon(c, 2), "Stick") == 0);
    CHECK(strcmp(c->face, "face.png") == 0);
    CHECK(strcmp(c->model, "model/block.glb") == 0);
    CHECK(c->own_skeleton == 0);
    CHECK(c->locked_with_base == 1);
    CHECK(c->has_gauges == 1 && c->gauges[0] == 1 && c->gauges[8] == 9);
    CHECK(c->number == 0);
    khdays_characters_free(&r);
}

/* Each broken description is left out with its reason and, for a bad line, the line. */
static void bad_descriptions_left_out(void)
{
    static const struct {
        const char *key, *text, *reason;
        int line;
    } cases[] = {
        {"newer", "[character]\nformat = 2\nbase = roxas\nname = A\n[model]\nfile = a.glb\n", "newer than this port",
         2},
        {"nobase", "[character]\nformat = 1\nname = A\n[model]\nfile = a.glb\n", "base missing", 0},
        {"badbase", "[character]\nformat = 1\nbase = cloud\nname = A\n[model]\nfile = a.glb\n", "not one of the game's",
         3},
        {"noname", "[character]\nformat = 1\nbase = axel\n[model]\nfile = a.glb\n", "name missing", 0},
        {"nomodel", "[character]\nformat = 1\nbase = axel\nname = A\n", "model missing", 0},
        {"noformat", "[character]\nbase = axel\nname = A\n[model]\nfile = a.glb\n", "format missing", 0},
        {"up", "[character]\nformat = 1\nbase = axel\nname = A\n[model]\nfile = ../a.glb\n", "inside the character's",
         6},
        {"root", "[character]\nformat = 1\nbase = axel\nname = A\nface = C:/a.png\n[model]\nfile = a.glb\n",
         "inside the character's", 5},
        {"gauges", "[character]\nformat = 1\nbase = axel\nname = A\ngauges = 1 2 3\n[model]\nfile = a.glb\n", "gauges",
         5},
        {"bigauge",
         "[character]\nformat = 1\nbase = axel\nname = A\ngauges = 1 2 3 4 5 6 7 8 256\n[model]\nfile = a.glb\n",
         "gauges", 5},
        {"otherkey", "[character]\nformat = 1\nkey = other\nbase = axel\nname = A\n[model]\nfile = a.glb\n",
         "folder's name", 3},
        {"noequals", "[character]\nformat = 1\nbase axel\n", "name = value", 3},
        {"before", "format = 1\n", "before any part", 1},
        {"skeleton", "[character]\nformat = 1\nbase = axel\nname = A\n[model]\nfile = a.glb\nskeleton = mine\n",
         "paired or own", 7},
        {"bad key!", "[character]\nformat = 1\nbase = axel\nname = A\n[model]\nfile = a.glb\n", "not a key", 0},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        KhdaysCharacters r;
        Reports reports;
        memset(&r, 0, sizeof(r));
        memset(&reports, 0, sizeof(reports));
        const int taken = read_text(&r, "test.characters", cases[i].key, cases[i].text, &reports);
        if (taken != 0 || reports.count != 1 || strstr(reports.last, cases[i].reason) == NULL ||
            reports.lines[0] != cases[i].line) {
            fprintf(stderr, "case %s: taken %d, %d reports, line %d, \"%s\"\n", cases[i].key, taken, reports.count,
                    reports.lines[0], reports.last);
            ++failures;
        }
        CHECK(r.count == 0);
        khdays_characters_free(&r);
    }
}

/* An unknown name or part is told, the character still taken. */
static void unknown_names_told(void)
{
    static const char text[] = "[character]\nformat = 1\nbase = goofy\nname = A\nhat = big\n[model]\nfile = a.glb\n"
                               "[sounds]\nhit = a.wav\n";
    KhdaysCharacters r;
    Reports reports;
    memset(&r, 0, sizeof(r));
    memset(&reports, 0, sizeof(reports));
    CHECK(read_text(&r, "m", "a", text, &reports) == 1);
    CHECK(reports.count == 2);
    CHECK(reports.lines[0] == 5 && reports.lines[1] == 8);
    CHECK(r.count == 1 && r.list[0].base == 18);
    khdays_characters_free(&r);
}

static void numbering(void)
{
    static const char text[] = "[character]\nformat = 1\nbase = xion\nname = A\n[model]\nfile = a.glb\n";
    KhdaysCharacters r;
    Reports reports;
    memset(&r, 0, sizeof(r));
    memset(&reports, 0, sizeof(reports));
    /* read in one order, numbered in their identities' */
    CHECK(read_text(&r, "zeta", "one", text, &reports) == 1);
    CHECK(read_text(&r, "alpha", "two", text, &reports) == 1);
    CHECK(read_text(&r, "alpha", "one", text, &reports) == 1);
    CHECK(read_text(&r, "alpha", "one", text, &reports) == 1); /* the same identity again (another folder) */
    CHECK(reports.count == 0);
    CHECK(khdays_characters_number(&r, reported, &reports) == 3);
    CHECK(reports.count == 1 && strstr(reports.last, "same identity") != NULL);
    CHECK(r.list[2].number == KHDAYS_CHARACTER_FIRST);     /* alpha/one */
    CHECK(r.list[1].number == KHDAYS_CHARACTER_FIRST + 1); /* alpha/two */
    CHECK(r.list[0].number == KHDAYS_CHARACTER_FIRST + 2); /* zeta/one */
    CHECK(r.list[3].number == 0);
    CHECK(khdays_characters_by_number(&r, KHDAYS_CHARACTER_FIRST + 1) == &r.list[1]);
    CHECK(khdays_characters_by_number(&r, 0) == NULL);
    CHECK(khdays_characters_by_number(&r, KHDAYS_CHARACTER_FIRST + 3) == NULL);
    /* taken out before numbering: the rest move up */
    khdays_characters_drop(&r, 3);
    CHECK(r.count == 3);
    khdays_characters_free(&r);
}

/* Past the byte's last number: the rest are left out, told. */
static void the_bytes_limit(void)
{
    static const char text[] = "[character]\nformat = 1\nbase = riku\nname = A\n[model]\nfile = a.glb\n";
    const int room = KHDAYS_CHARACTER_LAST - KHDAYS_CHARACTER_FIRST + 1;
    KhdaysCharacters r;
    Reports reports;
    memset(&r, 0, sizeof(r));
    memset(&reports, 0, sizeof(reports));
    for (int i = 0; i < room + 2; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "c%04d", i);
        CHECK(read_text(&r, "many", key, text, &reports) == 1);
    }
    CHECK(khdays_characters_number(&r, reported, &reports) == room);
    CHECK(reports.count == 2 && strstr(reports.last, "signed byte") != NULL);
    CHECK(r.list[room + 1].number == 0 && r.list[room - 1].number == KHDAYS_CHARACTER_LAST);
    khdays_characters_free(&r);
}

/* A skeleton of its own: its clips by the base's animations, its size, its weapon's bone. */
static void own_skeleton_read(void)
{
    static const char text[] = "[character]\nformat = 1\nbase = roxas\nname = Wind\n"
                               "[model]\nfile = model/wind.fbx\nskeleton = own\nscale = 100   # centimetres\n"
                               "weapon_bone = R_buki\nweapon_turn = 0 90 -45\n"
                               "[actions]\nfallback = clips/idle.fbx\n"
                               "b_ro_A000 = clips/idle.fbx\n"
                               "b_ro_A001 = clips/run.fbx blend 120\n"
                               "b_ro_B009 = clips/hit1.fbx root keep blend 0\n";
    KhdaysCharacters r;
    Reports reports;
    memset(&r, 0, sizeof(r));
    memset(&reports, 0, sizeof(reports));
    CHECK(read_text(&r, "m", "wind", text, &reports) == 1);
    CHECK(reports.count == 0);
    const KhdaysCharacter *c = &r.list[0];
    CHECK(c->own_skeleton == 1);
    CHECK(c->model_scale == 100.0f);
    CHECK(strcmp(c->weapon_bone, "R_buki") == 0);
    CHECK(c->has_weapon_turn == 1 && c->weapon_turn[1] == 90.0f && c->weapon_turn[2] == -45.0f);
    CHECK(strcmp(c->fallback, "clips/idle.fbx") == 0);
    CHECK(c->action_count == 3);
    const KhdaysCharacterAction *run = khdays_characters_action(c, "b_ro_A001");
    CHECK(run != NULL && strcmp(run->clip, "clips/run.fbx") == 0 && run->blend_ms == 120 && run->root_keep == 0);
    const KhdaysCharacterAction *hit = khdays_characters_action(c, "b_ro_B009");
    CHECK(hit != NULL && hit->blend_ms == 0 && hit->root_keep == 1);
    const KhdaysCharacterAction *idle = khdays_characters_action(c, "b_ro_A000");
    CHECK(idle != NULL && idle->blend_ms == KHDAYS_CHARACTER_BLEND_MS);
    CHECK(khdays_characters_action(c, "b_ro_A004") == NULL);
    khdays_characters_free(&r);

    static const struct {
        const char *text, *reason;
        int line;
    } bad[] = {
        {"[character]\nformat = 1\nbase = roxas\nname = A\n[model]\nfile = a.fbx\nscale = -2\n", "above 0", 7},
        {"[character]\nformat = 1\nbase = roxas\nname = A\n[model]\nfile = a.fbx\nweapon_turn = 1 2\n", "three angles",
         7},
        {"[character]\nformat = 1\nbase = roxas\nname = A\n[model]\nfile = a.fbx\n[actions]\nb_ro_A000 =\n",
         "clip missing", 8},
        {"[character]\nformat = 1\nbase = roxas\nname = A\n[model]\nfile = a.fbx\n[actions]\nb_ro_A000 = ../x.fbx\n",
         "inside the character's", 8},
        {"[character]\nformat = 1\nbase = roxas\nname = A\n[model]\nfile = a.fbx\n[actions]\nb_ro_A000 = x.fbx blend soon\n",
         "not milliseconds", 8},
        {"[character]\nformat = 1\nbase = roxas\nname = A\n[model]\nfile = a.fbx\n[actions]\nb_ro_A000 = x.fbx loop yes\n",
         "neither blend", 8},
        {"[character]\nformat = 1\nbase = roxas\nname = A\n[model]\nfile = a.fbx\n[actions]\nb_ro_A000 = x.fbx root\n",
         "without its value", 8},
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        KhdaysCharacters b;
        memset(&b, 0, sizeof(b));
        memset(&reports, 0, sizeof(reports));
        const int taken = read_text(&b, "m", "bad", bad[i].text, &reports);
        if (taken != 0 || reports.count != 1 || strstr(reports.last, bad[i].reason) == NULL ||
            reports.lines[0] != bad[i].line) {
            fprintf(stderr, "own case %zu: taken %d, %d reports, line %d, \"%s\"\n", i, taken, reports.count,
                    reports.lines[0], reports.last);
            ++failures;
        }
        khdays_characters_free(&b);
    }
}

static void the_bases(void)
{
    CHECK(strcmp(khdays_character_base_name(0), "roxas") == 0);
    CHECK(strcmp(khdays_character_base_code(0), "ro") == 0);
    CHECK(strcmp(khdays_character_base_code(14), "xo") == 0); /* Xion's folder */
    CHECK(strcmp(khdays_character_base_code(18), "go") == 0);
    CHECK(khdays_character_base_name(KHDAYS_CHARACTER_BASES) == NULL);
}

int main(void)
{
    a_description_read();
    bad_descriptions_left_out();
    unknown_names_told();
    numbering();
    the_bytes_limit();
    own_skeleton_read();
    the_bases();
    if (failures == 0) {
        printf("characters: all checks passed\n");
    }
    return failures != 0;
}
