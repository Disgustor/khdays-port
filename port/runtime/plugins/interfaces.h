/* The port's interfaces for native plugins (kh.services, kh.events, kh.hooks, kh.engine, kh.actors, kh.characters:
 * sdk/include/kh/), inside the plugin manager (plugins.c). */
#ifndef KHDAYS_PLUGINS_INTERFACES_H
#define KHDAYS_PLUGINS_INTERFACES_H

#include <kh/kh_actors.h>
#include <kh/kh_mod.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The interface `name` at `version` for `plugin`: the port's own, else a plugin's published service; NULL none. */
const void *khdays_plugins_interface(KhPlugin *plugin, const char *name, uint32_t version);

/* Everything `plugin` registered (services, subscriptions, hooks) dropped: at its stop, or a start that failed. */
void khdays_plugins_forget(KhPlugin *plugin);

/* For the interfaces: a line in the plugin's mod's log, and its mod's id. */
void khdays_plugins_tell(KhPlugin *plugin, const char *text);
const char *khdays_plugins_mod_id(const KhPlugin *plugin);

/* The game's thread: the one that started the plugins. */
void khdays_plugins_mark_game_thread(void);
int khdays_plugins_on_game_thread(void);

/* kh.engine (engine.c). */
const void *khdays_plugins_engine(void);

/* kh.actors (actors.c): its table; seat `seat`'s player's handle; once a frame, the enemies no longer standing ended.
 */
const void *khdays_plugins_actors(void);
KhActor khdays_actors_player(int seat);
void khdays_actors_frame(void);

/* kh.characters (characters.c): its table; once a frame, the seats' actors looked at and their listeners called;
 * a plugin's listeners dropped. */
const void *khdays_plugins_characters(void);
void khdays_plugins_characters_frame(void);
void khdays_plugins_characters_forget(KhPlugin *plugin);

#ifdef __cplusplus
}
#endif

#endif
