# Re.Trektor

A vertical tracker sequencer packaged as a **MIDI-only VST3 plug-in**. Re.Trektor
does not process audio: it turns patterns into MIDI notes and controller messages,
and follows the host transport for tempo and position.

* Manufacturer: Corthax
* Format: VST3 (MIDI effect)
* Plug-in code: `Cort` / `Rtrk`
* Version: 0.1.0

## Requirements

* Linux x86_64, a C++17 compiler, CMake 3.22 or newer
* JUCE 8 is vendored in `JUCE/` (no external download needed)
* A VST3 host that supports MIDI effects (tested with JUCE's host and Carla; in
  Reaper, put the plug-in on a track with "MIDI output" enabled)

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

The bundle is `build/ReTrektor_artefacts/Release/VST3/Re.Trektor.vst3`. Copy that
directory to `~/.vst3/` (or another VST3 path) and rescan in the host. A build keeps
its newest glibc requirement at `GLIBC_2.38`, which is what lets it load in a host whose
loader resolves an older `libm.so.6` - a binary linked on a newer system states
`GLIBC_2.43` and such a host refuses to load the plug-in at all rather than simply
missing two functions; the reason and the check that reads it are in
[project.md](project.md).

Hosts that hide MIDI effects (Reaper in some routing setups) can take the
instrument fallback build instead, which exposes a silent stereo output and
`IS_SYNTH` so it can be inserted like a normal instrument while still only
producing MIDI:

```sh
cmake -S . -B build-instrument -DCMAKE_BUILD_TYPE=Release \
    -DRETREKTOR_INSTRUMENT_FALLBACK=ON
cmake --build build-instrument -j"$(nproc)"
```

A Windows x86-64 VST3 of the same sources is optional and is not what a Linux
host installs. It is cross-built with Microsoft's own compiler and linker running
under Wine, so downloading the toolchain means accepting Microsoft's license:
Visual Studio is not redistributable, and neither is what these commands install.

```sh
cd ~/src/msvc-wine   # github.com/mstorsjo/msvc-wine
./vsdownload.py --accept-license --dest ~/opt/msvc
./install.sh ~/opt/msvc

export PATH="$HOME/opt/msvc/bin/x64:$PATH"
CC=cl CXX=cl cmake -S . -B build-msvc -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_ARCH=x86_64 \
    -DJUCE_TARGET_ARCHITECTURE=x86_64 -DJUCE_WINDOWS_HELPERS_CAN_RUN=FALSE \
    -DCMAKE_CXX_FLAGS_INIT="/utf-8 -DJUCE_USE_WINDOWS_MEDIA_FORMAT=0"
cmake --build build-msvc -j2

./build/ReTrektor_vst3_helper build-msvc/ReTrektor_artefacts/Release/VST3/Re.Trektor.vst3 \
    > build-msvc/ReTrektor_artefacts/Release/VST3/Re.Trektor.vst3/Contents/Resources/moduleinfo.json
```

Build the Windows tree with `-j 2`, not more. The `CMakeLists.txt` gate that turns
the manifest step off for a cross build, and what each flag answers for, is in
[project.md](project.md).

Development harnesses (their checks and what each one covers are in
[project.md](project.md)):

```sh
cmake -S tests -B tests/build -DCMAKE_BUILD_TYPE=Release
cmake --build tests/build -j"$(nproc)"

tests/build/ReTrektorCoreTest_artefacts/Release/ReTrektorCoreTest
tests/build/ReTrektorUiTest_artefacts/Release/ReTrektorUiTest
tests/build/ReTrektorSmoke_artefacts/Release/ReTrektorSmoke \
    build/ReTrektor_artefacts/Release/VST3/Re.Trektor.vst3
DISPLAY=:0 tests/build/ReTrektorEditorShot_artefacts/Release/ReTrektorEditorShot \
    /tmp/editor.png 1240 760 --dump
```

## The window

* **Top bar** — `TRACKER` / `ARP MACRO` / `VIB MACRO` / `SETTINGS` tabs, snap
  toggle, grid selector, zoom out/in, host position.
* **Left panel** — the pattern pool: every pattern by id and name, used ones marked
  "in use". Drag one onto the timeline to place it; right-click a pattern for edit,
  place, copy, paste, duplicate, rename, delete and "Toggle grid preview", which
  grows the item into a preview of its first 12 rows × up to 4 channels.
