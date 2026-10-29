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
build/ReTtrektor_artefacts/Release/VST3/Re.Trektor.vst3
```

Install it by copying that bundle to `~/.vst3/` (or another VST3 path) and
rescanning in the host.

## Tests

```sh
cmake -S tests -B tests/build -DCMAKE_BUILD_TYPE=Release
cmake --build tests/build -j"$(nproc)"
```

* `ReTtrektorCoreTest` – model, sequencing, snapping, slicing, paste, state
* `ReTtrektorSmoke` – loads the built VST3, checks MIDI behavior and state
* `ReTtrektorUiTest` – drives the editor components (tracker keys, column
  chips, tools, timeline drop, pool placement) and confirms MIDI output
* `ReTtrektorEditorShot` – renders the editor headlessly and can dump the
  component layout (`--dump`, optionally `--tab=settings|macros`)

Example:

```sh
tests/build/ReTtrektorCoreTest_artefacts/Release/ReTtrektorCoreTest
tests/build/ReTtrektorUiTest_artefacts/Release/ReTtrektorUiTest
tests/build/ReTtrektorSmoke_artefacts/Release/ReTtrektorSmoke \
    build/ReTtrektor_artefacts/Release/VST3/Re.Trektor.vst3
DISPLAY=:0 tests/build/ReTtrektorEditorShot_artefacts/Release/ReTtrektorEditorShot \
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
hex, `0`–`15` in decimal).

## MIDI model

* Each pattern row has 5 columns per tracker channel: Note, Instrument,
  Volume, Pan and Command.
* Instrument selects the **MIDI output channel** (0–15). When unset, the last
  value used on that tracker channel is reused. The same carry applies to
  volume, pan and command. Carried values are drawn dimmer; volume, pan and
  instrument only carry when a note exists, command carries even on empty
  rows.
* Volume is note velocity when a note is present, otherwise CC7. Pan is CC10.
* Commands are CC numbers 0–127 with a 0–127 value, shown color-coded by type.
  The command system is designed to be extended with pitch bend and macros.
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

* **Macros are stored, not executed.** The Macros tab keeps arpeggiator /
  glide / slide definitions with the song and serialises them, but playback
  integration is not enabled yet.
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
* Only the CC command type is implemented; pitch bend and macro commands are
  future work.
* Note entry assumes a US keyboard layout.

## Licensing

JUCE is vendored in `JUCE/` and is licensed under its own terms (AGPLv3 /
commercial). Review `JUCE/LICENSE.md` before distributing binaries.
