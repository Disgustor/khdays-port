/* The mods' new missions in the running game: read from each mod's missions/ at the start (registry.h),
 * their files added to the ROM, their rows put in the game's tables as it loads them (tables.h), their
 * progress answered where the game keeps a mission's (field_store.h) and kept beside each save file
 * (save_part.h). With no such mod nothing of the game changes. docs/RUNTIME_MAP.md, "Missions and worlds of
 * mods". */
#ifndef KHDAYS_MISSIONS_H
#define KHDAYS_MISSIONS_H

/* After the ROM is assembled, before the game starts: the mods' missions read, numbered, their files added. */
void khdays_missions_start(void);

/* Once the session log is open: what the start found, told there and in the mods' log. */
void khdays_missions_report(void);

/* The save list: file `file` (0 on) was loaded; KHDAYS_PLAYED_NONE / _GUEST (savelist.h) a game without one. */
void khdays_missions_loaded(int file);

/* A save action wrote file `file` (0 on): its part written beside it (removed when the file was deleted). */
void khdays_missions_saved(int file);

/* The game's joins (port/tools/prepare.py): a field or flag of a new mission (else -1 / 0: the game's own),
 * the best result of a mission past the game's 200 records, a file as it was loaded. */
long long khdays_missions_field_get(unsigned int field, unsigned int bits);
int khdays_missions_field_set(unsigned int field, unsigned int bits, unsigned int value);
int khdays_missions_flag_get(unsigned int flag);
int khdays_missions_flag_set(unsigned int flag, int on);
int khdays_missions_best(int number);
void khdays_missions_set_best(int number, int value);
/* Mission Mode's list: whether the row `record` of a new mission is shown for the list's `flags` (its category
 * << 8, the kinds it shows); -1 for the game's rows (the game's own test). */
int khdays_missions_eligible(const void *record, unsigned int flags);
void *khdays_missions_file_loaded(unsigned top, void *buffer, unsigned size, void *heap);

#endif