* **Timeline** — pattern clips on a free (non-grid) horizontal line: move, resize,
  slice, copy, paste. Timeline edits are refused while the host is playing; cell
  editing and pattern selection stay available. Clicking the timeline — with either
  button — parks the plug-in's playhead at the clicked beat, rounded to the current
  grid when snapping is on, so a paste, a slice or a clip menu acts where the mouse
  was. While the host plays, its transport owns the playhead instead.
* **Tracker** — the active pattern's rows, five columns per channel, with the
  column visibility chips (`I` `V` `P` `C`) above each channel so a hidden field can
  always be restored: one chip hides one field of one channel. A horizontal
  scrollbar appears when the channels are wider than the window; drag its thumb,
  click to jump, or shift + mouse wheel.
* **ARP MACRO / VIB MACRO tabs** — one card per defined macro, plus `+ Add macro`
  and `- Remove selected`, and a vertical scrollbar strip when the cards do not fit.
* **Right panel** — Transpose, Instrument Remapping and Selection Interpolation,
  each on a scope: selection only, whole pattern, all patterns in the song, or all
  created patterns.
* **Bottom bar** — octave and step skip for note entry, pattern name, pattern info,
  and a CHANNELS slider (1–127) setting the active pattern's channel count;
  narrowing a pattern keeps the removed columns' data.

A new editor starts at the size the closed one had, clamped to the 900–2600 × 560–1600
resize limits, because hosts such as Reaper throw a floating plug-in window away and ask
for a brand-new one. The size belongs to the plug-in instance, not to the saved song.

## Tracker notes

* Selection is per cell: a click selects one cell and parks the cursor on its field,
  a drag sweeps a rectangle, Shift unions another rectangle, Ctrl takes one away and
  leaves a hole, Ctrl winning over Shift when both are held. The rectangle snaps to
  channel boundaries, so a selected cell is one channel's visible fields. The
  selection lives in the view rather than in the saved song.
* Copied cells sit on a cell clipboard that keeps each cell's place inside the copied
  rectangle, so cells a Ctrl gesture left out stay missing from the paste as well.
  The clipboard is not saved with the song and survives a pattern switch.
* The cell menu keeps Clear (Selection / Cell / Channel / Pattern), Copy selection,
  Cut selection and two paste submenus, **Paste** and **Paste (Additive)**, each
  carrying Everything / Note / Instrument / Volume / Pan / Command / Command Value.
  The plain paste writes every field the buffer carries, empty ones included, so a
  field the copied cell left empty clears the target's; the additive one lets only
  the fields the buffer really carries land, so the target keeps its own value where
  the buffer was empty. Cut is the copy followed by emptying exactly the cells it
  took, leaving the painted selection and the cursor where they were, so a following
  paste moves those cells instead of repeating them.
* A paste never grows a pattern: what does not fit inside its rows or channels is
  discarded. Pasting cells is a cell edit, so it stays available while the host plays.
* Shortening a pattern keeps its rows past the new end, so enlarging it again restores
  the data rather than leaving blanks.
* The tools panel's Interpolation fills only blank cells that sit between two cells
  already naming a value inside the selection, linearly, so a ramp restarts at every
  anchor and a selection with a hole in it ramps in pieces; nothing is extrapolated and
  nothing outside the selection is touched. In the note column a `===` note-off has no
  number to ramp to or from, so it closes a segment. Transpose reads one amount
  (1–48 semitones) plus a direction, `↑` or `↓`; Remap reads a source and a target
  dropdown (`All` or an instrument) and rewrites only the cells whose instrument
  resolves to the source.

## Columns

Each pattern row has five columns per tracker channel: Note, Instrument, Volume, Pan
and Command.

* Instrument names the **MIDI output channel** (`0`–`15`), drawn `-0` to `-15`. When
  unset, the last value used on that tracker channel is reused. Volume, Pan and Command
  carry the same way; the command carries its letter and its argument together.
  Carried values are drawn dimmer, and only instrument, volume and pan carry when a
  note exists, while command carries even on empty rows.
* Volume and Pan send CC7 (channel volume) and CC10 (channel pan) by default; the
  Settings tab can point either column at any controller `0`–`127`. Controllers are
  sent before the note handling, so even a row holding only a note-off applies them.
  A volume on a row that also has a note is additionally the note-on velocity.
  Carried values are not re-sent.
