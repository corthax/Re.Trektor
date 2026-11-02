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
  chips, tools transpose, instrument remap with its source and target
  dropdowns across scopes and carries, timeline click parking the playhead,
  timeline drop, pool placement, channels slider, tracker horizontal
  scrollbar) and confirms which MIDI channel the notes end up on
* `ReTrektorEditorShot` – renders the editor headlessly and can dump the
  component layout (`--dump`, optionally `--tab=settings|macros`,
  `--expand-pool` to show the pool grid preview, `--channels=N` to
  widen the active pattern to N channels, narrow it to N/2 and widen it back,
  `--scroll=N` to jump N% along the tracker's horizontal scrollbar (so a wide
  pattern's far channels are shown), `--click-timeline=P` to click P% across the
  timeline and park the playhead at that snapped beat, and `--rows=N` to shrink the
  active pattern to N/2 rows and grow it back to N — both exercise the
  resize-preserves-data behaviour)

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
  marker. Drag a pattern onto the timeline to place it. Right-click a pattern
  for actions (edit, place, copy, paste, duplicate, rename, delete) including
  "Toggle grid preview", which grows the item into a preview of its first 12
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
  the playhead instead.
* **Tracker** – the active pattern's rows. Column visibility chips (`I`, `V`,
  `P`, `C`) sit above each channel's note column and stay visible so hidden
  columns can always be restored. The rows field above the line numbers sets
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
  pattern, all patterns in the song, or all created patterns. Remapping reads
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
Confirm in the DAW that Remap moves only the instrument named in the source dropdown, that the dim carried rows follow it, and that All in the source box covers every instrument in the scope.
Relevant Files
/media/data2/GameDev/Plugins/ReTrektor/CMakeLists.txt: JUCE plugin build (VST3, MIDI effect).
/media/data2/GameDev/Plugins/ReTrektor/Source/Model.h / Model.cpp: core data model, song/pattern/clip ops, snapping, slicing, paste, serialization.
/media/data2/GameDev/Plugins/ReTrektor/Source/PlaybackEngine.h / PlaybackEngine.cpp: host-synced MIDI generation, carry/duplicate logic, explicit note-off panic.
/media/data2/GameDev/Plugins/ReTrektor/Source/PluginProcessor.h / .cpp: processor, transport snapshot, state, lock.
/media/data2/GameDev/Plugins/ReTrektor/Source/PluginEditor.h / .cpp: main window layout, tabs, shortcuts, bottom bar.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/TimelineComponent.h / .cpp: timeline, drag/drop, context menus, zoom.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/TrackerComponent.h / .cpp: tracker grid, key entry, toggles.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/PoolComponent.h / .cpp: pattern pool, drag source, rename.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/ToolsPanel.h / .cpp: transpose, remap (source dropdown All/-0..-F -> target dropdown -0..-F, rowRange shared with applyToRows and applyInterpolate), interpolate scopes.
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
Git (newest first): f9acf12 instrument remap reads a source and a target dropdown; 08af633 anchor right-click menus at the cursor; 32499ef document that a timeline click parks the playhead; 02c0ca3 jump the playhead to a timeline click; 117744f document pool copy actions and the removed redundant clip menu item; ce6422d drop the redundant "Copy pattern data (unique)" clip menu action; ae95cac scrollbar + --scroll docs; 97f50ea tracker horizontal scrollbar; 93dd668 channel/limit docs; 0716702 hidden channel columns + constant 127 channel cap; 121ecd0 pattern resize preserves hidden rows; 9ffaaa4 EditorShot
--channels=N flag; a5e6e2d bottom-bar/settings channel docs; b02e52a CHANNELS slider direct edit; 1173682 stuck-note docs; b1725b1 stuck-note fix for hosts that skip MIDI-effect calls while stopped; de11909 pool grid preview; c4a8422 README macro-playback update; 49ec706 macro playback wiring; c36c6c7 ReTtrektor typo fix; 4263853 instrument fallback option; f89454e headless UI interaction test; c168e8b panels fit minimum window; a2ec7c9 Re.Trektor sources; ccc50f2 vendored JUCE 8.0.9.
DISPLAY=:0 / wayland-0 available; no Reaper; /usr/bin/carla exists.
Missing xcb-util-devel/xcb-util-wm-devel were installed via sudo dnf earlier (leftover from iPlug2 attempt, harmless).
iPlug2 and its VST3 SDK clone deleted from workspace; .gitmodules removed.
