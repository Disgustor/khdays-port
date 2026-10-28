/* The mods' characters in the running game (characters.h). */
#include "characters.h"

#include "../../host/app/instance.h"
#include "../core/rom.h"
#include "../core/session_log.h"
#include "../core/window.h"
#include "../scripts/folders.h"
#include "../scripts/manifest.h"
#include "../scripts/mods_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MODS "mods"

/* The game's four seats (data_020429c8, Slot4_GetIfOccupied), each a member (gPartyMembers[seat]). */
#define SEATS 4
#define TITLE_SCENE 1

static KhdaysCharacters registry;
static const KhdaysCharacter **numbered; /* by number - KHDAYS_CHARACTER_FIRST */
static int numbered_count;
static int tracing;
/* The new character each seat was given by the select (its number), 0 none: the seat itself has its
 * base's. */
static int seats[SEATS];

/* What the start found, told once the logs are open. */
typedef struct Note {
    char *mod, *key, *text;
    int line;
} Note;
static Note *notes;
static int note_count, note_capacity;

static char *copy(const char *s)
{
    const size_t n = strlen(s) + 1;
    char *c = malloc(n);
    if (c != NULL) {
        memcpy(c, s, n);
    }
    return c;
}

static void note(const char *mod, const char *key, int line, const char *why, void *context)
{
    (void)context;
    if (note_count == note_capacity) {
        const int grown = note_capacity == 0 ? 8 : note_capacity * 2;
        Note *n = realloc(notes, (size_t)grown * sizeof(*n));
        if (n == NULL) {
            return;
        }
        notes = n;
        note_capacity = grown;
    }
    notes[note_count].mod = copy(mod);
    notes[note_count].key = copy(key);
    notes[note_count].text = copy(why);
    notes[note_count].line = line;
    if (notes[note_count].mod != NULL && notes[note_count].key != NULL && notes[note_count].text != NULL) {
        ++note_count;
    }
}

static char *joined(const char *a, const char *b)
{
    const size_t n = strlen(a) + 1 + strlen(b) + 1;
    char *s = malloc(n);
    if (s != NULL) {
        snprintf(s, n, "%s/%s", a, b);
    }
    return s;
}

/* A mod's id: its mod.ini's, else its folder's name. */
static char *mod_id(const char *folder, const char *name)
{
    char *ini = joined(folder, "mod.ini"), *text = ini != NULL ? khdays_folders_read(ini, NULL) : NULL, *id = NULL;
    free(ini);
    if (text != NULL) {
        KhdaysManifest m;
        char reason[200];
        memset(&m, 0, sizeof(m));
        if (khdays_manifest_parse(text, strlen(text), &m, reason, sizeof(reason)) && m.id != NULL) {
            id = copy(m.id);
        }
        khdays_manifest_free(&m);
        free(text);
    }
    return id != NULL ? id : copy(name);
}

typedef struct ModReading {
    const char *id, *name, *folder;
} ModReading;

static void read_character(const char *key, void *context)
{
    const ModReading *mod = context;
    char path[1024];
    size_t size = 0;
    snprintf(path, sizeof(path), "%s/characters/%s/character.ini", mod->folder, key);
    char *text = khdays_folders_read(path, &size);
    if (text == NULL) {
        note(mod->id, key, 0, "characters/<key>/character.ini cannot be read", NULL);
        return;
    }
    khdays_characters_read(&registry, mod->id, mod->name, key, text, size, note, NULL);
    free(text);
}

static void read_mod(const char *name, void *context)
{
    (void)context;
    char *folder = joined(MODS, name), *characters = folder != NULL ? joined(folder, "characters") : NULL;
    if (characters != NULL) {
        char *id = mod_id(folder, name);
        if (id != NULL) {
            ModReading mod = {id, name, folder};
            khdays_folders_list(characters, read_character, &mod);
        }
        free(id);
    }
    free(characters);
    free(folder);
}

int khdays_characters_file(const KhdaysCharacter *c, const char *relative, char *path, int size)
{
    const int n =
        snprintf(path, (size_t)size, "%s/%s/characters/%s/%s", MODS, registry.mods[c->mod].folder, c->key, relative);
    return n > 0 && n < size;
}