* Command `0`–`127` are plain CC numbers with a `0`–`127` argument, drawn teal. A row
  that names only an argument plays it on the command the column carries down from
  above.
* Command letters, typed in the command number field (the claim is only that field's, so
  `V` still types an F natural in the note column):

  | Letter | Command | Does |
  | ------ | ------- | ---- |
  | `A` | 128 | calls an arpeggiator macro; the argument is the macro id `1`–`127` |
  | `B` | 144 | pitch bend; the argument is a point on the 14-bit wheel in steps of 128 (`0` bottom, `64` the exact centre 8192, `127` near the top) |
  | `U` | 145 | slides the wheel to its top; the argument is how many rows below the letter its extreme sits over |
  | `D` | 146 | slides the wheel to its bottom, same argument |
  | `V` | 147 | runs a vibrato macro; the argument is the macro id `1`–`127` |
  | `C` | 148 | cuts the notes that column's channel enters to a length; the argument is that length in subticks (a subtick being a row over the Settings' "Subticks per row", default 6) |

  `A` and the wheel letters are drawn orange and lilac, and an argument carried from
  above is drawn dim beside the letter. Every letter leaves an argument behind, so
  `A` and `V` start at the release id `0`, `B` at the wheel's centre `64`, a slide at
  the default span of 8 rows and `C` at a length of 3 subticks. Above the number the
  argument field can hold sit two spellings: `o` / `O` writes `OFF` — pitchbend off and
  the wheel back at its centre, the arpeggiator run off, a wobble off, a note-length
  gate off — and `s` / `S` writes `STP`, which stops a wheel move and leaves the wheel
  on the point it reached. `STP` is refused beside `A`, `V` and `C`, a run, a wobble and
  a cut having no wheel move to stop. `C` is the one command that keeps being carried
  down the column rather than answering the rows under it, so one letter gates a whole
  staccato passage: every note entering a row it is carried on is released that many
  subticks after it is struck, a row naming only a number retargets the length for the
  rows under it, and `0` or `o` writes `OFF`, which releases the gate and leaves the
  notes sustained as before.
