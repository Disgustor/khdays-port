/* The mods' characters in the running game: read from each mod's characters/ at the start (registry.h), given
 * numbers past the game's, their data folders ba/ch/<code>/ served (the mod's files, else copies of their base's),
 * and answered where the game's joins ask (port/tools/prepare.py): a new number is its base's where the game picks
 * behaviour or one of its tables, its own where it opens data. With no such mod nothing of the game changes.
 * docs/RUNTIME_MAP.md, "Characters of mods". */
#ifndef KHDAYS_CHARACTERS_H
#define KHDAYS_CHARACTERS_H

#include "registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* After the ROM is assembled, before the game starts: the characters read, checked, numbered, their folders
 * added. */
void khdays_characters_start(void);

/* Once the session log is open: what the start found, told there and in the mods' log. */
void khdays_characters_report(void);

/* The game's joins: whether `number` (a place's character in the select) is a new character's; the new
 * character of seat `slot` (0-3) when it has one and `kind` is its base (or below 0), else `kind`; the game
 * character a number plays as (a game number is its own). */
int khdays_characters_known(int number);
int khdays_characters_of(int slot, int kind);
int khdays_characters_base(int number);

/* The select wrote seat `seat` from the place on character `place` (-1: emptied): a new character's seat
 * gets its base's number in the game, its own kept here. */
void khdays_characters_seat(int seat, int place);

/* The camp's place made from seat `seat` (its select's own number `entry`, from the seat's base): the
 * seat's new character's number when it has one, else `entry`. */
int khdays_characters_place(int seat, int entry);

/* The game's scene now `scene` (at each change): the title forgets the seats (the story never has them). */
void khdays_characters_scene(int scene);

/* The select's place (its own numbers, 0-18) a character is shown as by the game's select screens: its base's
 * for a new one, else `id` itself. */
int khdays_characters_menu_entry(int id);

/* The numbered characters, in their numbers' order (the select's). */
int khdays_characters_count(void);
const KhdaysCharacter *khdays_characters_at(int index);
const KhdaysCharacter *khdays_characters_numbered(int number);

/* A file of the character's folder (`relative`: its description's reference) on disk, UTF-8, into `path`. */
int khdays_characters_file(const KhdaysCharacter *c, const char *relative, char *path, int size);

/* For the models (runtime/mods/models.cpp): the character whose own folder holds the game model `name`, NULL
 * for any other. */
const KhdaysCharacter *khdays_characters_of_model(const char *name);

/* The game starts the sounds of seat `seat`'s actor, which plays as `kind` (Ov022_PlayEntityVoice's join), until
 * it is called again with -1. */
void khdays_characters_speaker(int seat, int kind);

/* For the sounds (runtime/sound/sound_mods.c): what the new character whose actor starts the game's sound `game`
 * now makes of it -- 0 nothing (the game's plays, or a sound mod's), 1 its own file (`path`, `name` what the
 * traces call it), -1 silence. */
int khdays_characters_speaker_sound(const char *game, char *path, int size, char *name, int name_size);

/* For the sounds: whether any character has [sounds]; its files one by one (`index` from 0, 0 past the last; an
 * empty `path` for one that cannot be named), as khdays_characters_speaker_sound gives them. */
int khdays_characters_have_sounds(void);
int khdays_characters_sound_file(int index, char *path, int size, char *name, int name_size);

/* For the models (runtime/mods/models.cpp): the model file drawn for the game model `name` ("ba/ch/c16/def.p.z/
 * mdl/roxas_b"), a model of a character's own folder, into `path`; 0 for any other. */
int khdays_characters_model(const char *name, char *path, int size);

#ifdef __cplusplus
}
#endif

#endif
