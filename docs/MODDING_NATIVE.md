# Mods' native plugins

A mod can carry a **native plugin**: code written in C++ (or C) and built for the player's computer, which the game
loads and runs inside itself. Plugins are for what data and Lua scripts cannot do -- new mechanics, combat, Limits,
AI, cameras -- and get the port's interfaces through the **KH Mod SDK** (`sdk/`). Everything mods did before
(content, Lua scripts, missions, characters by description) works as it did, without plugins.

## For players: turning native code on

A plugin can do anything a program on your computer can: read and write your files, crash the game, damage its
saves. The port does not isolate it. So the game loads no plugin until you turn native code on, in the settings
file's `[mods]`:

```ini
[mods]
native_code = on
```

Only turn it on for mods you trust, from people you trust. The mods' log (`saves/logs/mods.log`) names each plugin
with its mod and the author its mod declares, and says why one is not loaded. The game never downloads a plugin.

**Safe mode**: start the game with `--safe-mode` (or `KHDAYS_SAFE_MODE=1` in the environment) and no plugin is
loaded whatever the setting -- for when a plugin keeps the game from starting.

## For modders: a mod with a plugin

```text
mods/<Mod>/
  mod.ini
  native/windows-x64/<plugin>.dll
  native/linux-x64/<plugin>.so       (once the port runs on Linux)
  native/linux-arm64/<plugin>.so
  ...                                the mod's content, scripts, characters as any mod
```

`mod.ini`'s keys for a plugin, besides `id`, `name`, `version`, `author`:

| Key | |
|---|---|
| `sdk` | the SDK's ABI major the plugin was built for (`KH_ABI_MAJOR`, 1 today); the port loads only its own |
| `native.<platform>` | the plugin's library for `windows-x64`, `linux-x64` or `linux-arm64`, a path inside the mod |
| `requires` | mods (their ids, comma-separated) that must be in `mods/`; a required mod's plugin starts first |

A mod with `sdk` and no `scripts/` folder is not a Lua mod (it needs no `kind` or `api`); one with both runs its
scripts and its plugin.

### Building a plugin

The SDK builds on its own with CMake; its `cmake/KhModSdk.cmake` gives `kh_add_plugin(<name> <sources>)`:

```bash
cmake -S sdk -B build/sdk -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/sdk
```

`build/sdk/examples/HelloWorld/hello_world.dll` is the Hello World plugin: copy it into
`mods/HelloWorld/native/windows-x64/` beside `sdk/examples/HelloWorld/mod.ini`, turn native code on, start the game:
the mods' log says hello at the start and goodbye when the game closes. `sdk/docs/README.md` explains the interface.

### What a plugin may count on

- `kh_plugin_query` is called once; the descriptor's `start` once with the host table, on the game's thread; if it
  answers anything but `KH_OK`, the plugin is released without `stop`.
- `stop` is called once on the game's thread as the game closes (the window's close asks; the game's thread does it
  at its next frame, the window waiting up to a second), the last plugin started first; then its library is
  released and nothing of it is called again. A game killed (its process ended) stops nothing.
- Interfaces are asked by name and version (`find`); an unknown one answers NULL.

## The universal core

The SDK is not built around one kind of mod: its core reaches every part of the game, and a kind of mod nobody
planned for is one plugin's service that others build on. Four interfaces, each in its header under `sdk/include/kh/`:

| Interface | What it gives |
|---|---|
| `kh.services` (`kh_services.h`) | publish a table of functions under a name and version (`<mod id>.<name>`) for other plugins, which find it with `find` as they find the port's; withdrawn at the plugin's stop |
| `kh.events` (`kh_events.h`) | subscribe to the game's events by name (frame, scene, title, mission start and end, HP, items, munny, menus, saving: the Lua mods' own) and to other plugins' events; raise events of one's own (`<mod id>.<name>`, a payload of bytes) |
| `kh.hooks` (`kh_hooks.h`) | functions before and after the game's routines at its hook points, with priorities: before-functions may change the arguments or skip the routine giving its result, after-functions may change the result; two plugins that both replace a routine are told |
| `kh.engine` (`kh_engine.h`) | the game's memory (a reference to a host address and back), every routine and every datum of the game by its decompilation name, whether an overlay is loaded, the frame, the game's thread |