* `` ` `` enters a note-off, drawn `===`.
* A tracker pattern can use up to 127 tracker channels, independent of the 16 MIDI
  output channels; identical notes triggered on several tracker channels in one row
  are sent only once.

An arpeggiator sounds one step per subtick — a subtick being a row over the Settings'
"Subticks per row" (default 6) — each step a signed number of semitones away from the
note that column named, up to 64 steps, and steps past the calling row play on while
the blocks below run. `--` sounds nothing and sustains the step before it; `OFF` opens
whatever the run was holding. A looped macro cycles until a note-off, an `A 0` call or
the end of the clip takes it away; an unlooped one leaves its last step's note held.
A vibrato swings the wheel about whatever point the wheel is already at, over the
macro's own shape, amplitude and cycle, and repeats for as long as the column carries
the letter, so a release row is what puts the pitchbending away. The full rules for
those are in [project.md](project.md).

## Macros

The ARP MACRO tab holds the arpeggiator macros the `A` calls, the VIB MACRO tab the
vibrato macros the `V` calls. `+ Add macro` defines the lowest id that has none yet:
eight arpeggiator steps that all sound nothing, so a macro's shape is only ever where
it was entered, or the shape, amplitude and cycle the model gives a fresh vibrato.
`- Remove selected` clears the macro the step cursor or the last card click sits on.
Ids stay put, so clearing one leaves the rows that called that id silent.

An arpeggiator card shows its `LENGTH` (1–64 steps, the plug-in's number widget), a
`LOOPED` tick and its steps 16 to a line as signed semitones, the groups of "Subticks
per row" shaded so a macro longer than a row reads as row spans. Steps are entered by
the arrows and a click of a drawn step putting the cursor there, and a sign plus up to
three digits build one number in the cell before it lands, `Enter` or a move of the
cursor committing it and `Escape` dropping it; the note column's own note-off key
spells the `OFF` step. `Delete` / `Backspace` make a step sound nothing.

A vibrato card shows the shape as a dropdown over `Sin` / `Sin Up` / `Sin Down` /
`Triangle` / `Triangle Up` / `Triangle Down` — the sinusoidal one curving, the triangle
one ramping in four straight sides whose corners sit on the cycle's quarters, the `Up`
spellings folding the cycle's second half back so the pitch only goes above the base and
the `Down` spellings folding its first half — plus the amplitude, spelled as the share of
full pitchbend and turned in hundredths, and the cycle in whole subticks, one to 127.
The arrows and `Tab` move a macro cursor from card to card, and `PageUp` / `PageDown`
go to the list's ends.

## Settings tab

* Ignore mid-row transport start (default on): a fresh playback start inside a row, or
  a seek arriving from behind it, does not play that row; a transport that came back to
  an earlier position — a loop wrap or a scrub — plays the row its block boundary fell
  inside either way.
* Subticks per row (default 6): how many equal parts a row is divided into, which is
  what names an arpeggiator step's width, a slide wheel's next move and one vibrato
  cycle's width.
* Rows per beat (default 4, one row per 16th note).
* New pattern length in rows (default 64, maximum 1024).
* Snap to grid, adaptive grid, fixed grid size.
* Record octave and step skip.
* Volume column CC and Pan column CC (default 7 and 10), each `0`–`127`.
* Undo steps (default 50, `0`–`200`): how many model states the undo keys can walk back.
  `0` turns undoing off and stops the plug-in from holding states at all; a 32 MB budget
  also drops the oldest states, so a pattern whose states do not fit keeps fewer.
  Every number row in the plug-in also answers a drag along itself, the whole area beside
  a row's text box being that row's `+`/`-` pair: the number moves with the pointer, one
  drag is one action of the undo line rather than one point per movement, and a drag that
  ended where it began leaves no point at all.

## Commands

Note entry uses a US layout, with the octave set in the bottom bar:

```
Z S X D C V G B H N J M   ->  C C# D D# E F F# G G# A A# B
Q 2 W 3 E R 5 T 6 Y 7 U   ->  next octave
```

| Key | Action |
| --- | --- |
| `/` or `F1` | octave down |
| `*` or `F2` | octave up |
| `` ` `` | note-off (`===`); in the ARP MACRO tab, the `OFF` step |
| `Enter` | advance by step skip |
| `Tab` / `Shift+Tab` | next / previous field |
| Arrow keys, Page Up/Down, Home/End | move the cursor |
| `Delete` / `Backspace` | clear the current field |
| `Ctrl+C` / `Ctrl+V` | copy the selected cells / paste them at the cursor (everything) |
| `Ctrl+Z` | undo the last change |
| `Ctrl+Y` / `Ctrl+Shift+Z` | redo the last undone change |
| `Alt+Z` / `Alt+Y` | undo / redo, for hosts that answer the Ctrl trio with their own |
| `Alt+C` / `Alt+V` / `Alt+X` | copy / paste / cut the selected cells, same hosts |
| `Alt+S` | slice the clip under the plug-in playhead, the clip becoming two clips over two patterns |
| `F5` | switch to the Tracker tab |

| Gesture | Tracker action |
| ------- | -------------- |
| left click | select one cell, park the cursor on its field |
| left drag | sweep a rectangle of cells |
| Shift + click or drag | union that rectangle into the selection |
| Ctrl + click or drag | remove that rectangle, leaving a hole |
| right click | open the cell's context menu |
| click or drag on the bottom strip | jump or drag the horizontal scrollbar's thumb |
| shift + mouse wheel | scroll the channel columns that do not fit |

Undo belongs to the plug-in rather than to the host, since the VST3 spec hands a
plug-in no undo of its own: every model change appends the state it reached to the
plug-in's own line, and one step restores exactly what reloading that state block
would have restored — a cell written, a clip dropped or sliced, a pattern resized, a
setting moved. What the line does not carry is not rewound either: the pattern
clipboard, the selection, the cursor and both playheads keep their current values, so
undoing reads as a rewind of the song rather than a jump of where the user is working
in it. Hosts such as Bitwig and Reaper answer `Ctrl+Z`, `Ctrl+Shift+Z` and `Ctrl+Y`
themselves before the plug-in is offered the event, which is why `Alt+Z` and `Alt+Y`
exist — one spelling each, the whole `Alt+Shift` family left to the host — and why
every panel answers them, so a press aimed at undo is heard while a context menu is
open or while a pattern is being dragged from the pool. The tracker's cell menu ends
with an Undo / Redo pair for the same line. Pressing undo while a number is half
typed closes that buffer instead of stepping, so one press stays one effect. Both
spellings work while the transport is playing: the rows ahead of the playhead play
what the restored song holds, while what the plug-in already sent to the host is not
rewound.

