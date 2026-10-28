# Characters of mods

A mod can add playable characters to Mission Mode: each one is offered in the character select after the game's,
chosen alone or in a room, and played in a mission with its own model, name, picture, gauges and data, on the
behaviour of one of the game's characters, its **base**. The base stays in the select as it was; a character never
replaces one of the game's. How the port does it: [RUNTIME_MAP.md](RUNTIME_MAP.md) section 30.

Nothing here needs code or scripts: a folder of files, read when the game starts.

## The folder

```text
mods/<Mod>/
  mod.ini                       the mod's (SCRIPTING.md, "A mod's folder"); kind = gameplay
  characters/<key>/             one folder a character; its name is the character's key
    character.ini               its description (below)
    face.png                    its picture in the select (any size, drawn fitted)
    model/<name>.gltf           its model: .gltf, .glb or .fbx
    model/<name>.bones.txt      its bones paired with its base's (RENDERING.md, "Mods' graphics")
    data/<file>                 optional: one of the game's files of a character's folder (ba/ch/<code>/),
                                in the game's format, used instead of its base's (lv.b.z: its levels)
```

A key is letters, digits, `_` and `-`. A character is known by its mod's id and its key (`test.character/block`):
two mods may use the same key; the same identity twice keeps the first one read.

## character.ini

```ini
# a made-up character
[character]
format = 1
base = roxas
name = Block
name es = Bloque
weapon = Stick
face = face.png
locked = no
gauges = 2 2 2 2 4 2 2 2 2

[model]
file = model/block.gltf
skeleton = paired
```

- `format` -- the description's version: 1. A newer one is refused, naming both.
- `base` -- the game's character it plays as: roxas, axel, xigbar, saix, xaldin, sora, demyx, larxene, lexaeus,
  luxord, marluxia, riku, vexen, xemnas, xion, zexion, mickey, donald, goofy.
- `name`, `weapon` -- for every language; `name en`, `name fr`, `name de`, `name it`, `name es` (and `weapon ...`)
  for one.
- `face` -- its picture in the select.
- `locked` -- `no`: always offered; `base`: offered only when its base can be chosen.
- `gauges` -- the select's nine numbers, 0-10, in the order of the game's rows (RUNTIME_MAP section 16.3): strength,
  magic, defence, critical %, HP, two the select does not show, attack speed, critical bonus; without it, its
  base's.
- `[model] file` -- its model (glTF, GLB or FBX). `skeleton = paired`: its bones follow its base's (by name, or
  as `<name>.bones.txt` pairs them), moved by its base's animations. `skeleton = own`: its own skeleton, moved by
  its own clips (below); nothing of it is paired with the base's.
- `[model] scale` -- its size to the game's: 100 for an FBX made in centimetres (the game's characters are about
  1.4 tall in its units, a model in metres 1.4).
- `[model] weapon_bone` -- its bone the weapon follows (`R_buki`); without it the weapon stays where the base's
  hand is. `weapon_turn = x y z` -- the weapon held on the bone's own axes, turned by these degrees (a weapon
  bone made for its own weapon: the grip the model was made with); without it, the weapon is held as the base
  holds it. `base_weapon_node` -- the base model's node the game puts the weapon on, when not its `*_w_tg_R`.

`#` starts a comment (at a line's start or after a space). A name or part the port does not know is told in the
mods' log with its line and left out; a character with a missing `format`, `base`, `name` or `[model] file`, a bad
value or a file that cannot be read is left out, told in the mods' log, and the game plays on.

## Its own animations (`skeleton = own`)

The character plays on its base's code, so the game asks for its base's animations by their names; each one is
given a clip of the character's own in `[actions]`:

```ini
[actions]
fallback = clips/002.fbx                    # any animation with no clip of its own
b_ro_A000 = clips/002.fbx                   # standing
b_ro_A001 = clips/004.fbx blend 120         # running
b_ro_B009 = clips/200.fbx root keep         # the combo's first hit, its own forward step kept
```

- `<the base's animation> = <clip> [blend <ms>] [root keep|remove]`: the clip (its file's first animation, its
  bones by name) plays at the same point as the game's animation, stretched to its own length, so a hit lands
  when the game's does. `blend` (default 100 ms) is how long it blends from the pose before; the root's own travel
  is removed (the game moves the character) unless `root keep`.
- The names: Roxas's standing `b_ro_A000`, running `b_ro_A001`, the ground combo `b_ro_B009`, `b_ro_B008`,
  `b_ro_B015` and `b_ro_A004` after it (seen; more as they are found). With `KHDAYS_TRACE_MODELS=1` and the session
  log on, the log names each animation as the game plays it and the clip shown; an animation with no clip is told
  once in it either way.
- Clips are drawn at every displayed frame between two of the game's (the port's 3D smoothing): the game's logic
  keeps its own pace.

## Limits

The select keeps a place's character in a signed byte: the mods' characters take the numbers 0x16-0x7f, 106 at
once (more are left out, told). The numbers follow the characters' identities, so copies with the same mods give
the same ones. The rest of the game -- and the saves -- only ever hold the base's: a save made with a mod's
character loads without the mod, its base standing in.

## Not yet

- The game's own select (two screens, without the select's layout of the HD pieces) does not list them.
- Their own movesets, Limits and voices, the studio's editor (spec 034's later phases); the field's portrait
  and the results' model are still the base's.