**The hook points** are placed where the game is built (`port/tools/prepare.py`, `HOOK_POINTS`): the routine keeps its
name as a wrapper that calls it at once while no plugin hooks it. The catalogue grows with what plugins need --
`point_count`, `point_name` and `point_signature` list it -- and a plugin that needs a point asks for it. Arguments
and results travel as 64-bit slots, the game's pointers as references. The points so far:

| Point | Subsystem | What it is |
|---|---|---|
| `Ov022_ActorSetHp(ref entity, int value)` | combat | a player's HP set: a hit, a cure, a defeat (the enemies' HP goes the next way) |
| `Ov107_AiState_ApplyHit(ref enemy, ref source, ref hit)` | combat | an enemy takes a hit; its damage is worked out inside |
| `Ov022_ComputeStrengthDamage(ref attacker, int scale)` | combat | the damage of a player's hit: its strength times the hit's factor |
| `Archive_LoadFile(ref name, uint kind)` | resources | a file of the game loaded whole |
| `PlaySound(int archive, int entry)` | audio | a sound effect: the menus' |
| `Slot_Spawn(uint handler, uint entry, ref place, uint flags)` | audio | a positioned sound: the field's, an actor's cues and voices |
| `Ov002_TickCamera()` | camera | the field's camera, once a frame |

**The engine by name** is the advanced level: a plugin can call any of the game's compiled routines (cast to its
decompilation's prototype, pointers passed as references, on the game's thread) and read or write any of its data.
It is the game's own, as the decompilation names and lays it out: a later port may change it, and the port checks
nothing a plugin does with it. An overlay's routines are always there, but its data and state only while it is
loaded (`module_loaded`).

`sdk/examples/AdvancedNativeMod` uses all four: it halves the damage the player takes through `Ov022_ActorSetHp`'s
hook, reading the actor's HP from the game's memory, logs the enemies' HP as they are hit, counts the menus' and the
field's sounds, logs scene changes, and calls `GetLanguage` and reads `gSceneCtl` by name.

## Actors and characters

Two interfaces built on the core give the field's bodies and the playable characters to plugins that would rather
not know the game's memory:

| Interface | What it gives |
|---|---|
| `kh.actors` (`kh_actors.h`) | the players (by seat) and the standing enemies as handles: their kind, character, place and HP, a player placed or given HP through the game's own routines, the actor a hook point's argument names (`of`) |
| `kh.characters` (`kh_characters.h`) | the game's 19 characters and the mods' ([CHARACTERS.md](CHARACTERS.md)), found by key, and listeners called for the actors that play one: as it enters the field, each frame, as it starts an attack, as it leaves |

**A handle names one actor for its life.** An enemy's ends when it is defeated, retired or sent away, every actor's
when its area ends or the scene changes; after that every call with it is refused, and the actor made again in its
place (the game reuses its enemies) gets a new one. Each call also checks that the game still has the actor where it
was found.

**A character's behaviour from a plugin**: a mod with a character (its `characters/<key>/`) and a plugin finds its
character with `find(plugin, "<key>")` and listens to it: the plugin is called for that character's actors alone,
and the character plays by its description whenever native code is off. `sdk/examples/CustomCharacter` is such a mod:
a made-up character (three boxes on Roxas's code) whose plugin logs each attack and gives 1 HP back for it, and,
through `Ov022_ComputeStrengthDamage`'s hook and `of`, doubles its hits' damage while everyone else's stays.

In a room every copy of the game calls its plugins for every seat: a plugin that changes the game's state should do
it on the host's copy alone (`kh.actors`' `local_seat` tells the seats apart; the host is seat 0), until plugins get
their own multiplayer interface.

## What is next

The SDK grows by parts, each tested before it is called done: more hook points across the game's subsystems;
combat and Limits from plugins (moves of their own through the game's hit routines, an enemy's behaviour); Linux;
KH Studio reading plugins' parameters; multiplayer (plugins in the rooms' fingerprint, their messages); plugins
registered at build time where a platform has no dynamic loading.