`Ctrl+X`, `Ctrl+Shift+C` and `Ctrl+Shift+V` are not answered at all, so a host keeps
its own cut and its own duplication; the pattern clipboard therefore has no keyboard
spelling, and copying or pasting a whole pattern is what the timeline's clip menu, its
empty-area menu and the pool's item menu do, each naming its own instance or unique
choice. An `Enter` pressed to confirm a typed number confirms rather than re-firing
whatever the last click touched, since no Button in the editor's tree holds the
keyboard focus: it rests on the control whose text box the value is typed into.

## Behaviour to know

* There is no audio processing of any kind: the plug-in outputs MIDI and passes
  incoming MIDI through.
* Reaper does not call a MIDI effect while the transport is stopped, so stop-time
  note-offs are silently dropped. A stuck-note safety net remembers every pitch it
  has sent a note-on for, per MIDI channel, and precedes a later note-on of that
  pitch with a matching note-off, which re-articles the key at the cost of one
  extra note-off.
* A fresh transport start or jump releases what the synth may still be holding at
  that block's first sample, with the releases ordered ahead of the note-ons queued
  for that same sample, so a note whose row sits exactly at a start or exactly at a
  looped region's start survives rather than being released the moment it is struck.
  Only all-notes-off (CC 123) is sent there; no All Sound Off (CC 120), since
  nothing turns a channel that reads it back on again.
* Arpeggiator macros are named by id, so clearing one leaves the rows that called
  that id silent rather than renumbering the ones below.
* An older saved song loses its own macro section: the build has one command letter
  for that half of the column, so an older state's `MACROS` block is dropped and an
  older row's command value `128`–`143` lands on the letter `A` while the argument it
  holds stays the argument it was — usually the release, so those calls have to be
  re-entered by hand. This build's bend, slide, vibrato and cut letters are values
  `144`–`148` an older build could not have written, so no older row turns into one of
  those. Two settings rows the older state may still spell — the pattern paste's
  default and the hex display — were dropped outright and simply read nothing.
* The argument spellings that sit above the numbers that field can hold read
  unchanged in a saved state, so an older argument of `128` or `129` beside a call
  lands as the release and as nothing at all rather than as one of the ids the older
  encoding ignored.
* Selection snaps to channel boundaries, so one field of one channel — its volume
  alone, its command value alone — cannot be selected yet; a paste carries whole
  channels of cells, and it is the paste filter that points at one field.
* A pitch wheel left bent by a channel whose clip was deleted outright is taken back
  at the next release point or transport re-arm rather than by the loop that closes
  the notes, runs and wobbles of a clip the playhead has left.
* Note entry assumes a US keyboard layout.
* The Windows x86-64 build above was checked at a real Windows 11 host: it loads, its
  editor renders and it works, so nothing in the tree is bent for Windows. Wine is a
  different matter and is not pursued: the same binary loads and runs there, but its
  editor window comes up as an unpainted black rectangle, which is wine never
  completing the DirectComposition tree JUCE 8 hands a plug-in's child window rather
  than anything the plug-in does — a Linux host has the native build. The full reason
  is in [project.md](project.md).
* Interactive behaviour has been verified with JUCE's own plug-in host, headless
  editor interaction tests and Carla; a full Reaper session has not been run in this
  environment.
* Command letters beyond the arpeggiator call, the pitch wheel letters, the vibrato and
  the note-length cut are future work (a dedicated pitch bend CC105/CC106 pair rather
  than the standard pitch wheel message, tempo, song position).

## Licensing

JUCE is vendored in `JUCE/` and is licensed under its own terms (AGPLv3 /
commercial). Review `JUCE/LICENSE.md` before distributing binaries.

## Development

The development notes live in [project.md](project.md): why the build is shaped the
way it is (the glibc pin, the Windows cross-build flags, what the CMake gates answer
for), what the test harnesses cover, the full behaviour rules — layout, mouse and
keyboard maps, the MIDI model, the settings, the known limitations in their long
wording — and the running Session block of what each round changed.