/* Whether the files the description names are there; told when one is not. */
static int files_there(const KhdaysCharacter *c)
{
    const char *const named[] = {c->model, c->face};
    char path[1024], why[300];
    for (int i = 0; i < 2; ++i) {
        if (named[i] != NULL &&
            !(khdays_characters_file(c, named[i], path, sizeof(path)) && khdays_folders_is_file(path))) {
            snprintf(why, sizeof(why), "%.200s cannot be read: left out", named[i]);
            note(registry.mods[c->mod].id, c->key, 0, why, NULL);
            return 0;
        }
    }
    /* a clip that cannot be read: its animation plays the fallback (told) */
    for (int i = 0; i < c->action_count; ++i) {
        if (!(khdays_characters_file(c, c->actions[i].clip, path, sizeof(path)) && khdays_folders_is_file(path))) {
            snprintf(why, sizeof(why), "%.200s (for %.60s) cannot be read: the fallback plays", c->actions[i].clip,
                     c->actions[i].game);
            note(registry.mods[c->mod].id, c->key, 0, why, NULL);
        }
    }
    if (c->own_skeleton && c->fallback == NULL) {
        note(registry.mods[c->mod].id, c->key, 0,
             "skeleton = own without [actions] fallback: an animation with no clip shows its rest pose", NULL);
    }
    return 1;
}

/* Its folder ba/ch/<code>/: every file of its base's folder, the mod's data/<file> when there, else a copy of
 * the base's bytes (its own place in the ROM: what is loaded from it is known as the character's). */
static int serve_folder(const KhdaysCharacter *c)
{
    char base[24], own[24], path[256], target[256], disk[1024];
    snprintf(base, sizeof(base), "ba/ch/%s/", khdays_character_base_code(c->base));
    snprintf(own, sizeof(own), "ba/ch/c%02x/", c->number);
    const size_t base_length = strlen(base);
    int served = 0;
    for (int id = 0; id < khdays_rom_file_count(); ++id) {
        u32 offset, bytes;
        if (!khdays_rom_file_info(id, path, (int)sizeof(path), &offset, &bytes) ||
            strncmp(path, base, base_length) != 0) {
            continue;
        }
        const char *rest = path + base_length;
        char relative[300];
        snprintf(target, sizeof(target), "%s%s", own, rest);
        snprintf(relative, sizeof(relative), "data/%s", rest);
        if (khdays_characters_file(c, relative, disk, sizeof(disk)) && khdays_folders_is_file(disk)) {
            served += khdays_rom_add_file(target, disk);
            continue;
        }
        unsigned char *data = malloc(bytes > 0 ? bytes : 1);
        if (data == NULL) {
            continue;
        }
        khdays_rom_read(offset, data, bytes);
        served += khdays_rom_add_bytes(target, data, bytes);
        free(data);
    }
    return served;
}

void khdays_characters_start(void)
{
    tracing = getenv("KHDAYS_TRACE_CHARACTERS") != NULL;
    khdays_folders_list(MODS, read_mod, NULL);
    if (registry.count == 0) {
        return;
    }
    for (int i = registry.count - 1; i >= 0; --i) {
        if (!files_there(&registry.list[i])) {
            khdays_characters_drop(&registry, i); /* numbering is over the rest */
        }
    }
    khdays_characters_number(&registry, note, NULL);
    numbered_count = 0;
    for (int i = 0; i < registry.count; ++i) {
        if (registry.list[i].number > 0 && registry.list[i].number - KHDAYS_CHARACTER_FIRST + 1 > numbered_count) {
            numbered_count = registry.list[i].number - KHDAYS_CHARACTER_FIRST + 1;
        }
    }
    numbered = numbered_count > 0 ? calloc((size_t)numbered_count, sizeof(*numbered)) : NULL;
    for (int i = 0; i < registry.count && numbered != NULL; ++i) {
        KhdaysCharacter *c = &registry.list[i];
        if (c->number == 0) {
            continue;
        }
        if (serve_folder(c) == 0) {
            note(registry.mods[c->mod].id, c->key, 0, "its base's folder cannot be copied: left out", NULL);
            c->number = 0;
            continue;
        }
        numbered[c->number - KHDAYS_CHARACTER_FIRST] = c;
    }
}

