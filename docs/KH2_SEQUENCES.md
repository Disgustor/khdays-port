# KH2 sequences

Animated player and party portrait replacements are documented in
[KH2 portraits](KH2_PORTRAITS.md).

The native KH2 implementation includes an independent sequence viewer, reference
renderer, GPU sequence images, and a mod-provided Days HP gauge replacement. It reads
local PC BAR/IMGD/SEQD resources and a PNG or supported DDS atlas. General layout
composition and LAD remain subsequent stages. Neither
a Days installation nor generated Days sources are needed to run the viewer.

## In-game HP gauge

The installer's existing **HD menus and HUD** import now makes this gauge from
the player's installed collections. `installer/setup_steps.py build-hd --menus`
reads the five `field2d/<region>/zz0field.2dd` entries from KH2's `kh2_fifth`
package using the existing archive reader. `tools/hd/kh2_gauge.py` copies the
corresponding remastered `*_zz0field_2dd1.png`, recenters clips 132–138, adds the
yellow limit clips and white divider, and writes `gauge.json` with the resulting
indices (157/158/159 for the verified PC resources). The output goes into
`mods/HD PACK/layouts/gauge/<language>/` for en/us, es/sp, fr/fr, de/gr and it/it.
Source archives and the game installation are read only; private development
extractions and backups are not inputs to the installer.

The JSON starts with `docs/examples/kh2-gauge.json`. On rebuilding an existing
pack, custom animation assignments, positions, scale and enable/disable settings
are retained. Resource filenames, entry and source origin follow the freshly
generated files. The converters and JSON template are included in the installer's
tool fingerprint, so changing them invalidates an old completed HD import. The
installer stages the necessary Python files and template with the other HD tools;
the player needs no additional Python installation or manual conversion command.

Launch the updated executable normally. An active mod providing
`layouts/gauge/<language>/gauge.json` enables the gauge automatically, with an
English fallback. JSON selection uses the existing mod priority; its archive
and texture are resolved beside that JSON so different mods' resources are not
mixed. No external KH2 directory or launch environment is required. The HUD part
must be `on`; `hud=off` or `hud=check` retains the normal Days gauge. A bad config
or asset is logged once and retains the normal gauge. Set `enabled` to false in
the JSON to disable it. `KHDAYS_KH2_HUD=0` remains a diagnostic override.

Copy [the default config](examples/kh2-gauge.json) beside `zz0field.2dd` and
`US_zz0field_2dd1.png` in the mod's language folder. Restart after editing.

| Settings | Meaning |
|---|---|
| `archive`, `texture`, `entry` | Local filenames (PNG or supported DDS), BAR entry name |
| `offset_x`, `offset_y` | Move the complete widget, including the portrait, in DS dots |
| `scale` | Gauge size in DS dots per KH2 unit; portrait keeps its existing size |
| `geometry_scale` | Intrinsic mask/extension ratio: 1 for player art, 23/45 for small party art |
| `portrait_offset_x/y` | Move the face relative to the animated center, in DS dots |
| `center/circle/bar/cap_offset_x/y` | Move each gauge element in KH2 logical units; circle masks follow the ring |
| `center_normal/alert/dead`, `circle_normal/alert/dead`, `bar_normal/alert/dead` | State-to-clip assignments |
| `center_damage_small/medium/large`, `circle_damage`, `bar_damage` | Damage-reaction assignments |
| `circle_fill`, `bar_fill`, `cap` | Current HP and endpoint clips |
| `limit_circle`, `limit_bar` | Mod-added yellow clips (157/158 in the supplied conversion); both -1 disables them |
| `limit_marker` | White limit divider clip (159 after conversion); -1 disables it |
| `limit_marker_offset_x/y` | Divider offset in KH2 logical units |
| `center_override` | -1 selects states normally; a clip index forces that center animation |
| `play_intro`, `center_intro` | Optionally play the creation clip once on entering the field |
| `center_fade` | Fade clip for the evaluator's explicit exit state; the Days adapter currently hides immediately on leaving the field |
| `canvas_padding` | Extra DS dots around the image, including room for alert effects; default 16 |
| `source_origin_x/y` | 0 for the centered mod archive; 455/355 for an untouched KH2 archive |

