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
  paste, state, the volume/pan columns' controller numbers, per-channel column
  visibility (and that hidden columns still play), interpolation of one column
  between the values that already exist (the ramp restarting at each anchor,
  nothing extrapolated, anchors outside the range ignored, a channel range
  narrowing what is touched, the note column's `===` closing a segment,
  rounding and each field's range clamping)
* `ReTrektorSmoke` – loads the built VST3, checks MIDI behavior and state
* `ReTrektorUiTest` – drives the editor components (tracker keys, column chips
  clicked per channel, volume column entering three decimal digits and emitting
  CC7, the settings volume/pan CC sliders driving those columns, tools
  transpose, instrument remap with its source and target dropdowns across
  scopes and carries, selection interpolation with its field and channel
  dropdowns (and that a fresh panel opens on the selection-only scope, so the
  default transposes nothing), timeline click parking the playhead, timeline drop, pool
  placement, pool drag preview covering one item cell rather than the panel,
  window size remembered across a closed and reopened editor,
  channels slider, tracker horizontal scrollbar) and confirms which
  MIDI channel and which controller value the notes and columns end up on
* `ReTrektorEditorShot` – renders the editor headlessly and can dump the
  component layout (`--dump`, optionally `--tab=settings|macros`,
  `--expand-pool` to show the pool grid preview, `--channels=N` to
  widen the active pattern to N channels, narrow it to N/2 and widen it back,
  `--scroll=N` to jump N% along the tracker's horizontal scrollbar (so a wide
  pattern's far channels are shown), `--click-timeline=P` to click P% across the
  timeline and park the playhead at that snapped beat, and `--rows=N` to shrink the
  active pattern to N/2 rows and grow it back to N — both exercise the
  resize-preserves-data behaviour, `--hide=0v1c` to hide the volume column of
  channel 0 and the command column of channel 1 (channel digit plus
  `i`/`v`/`p`/`c` pairs, so a shot can show that chips hide per channel), and
  `--hex` to render the narrower hex field widths, with `--drag-preview=IDX`
  compositing the image JUCE floats at the cursor while pool item IDX is dragged
  out of the pool, so a shot can show that only that item's cell follows the
  pointer), and `--cmd-carry` to write a row that only names a command argument
  below the demo's command at row 20, together with an explicit `0` argument in
  the other channel that never named one, so a shot can show the dim carried
  number over the bright argument next to the explicit zero)

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
  marker. Drag a pattern onto the timeline to place it: while it is dragged it
  floats at the cursor as only its own item — id badge, name, rows/channels, and
  the grid preview when the item is expanded — never the whole pool panel, and
  semi-transparent so it reads as a copy lifted out of the pool. Right-click a
  pattern for actions (edit, place, copy, paste, duplicate, rename, delete)
  including "Toggle grid preview", which grows the item into a preview of its
  first 12
  rows × up to 4 channels (notes, `===` note-offs, channel dividers, beat
  lines, and a teal/orange tick where a command is present). "Copy pattern"
  and "Copy pattern data" both put the pattern's cells on the clipboard; the
  second is a shortcut for grabbing the whole pattern's data so a later paste
  can start a new pattern from it — instance vs unique is chosen when pasting.
* **Timeline** – free (non-grid) horizontal arrangement of pattern clips.
  Clips can be moved, resized, sliced, copied and pasted. Timeline edits are
  disabled while the host is playing; cell editing and pattern selection stay
  available. The plug-in playhead follows the host transport, and clicking the
  timeline (left or right button) parks it at the clicked beat — rounded to the
  current grid when snapping is on — so a paste, a slice or a clip menu act
  where the mouse was; while the host is playing, its transport position owns
  the playhead instead. The ruler above the clips carries only the bar numbers:
  the transport readout (playback state, song position, bpm) that used to sit at
  its right end repeated the top bar's position label and overprinted the bar
  numbers there, so it is gone — the playhead line and its marker stay because
  they show where in the timeline the position is, which the top bar cannot.
* **Tracker** – the active pattern's rows. Column visibility chips (`I`, `V`,
  `P`, `C`) sit above each channel's note column and stay visible so hidden
  columns can always be restored: one chip hides one field of that one channel,
  so hiding the volume of channel 2 leaves the volume of every other channel
  alone. The flags belong to the pattern and the channel, and they follow the
  grow-only channel capacity — a channel dropped by the CHANNELS slider keeps
  its flags, so widening the pattern again restores them. Field widths follow
  the widest text a field can show, so a decimal volume of `0`–`127` needs three
  digits like the pan column. The rows field above the line numbers sets
  the pattern length (shared by every clip that references the pattern).
  Shortening a pattern keeps its rows past the new end, so enlarging it again
  restores the data instead of leaving blanks.
  When the channel columns are wider than the window, a horizontal scrollbar
  appears at the bottom of the view: drag its thumb (or click to jump) to
  reach the channels that do not fit, and shift + mouse wheel scrolls the
  same way. The row-number gutter stays fixed while the columns slide under
  it. Without a strip the bottom edge is a normal row click.
* **Macros tab** – macro definitions (name, kind, two parameters). Definitions
  live with the song and are called from the tracker's Command column with
  command `128`–`143`.
* **Right panel** – Transpose, instrument Remapping and Selection
  Interpolation. Each tool uses the selected scope: selection only, whole
  pattern, all patterns in the song, or all created patterns, but
  Interpolation only reads rows the user picked by hand, so it does nothing
  until the panel is on `Selection only` – where it opens – and Transpose and
  Remap are the tools that need the dropdown widened when nothing is selected.
  Interpolation takes two more dropdowns: what to ramp (`Note`, `Instrument`,
  `Volume`, `Pan`, `CC number`, `CC value`) and how wide it reaches (`All
  channels` across the pattern width, or `Active channel` for the one channel
  the cursor sits in). Only blank cells between two cells that already name a
  value inside the selection get filled, with the value linearly between them,
  so a ramp restarts at every anchor rather than only between the ends of the
  range, and nothing is written before the first anchor or after the last:
  values outside the selection are ignored and nothing outside it is touched.
  In the note column a `===` note-off has no number to ramp to or from, so it
  closes a segment and the blanks on either side of it stay blank. A filled
  cell becomes an explicit value, which is why the dim carried cells are
  written as well. Remapping reads
  a source and a target dropdown side by side with an arrow between them
  (`All` or an instrument, then the target instrument): only the cells whose
  instrument resolves to the source are rewritten, so another instrument
  inside the scope stays put, the dim cells carrying it follow it, and rows
  with no instrument of their own do not get one. `All` targets every
  instrument in the scope. The `Remap` button below the pair applies the edit.
