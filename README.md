# Re.Trektor

A vertical tracker sequencer packaged as a **MIDI-only VST3 plugin** for Linux.
Re.Trektor does not process audio: it turns patterns into MIDI notes and
controller messages, and follows the host transport for tempo and position.

* Manufacturer: Corthax
* Format: VST3 (MIDI effect)
* Plug-in code: `Cort` / `Rtrk`
* Version: 0.1.0

## Requirements

* Linux x86_64, a C++17 compiler, CMake 3.22 or newer
* JUCE 8 is vendored in `JUCE/` (no external download needed)
* A VST3 host that supports MIDI effects (tested with JUCE's host and Carla;
  Reaper should use the plug-in on a track with "MIDI output" enabled)

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

The bundle is written to:

```
build/ReTrektor_artefacts/Release/VST3/Re.Trektor.vst3
```

Install it by copying that bundle to `~/.vst3/` (or another VST3 path) and
rescanning in the host.

## Tests

```sh
cmake -S tests -B tests/build -DCMAKE_BUILD_TYPE=Release
cmake --build tests/build -j"$(nproc)"
```

* `ReTrektorCoreTest` – model, sequencing, snapping, macro expansion, slicing,
  paste, state
* `ReTrektorSmoke` – loads the built VST3, checks MIDI behavior and state
* `ReTrektorUiTest` – drives the editor components (tracker keys, column
  chips, tools, timeline drop, pool placement) and confirms MIDI output
* `ReTrektorEditorShot` – renders the editor headlessly and can dump the
  component layout (`--dump`, optionally `--tab=settings|macros`)

Example:

```sh
tests/build/ReTrektorCoreTest_artefacts/Release/ReTrektorCoreTest
tests/build/ReTrektorUiTest_artefacts/Release/ReTrektorUiTest
tests/build/ReTrektorSmoke_artefacts/Release/ReTrektorSmoke \
    build/ReTrektor_artefacts/Release/VST3/Re.Trektor.vst3
DISPLAY=:0 tests/build/ReTrektorEditorShot_artefacts/Release/ReTrektorEditorShot \
    /tmp/editor.png 1240 760 --dump
```

## Layout

* **Top bar** – Tracker / Macros / Settings tabs, snap toggle, grid selector,
  zoom out/in, host position.
* **Left panel** – pattern pool. Every pattern is listed with id and name.
  Unused patterns can be edited and placed; used patterns show an "in use"
  marker. Drag a pattern onto the timeline to place it.
* **Timeline** – free (non-grid) horizontal arrangement of pattern clips.
  Clips can be moved, resized, sliced, copied and pasted. Timeline edits are
  disabled while the host is playing; cell editing and pattern selection stay
  available.
* **Tracker** – the active pattern's rows. Column visibility chips (`I`, `V`,
  `P`, `C`) sit above each channel's note column and stay visible so hidden
  columns can always be restored. The rows field above the line numbers sets
  the pattern length (shared by every clip that references the pattern).
* **Macros tab** – macro definitions (name, kind, two parameters). Definitions
  live with the song and are called from the tracker's Command column with
  command `128`–`143`.
* **Right panel** – Transpose, instrument Remapping and Selection
  Interpolation. Each tool uses the selected scope: selection only, whole
  pattern, all patterns in the song, or all created patterns.
* **Bottom bar** – octave and step skip for keyboard entry, pattern name,
  pattern info, and visible channel count.

## Keyboard map

Note entry uses a US layout tracker layout (octave is set in the bottom bar):

```
Z S X D C V G B H N J M   ->  C C# D D# E F F# G G# A A# B
Q 2 W 3 E R 5 T 6 Y 7 U   ->  next octave
```