Player gauge settings remain at the root; party and label settings use the
objects described below. Unknown keys, duplicate keys, invalid types and
out-of-range values are errors rather than silently ignored misspellings.

The visible center quads are drawn behind the HP ring and Days portrait. Original
`GAUGE_PLAYER::create` creates clip 136; `setDamage` and the three damage wrappers
select 135/134/133, then return to the status table's 137/132/137. Clip 138's
resource track fades its alpha to zero; its original caller has not been verified
in the provided dump. It is available through the override and explicit exit
state, rather than inventing a KH2 trigger for it. The default keeps the existing
normal/status/damage selection, with the intro optional.

The Days portrait and player tag follow the center's active X/Y translation,
including sway and the configured center offset. Portrait offsets remain local
to that moving center; portrait size is unchanged.

`port/tools/kh2_center_gauge.py ARCHIVE` subtracts 455 and 355 from both X/Y
translation endpoints of only clips 132 through 138, preserving pivots, sway,
timing, curves, UVs and all other resources. It refuses shared tracks affecting
other clips, keeps an `.original.bak`, and recognizes its own converted output
on subsequent runs. This local conversion can be used by the eventual installer;
runtime playback does not rewrite assets. No private archive or backup belongs
in version control.

`port/tools/kh2_add_limit_gauge.py ARCHIVE --config GAUGE_JSON` then adds the
limit resources to that centered archive. It deep-copies sprite records 95/96
(their four group records and two referenced texture parts), writes RGBA bytes
`FF E6 00 80` into all four copied corner colors, and copies clips 128/129 with
their animation tracks redirected to the new sprite records. This is RGBA hex
`FFE60080`; its little-endian word is `0x8000E6FF`. The copied animations retain
their neutral modulation colors, so there is no runtime tint override.
Original resources are preserved. The replacement SEQD is appended and only its
BAR entry's offset/size change; other resources keep their bytes and offsets.
The tool keeps `.before-limit.bak` and updates only the JSON's two limit indices.

`port/tools/kh2_add_limit_marker.py ARCHIVE --config GAUGE_JSON` adds the divider
after the center and yellow conversions. It copies clip 123's neutral endpoint
transform, creates a separate sprite/group/texture part, and samples the bottom
logical pixel row of sprite 95's gray ring. Its radial strip is two KH2 units
thick and writes RGBA `FF FF FF 80` to all four corner colors (little-endian word
`0x80FFFFFF`). Rotation and extension placement use the same endpoint transform
as the HP cap. No texture edit or runtime tint is needed. The tool preserves all
existing records, keeps `.before-marker.bak`, and updates only `limit_marker` in
the JSON, preserving other settings. Run conversions in center/limit/marker order;
earlier tools deliberately refuse later edits instead of overwriting them.

The overlay uses `khdays_link_player_limit_hp`, which calls the original
`Ov022_ComputeChipGauge` on the local actor's block at +0x22f8, as
`Ov022_UpdateSubsystems` does. At decompilation commit
`a2b0478b87e3102732094c34f6ca89595f98ab93`, that computes a base of `0x4cd`,
increased by ability 0x52's value times `(0x800 - 0x4cd)`, reduced by the
block's use count at +0x335 times `0x19a`, multiplied by max HP, clamped to at
least 4096 and rounded up after division by 4096. Battle-mode settings can
disable it or replace it with half maximum HP. This is distinct from the
quarter-HP low-health warning predicate.

The vanilla path passes that threshold to `Ov002_SetScrollPosition(1, threshold)`;
`Ov002_DrawGaugeTweenCell` selects the alternate color for cells up to that point.
The port keeps the threshold in HP units, avoiding the DS bar's 77-cell rounding.
The new circle is clipped by both the live HP mask and its threshold mask, and
the new extension uses `min(current HP, threshold, maximum HP)`. Yellow therefore
shrinks with HP, covers all remaining HP below the threshold, and vanishes at
zero HP or when the original calculation disables it. The existing warning,
damage reaction and limit gameplay rules are not modified.