* **Bottom bar** – octave and step skip for keyboard entry, pattern name,
  pattern info, and a CHANNELS slider (1–127) that sets the active pattern's
  channel count directly. 127 is a constant limit, there is no channel setting,
  and narrowing a pattern keeps the removed columns' data.

* Right-click menus open beside the cursor: the timeline clip menu, the timeline
  empty-area menu, the pool item menu and the tracker cell menu all anchor to
  where the mouse was when they were opened.
* The window remembers its size: hosts such as Reaper close a floating plug-in
  window by throwing it away and ask the plug-in for a brand new editor when the
  GUI is opened again, so a new editor starts at the size the closed one had
  (clamped to the 900–2600 × 560–1600 resize limits) instead of always the
  built-in 1240×760. The size belongs to the plug-in instance, not to the saved
  song, so a fresh instance still opens at 1240×760.

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
hex, `0`–`15` in decimal). Volume and Pan take `0`–`127`: three decimal digits
(two in hex), displayed right-aligned like the pan column. The Command field
accepts `0`–`143`: `128`–`143` (hex `80`–`8F`) call macros `M0`–`M15`.

## MIDI model

* Each pattern row has 5 columns per tracker channel: Note, Instrument,
  Volume, Pan and Command.
* Instrument selects the **MIDI output channel** (0–15). When unset, the last
  value used on that tracker channel is reused. The same carry applies to
  volume, pan and command. Carried values are drawn dimmer; volume, pan and
  instrument only carry when a note exists, command carries even on empty
  rows.
* An explicit volume or pan cell always sends its controller value: CC7 (channel
  volume) and CC10 (channel pan) by default, and the Settings tab can point
  either column at any controller `0`–`127`. The controllers are sent before the
  note handling, so even a row that only holds a note-off (`===`) applies them.
  A volume on a row that also has a note is additionally used as the note-on
  velocity, so both arrive; a volume on a row without a note only drives the
  controller. Carried (implicit) values do not re-send.
* Command column: values `0`–`127` are CC numbers with a `0`–`127` value,
  drawn teal. Entering just an argument, with the command number left empty,
  keeps the cell implicit and plays that argument on the command number the
  column carries down from above, which is drawn dim over the bright argument;
  only a column that has never named a command gets the explicit `0` the entry
  has to write, and a column that carries nothing plays nothing. Values
  `128`–`143` (hex `80`–`8F`) call macros `M0`–`M15` from the Macros tab, drawn
  orange; the value field is ignored for macro calls, and a value-only row
  below a macro call stays silent because a macro has no controller argument.
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
* Stuck-note safety net: hosts like Reaper never call a MIDI effect while the
  transport is stopped, so note-offs emitted at stop time may never reach the
  synth. The engine remembers every pitch it has ever sent a note-on for, per
  MIDI channel (survives transport re-arms), always precedes a later note-on
  of such a pitch with a matching note-off, and closes clip ends with a
  tolerance for hosts that report block bounds slightly short.
* There is no audio processing; the plug-in outputs MIDI and passes incoming
  MIDI through.

## Settings

* Hex or decimal value display
* Ignore mid-row transport start (default on)
* Subticks per row (default 6)
* Rows per beat (default 4, i.e. one row per 16th note)
* New pattern length in rows (default 64, maximum 1024) — resizing an existing
  pattern never discards rows: shortening hides the tail and enlarging brings it
  back; only rows inside the visible length are played