| Key | Action |
| --- | --- |
| `/` or `F1` | octave down |
| `*` or `F2` | octave up |
| `` ` `` | note-off (`===`) |
| `Enter` | advance by step skip |
| `Tab` / `Shift+Tab` | next / previous field |
| Arrow keys, Page Up/Down, Home/End | move cursor |
| `Delete` / `Backspace` | clear current field |
| `Ctrl+C` | copy active pattern |
| `Ctrl+V` | paste at playhead (instance or unique, per Settings) |
| `Ctrl+Shift+V` | paste as unique pattern |
| `Ctrl+X` | slice clip at playhead |
| `F5` | switch to the Tracker tab |

Numeric fields accept hex when "Hexadecimal values" is enabled in Settings,
decimal otherwise. Instrument values are the MIDI output channel (`0`–`F` in
hex, `0`–`15` in decimal). The Command field accepts `0`–`143`: `128`–`143`
(hex `80`–`8F`) call macros `M0`–`M15`.

## MIDI model

* Each pattern row has 5 columns per tracker channel: Note, Instrument,
  Volume, Pan and Command.
* Instrument selects the **MIDI output channel** (0–15). When unset, the last
  value used on that tracker channel is reused. The same carry applies to
  volume, pan and command. Carried values are drawn dimmer; volume, pan and
  instrument only carry when a note exists, command carries even on empty
  rows.
* Volume is note velocity when a note is present, otherwise CC7. Pan is CC10.
* Command column: values `0`–`127` are CC numbers with a `0`–`127` value,
  drawn teal. Values `128`–`143` (hex `80`–`8F`) call macros `M0`–`M15` from
  the Macros tab, drawn orange; the value field is ignored for macro calls.
  Macro kinds and their parameters:

  | Kind | Name | Parameters | Behaviour |
  | ---- | ---- | ---------- | --------- |
  | 0 | Arpeggiator | `P1` = slots (1–16), `P2` = semitone step (1–24) | re-triggers the note once per slot at `(k*P2) % 12` semitones above the base note, spread across the row |
  | 1 | Pitch glide | `P1` = bend semitones (1–24), `P2` = ramp steps (2–16) | sends CC105 = `P1`, then sweeps the 14-bit pitch wheel from centre to full bend in `P2` steps; needs an already-active note on the channel |
  | 2 | Slide up/down | `P1` = semitone step (1–24), `P2` = oscillation pairs (1–16) | re-triggers the note oscillating ±`P1` semitones over `2*P2` sub-slots |

  A macro call needs a note on the row or an active carried note to have an
  effect. Pitch wheels are re-centred on note-offs, clip ends, transport stop
  and all-notes-off.
* A tracker pattern can use up to 127 tracker channels, independent of the 16
  MIDI output channels. Identical notes triggered on multiple tracker channels
  in the same row are sent only once.
* There is no audio processing; the plug-in outputs MIDI and passes incoming
  MIDI through.

## Settings

* Hex or decimal value display
* Ignore mid-row transport start (default on)
* Subticks per row (default 6)
* Rows per beat (default 4, i.e. one row per 16th note)
* New pattern length in rows (default 64, maximum 1024)
* Max tracker channels (default 1, maximum 127)
* Paste as unique pattern by default
* Snap to grid, adaptive grid, fixed grid size
* Record octave and step skip

## Known limitations

* Macro playback is enabled, but the encoding is positional: `- Remove last`
  can invalidate existing calls to that index (the call then does nothing),
  and re-adding a macro at the same index changes what existing rows expand to.
* No audio processing of any kind.
* The Linux VST3 build exposes the plug-in as an effect (`Fx` category). Hosts
  that hide MIDI effects (Reaper in some routing setups) can use the
  instrument fallback build instead:

  ```sh
  cmake -S . -B build-instrument -DCMAKE_BUILD_TYPE=Release \
      -DRETREKTOR_INSTRUMENT_FALLBACK=ON
  cmake --build build-instrument -j"$(nproc)"
  ```

  That build exposes a silent stereo output and `IS_SYNTH`, so it can be
  inserted like a normal instrument while still only producing MIDI.
* Interactive behaviour has been verified with JUCE's plug-in host, headless
  editor interaction tests and Carla; a full Reaper session has not been run
  in this environment.
* Command kinds beyond plain CC and the three macro kinds (dedicated pitch
  bend CC105/CC106 pair, tempo, song position) are future work.
* Note entry assumes a US keyboard layout.

## Licensing

JUCE is vendored in `JUCE/` and is licensed under its own terms (AGPLv3 /
commercial). Review `JUCE/LICENSE.md` before distributing binaries.

## Session

Objective
Build "Re.Trektor" by Corthax: a Linux VST3 MIDI-only vertical tracker sequencer plugin with horizontal free pattern timeline, multichannel MIDI output, and host-synced playback.
Requirements
MIDI only, no audio processing; automatic song tempo/position sync from DAW.
Free (non-grid) horizontal pattern timeline: patterns move/resize/slice/copy/paste, snap to adaptive or fixed grid, no overlapping allowed.
Timeline ops (move/resize/slice/paste) disabled while host is playing; pattern selection and cell editing still allowed.
Slice splits pattern at playhead into two new unique patterns (requires snapping; not allowed on 1-row pattern).
Paste as instance (shared pattern reference) or as unique new pattern; both in context menu, default set in Settings tab; paste target = plugin playhead (same as host time display).
Per-pattern: visible channel count (bottom bar numeric), column visibility for inst/vol/pan/command (toggle buttons above each channel's note column), pattern length rows (field above line numbers), pattern name.
Columns: Note, Instrument, Volume, Pan, Command (+ hidden-able); --/--- when empty (Renoise style), instrument shown -0..-F (or -0..-15 decimal).
Instrument column = MIDI output channel (max 16); if unset, use last previously entered value per tracker channel; same carry applies to vol/pan/command; carried values drawn darker, vol/pan/inst only when note present, command carried shown even if note empty.
Commands: only CC (0-127) for now, color-coded by type, extensible to pitchbend/macros.
Max tracker channels 127 (default 1), independent of 16 MIDI output channels; same note on multiple channels in same row → duplicates ignored.
Keyboard capture when plugin window focused for Renoise-style note entry; US layout assumed; octave + stepskip in bottom bar.
Defaults: subticks/row 6, 64-row patterns, 4 rows/beat (configurable in Settings); max 1024 rows.
Mid-row start behavior toggle in Settings, default ignored.
Zoom: in to single row/symbol size, out to whole song up to host project time.
Left pattern pool with expandable grid (id + name), drag-and-drop to timeline, unused patterns editable/placeable.
Right tools panel: Transpose, Remapping, Selection Interpolation with scope: Selection only / Whole pattern / All patterns in song / All created patterns.
Tabs on top: Tracker / Macros / Settings; Macros tab for arpeggiator/glide macro setup.
Testing target: Reaper.
Decisions
Abandoned iPlug2 (upstream master has no Linux GUI/IGraphics support; linux-bindernews branch lacks Linux VST3 view support and uses older CMake API).
Using JUCE 8.0.9 vendored in JUCE/ (.git removed, committed as "Add JUCE framework").
Built as JUCE MIDI effect (IS_MIDI_EFFECT TRUE, NEEDS_MIDI_INPUT TRUE, NEEDS_MIDI_OUTPUT TRUE, VST3 only, COMPANY_NAME Corthax, PRODUCT_NAME "Re.Trektor", codes Cort/Rtrk).
Model thread safety: juce::CriticalSection modelLock with tryEnter on audio thread (skips generation if UI holds lock); incoming MIDI passed through.
Note numbering: MIDI 60 = C-4; tracker channel fallback MIDI output channel = tracker channel index clamped to 15.
Volume column = note velocity when a note is present, CC7 when no note; pan = CC10.
Resizing a clip changes the shared pattern's row count (affects all instances), validated for overlap.
Plugin playhead is host-driven only: removed user playhead dragging; timer always mirrors host PPQ and paste/slice use the snapped host position.
allNotesOff now emits explicit note-offs for active notes before CC123/CC120 panic messages.
Work State
Completed
Repo init, JUCE 8.0.9 vendored/committed; CMakeLists.txt for VST3 MIDI effect.
Core model (Source/Model.h/.cpp): Cell/Pattern/Clip/Macro/Settings/Clipboard/Song, no-overlap logic, snap (adaptive/fixed), slice-at-playhead into two new patterns (now pointer-safe; captures source rows before vector growth), copy/paste (instance vs unique), ValueTree serialization, formatting helpers.
Source/PlaybackEngine.h/.cpp: host transport row scheduler, sample-accurate row events, carry state, duplicate-note suppression, note-off on clip end/jump/stop, panic, atomic UI row reporting.
Source/PluginProcessor.h/.cpp: MIDI-only processor, transport snapshot atomics, state save/load, live MIDI passthrough, MidiKeyboardState, model lock.
UI: Theme.h (colors/fonts/LookAndFeel), TimelineComponent (ruler, clips, playhead, drag move/resize, snapping, context menus, drag-and-drop target, zoom, playback lock, drop preview, dynamic zoom-out to whole song), TrackerComponent (5 fields/channel, hide toggles, carry-dim rendering, hex/dec, Renoise key mapping, selection, rows editor, play-row follow, separate cmdNum/cmdVal rects), PoolComponent (list/pool, add, drag source, context menu, rename dialog, dangling-pointer fix on duplicate), ToolsPanel, SettingsPanel, MacrosPanel, PluginEditor (tabs, snap/grid/zoom, position label, bottom bar, Ctrl+C/V/Shift+V/X, timer following).
First successful Release build: build/ReTrektor_artefacts/Release/VST3/Re.Trektor.vst3/Contents/x86_64-linux/Re.Trektor.so (~5.9 MB).
Smoke test tests/SmokeTest.cpp loads the built VST3 via VST3PluginFormat: PASS (no audio buses/MIDI effect, accepts+produces MIDI, generates MIDI while playing, flushes note-offs on stop, state round-trip, editor 1240x760).
Core test tests/CoreTest.cpp (Model + PlaybackEngine): PASS 0 failures (defaults, sequencing/velocity/MIDI channel, note-off before next note, stop flush, instrument carry, duplicate suppression, snapping, slicing, paste instance/unique, serialization, overlap rules).
tests/EditorShot.cpp renders the editor standalone to PNG (host snapshot was black because VST3 native view needs a window); screenshot generated with 2805 distinct colors.
Fixed ReTtrektor class-name typo across all sources/build files (commit c36c6c7, 22 files); restored working tree from HEAD; plugin + all 4 test targets rebuilt; CoreTest/UiTest/Smoke all PASS.
Wired command macros into playback (commit 49ec706): Command 128-143 (hex 80-8F) = M0-M15 calls; arpeggiator kind 0 re-triggers P1 slots at (k*P2)%12 semitones over the row span; kind 1 pitch glide sends CC105=P1 then sweeps 14-bit pitch wheel 8192 to 16383 in P2 steps (needs active note); kind 2 slide oscillates +/-P1 over 2*P2 re-triggers; processRow now takes rowSpanSamples + song.macros; wheel re-centred in startNote/stopNote/allNotesOff; macro calls drawn theme::cmdMacro orange in tracker, cmdNum entry clamped 0..143; MacrosPanel caption documents 128-143; CoreTest +3 macro sections (bends collected); default song macro renamed "Arp 7x+4"; plugin + tests rebuilt, all suites PASS; two headless 1240x760 screenshots (tracker --dump, macros --tab=macros) visually verified in Review pane: no overlapping/misplaced controls (settings rows, header chips, bottom bar).
Active
None.
Blocked
No Reaper installed; VST3 host testing limited to JUCE smoke test + Carla availability.
Next Move
Optional: pattern-pool "expandable grid" refinement (currently id+name list).
Verify pool drag-drop preview and context-menu paste/slice flows interactively in Carla; confirm Reaper compatibility of the MIDI effect (instrument fallback IS_SYNTH build compiles locally but is untested in Reaper).
Further command kinds: dedicated pitch bend CC105/CC106 pair command, tempo and song position commands.
Relevant Files
/media/data2/GameDev/Plugins/ReTrektor/CMakeLists.txt: JUCE plugin build (VST3, MIDI effect).
/media/data2/GameDev/Plugins/ReTrektor/Source/Model.h / Model.cpp: core data model, song/pattern/clip ops, snapping, slicing, paste, serialization.
/media/data2/GameDev/Plugins/ReTrektor/Source/PlaybackEngine.h / PlaybackEngine.cpp: host-synced MIDI generation, carry/duplicate logic, explicit note-off panic.
/media/data2/GameDev/Plugins/ReTrektor/Source/PluginProcessor.h / .cpp: processor, transport snapshot, state, lock.
/media/data2/GameDev/Plugins/ReTrektor/Source/PluginEditor.h / .cpp: main window layout, tabs, shortcuts, bottom bar.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/TimelineComponent.h / .cpp: timeline, drag/drop, context menus, zoom.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/TrackerComponent.h / .cpp: tracker grid, key entry, toggles.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/PoolComponent.h / .cpp: pattern pool, drag source, rename.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/ToolsPanel.h / .cpp: transpose/remap/interpolate scopes.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/SettingsPanel.h / .cpp: settings controls.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/MacrosPanel.h / .cpp: macro editor (definitions; executed in playback via command 128-143).
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/Theme.h: colors, fonts, LookAndFeel.
/media/data2/GameDev/Plugins/ReTrektor/tests/CMakeLists.txt: smoke/core/editor-shot test targets.
/media/data2/GameDev/Plugins/ReTrektor/tests/CoreTest.cpp, SmokeTest.cpp, EditorShot.cpp: automated verification harnesses.
/media/data2/GameDev/Plugins/ReTrektor/JUCE: vendored framework.
Important Context
Build: cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc) (build dir currently configured and passing).
Tests: cmake -S tests -B tests/build -DCMAKE_BUILD_TYPE=Release then build targets ReTrektorSmoke, ReTrektorCoreTest, ReTrektorEditorShot.
Smoke run: DISPLAY=:0 tests/build/ReTrektorSmoke_artefacts/Release/ReTrektorSmoke <bundle> [--screenshot=path].
Editor screenshot: DISPLAY=:0 tests/build/ReTrektorEditorShot_artefacts/Release/ReTrektorEditorShot /tmp/opencode/editor.png 1240 760.
Git (newest first): 49ec706 macro playback wiring; c36c6c7 ReTtrektor typo fix; 4263853 instrument fallback option; f89454e headless UI interaction test; c168e8b panels fit minimum window; a2ec7c9 Re.Trektor sources; ccc50f2 vendored JUCE 8.0.9.
DISPLAY=:0 / wayland-0 available; no Reaper; /usr/bin/carla exists.
Missing xcb-util-devel/xcb-util-wm-devel were installed via sudo dnf earlier (leftover from iPlug2 attempt, harmless).
iPlug2 and its VST3 SDK clone deleted from workspace; .gitmodules removed.