The divider follows the original Days HUD context (`data_ov002_0207f618`), rather
than an independently timed SEQD animation. `Ov002_RepaintPanelRows` selects
the latched halfword at +0x182 while +0x178 is active; otherwise it selects the
shown HP count at +0x112 while the ready flag at +0x2c is set. Both flags clear
hides it. `Ov002_DrawPromptMarker` clamps positions to 76 on the 77-cell bar.
Those rounded pixel positions must not be mapped back into HP: that offsets the
divider from exact HP and prevents it reaching a full-length KH2 gauge. The
adapter uses current HP directly while ready. While active, `game_link` reads
the local actor's block at +0x22f8: elapsed counter +0x32c, entry HP +0x336, and
outward budget +0x338. The HUD's +0x184 supplies the total budget and +0x17c the
double mode. The same native fractions interpolate from exact entry HP to max
HP, then to exact current HP on the return phase. Values clamp to their actual
endpoints. The first native update crossing the double-mode turn lands at max
HP even if that fixed-point boundary falls between updates. No independent
timer is introduced; pause, message-driven counter updates and visibility during
wind-down still follow the game. The marker follows its circle/bar offset plus
its own local offset and is
drawn after the HP layers without their depth masks, so it can travel above the
current HP; it remains behind the portrait like the rest of the gauge.

At the decompilation commit above, `func_ov022_02094678(start_hp, max_hp)` returns
`150 * (4096 - FX_Div(start_hp << 12, max_hp << 12))` in normal frame modes.
`Ov022_BeginScene` sets the battle step to 4096 (6144 in frame mode 1), and
`Ov022_StepRun` advances its timer with that step. At 30 battle updates per second,
the outward sweep is approximately `5 * (1 - start_hp / max_hp)` seconds: at 30%
HP the fixed-point budget is 430050, about 3.499756 seconds (105 updates to cross).
Equal HP fractions have equal timing regardless of gauge length. The native
alternate frame mode uses a budget multiplier of 100; the adapter does not
substitute a wall-clock duration or change those original rules.

`func_ov022_020946e4` adds another budget based on current HP when ability 0x58
selects the double mode. `Ov002_UpdateScrollThumb` moves from starting shown HP
to full over the first budget, then back to current shown HP over that additional
budget. Without double mode it only moves outward. `Ov022_IsActionOver` checks
timer budget and interruption conditions independently; state 7 can wait for a
queued message before `Ov002_ClosePrompt` hides the marker. Reaching the yellow
range is therefore not a renderer-side signal to end a limit or hide the divider.
The native timing test compiles and exercises the unchanged duration and divider
sources directly; gameplay and generated sources are not edited.

The circle is centered at 230,160 DS dots around the existing face, at 0.65 of
KH2's logical size, and the bar extends left. The existing face and player tag
are composed over it. No portrait texture is edited: the border line embedded
in that artwork remains for a later texture edit. The separately drawn HP fill,
frame and HP label are suppressed. Party replacements are configured separately
within the same JSON, as described below.

The game adapter reads the local battle actor through `game_link`, including the
game's low-HP predicate (`func_ov022_020ad7b0`, quarter HP with ability 0x42's
exception). A decrease restarts the previous-HP segment and the small reaction;
this is an explicit Days policy, not a reconstruction of KH2 hit-event severity.
Healing updates HP immediately. Maximum-HP changes reseed the gauge. The timeline
advances once per VBlank and freezes during the game's pause; the standalone
viewer's configurable clock remains the reference for further timing comparison.

The GPU renders the gauge's normal-alpha clips and masks into an existing UI
image, then places the face over it. A separate layout box preserves
the portrait's layout and widescreen anchor. On one screen the image is placed
directly from that box's composition transform, so the circle and extension can
extend outside the party's source crop. Its image allocation is bounded to a
1024-dot view; the HP-to-geometry mapping is uncapped, with offscreen geometry
clipped instead of allocating an image proportional to HP. The image includes
16 dots beyond the portrait's right edge so the circle is not cut at x 256;
its one-screen anchor stays inside the original screen and its center stays
at 230,160. Mask rotations snap at quarter turns as in `Ft4Base::calcXYZ`,
avoiding a fractional crack between the rotated and rectangular masks.

The original fill is identified from `Ov002_InitGauges`' offsets and lengths,
`Ov002_TickScene`'s queue 7 upload, and the transfer table at `data_02041934`
(queue 7 is `GX_LoadBG3Char`). The local slot is
`Ov002_GetPanelWord0220Idx(Session_GetLocalPlayerIndex())`. The static frame/label
occupy the portrait's strip from y 183 down. The challenge's OBJ path is matched
against the live gauge tiles; that path still needs an in-game challenge capture.