* Paste as unique pattern by default
* Snap to grid, adaptive grid, fixed grid size
* Record octave and step skip
* Volume column CC and Pan column CC (default 7 and 10): the controllers those
  columns drive, each `0`–`127`

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
* Reaper does not call a MIDI effect while the transport is stopped: stop-time
  note-offs (e.g. transport stopping exactly at a clip end) are silently
  dropped. The safety net above re-articles the key on the next note-on of the
  same pitch, at the cost of one extra note-off; the moved-clip-from-time-0
  scenario is covered by the CoreTest "Stuck-note guard" section but has not
  been retested in Reaper itself yet.
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
Tracker channel limit is the constant 127 (kMaxTrackerChannels, no user setting; the bottom-bar CHANNELS slider sets it per pattern), independent of 16 MIDI output channels; same note on multiple channels in same row → duplicates ignored.
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
Playhead follow is position-change driven (commit 02c0ca3): the editor only copies host PPQ into the edit playhead when that position actually changed (or the host is playing), so a playhead parked by a timeline click stays put while the host is stopped.
allNotesOff now emits explicit note-offs for active notes before CC123/CC120 panic messages.
Volume column always drives its controller (commit cd3fc2c): an explicit volume/pan cell sends CC7/CC10 by default, the Settings tab names either column's CC (0-127, serialized with the state), and a volume on a row that also has a note additionally sets the note-on velocity, so the column is not velocity-only any more. Field widths derive from a new TrackerComponent::fieldGlyphs (widest text a field can show: note 4, inst 1 + maxDigits, vol/pan maxDigits, cmd cmdNum + cmdVal) instead of fixed character counts, so a decimal volume of 0-127 keeps three digits and the drawn values use the same digit count (right-aligned, space-padded like pan). Column visibility became one byte per channel column (Pattern::vis, bits 1 inst / 2 vol / 4 pan / 8 cmd, default kVisAll) read by showColumn and written by setColumn/toggleColumn, kept grow-only up to channelCapacity() like the cell columns, serialized as base64 bytes in a new "vis" property, and older saves' whole-pattern showInst/showVol/showPan/showCmd are applied to every column as a fallback so previous states keep their display. The chips above a channel toggle that channel only, the pool's duplicate action copies the flags, and hiding the field the cursor sits in parks the cursor on that channel's note column.
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
Expandable grid preview in pattern pool (commit de11909): pool items have variable pitch (collapsed 42 / expanded kExpandedPitch 138; cumulative cellBounds/paint layout); right-click context menu "Toggle grid preview" (expandedIds by pattern id, not serialized); drawGridPreview shows first 12 rows x up to 4 channels (noteName green, === noteOff, dim ---, channel dividers, beat lines every song.settings.rowsPerBeat, 2px cmdCC/cmdMacro tick per command cell); EditorShot gained --expand-pool; plugin + tests rebuilt, Core/Ui/Smoke PASS; third 1240x760 screenshot with expanded pool item visually verified (grid fits, second item shifts down cleanly, dump shows no new OUTSIDE PARENT lines).
Fixed Reaper stuck-note bug (commit b1725b1): root causes = (a) hosts never call a MIDI effect while stopped so stop-time note-offs are lost, (b) "not under playhead" used the block start vs clip start so a block straddling a clip's start closed a note started by the same block at row 0 (clip moved off time 0), leaving the note unclosed at the real clip end, (c) the row-membership 1e-9 guard slack reprocessed rows at exact block boundaries (double note-on). Fixes: sticky per-MIDI-channel everPlayed bitset set by startNote, cleared only by reset(); startNote emits a pre note-off for a pitch it has ever played when the state no longer holds it; PlaybackEngine::forgetTransport() (transport-only reset) used by prepareToPlay instead of reset(); clip-end close tolerance ppqEnd + spanEps >= clipEnd; stillUnder switched to block/clip overlap (ppqStart < end && ppqEnd > clipStart - 1e-9); row guard rewritten to dedup rows already covered by the engine's previous block (nearContinuous/drift). CoreTest + "Stuck-note guard (Reaper-style host)" section (94 play blocks, 1 discarded stopped block, move clip 0 -> 8 beats, 1130 blocks: exactly one re-trigger, pre-off before it, clip-end close at 24.0 beats). Plugin + tests rebuilt; CoreTest/UiTest/Smoke all PASS.
Fixed CHANNELS slider trap (commit b02e52a): bottom-bar slider range was (1, settings.maxTrackerChannels), which defaults to 1, so per-pattern channel growth required the Settings tab first. Range now fixed (1, kMaxTrackerChannels=127); channelsSlider.onValueChange sets active pattern channels and auto-raises song.settings.maxTrackerChannels to the new count (max = biggest count in use + starting count for new patterns; Settings lowering still clamps patterns via applySettingsToModel); handler also refreshes pool + settings panel displays. UiTest + 4 checks driving the real slider via setValue(value, sendNotificationSync): active pattern grows to 3 ch, cap raised, shrinks to 2 ch, cap unchanged by shrinking. Plugin + tests rebuilt; CoreTest/UiTest/Smoke all PASS.
Pattern resize keeps data (commit 121ecd0): Pattern::setLength used to truncate with rows.resize, so shortening a pattern discarded the rows past the new end. Now grow-only: row capacity (rows.size()) never shrinks below the last row holding data, trimHiddenRows() drops trailing all-empty hidden rows to keep memory and saved state compact, capacity() accessor added; the toValueTree data block covers capacity and a new "capacity" property stores it, so hidden rows survive a state round trip (older saves default capacity == length); fromValueTree resizes rows to capacity then trims. sliceClipAt keeps the hidden tail rows in half B and trims empties; syncPlaybackModel shrinks engine-snapshot rows to pattern length (playback reads only visible rows; engine row bounds already use pattern->length); TrackerComponent::refresh clamps cursor and selection rows to the new length; EditorShot gained --rows=N (shrink to N/2 then grow back). CoreTest + "Pattern resize keeps hidden rows" section: shrink 16->8 keeps the row-12 note and instrument, capacity trims to the last content row (13), empty hidden rows trim to length, state round trip restores hidden data, playback over 120 blocks (about 2.56 beats) proves the hidden row is not played while shortened and visible rows still play, and 60 more blocks (to about 3.84 beats) after growing to 16 rows emit the restored row-12 note-on. Plugin + tests rebuilt; CoreTest/UiTest/Smoke all PASS; 1240x760 screenshot with --rows=16 visually verified: demo notes at rows 008-014 present after the temporary shrink to 8, pool grid preview draws only visible row groups, no layout overlap.
Channel columns preserved + constant cap (commit 0716702): the same grow-only capacity now applies to the channel axis: Pattern::setChannels grows each row's cells and never shrinks them (channels is only the visible count), channelCapacity()/trimHiddenChannels() drop hidden columns only when every row's cell there is empty; setLength's inner resize became grow-only too so widening rows cannot discard columns. toValueTree packs the capacity x channelCapacity rectangle (empty fallback cell for ragged rows) and a "capacityChannels" property; fromValueTree resizes both axes then trims. The "Max tracker channels" Settings slider is gone: Settings has no maxTrackerChannels field, the limit is the constant kMaxTrackerChannels = 127, new patterns start at 1 channel, the bottom-bar CHANNELS slider range is that constant, applySettingsToModel no longer clamps patterns (only re-syncs the bottom bar), SettingsPanel.h/.cpp lose maxChannelsSlider and its refresh line, and the settings property write/read was dropped from the state. CoreTest + "Pattern channel count keeps hidden columns" section (12 checks: narrowing keeps the widest column's data, capacity trims only past the data, hidden columns that hold nothing trim away, state round trip of a narrowed pattern restores the hidden column, playback gate: hidden column silent over 60 blocks while visible column plays, and after widening the restored column's note-on fires). UiTest bottom-bar section rewritten to assert channelCapacity stays 3 across a 3->2->3 slider cycle with a note in the widest column and that the note is back after widening. EditorShot --channels=N widens to N channels, fills them, narrows to N/2 and widens back. Plugin + tests rebuilt; CoreTest/UiTest/Smoke all PASS; 1240x760 --channels=4 screenshot visually verified (all 4 columns' notes present after the narrow/widen cycle); --tab=settings screenshot shows the Settings tab with 11 rows, no Max tracker channels row, clean layout (dump OUTSIDE PARENT count 13 = the pre-existing hidden scrollbars and zero-size buttons, tracker tab 27).
Tracker horizontal scrollbar (commit 97f50ea): with up to 127 channel columns the pattern is far wider than the window, and the only way to reach the rest had been shift + mouse wheel (unclamped, undiscoverable). TrackerComponent gained a bottom strip (scrollbarHeight 14px) drawn only while totalWidth > getWidth(): a track from the gutter to the right edge plus a thumb whose width is the visible fraction of the columns and whose position maps scrollX over maxScrollX() = totalWidth - getWidth(). dragScrollbar hit-testing lives in mouseDown (click on the thumb keeps the grab offset, a click elsewhere centers the thumb) and mouseDrag moves the thumb; mouseUp clears the drag state; wheel scroll now uses setScrollX which clamps to [0, maxScrollX]. visibleRowCount() subtracts the strip while it is shown (so the strip never covers rows), and refresh()/clampScroll() clamps scrollRow and scrollX after a model change. Columns are clipped to the columns area by visibleColumn(): a scrolled column never paints over the row-number gutter (header chips, cells and the fieldIndexAtX hit test use the clipped rect, and a click at x < gutterWidth hits nothing), so the gutter stays fixed while columns slide under it. ensureCursorVisible() now also shifts scrollX minimally so keyboard entry into a far channel is not blind. EditorShot gained --scroll=N (percent of max scroll, implemented as a simulated strip click via the public mouseDown/mouseUp, after refresh() so the columns are laid out) and its --channels=N demo notes are kept within the first 23 rows so a scrolled shot shows whole far columns. UiTest + 6 checks: 30-channel pattern, strip click at the right end makes a probe near the right edge land on channel 29 exactly (probe numbers: far=29, leftAtMax=24, mid=8, tracker 908x538), dragging the thumb left shifts the window, at full scroll the first visible column is no longer channel 0, a click in the gutter does not pick a scrolled column, and after narrowing to 1 channel (no strip) the same bottom-edge click selects a row and leaves the channel alone. Plugin + tests rebuilt; CoreTest 93 checks, UiTest 31 checks, Smoke all PASS; 1240x760 screenshots visually verified: unscrolled (strip, thumb at the left, channels 00-06), --scroll=50 (channels 13-18, thumb mid-track, far columns' notes at row 023 visible), --scroll=100 (channels 24-29 flush with the right edge, thumb at the track end), 620x520 window (288x298 tracker, strip still correct with a ~236px track).
Copy pattern data (unique) removed (commit ce6422d): the timeline clip context menu had "Copy pattern data (unique)" whose handler called the same song.copyPattern (patternId) as "Copy pattern" and only appended " data" to the clipboard name, so the option was redundant: the unique/instance choice belongs to paste, and the menu already offers both "Paste as instance at playhead" and "Paste as unique pattern at playhead". The item and its switch case were dropped, the remaining ids renumbered (1 Copy pattern, 2 Paste instance, 3 Paste unique, 4 Slice at playhead, 5 Duplicate, 6 Delete instance, 7 Delete pattern from pool), and the menu layout in the empty-area branch (ids 3/4/10) is untouched. The pool context menu keeps its pair on request: there "Copy pattern data" is a deliberate shortcut that copies the whole pattern data for a later paste, no code change. Plugin + tests rebuilt; CoreTest 93 checks, UiTest 32 checks, Smoke all PASS.
Timeline click parks the playhead (commit 02c0ca3): clicking the timeline gave no visual target, so paste/slice/clip-menu placement was guesswork. TimelineComponent::jumpPlayheadToClickedX (int x) sets song.editPlayheadBeats = song.snapBeats (beatAtX ((float) x), pixelsPerBeat) and returns while playbackLocked() (the host owns the playhead while it plays); mouseDown calls it right after the middle-button pan return and before the clip hit test, so the click works for left and right buttons alike and both the clip menu and the empty-area menu (which also gained a repaint()) build their pasteBeat = snapBeats (editPlayheadBeats, ppb) at the clicked spot. beatAtX / xAtBeat moved from private to public so tests can name the clicked beat. The mirror in PluginEditor::timerCallback had run on every tick with |ppq - editPlayheadBeats| > 1e-6, which pulled a parked playhead back on the next 20 Hz tick; it now compares against a new Song::transportMirrorBeats stamp (PPQ beats, negative before the first mirror, reset by Song::reset) and copies only when the host is playing or its position actually changed, so a stopped host leaves the parked playhead alone while any host move resumes the follow; the mirror body became the public ReTrektorAudioProcessorEditor::followHostTransport(). UiTest + 8 checks use a tickHost lambda (move the mock playhead, one processBlock so the transport snapshot atomics refresh, then followHostTransport() for one tick - the JUCE editor timer never fires in a console app, which is why the earlier wall-clock waits left the stamp at -1): a stopped tick records the mirrored position, a click at x=300 (grid 0.5 beats, ppb 48) parks the playhead exactly on the snapped beat, a stopped tick leaves it there, a click at x=120 moves it, a host move to 7.25 resumes the follow and records that stamp, and moving back to 0 keeps following. EditorShot gained --click-timeline=P (percent across the timeline, needs the new ui/TimelineComponent.h include) printing "timeline click at x=704/908 parked the playhead at beat 14.5 (ppb 48)"; the 1240x760 shot visually shows the red playhead line at 696px = 14.5 * 48, inside the 16-beat Main clip. Plugin + tests rebuilt; CoreTest PASS, UiTest 40 checks, Smoke all PASS, EditorShot exit 0.
Menus open at the cursor (commit 08af633): every popup appeared beside the bottom-left corner of the panel instead of under the mouse. All four showMenuAsync call sites (timeline clip menu, timeline empty-area menu, pool item menu, tracker cell menu) passed juce::PopupMenu::Options().withTargetComponent (this) only, and withTargetComponent also sets the target screen area to the whole component's bounds, which is what PopupPanel anchors the menu to. Each chain now ends with .withMousePosition () after withTargetComponent (), which re-anchors the target area to Desktop::getMousePosition() (the menus are built synchronously inside mouseDown, so that is the click point) while the target component is kept for hi-DPI scaling; this is the same chain JUCE's own widgets use (TableHeaderComponent, TextEditor, Slider). Plugin + tests rebuilt; CoreTest 93 checks, UiTest 40 checks, Smoke 13 checks, all PASS - popups stay interactive-only, so the anchoring itself needs a click in a host to confirm.
Instrument remap takes a source and a target (commit f9acf12): the right panel had one instrument dropdown whose choice meant "write this instrument over every note in the scope", which clobbered rows that had no instrument of their own and ignored the dim cells carrying one down, so moving one instrument onto another was not expressible. ToolsPanel now has remapFromBox (items All id 1, then -0 .. -F ids 2..17) and remapToBox (only -0 .. -F ids 1..16), both styled by a shared styleCombo lambda and filled by an addInstruments lambda that reuses the tracker's hex labels, with addInstruments taking the id offset; the defaults All -> -0 keep the old meaning. applyRemap walks each tracker channel from row 0 keeping the instrument the column resolves to before (before) and after (after) the edit: an explicit cell inside the row range matching the source (or any instrument when the source is All) is rewritten, and an implicit cell is pinned to the target only when its carried source value would otherwise still resolve to the old instrument, which is why a row stays implicit once the explicit cell above it already moved (only the first rows inside a narrower selection need an explicit value). rowRange (new private helper returning { start, end } or { -1, -1 }) names the rows a scope selects and is now shared by applyToRows, applyRemap and applyInterpolate, which had the selection maths duplicated inline. Layout: the two dropdowns sit in one row 62 px wide each with an 18 px gap, paint() draws an arrow there (three juce::Line<float> strokes in theme::textDim from getBounds(); GraphicsContext::drawLine takes a Line<float> plus thickness, not two points), the Remap button has its own row below and the section label is just REMAP; the panel still fits the minimum window (dump: scope combo 8,38 140x22, from 9,159 60x20, to 87,159 60x20, Remap 9,185 138x20, Interpolate 8,232 140x22 inside 156x448). prepareToPlay now calls setRateAndBufferSizeDetails (sampleRateIn, samplesPerBlock): the transport maths reads the rate back with getSampleRate(), which a bare prepareToPlay call (tests, and hosts that do not go through the JUCE wrapper) left at 0, and engine.process guards the divide with jmax (1.0, sampleRate), so one 64-sample block covered 128 beats at 120 BPM and the UI test's MIDI checks passed only by accident. UiTest + 26 checks (66 total, PASS) with a collectCombos helper (dropdowns in child order: scope, from, to) and a noteChannel helper (JUCE's getChannel() is 1-based, the tracker numbers channels from 0, so the helper subtracts 1): item counts and labels (from = 17 items with "All" first, to = 16 items "-0" .. "-F"), defaults, -3 -> -5 whole pattern (explicit cell moves to inst 5, another channel's inst 7 stays, the row below stays implicit kEmpty, and playback puts notes 65, 66 and 67 on channels 5, 5 and 7), a selection scope of rows 6-7 with the source row 5 outside it (the carried row 6 is pinned to inst 9, row 7 stays implicit, the explicit inst 6 at rows 5 and 9 outside the selection stays put, playback puts 68, 69 and 71 on channels 9, 9 and 6) and All -> -B (every explicit instrument moves to 11, the rows already resolving to 11 stay implicit, rows with no carried instrument get none, and playback puts all of them on 11). Plugin + tests rebuilt; CoreTest 93 checks, UiTest 66 checks, Smoke 13 checks, all PASS; two --dump screenshots (1240x760 and the 620x520 minimum) cropped to the tools panel and blown up 4x visually verified: "All" and "-0" in one row with the arrow between the boxes, the Remap button on its own row, no overlap.
Per-channel column visibility, three-digit volume and configurable column CCs (commit cd3fc2c): the volume field had a fixed two-character width in TrackerComponent::layoutFields while maxDigitsFor (vol) already allowed three decimal digits, so volumes above 99 were squeezed into a two-glyph field and the column read like a two-digit one. Field widths now come from a new TrackerComponent::fieldGlyphs (widest text a field can show: note 4, instrument 1 + digits, volume/pan digits, command cmdNum + cmdVal digits) that layoutFields multiplies by the mono glyph width, and drawRows prints vol, pan, cmdNum and cmdVal with that same digit count, so decimal numbers are right-aligned and space-padded like pan always was while hex keeps its two digits (the instrument field also learned the dash: 2 hex, 3 decimal characters). Song::Settings gained volumeCc = 7 and panCc = 10, written and read back clamped to 0-127 in the SETTINGS tree, exposed as the last two rows of the Settings tab ("Volume column CC" and "Pan column CC", 0-127, refresh() pushes the model values back into the sliders), whose onChange writes song.settings and fires onSettingsChanged, which re-syncs the playback snapshot. PlaybackEngine caches the two numbers in new members at the start of process() so processRow can use them; the volume emission no longer requires cell.note == kEmpty, so an explicit volume cell always emits controllerEvent (midiCh + 1, volumeCc, vol) next to the note-on velocity path, and pan emits panCc. The two emissions were then hoisted above the note-off branch of processRow (commit 3f14d6f), so a row whose note is only "===" still applies an explicit volume/pan instead of being skipped; the pending-event sort keeps the controller (order 2) just after that row's note-off (order 0) at the same offset, so the channel changes once the note has stopped. Pattern visibility became Pattern::vis (one byte per channel column, bits 1 instrument / 2 volume / 4 pan / 8 command, kVisAll default) with showColumn, setColumn and toggleColumn accessors kept grow-only alongside channelCapacity(); the state stores them base64-encoded in a new "vis" property (visCount() entries) and reads an older save's whole-pattern showInst/showVol/showPan/showCmd by applying them to every column, so previous sessions keep their display. TrackerComponent::layoutFields gates each field by p.showColumn (ch, i), drawHeader reads the per-channel states, mouseDown toggles only the channel whose chip was clicked - and parks the cursor on that channel's note column when it just hid the field the cursor was in - moveCursor rebuilds the field list for the channel it lands on (a fieldsFor lambda, so wrapping backwards lands on the last visible field of the previous channel), and the pool's duplicate action copies the vis vector. TrackerComponent gained public inline getCursorField, fieldBoundsFor and chipBoundsFor so the tests can name a field or a chip; EditorShot gained --hex and --hide= pairs of channel digit plus i/v/p/c. CoreTest + "Volume + pan controllers" (9 checks: three-digit decimal volume kept, CC7 emitted with and without a note, CC10 from pan, velocity still set, a note-off row still applies its explicit volume and pan controllers, configured CC5/CC11 drive the columns, no stray CC7/CC10 after reconfiguring, both numbers restored from a state), + a defaults check that the columns start on CC7 and CC10, and + "Column visibility" (11 checks: hidden on one channel, visible on the others, command hidden only there, the hidden channel survives in the capacity, a state round trip restores the per-field flags and the hidden channel's cells, widening restores that channel's flags, new patterns start all visible, hidden columns still play their controllers). UiTest + 19 checks: the volume field is as wide as the pan field, three digits enter 100, a lone volume emits CC7, a volume next to a note emits CC7 and sets the note-on velocity, the two settings sliders change the CC numbers and the columns follow them with no CC7/CC10 left, chip clicks on channels 1 and 2 hide only there, hidden columns still play, and both chips restore. Plugin + tests rebuilt; CoreTest 114 checks, UiTest 85 checks, Smoke 13 checks all PASS; screenshots verified: 1240x760 --hide=0v1c --channels=4 (channel 0 volume chip dim, channel 1 command chip dim, the remaining columns pack left, decimal volumes right-aligned such as 100), --hex --hide=1v --channels=3 (two-character hex volume, narrower command pair), and --tab=settings at 1240x760 and at the 620x520 minimum (12 settings rows fit, last slider box at 408 plus 21 inside the 448-high panel, showing the default 7 and 10).
Pool drags float one item, not the panel (commit df03559): startDragging was called with a null ScaledImage, so JUCE snapshotted the whole source component and dragging a pattern out of the pool carried the entire panel (all its rows, header and the "+" button) along the cursor. PoolComponent now builds the drag image itself: dragCellBounds gives the on-screen part of the item's cell (cellBounds clipped to the panel, so a partly scrolled item clips too), dragPreviewImage snapshots just that rect at 2x and fades it to 75% alpha so it reads as a copy lifted out of the pool, and mouseDrag passes it with imageOffset = cell top-left minus the current pointer position, which JUCE negates and clamps into the image so the point of the item that was grabbed stays under the pointer (the same idiom JUCE's own TreeView uses). The pool panel is opaque, so the snapshot is converted to ARGB before the fade. Placement did not change: the timeline reads details.localPosition, which JUCE computes in the target component's own coordinates, so the drop still lands at the cursor's x. UiTest + 5 checks (the preview is the item's cell captured at 2x, one row of the pool rather than the panel, and an expanded item drags its taller grid preview); EditorShot gained --drag-preview=IDX, which composites the chip mid-height next to the pool, and the shot with --expand-pool shows a 168x134 chip (image 336x268, scale 2) with the item's id badge, name, rows/channels line and its note grid, over the tracker area rather than a full-height panel copy. Plugin + tests rebuilt; CoreTest 114, UiTest 90, Smoke 13 all PASS.
Reopened editors remember the window size (commit c12431b): Reaper closes a floating plug-in window by destroying the editor view and asks the plug-in for a brand new editor when the GUI is opened again, so the fresh editor always came back at the built-in 1240x760 no matter what size the user had dragged it to. JUCE 8 has no GUI-state hook (there is no GUIStateHandler, and AudioProcessor::editorBeingDeleted is not virtual, so it cannot be overridden), so the memory is ours: ReTrektorAudioProcessor holds juce::Rectangle<int> rememberedEditorBounds read by getRememberedEditorBounds and written by rememberEditorBounds, ReTrektorAudioProcessorEditor::resized() records getLocalBounds() on every layout (JUCE calls resized() synchronously inside setBounds, so a host resize is captured as it happens), and the editor's constructor starts from that size with setBoundsConstrained (remembered.withZeroOrigin()) instead of setSize (1240, 760). The remembered size is read before setResizeLimits because setResizeLimits ends in setBoundsConstrained (getBounds()), which on a fresh zero-sized editor clamps it to the 900x560 minimum and fires resized() - reading after that call recorded the minimum instead of the previous window's size. The applied size is clamped by the 900-2600 x 560-1600 limits, and resized() then records the clamped value, so an out-of-range remembered size (4000x3000) never comes back wider than 2600x1600. The size belongs to the plug-in instance rather than to the saved song, so a fresh instance still opens at 1240x760. UiTest + 5 checks: the laid-out size is recorded, a fresh editor created after proc.editorBeingDeleted + deleting the old one comes back at 1000x640, and a remembered 4000x3000 comes back clamped to 2600x1600. Plugin + tests rebuilt; CoreTest 114, UiTest 95, Smoke 13 all PASS.
Timeline ruler drops the transport readout (commit d2f672e): the ruler drew PLAY/STOP, bar:beat:row and bpm at its right end, which repeated the top bar's position label and overprinted the bar numbers that sit under it, so the numbers near the right edge were unreadable. TimelineComponent::drawRuler now draws only the fill, the bottom edge, the bar ticks and the bar numbers; the playhead line and its marker (drawPlayhead) stay because they place the position inside the timeline, which the top bar cannot. Two members the readout had once driven (lastPlayheadBar, transportText) were already unused and removed from the header. Plugin + tests rebuilt; CoreTest 114, UiTest 95, Smoke 13 all PASS (no check covers the drawing itself); the 1240x760 EditorShot ruler strip (timeline at 176,38 908x150) blown up 2x shows bar numbers 1-5 with their ticks and no text at the right end, while the top bar's own label stays host-driven (updatePositionLabel runs only from the editor timer, which never fires in a console app, so the headless shot leaves it blank).
Command argument carries the command number (commit 76115ec): typing only an argument in the command column pinned it to CC 0, because TrackerComponent::applyValue (Field::cmdVal) wrote cell.cmd = 0 whenever the cell had no command number, so an argument entered below a row that named CC17 fired CC0 instead. The entry now leaves the cell implicit when the column already carries a command down to that row: a new public TrackerComponent::carriedCommandNumber (row, ch) walks up the column and returns the number the cell would carry, or -1, and applyValue writes the explicit 0 only when nothing is carried, which is the only case where a 0 means anything. PlaybackEngine::processRow answers that shape with a new else-if: a row whose cmd is kEmpty but whose cmdVal is set emits controllerEvent (midiCh + 1, state.lastCmd, cmdVal) when state.lastCmd names a plain CC, and emits nothing when the column never named a command or named a macro (macros take no controller argument), so the silent cases invent no CC0. drawRows also stopped hiding that argument: cmdVal is read from the cell instead of only when the cell owns a command number, so the implicit row paints its bright argument under the dim carried number. CoreTest + "A command argument without a command number" (6 checks: the row naming CC37 emits 8, the value-only row below it emits the carried CC37 with 99, no CC0, the value-only cell stays implicit in the model, a lone argument with nothing carried above emits no controller, a value-only row under a macro call emits none), UiTest + 10 checks (three decimal digits enter 45 on the cursor row and CC0 plays it while nothing is carried above, once CC17 with 8 is named at row 0 a value-only entry at row 5 keeps cmd == kEmpty and the carried CC17 plays 63 with nothing on CC0); plugin + tests rebuilt, CoreTest 120, UiTest 105, Smoke 13 all PASS, and the 1240x760 EditorShot --cmd-carry tracker crop shows 74 64 bright on row 20 with 74 dim and 99 bright on row 21 in the same channel, plus 0 12 in the command pair of a channel that never named one.
Interpolation fills the gaps between the values that already exist (commit 2e67b16): the right panel had only transpose and remap, so ramping a column across a selection meant writing every cell by hand. Pattern::interpolate (InterpField field, rowStart, rowEnd, chStart, chEnd) lives in the model layer so CoreTest can cover the algorithm and the panel stays thin: InterpField (note, inst, vol, pan, cmdNum, cmdVal) is a separate enum in Model.h, named that way because rtr::TrackerComponent::Field is taken, and it has to be declared before struct Pattern because the member declaration names it, which is why inserting it where the earlier enums sit broke the build. interpolate walks each requested channel keeping the previous anchor, writes the value linearly between two neighbouring anchors into the blank cells between them, rounds with std::llround, clamps each field to its own range with juce::jlimit through a fieldTop helper (instrument kMidiChannelCount - 1, the macro range kMacroCmdBase .. kMacroCmdBase + kMaxMacros - 1, otherwise 127, with every enumerators listed so no default: case hides a new field) and returns the number of cells written; a file-local at() lambda answers out-of-range and ragged rows with nullptr. Anchors are explicit values only, so the dim carried cells are kEmpty blanks and get written, which is why an interpolated cell is explicit; nothing is extrapolated before the first or after the last anchor, cells outside the rectangle are ignored and nothing outside it is written. In the note column kNoteOff has no number to ramp to or from, so it closes a segment: it is never an anchor, never written, and it parks the anchor at row -1, which is why blanks either side of a === stay blank. ToolsPanel gained interpolateFieldBox (Note / Instrument / Volume / Pan / CC number / CC value) whose ids follow the InterpField enumerators so applyInterpolate only has to subtract 1 and cast, and interpolateWhereBox (All channels for the whole pattern width, Active channel for one channel), a getCursorChannel hook that PluginEditor wires to the tracker's cursor channel, and applyInterpolate, which maps the where choice to chStart/chEnd (both the cursor channel clamped, so a pattern that is only selected rather than being edited interpolates track 0) and reports onModelChanged. The scope gate was kept rather than auto-widened and the scopes were not widened for interpolation: interpolation only makes sense on a row range the user picked by hand, so scopeBox now opens on "Selection only" with setSelectedId (1), at the cost the user accepted that Transpose and Remap need the dropdown moved to a wider scope when nothing is selected. Layout: the two dropdowns are stacked, one per 22 px row with their own INTERPOLATE and CHANNELS labels in paint() and in resized(), rather than sitting side by side as the remap pair does, which grew the panel from 156x448 to 156x488 (the words "channels" match the bottom bar's CHANNELS slider rather than saying "columns"). CoreTest + "Interpolation fills the gaps between anchors" (segment-wise fill inside a range, nothing extrapolated, anchors outside the range ignored, a channel range narrowing what is touched, an instrument anchor of 99 clamping to 15, a cmdVal ramp leaving cmd at kEmpty and a cmdNum ramp leaving cmdVal at kEmpty, the macro range clamping at 143, rounding 3.5 -> 4 and 63.5 -> 64, and the note-off barrier in both directions); UiTest + a "Tools selection interpolation" block (drag rows 10-15 inside channel 1's columns with chipBoundsFor (1, 1).getCentreX(), park the cursor channel with the tracker's own key, the wider scope no-ops then "Selection only" fills, "Active channel" touches only channel 1, "All channels" also fills channel 0, and CC7 60 playing at row 13 proves the written cells reached the playback snapshot) plus the fresh-panel default-scope checks that also assert the default transposes nothing while nothing is selected. Plugin + tests rebuilt; CoreTest 157, UiTest 127, Smoke 13 all PASS; the 900x560 --dump keeps the tools panel 156x488 with the two interpolation combos at y=232 and y=268 and the Interpolate button at 8,294 140x22, and a 4x crop of the panel shows the labels and dropdowns with no overlap.
Active
None.
Blocked
No Reaper installed; VST3 host testing limited to JUCE smoke test + Carla availability.
Next Move
Re-test in Reaper: moving a pattern from time 0 (the stuck-note scenario) is now guarded by the engine and covered by CoreTest, but not yet confirmed in Reaper itself.
Confirm in the DAW that a timeline click (left and right button) parks the playhead where a paste lands, that the parked playhead survives while the host is stopped, and that the follow resumes once the host transport moves.
Confirm interactively that the right-click menus now open beside the cursor in the timeline, pool and tracker views.
Verify pool drag-drop preview and context-menu paste/slice flows interactively in Carla; confirm Reaper compatibility of the MIDI effect (instrument fallback IS_SYNTH build compiles locally but is untested in Reaper).
Further command kinds: dedicated pitch bend CC105/CC106 pair command, tempo and song position commands.
Grid preview currently always starts at row 0 (no preview scrolling yet).
Confirm in the DAW that the volume column now emits CC7 (CC10 for pan), that a note-off row (`===`) with an explicit volume/pan still applies both controllers, that the Settings tab's Volume column CC / Pan column CC sliders repoint them, and that a channel's column chips hide only that channel's field.
Confirm interactively that dragging a pattern from the pool floats only that item (its grid preview when expanded, semi-transparent) under the pointer, that the grabbed spot stays under the cursor, and that the clip still lands at the drop position.
Confirm in the DAW that Remap moves only the instrument named in the source dropdown, that the dim carried rows follow it, and that All in the source box covers every instrument in the scope.
Confirm in the DAW that closing a floating Re.Trektor window and opening it again reopens it at the size it had before, and that a size dragged to the window's corner is what comes back.
Confirm in the DAW that the timeline ruler now shows only bar numbers, with the playback state, song position and bpm coming from the top bar's position label.
Confirm in the DAW that a command argument entered with the command number empty plays the command number carried down that column (the row shows the carried number dim next to the bright argument), that a column nothing was ever entered in still needs the explicit 0, and that a value below a macro call does nothing.
Confirm in the DAW that interpolation ramps only the blanks between the values that already exist inside the rows selected by hand, that Active channel is the channel the cursor sits in, and that Transpose and Remap now need the scope dropdown moved to a wider scope while nothing is selected.
Relevant Files
/media/data2/GameDev/Plugins/ReTrektor/CMakeLists.txt: JUCE plugin build (VST3, MIDI effect).
/media/data2/GameDev/Plugins/ReTrektor/Source/Model.h / Model.cpp: core data model, song/pattern/clip ops, snapping, slicing, paste, serialization, per-channel column visibility bytes (Pattern::vis), the volume/pan CC settings and the interpolation of one column between the values that already exist (InterpField, Pattern::interpolate).
/media/data2/GameDev/Plugins/ReTrektor/Source/PlaybackEngine.h / PlaybackEngine.cpp: host-synced MIDI generation, carry/duplicate logic, explicit note-off panic, volume/pan driven by the Settings CC numbers, a value-only row using the command number carried in the column.
/media/data2/GameDev/Plugins/ReTrektor/Source/PluginProcessor.h / .cpp: processor, transport snapshot, state, lock, the size of the last GUI shown.
/media/data2/GameDev/Plugins/ReTrektor/Source/PluginEditor.h / .cpp: main window layout, tabs, shortcuts, bottom bar, starting a new editor at the remembered window size.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/TimelineComponent.h / .cpp: timeline, drag/drop, context menus, zoom, ruler (bar numbers only, no transport readout).
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/TrackerComponent.h / .cpp: tracker grid, key entry, per-channel column visibility chips, field widths from fieldGlyphs, an entered command argument staying implicit over the command number carried in the column (carriedCommandNumber).
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/PoolComponent.h / .cpp: pattern pool, drag source (the drag image is the item's cell), grid preview, rename.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/ToolsPanel.h / .cpp: transpose, remap (source dropdown All/-0..-F -> target dropdown -0..-F, rowRange shared with applyToRows and applyInterpolate), interpolation with its field dropdown (Note/Instrument/Volume/Pan/CC number/CC value, ids following the InterpField enumerators), its channel dropdown (All channels/Active channel) and the scope box defaulting to "Selection only".
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/SettingsPanel.h / .cpp: settings controls, including the Volume column CC and Pan column CC sliders.
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
Git (newest first): ee29e69 pin the two float routines the plug-in takes through libm's newest symbol versions so a host with an older libm loads the binary; 2e67b16 interpolation fills the blanks between the values that already exist, with field and channel dropdowns and the panel opening on the selection-only scope; 40345cc document that a command argument uses the carried command number; 76115ec a command argument without a command number uses the command carried in the column; 205cfe1 document that the timeline ruler keeps only bar numbers; d2f672e the timeline ruler drops the transport readout; 8d077f7 document that a reopened editor keeps the window size; c12431b a reopened editor comes back at the size the closed one had; ece35e2 document that a pool drag floats one item, not the pool panel; df03559 a pool drag floats only the item's cell at the cursor; 3f14d6f volume/pan controllers precede the note-off branch; cd3fc2c volume column drives CC7, Settings name the volume/pan CC numbers, chips hide one field of one channel; f9acf12 instrument remap reads a source and a target dropdown; 08af633 anchor right-click menus at the cursor; 32499ef document that a timeline click parks the playhead; 02c0ca3 jump the playhead to a timeline click; 117744f document pool copy actions and the removed redundant clip menu item; ce6422d drop the redundant "Copy pattern data (unique)" clip menu action; ae95cac scrollbar + --scroll docs; 97f50ea tracker horizontal scrollbar; 93dd668 channel/limit docs; 0716702 hidden channel columns + constant 127 channel cap; 121ecd0 pattern resize preserves hidden rows; 9ffaaa4 EditorShot
--channels=N flag; a5e6e2d bottom-bar/settings channel docs; b02e52a CHANNELS slider direct edit; 1173682 stuck-note docs; b1725b1 stuck-note fix for hosts that skip MIDI-effect calls while stopped; de11909 pool grid preview; c4a8422 README macro-playback update; 49ec706 macro playback wiring; c36c6c7 ReTtrektor typo fix; 4263853 instrument fallback option; f89454e headless UI interaction test; c168e8b panels fit minimum window; a2ec7c9 Re.Trektor sources; ccc50f2 vendored JUCE 8.0.9.
DISPLAY=:0 / wayland-0 available; no Reaper; /usr/bin/carla exists.
Missing xcb-util-devel/xcb-util-wm-devel were installed via sudo dnf earlier (leftover from iPlug2 attempt, harmless).
iPlug2 and its VST3 SDK clone deleted from workspace; .gitmodules removed.