void khdays_characters_report(void)
{
    if (registry.count == 0 && note_count == 0) {
        return;
    }
    khdays_mods_log_open(khdays_host_user_folder());
    for (int i = 0; i < note_count; ++i) {
        char file[300];
        snprintf(file, sizeof(file), "characters/%s/character.ini", notes[i].key);
        khdays_mods_log(notes[i].mod, "characters", file, notes[i].line, notes[i].text);
        khdays_log("characters: %s/%s: %s%s%d", notes[i].mod, notes[i].key, notes[i].text,
                   notes[i].line > 0 ? ", line " : "", notes[i].line);
        free(notes[i].mod);
        free(notes[i].key);
        free(notes[i].text);
    }
    note_count = 0;
    for (int n = 0; n < numbered_count; ++n) {
        const KhdaysCharacter *c = numbered[n];
        if (c != NULL) {
            khdays_log("characters: %s, number 0x%02x, plays as %s, its folder ba/ch/c%02x/", c->identity, c->number,
                       khdays_character_base_name(c->base), c->number);
        }
    }
}

const KhdaysCharacter *khdays_characters_numbered(int number)
{
    const int index = number - KHDAYS_CHARACTER_FIRST;
    return index >= 0 && index < numbered_count ? numbered[index] : NULL;
}

int khdays_characters_known(int number)
{
    return khdays_characters_numbered(number) != NULL;
}

int khdays_characters_base(int number)
{
    const KhdaysCharacter *c = khdays_characters_numbered(number);
    return c != NULL ? c->base : number;
}

void khdays_characters_seat(int seat, int place)
{
    if (seat < 0 || seat >= SEATS) {
        return;
    }
    const KhdaysCharacter *c = place >= 0 ? khdays_characters_numbered(place & 0xff) : NULL;
    const int number = c != NULL ? c->number : 0;
    if (tracing && number != seats[seat] && (number != 0 || place >= 0)) {
        khdays_log("characters: seat %d is %s", seat, c != NULL ? c->identity : "one of the game's");
    }
    seats[seat] = number;
}

int khdays_characters_place(int seat, int entry)
{
    const int n = khdays_characters_of(seat, -1);
    return n >= 0 && khdays_characters_menu_entry(n) == entry ? n : entry;
}

void khdays_characters_scene(int scene)
{
    if (scene == TITLE_SCENE) {
        memset(seats, 0, sizeof(seats)); /* Mission Mode left: the story never has them */
    }
}

int khdays_characters_of(int slot, int kind)
{
    if (slot < 0 || slot >= SEATS || seats[slot] == 0) {
        return kind;
    }
    const KhdaysCharacter *c = khdays_characters_numbered(seats[slot]);
    if (c == NULL || (kind >= 0 && kind != c->base)) {
        return kind; /* not the seat's character's actor */
    }
    if (tracing) {
        static int told[SEATS];
        if (told[slot] != c->number) {
            told[slot] = c->number;
            khdays_log("characters: member %d is %s (0x%02x), on %s's code", slot, c->identity, c->number,
                       khdays_character_base_name(c->base));
        }
    }
    return c->number;
}

/* The select's place of each game character by its data folder's number (Ov006_MenuEntryToSlot's inverse). */
static const int menu_of_base[KHDAYS_CHARACTER_BASES] = {12, 7,  1, 6, 2,  15, 8,  11, 4, 9,
                                                         10, 16, 3, 0, 13, 5,  14, 17, 18};

int khdays_characters_menu_entry(int id)
{
    const KhdaysCharacter *c = khdays_characters_numbered(id & 0xff);
    return c != NULL ? menu_of_base[c->base] : id;
}

int khdays_characters_count(void)
{
    int n = 0;
    for (int i = 0; i < numbered_count; ++i) {
        n += numbered[i] != NULL;
    }
    return n;
}

const KhdaysCharacter *khdays_characters_at(int index)
{
    for (int i = 0; i < numbered_count; ++i) {
        if (numbered[i] != NULL && index-- == 0) {
            return numbered[i];
        }
    }
    return NULL;
}

const KhdaysCharacter *khdays_characters_of_model(const char *name)
{
    /* the body's packs (def.p.z, def_hb.p.z, def_hho.p.z: the overlays' constructors load one of them) */
    static const char head[] = "ba/ch/c";
    const size_t n = sizeof(head) - 1;
    if (numbered_count == 0 || strncmp(name, head, n) != 0 || strncmp(name + n + 2, "/def", 4) != 0) {
        return NULL;
    }
    char digits[3] = {name[n], name[n + 1], '\0'};
    char *end;
    const long number = strtol(digits, &end, 16);
    return *end == '\0' ? khdays_characters_numbered((int)number) : NULL;
}

int khdays_characters_model(const char *name, char *path, int size)
{
    const KhdaysCharacter *c = khdays_characters_of_model(name);
    return c != NULL && c->model != NULL && khdays_characters_file(c, c->model, path, size);
}