Validation includes synthetic adapter tests for HP changes, pause, leaving the
field, missing party slots, selective tile/sprite suppression, face occlusion,
and bounded allocation for maximum HP. Compositor tests cover independent image
alpha and omission from one-screen source crops. A copied-save `play` walk has
also reached the field with both the normal and replacement HUDs, with screenshots
kept only in the ignored build directory. The GPU path has run through the field
with one-screen layout, HD graphics, and the local mod directory. Challenge and
multiplayer behavior still need manual validation.

## Runtime cost and GPU validation

The live path evaluates and serializes quads on the game thread, then includes
the commands in the frame package. It does not rasterize or upload the gauge's
pixels on the CPU. A compute pass writes the output region in the existing GPU
replacement-image store before the normal 2D and one-screen composition passes.
The atlas is registered once without unused mipmaps; the pipeline is created at
startup. The face's palette conversion reuses the existing versioned image cache.
Buffers are reused and command uploads cycle normally; the live pass performs no
GPU readback or explicit fence wait.

The quad edge calculation uses HLSL `precise` for its view transform and
barycentric coordinates, preventing fused arithmetic from selecting a different
side of a shared diagonal mask edge than the CPU reference. Texture sampling and
color calculations retain their existing optimization. The white divider adds
one quad and no additional image allocation or GPU pass. With that quad enabled,
the local 8x private-asset test measured about 0.29 ms median and 0.59 ms at the
95th percentile (100 draws, including the test-only submission wait).

The CPU renderer remains the reference for headless checks, recording and
reference screenshots. Those diagnostic paths still rasterize on the CPU and
are not a live performance measurement.

`khdays-kh2-gpu-test` checks the compute output against the CPU renderer for
filtering, corner colors, all three blend modes, depth-only masks, face occlusion,
HD face transparency and a synthetic 4096x2048 atlas. With the two private asset
paths as arguments, it also compares real
gaug compositions at several HP values and benchmarks repeated draws at 1x, 4x
and 8x. Its timings include submission and an explicit wait used only by the test.
On the local Direct3D 12 device, the 1024x512 atlas at 8x measured about 0.21 ms
median and 1.4 ms at the 95th percentile over 100 draws. These are gauge-pass
measurements, not a guarantee for other hardware or the rest of the game.
The synthetic HUD adapter test separately verifies that repeated live composition
at 8x performs no CPU pixel reads or pixel-image updates.

Matching 145-second local `play` walks with HD2, one-screen layout, HD graphics
and 2x antialiasing reached the field with the replacement both enabled and
disabled. In the steady field interval at VBlanks 6600 through 8400, each run
reported 1800 presented frames and zero missed frames. Mean reported 2D time was
about 2.10 ms with the gauge and 2.08 ms without it; this small difference is
within the variation of these runs. Logs are local build artifacts, not fixtures.

## Build and run

Use the existing x64 MSVC environment and build configuration:

```text
cmake --build build\port\obj-x64 --target khdays-kh2-sequence-viewer khdays-kh2-sequence-test khdays-native
build\port\obj-x64\khdays-kh2-sequence-test.exe
build\port\obj-x64\khdays-kh2-sequence-viewer.exe --root "D:\Tools\openkh new\data\kh2" --sequence 121
```

`--root` (or `KHDAYS_KH2_ASSET_DIR`) supplies the KH2 data directory. The defaults
within it are `field2d/us/zz0field.2dd` and
`remastered/field2d/us/zz0field.2dd/US_zz0field_2dd1.png`. `--archive`, `--texture`,
and `--entry` override that selection. No extraction is performed.

Space plays or pauses; Left/Right step one source frame; Up/Down select a sequence;
Home restarts; +/- change zoom. The view is centered once at the sequence's initial
frame so subsequent movement remains visible. `--frame` selects a starting frame,
`--list` prints the clips, and `--output FILE.png` renders one frame without opening
a window. `--fps` sets source timeline units per second (default 60); it is an
inspection control, not a verified claim about the original Vsync clock.
`--play` starts playback immediately; `--run-frames N` exits after N presentations
for automated viewer smoke tests.

The separate HP composition can be inspected with:

```text
build\port\obj-x64\khdays-kh2-sequence-viewer.exe --root "D:\Tools\openkh new\data\kh2" --hp 248 --max-hp 372 --previous-hp 372 --damage-frame 30 --scale 1.5
```

`--reaction small|medium|large` selects the original parent damage reaction.
`--status normal|alert|dead` selects the status explicitly. In HP mode Up/Down
change current HP by 31 Days units; Shift+Up/Down change maximum HP. A decrease
restarts the damage segment and the small reaction for inspection; it does not
infer a KH2 battle-event severity from Days damage.
`--damage-frame 100` is the default: the previous-HP clips are then blank, as at
their original restart frame. HP mode advances an absolute timeline rather than
looping the viewer's selected standalone clip. Long bars may extend beyond the
viewport; zoom out to inspect them. Their length is not capped.

For additional private-asset checks, supply the archive and texture as the two
arguments to `khdays-kh2-sequence-test.exe`. This checks the expected gaug counts,
evaluates all 157 clips at their beginning/middle/end, and exercises HP compositions
through the full unsigned-16-bit Days HP range. The ordinary test needs only
synthetic data. Images and extracted resources belong outside version control;
the viewer's output can go under the ignored `build/` directory.
The private check also renders mask coverage across the circular thresholds and
checks that the previous-HP segment disappears at its blank restart frame.

## Small party gauges

The importer also appends small versions of the player gauge clips and writes
a `party` object in `layouts/gauge/<lang>/gauge.json`. This independently enables all three party
slots; `enabled: false` restores their native gauges. The player configuration
and portrait replacement settings remain independent. Both JSON files expose the
same animation, placement and display-scale settings; the generated party file
disables limit clips because Days does not display party limit gauges.
The [combined defaults](examples/kh2-gauge.json) include shared party settings and
`party.slots.1`, `.2`, and `.3` overrides. These are HUD slots, ordered upward
from the player, rather than character IDs. Each override inherits shared party
settings; its `offset_x/y` adds to the shared value for that slot. Icon offsets
also add to the shared icon offsets; other settings replace their shared values.

The first party gauge is raised eight DS pixels from its native portrait center.
This is part of the default placement: JSON `offset_y` remains zero. Subsequent
gauges are stacked using their artwork radii, with the same edge-to-edge gap as
the player and first party gauge. Independent offsets apply after that placement,
so adjusting one slot does not push its neighbors. Portraits and player-number
labels follow the corresponding animated center.

Rebuilding migrates an old `party.json` into `gauge.json` and removes the separate
file. The old experimental -8 Y offset becomes zero because the default now
includes that lift; other custom settings survive. Runtime reads only the combined
file. Installer conversion validates all languages before publishing outputs.

The smaller atlas quarters have radius 23 rather than 45 sequence units. Their
`geometry_scale` is therefore `23 / 45`, while the display `scale` stays `0.65`.
186 Days HP still fills the three-quarter circle. Extension lengths and mask
geometry scale by the same ratio, so equal HP produces proportionally smaller
party bars. Changing display scale alone does not change the HP mapping.

Generated groups use parts 101 (background), 103 (green), and 105 (damage,
tinted with the player damage color). Two mirrored copies of half-circle 102
form the backing at the origin; the player backing's thin outer ring is omitted.
A one-unit bottom strip of part 101, tinted `00 00 00 80`, supplies the end cap.
The copies of clips 121–129 and 132–138 preserve timing, rotations, scale and colors,
and scale positions and sway by `23 / 45`. Mirroring reverses UVs and corner colors
while keeping rectangle bounds ordered, so adjacent quarters share a continuous
pixel edge. Existing player clips and groups remain unchanged.
Axis-aligned masks use ordered bounds too, preventing stray lines at exact
quarter-turn HP values.

`port/ui/kh2_party_hud` reads each party slot through the existing read-only
portrait query, tracks independent HP and damage trails, and uses the existing
sequence GPU renderer with cached images and atlas. It finds portrait positions
from the native BG3 tile ranges rather than fixed screen coordinates. Native and
replacement portraits follow their slot's animated center; the existing UI
element layout handles widescreen positioning. Only the matching native HP tiles
and their OBJ copies are suppressed. No game behavior or generated source changes.

The headless viewer accepts `--config PATH/gauge.json --party 1 --hp 248 --max-hp 372
--previous-hp 310 --damage-frame 30 --output FILE.png` to inspect these resources.
Focused tests cover the generated geometry, centered backing, unchanged original
records, animation timing, proportional HP mapping, independent live slots,
damage, pause, and reuse without repeated pixel uploads. Private asset checks
compare CPU/GPU output at circular thresholds and along the extended bar. Manual
game validation should include all occupied slots, native/HD/KH2 portraits,
damage and defeat/revival, mission mode, widescreen, and disabling each replacement.

## Player-number icons and limit text

`player_icon` at the root configures the player's P1–P4 label; `party.player_icon`
configures party labels, with overrides under each party slot. `limit_text` at
the root configures the native LIMIT-available indicator. These objects expose
`enabled` (false), `animation` (-1), `archive`, `texture`, `entry`, `scale`,
`source_origin_x/y`, `frames_per_tick`, and `offset_x/y`. Resource names resolve
beside the combined JSON and default to the gauge's archive and atlas. PNG and
supported DDS textures use the existing decoders. A label sequence's origin is
placed at the native label's center; source-origin settings preserve relative
animation motion when recentering an existing resource.

Player icons also expose `p1`, `p2`, `p3`, and `p4` clip assignments (-1). A
nonnegative number-specific assignment overrides `animation`, selected by the
number actually drawn by Days, including when the local multiplayer player is
not P1. Enabling a replacement requires both `enabled: true` and an assigned
clip. Missing assets or invalid clips log once and keep the native icon. Offsets
move native artwork too, so a replacement is not required for positioning.

The native BG3 player-number and LIMIT tiles were identified from the original
Days `UI/btl/main.p2` NSCR/NCGR/NCLR resources, independently of face tiles and HP
fill uploads. Only those tiles are regrouped. LIMIT replacement visibility follows
the presence of Days' own LIMIT text, preserving its display conditions instead
of inferring availability from HP. Slot geometry and origins retain the checks
against the native HUD resource. Sequence labels use cached GPU images and
atlases; playback pauses with the game. These replacements are disabled in the
generated JSON until artwork is supplied.

Number rows are assigned once in native HUD order rather than searched separately
below each portrait. Party numbers attach to the bar's reduced 37-unit baseline,
with their configured offsets applied afterward. Native label images are claimed
before gauge composition and emitted after gauges and portraits, just like their
sequence replacements, so neither the number badges nor LIMIT text is covered.

## Enemy gauge

`enemy` in the same `layouts/gauge/<lang>/gauge.json` independently enables the
enemy replacement. Its default display scale is `0.65`, with zero X/Y offsets.
An older JSON without this section keeps the native enemy gauge. The importer
adds the defaults when rebuilding the HD pack and preserves customized enemy
settings. The existing archive already contains the enemy clips; no enemy sprite
conversion or additional extraction inputs are required.

Days retains its original capacity: **250 HP per bar in Story Mode and 750 in
Mission Mode**. The adapter reads `Ov002_InitGaugeScene`'s live unit multiplier,
not a new gameplay setting. KH2 uses 200 HP per bar; its artwork therefore draws
`200 * remainder / days_capacity` sequence units. At exactly one full bar there
is no extra tank. A 1.5-bar maximum shows the dim full-length preview behind the
shorter initial bar; after rollover the main bar becomes full length. Green and
red lengths are independently anchored at the right, as in `updateHP`.

The settings select `background` (38), `preview` (42), `bar_background` (39),
`cap` (40), `damage` (41), `fill` (43), `tank_full` (44), `tank_empty` (45), and
`tank_damage` (46). They also expose `archive`, `texture`, `entry`, `scale`,
`offset_x/y`, `frames_per_tick`, `source_origin_x/y` (498,31), and independent
`bar_offset_x/y` and `tank_offset_x/y`. The original tank spacing comes from
clip 45's extended parameter. Damage restarts the red clips; their out-of-range
restart frame makes the faded bar and depleted tanks disappear. Healing clears
the trail and restores green tanks. Playback pauses with Days.

`port/ui/kh2_enemy_hud` reads the field gauge context at `0x0207f618`, including
its current/maximum HP and native tween reset state. It does not write game
memory. Only main.p2's enemy bar, HP label, and tank cells (47–59) are hidden,
after a replacement image has been prepared successfully. Days' separate enemy
name surface remains unchanged. Missing resources, hidden native HUD, or
unsupported command/canvas sizes retain the native gauge.

The normal path uses the existing sequence GPU pass, cached atlas, and output
image. The canvas encloses all evaluated geometry instead of using Days' narrow
bar rectangle. A separate native composition anchor lets the wider image and
JSON offsets follow the top-right HUD placement without being clipped to the
original one-screen crop. The CPU rasterizer remains a reference path.

The headless viewer accepts `--enemy --bar-capacity 250 --hp 250 --max-hp 375
--previous-hp 375 --damage-frame 30 --config PATH/gauge.json --output FILE.png`.
Synthetic tests cover both capacities, right anchoring, partial bars, rollover,
red disappearance, healing, target reset, pause, selective cell suppression,
legacy/missing-resource fallback, extended canvas bounds, and image reuse without
CPU uploads. The optional GPU test mode `--enemy ARCHIVE TEXTURE` compares the
local HD resource with the CPU reference at 1x, 4x, and 8x and reports warm draw
timings. In-game validation should include both modes, target changes, damage
across multiple bars, healing, hiding the HUD, HUD-size settings, and widescreen.

KH2 provenance: `GAUGE_ENEMY::create` (`0x9DD1C`), `start` (`0x9E06C`),
`updateHP` (`0x9D890`), `damage` (`0x9E208`), and `cure` (`0x9DC64`), checked
against the supplied PS3 debug pseudocode/disassembly and little-endian PC gaug.
Days provenance: overlay 002's `Ov002_InitGaugeScene`, `Ov002_RestartRequestQueue`,
`Ov002_UpdateGaugeUnits`, `Ov002_RepaintSubPanelRows`, `Ov002_TickScene`, and
`Ov002_BuildPanelLabels` at the decompilation revision recorded by this repository's
submodule. Target replacement is observed through native tween resets, without
adding a gameplay hook; original KH2 transition timing still needs capture comparison.

## Boundaries

In the one-screen picture, independent field HUD images carry `KES_HUD` and
follow the situation's HUD visibility. `host/render/onescreen_images` inserts
them immediately above their source screen shape, preserving all earlier
foreground shapes. A tutorial's page therefore covers the HUD; exposed HUD
receives its source shape's dimming. Dialogs, mission information, pause, game
over, and the HUD-off setting do not promote hidden HUD images. The images keep
their full canvas outside the native crop, and labels retain their order above
gauges and portraits. The shared C/HLSL one-screen pass applies image dimming.
It also reads the native layer's complementary EVA/EVB fade from the frame's
blend registers. Images sharing that layer compose together before the fade is
applied, so an opaque portrait does not reveal its gauge through a fade or darken
twice. Native number/LIMIT label fallbacks join the same image group through the
existing cached palette conversion. Two-screen composition retains its normal
native blending path.

Native gauge suppression follows tile identity outside the usual portrait or
enemy region while the HUD transitions. Temporary 8/16/32/64-pixel OBJ strips
are matched against the live gauge data; outside their original region the OBJ
source must also belong to main.p2. Enemy teardown retains ownership until the
native bar disappears, including a boss reaching zero HP. Fade coefficients do
not require changing image pixels or uploading textures every frame. The native
blend source is `Ov002_CommitDisplayState` and `Ov002_UpdatePanelBlend` in overlay
002; their BG3 targets and complementary coefficients are checked by regressions
at all 17 hardware fade levels.

- `assets/kh2_bar` is the BAR reader factored from `kh2_map`, shared by both callers.
- `assets/kh2_sequence` owns endian-safe parsing, playback, transforms, colors, and
  ordered quad evaluation. It has no filesystem, HP, SDL, or Days dependencies.
- `port/host/render/kh2_sequence_asset` resolves a local archive/image pair and
  uses the existing image/DDS decoders. The installer supplies these same local
  inputs from the player's collection through `tools/hd/kh2_gauge.py`.
- `port/gfx/kh2_sequence` rasterizes the ordered quads against an actual destination
  with normal/add/subtract blending and depth-only masks, and encodes GPU commands.
  It remains the CPU reference. `port/host/render/kh2_sequence_gpu` and
  `port/shaders/kh2_sequence.hlsl` render those commands into a UI image. Blend
  modes operate against that image's accumulated pixels; destination-dependent
  blending against the scene itself still needs compositor integration.
- `port/ui/kh2_hp_gauge` selects the gauge clips and builds mask geometry. The
  reusable sequence code has no knowledge of gauge thresholds or Days HP units.

## Provenance and supported behavior

The authority is the KH2 HD PS3 debug executable's symbols/disassembly, its DWARF
offsets, and the supplied little-endian PC resource bytes. The proprietary dump
is kept locally; none of it is a repository test fixture. Relevant original symbols:

| Behavior | Original symbol/address |
|---|---|
| Table layout | `YI::SPRITE::Init` `0xC6A28`, `YI::SEQUENCE::Init` `0xC23D8` |
| Frame bounds and looping | `GetAllFrame` `0xC2674`, `ChangeNumber` `0xC27A8`, `Control` `0xC3AC8` |
| Evaluation and parent attachment | `Calc` `0xC387C`, `Draw` `0xC3C58`, `UpdateActive` `0xC2D20` |
| Geometry and color | `SPRITE::SetRotate` `0xC67E0`, `_Draw2` `0xC8D80`, `MultiRGBA` `0xCA508`, `RateRGBA` `0xCA5E0` |
| Gauge construction | `GAUGE_PLAYER::createHP` `0xA36E4`, `createCircle` `0xA286C`, `createBar` `0xA2728` |
| Mask geometry | `CIRCLE_MASK::create` `0x64230`, `Ft4Base::calcXYZ` `0x9CEE8` |

The PC SEQD version is `0x160`; the header is 48 bytes and the record strides are
44/20/4/144/36 bytes. Unknown bits and clip parameters are retained. Sequence flags
are not blindly interpreted as runtime disables: the original Calc derives its
optimization state from the animation records. Palette selection and scrolling
currently produce an explicit unsupported-feature error when encountered in a
drawn part. The gaug fixture uses neither.

Playback evaluates whole source frames, includes animation endpoints except
`last_cut`, and discards overshoot on a loop transition. Restart frames outside
the visible animation interval are allowed. Tags are returned as evaluation data;
the caller controls callback dispatch, so extra draws do not trigger side effects.
The active attachment is optional; a future wrapper must decide how to retain a
previous active value when no active track is present.

The renderer supports mirrored and degenerate UV rectangles, four corner colors,
the projected X/Y/Z rotation matrix, scale, sway, and cubic easing using floats at
animation offsets 0x64 and 0x6C. Coordinates are quantized to the original 1/16
units. Packed modulation colors use 128 as unity; the replacement PNG uses ordinary
RGBA8. The logical 512x256 gaug atlas addresses the 1024x512 replacement without
changing geometry size.

## Remaining fidelity validation

This implementation has not yet been matched pixel-for-pixel against original
KH2 captures. In particular, `_adjustSPRT`'s resolution-dependent edge adjustments
are not emulated: the reference currently uses the sprite path's half-texel UV
and position convention. Filtering selected from the format and HD blend/clamp
rounding still need comparison against the original backend. The source clock
unit remains configurable until Vsync timing is verified. Sway uses full cycles
per eased interval; the disassembly establishes the multiplication by the sway
count, but the referenced period constant still needs independent confirmation.

The HP composition uses the original mask thresholds and depth ordering, and the
provided `CIRCLE_RAD = 0.078539819`. The supplied PS3 status tables decode as
normal/alert/dead circle clips 121/122/121, bars 124/125/124, and parent clips
137/132/137. Status and reaction are explicit inputs; the Days adapter supplies
them with the policy described above.

Days HP maps to KH2 HP as `days_hp * 10 / 31`: 186 is the 60-HP circular capacity,
372 is KH2's 120-HP extent, and the extension continues by `3 * (kh2_hp - 60)`
logical units without a gameplay cap. The parent position is normalized from the
asset's 455,355 center. The Days adapter places its own existing portrait artwork
over this gauge and advances before the display's unchanged-frame shortcut.
