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

The binary keeps its newest glibc requirement at `GLIBC_2.38`. glibc 2.43 moved
the default symbol versions of `sqrtf` and `atan2f`, which JUCE's geometry code
(`std::atan2`/`std::sqrt` on floats) and the vendored HarfBuzz (`hb-ft.cc`) call
through libm, so a binary linked on a newer system states `GLIBC_2.43` and a
host whose loader resolves an older `libm.so.6` refuses to load the plug-in at
all rather than simply missing those two functions. `Source/Portability.cpp`
defines both so the calls bind inside the plug-in's own shared object. Check
what a build asks for with:

```sh
readelf -VW build/ReTrektor_artefacts/Release/VST3/Re.Trektor.vst3/Contents/x86_64-linux/Re.Trektor.so \
    | grep -oE "GLIBC_[0-9]+\.[0-9]+" | sort -uV | tail -3
```

Bitwig Studio 6.1.4 runs as a Flatpak whose root ships glibc 2.42 and reported
`version \`GLIBC_2.43' not found` for the unpinned binary; Reaper links natively
and was unaffected.

## Tests

```sh
cmake -S tests -B tests/build -DCMAKE_BUILD_TYPE=Release
cmake --build tests/build -j"$(nproc)"
```

* `ReTrektorCoreTest` – model, sequencing, snapping, macro expansion, slicing,
  paste, state, the volume/pan columns' controller numbers, per-channel column
  visibility (and that hidden columns still play), interpolation of one column
  between the values that already exist (the tool is handed a predicate over
  (row, channel) rather than a row and channel range, so the checks drive it with
  a rectangle mask helper and a selection with a hole in it: the ramp restarting
  at each anchor, a hole ending a run so the column beside it stops there while
  the neighbouring channel keeps ramping, the row left out never written, nothing
  extrapolated, anchors outside the mask ignored, the note column's `===` closing
  a segment, rounding and each field's range clamping, and pasting a cell
  clipboard at an anchor cell (the clipboard holds one entry per copied cell
  with its offset inside the copied rectangle, so the block lands around the
  anchor again and a hole inside it pastes nothing, a field the copied cell
  left empty clears the target so it carries again, the paste filter says
  which fields reach — everything, one field, the command number or its
  argument — and a target outside the pattern's rows or channels is simply
  discarded, so a paste never grows the pattern in either axis)
* `ReTrektorSmoke` – loads the built VST3, checks MIDI behavior and state
* `ReTrektorUiTest` – drives the editor components (tracker keys, column chips
  clicked per channel, selection gestures — a click selecting one cell, a drag
  sweeping a rectangle of cells, a Shift-drag unioning another column and a Ctrl-drag
  taking a band away, all named through the channel columns' bounds and read back
  cell by cell —, the cell clipboard — Ctrl+C copying what the gestures
  selected through the editor's key handler, Ctrl+V landing the block at a
  clicked cursor with everything, a paste filtered to one field only, a hole
  inside the clipboard pasting nothing so the targets keep their own values,
  a paste at the last channel keeping only the block's first column, copying
  with nothing selected leaving the clipboard of before alone, and the
  pattern clipboard's own Ctrl+C / Ctrl+Shift+V pair living under Shift —,
  volume column entering three decimal digits and emitting
  CC7, the settings volume/pan CC sliders driving those columns, tools
  transpose with its semitone amount field and its two direction buttons (one
  amount moving both ways, notes stopping at the edge of the note range, a
  `===` row having no number to move), instrument remap with its source and
  target dropdowns across
  scopes, carries and a selection that covers one channel column only (a cell in
  the same row of the other channel stays put), selection interpolation with its
  one field dropdown (four dropdowns in the panel, the channel one gone, and the
  ramp restarts at a hole the selection has in it; and that a fresh panel opens
  on the selection-only scope, so the default transposes nothing), timeline click
  parking the playhead, timeline drop, pool
  placement, pool drag preview covering one item cell rather than the panel,
  window size remembered across a closed and reopened editor,
  channels slider, tracker horizontal scrollbar, undo and redo — Ctrl+Z
  walking a note entered, a cell pasted, a clip placed from the pool, a
  settings toggle and the bottom bar's channel slider back and Ctrl+Y or
  Ctrl+Shift+Z walking them forward again, the cursor left where it was by an
  undo, a redo at the line's newest state inert in both spellings, the Alt
  spellings of the same pair walked in both cases of the letter because a key
  held by Alt is handed over as the character that press printed, the pair
  answered by every panel rather than only by the editor, so the tracker,
  timeline, pool, settings, tools and macros panels each walk one point per
  press against the plug-in's line and a letter the pair does not name stays
  refused there too, the keyboard focus handed back by the tracker, timeline and
  pool once their menus, their alerts or their drags have closed — the hand-back
  runs at the drop and at the release ahead of the guard that refuses a change
  while the host plays and spends no point of the line, and the Alt pair heard
  straight after a pattern was dropped onto the timeline takes that clip back
  and puts it again for one point per press, with the pattern the drop made
  active coming back to what it was, an Alt+Shift
  combination left to the host rather than spent on a redo alias, an action
  that wrote nothing leaving the depth alone, the Settings tab's Undo steps
  row capping how far the walk reaches with its third press inert and 0
  turning undoing off, a Ctrl+Z or Alt+Z pressed over a half-typed number
  closing that buffer on the first press and stepping on the second, and undoing
  while the mock transport runs leaving the row ahead of the playhead without the
  note it played and redoing it putting it back) and confirms which
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
  `i`/`v`/`p`/`c` pairs, so a shot can show that chips hide per channel and that
  a hidden chip inverts its plate instead of dimming), and
  `--hex` to render the narrower hex field widths, with `--drag-preview=IDX`
  compositing the image JUCE floats at the cursor while pool item IDX is dragged
  out of the pool, so a shot can show that only that item's cell follows the
  pointer), `--cmd-carry` to write a row that only names a command argument
  below the demo's command at row 20, together with an explicit `0` argument in
  the other channel that never named one, so a shot can show the dim carried
  number over the bright argument next to the explicit zero, and `--select=6,1,9,1`
  to hold a rectangle of cells selected for the shot (row0,channel0,row1,channel1,
  inclusive at both ends), so a shot can show that the tint paints those cells'
  channel columns rather than a band across the pattern, and `--paste-at=ROW,CH`,
  which takes that selection through the same copy Ctrl+C makes and lands it at
  ROW,CH with everything, so a shot shows the pasted block at the cursor and
  prints how many cells it wrote: a block that fits writes all of it, one
  anchored at the last channel writes only the block's first column and drops
  the rest, and one below a shortened pattern writes only the rows that still
  fit, and `--undo=N` / `--redo=N` walk that many points of the undo line once
  the other actions ran, so a shot paired with `--select=` / `--paste-at=`
  shows the pasted block retracted the way Ctrl+Z takes it back and prints how
  many steps walked before the render, a walk running out of states stopping at
  the line's end rather than failing the shot)

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
  alone. A hidden chip inverts rather than dims — its plate takes the letter's
  colour and the letter is cut out of it in the plate's — so a hidden chip
  reads as legibly as a shown one and only the fill says which state the field
  is in. The flags belong to the pattern and the channel, and they follow the
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
  Selection is by cell rather than by row: a plain left click selects the one
  cell it lands on, a drag sweeps a rectangle of cells over rows and channels,
  Shift unions another rectangle into what is already selected and Ctrl takes the
  rectangle it sweeps away from it, so the selection can be any shape and a tool
  can be pointed at a single column of a single row. The rectangle snaps to
  channel boundaries, so a selected cell is one channel's visible fields and the
  gap between channels is never part of a cell: one channel column of one row
  tints on its own instead of a band across the pattern. Ctrl wins over Shift when
  both are held so a gesture never adds and removes at once, and a modifier
  pressed or released mid-drag changes what the sweep does from that moment on,
  with the corner where the gesture started keeping its place so sweeping back
  shortens the rectangle instead of turning it around. The selection is not
  saved with the song.
  The selected cells can be copied and pasted again: `Ctrl+C` or the cell
  menu's "Copy selection" puts them on a cell clipboard that lives in the
  tracker view beside the selection — it is not saved with the song either,
  and it survives a pattern switch — keeping each cell's place inside the
  copied rectangle counted from its top-left cell, so the cells a Ctrl
  gesture left out stay missing from the clipboard and a paste never writes
  through them. `Ctrl+V` or the cell menu's "Paste" submenu then writes them
  at the cursor, starting at its active row and channel, either all of it
  ("Everything") or one field at a time (`Note`, `Instrument`, `Volume`,
  `Pan`, `Command` — the number the column carries — and `Command Value` its
  argument); a field the copied cell left empty clears the target's, so the
  pasted cell goes back to carrying whatever that column carried above it.
  What does not fit inside the pattern's rows or channels is simply
  discarded, so a paste never grows the pattern, and since pasting cells is
  a cell edit rather than a timeline edit it stays available while the host
  is playing.
  Undoing belongs to the plug-in rather than to the host, since the VST3 spec
  hands a plug-in no undo of its own: every model change appends the state it
  reached to an undo line here, `Ctrl+Z` walks back along it and `Ctrl+Y` or
  `Ctrl+Shift+Z` walks forward again, `Alt+Z` and `Alt+Y` walk the same line for
  the hosts that answer the Ctrl trio with their own undo and never hand the
  plug-in the event, and the tracker's cell menu ends with an Undo / Redo pair
  for the same two steps, greyed out at the line's ends.
  The states are the song in the form the host's state blocks use, so one step
  restores exactly what reloading that state would have restored, and it
  restores the whole song: a cell edit, a clip dropped or sliced, a pattern
  resized and a setting moved all take back. What the line does not carry is
  not rewound either — the pattern clipboard, the cursor, the timeline's playhead
  and the position mirrored from the host keep their current values, which is
  why an undo reads as a rewind of the song rather than as a jump of where the
  user is working in it. How far it goes is the Settings tab's Undo steps row
  (default 50) and a 32 MB budget drops the oldest states on top of that, so a
  pattern whose states do not fit that comfortably keeps fewer of them;
  setting the row to 0 turns undoing off and stops the plug-in from holding
  states at all. An undo reaches the playback snapshot the way a cell edit does,
  so while the transport runs the rows ahead of the playhead play what the
  restored song holds. `Ctrl+Z` or `Alt+Z` while a number is still half typed
  closes that buffer instead of stepping, which is what makes one press one
  effect.
* **Macros tab** – macro definitions (name, kind, two parameters). Definitions
  live with the song and are called from the tracker's Command column with
  command `128`–`143`.
* **Right panel** – Transpose, instrument Remapping and Selection
  Interpolation. Each tool uses the selected scope: selection only, whole
  pattern, all patterns in the song, or all created patterns, but
  Interpolation only reads cells the user picked, so it does nothing
  until the panel is on `Selection only` – where it opens – and Transpose and
  Remap are the tools that need the dropdown widened when nothing is selected.
  Interpolation takes one dropdown, what to ramp (`Note`, `Instrument`,
  `Volume`, `Pan`, `CC number`, `CC value`): where it reaches is the selection
  itself, so the panel has no channel dropdown beside it any more, and the wider
  scopes name whole patterns and ignore which cells are selected. Only blank
  cells between two cells that already name a value inside the selection get
  filled, with the value linearly between them, so a ramp restarts at every
  anchor rather than only between the ends of the range, a selection with a hole
  in it ramps in pieces instead of across the hole, and nothing is written
  before the first anchor or after the last:
  values outside the selection are ignored and nothing outside it is touched.
  In the note column a `===` note-off has no number to ramp to or from, so it
  closes a segment and the blanks on either side of it stay blank. A filled
  cell becomes an explicit value, which is why the dim carried cells are
  written as well. Transposing reads one amount and a direction: the row its
  `TRANSPOSE SEMITONES` label heads holds an editable number field in the same
  style as the bottom bar's number sliders (its drag buttons step one semitone
  at a time, its text box takes a typed amount) kept to 1–48 whole semitones and
  opening at 12, plus two buttons that only choose which way to move, drawn `↑`
  and `↓`, so an amount typed once applies until it is changed. Remapping reads
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
* The tracker cell menu keeps three items at its top level, each carrying its
  own choices: a "Clear" submenu (Selection / Cell / Channel / Pattern, with
  Selection greyed out while nothing is selected), "Copy selection" (greyed out
  while nothing is selected) and a "Paste" submenu (greyed out while the cell
  clipboard is empty) whose items are Everything / Note / Instrument / Volume /
  Pan / Command / Command Value. What the cursor's last click parked is what the
  clears work on and what a paste starts at. The menu's Undo and Redo items sit
  after that trio and walk the same line the undo and redo keys walk, so they
  reach the whole editor rather than only the grid; like the keys, an Undo
  pressed while a number is half typed closes that buffer first.
* The window remembers its size: hosts such as Reaper close a floating plug-in
  window by throwing it away and ask the plug-in for a brand new editor when the
  GUI is opened again, so a new editor starts at the size the closed one had
  (clamped to the 900–2600 × 560–1600 resize limits) instead of always the
  built-in 1240×760. The size belongs to the plug-in instance, not to the saved
  song, so a fresh instance still opens at 1240×760.

## Mouse map

The tracker's selection gestures are the modifiers they hold while they sweep:

| Gesture | Tracker action |
| ------- | -------------- |
| left click on a cell | selects that one cell and parks the cursor on its field |
| left drag | sweeps a rectangle of cells, handing the selection over to it |
| Shift + click or drag | unions the rectangle into what is already selected |
| Ctrl + click or drag | takes the rectangle away from the selection, leaving a hole |
| right click | opens the cell's context menu, so Ctrl is free for selecting |
| click or drag on the bottom strip | jumps or drags the horizontal scrollbar's thumb |
| shift + mouse wheel | scrolls the channel columns that do not fit |

Ctrl beats Shift when both are held, so a gesture only ever does one thing; a
modifier pressed or released mid-drag changes what the sweep does from there on,
and the corner where the gesture began keeps its place. The selection lives in the
tracker view rather than in the saved song, and it survives the pattern being
shortened or switched: cells outside the pattern that is showing simply read as
not selected. The cell menu groups its clears under a "Clear" submenu
(Selection / Cell / Channel / Pattern), offers "Copy selection", greyed out while
nothing is selected, and a "Paste" submenu, greyed out while the cell clipboard
is empty, whose items are Everything / Note / Instrument / Volume / Pan /
Command / Command Value; either menu paste writes at the cursor the last click
parked, at its row and channel, the same place `Ctrl+V` writes at, and the two
items after them are the Undo / Redo pair.

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
| `Ctrl+C` | copy the selected cells to the cell clipboard |
| `Ctrl+V` | paste the copied cells at the cursor (everything) |
| `Ctrl+Shift+C` | copy the active pattern to the pattern clipboard |
| `Ctrl+Shift+V` | paste that pattern at the playhead as a unique pattern |
| `Ctrl+X` | slice clip at playhead |
| `Ctrl+Z` | undo the last change |
| `Ctrl+Y` / `Ctrl+Shift+Z` | redo the last undone change |
| `Alt+Z` | undo, for the hosts that answer `Ctrl+Z` with their own undo, in whichever panel the host routed the press to |
| `Alt+Y` | redo, the same hosts, the same panel |
| `F5` | switch to the Tracker tab |

The field filters (`Everything`, `Note`, `Instrument`, `Volume`, `Pan`,
`Command`, `Command Value`) only sit in the tracker's right-click "Paste"
submenu, so the keyboard paste always writes everything. The pattern
clipboard's paste as an instance — the choice Settings names — is in the
timeline's and pool menus rather than on a shortcut, since the plain `Ctrl+V`
now belongs to the cells.

Undo and redo belong to the plug-in rather than to the host, since VST3 gives a
plug-in no undo of its own, so `Ctrl+Z` steps back through the states the plug-in's
own changes produced — a cell written, a clip dropped, a setting moved — and
`Ctrl+Y` or `Ctrl+Shift+Z` steps forward again over the ones just undone, with the
tracker's menu Undo / Redo pair reaching the same line. Hosts such as Bitwig and
Reaper answer `Ctrl+Z`, `Ctrl+Shift+Z` and `Ctrl+Y` themselves, with their global
undo, before the plug-in is offered the event, so `Alt+Z` and `Alt+Y` walk the
same line for them, one spelling each. The Alt redo has no Shift alias:
`Alt+Shift+Z` is swallowed on the way to both of those hosts, so claiming it
would make a key the user aims at the redo mean an undo wherever it does arrive,
and the whole Alt+Shift family is left to the host rather than claimed by any
panel. Where a host does forward the
Ctrl spellings they still mean the plug-in's own undo, so neither spelling
changes meaning per host. The Alt pair belongs to every panel rather than only to
the editor, and the panels that own a menu, an alert or a drag hand the keyboard
focus back once one has closed: a context menu's window and a pattern the pool
started dragging are desktop windows with no parent component, so while one is
about a press is answered there and the editor is never consulted at all, and when
one closes it takes the keyboard focus with it and leaves nothing focused, which
starts the walk up from a press at the plug-in's top component and leaves only the
editor's own shortcuts answered. That is why the whole keyboard went dead after a
right-click menu, a greyed item, a menu closed with `Esc` or a pattern dropped out
of the pool onto the timeline, and why only clicking a panel again brought it back;
JUCE's own hand-back after a menu re-grabs focus on whatever component still holds
it, which is nothing in that case, and a drag has no hand-back at all. Every panel
now answers the pair with the editor's own step, so whichever panel holds the event
walks one point of one line, and the tracker, timeline and pool re-grab focus on the
tracker rather than on themselves, since focus resting on a panel would leave the
tracker, the only note entry there is, deaf the same way. The dragged preview keeps
floating across the panels, because only the focus is put back at the drop and at
the release. Grabbing keyboard focus also re-focuses the plug-in's own window on the
desktop, which is why a click fixed the keyboard where a bare press had not. The
tracker hands a
letter held with Alt up untouched instead of typing it as a note, and the letter
is matched over the key code and over the character the press printed, since that
is what an Alt-held key arrives as over X11. While a numeric field is half typed
the undo press closes that buffer instead of stepping, in the tracker's own answer
as well as in the editor's, so the step comes on the next press and one press
stays one effect, in either spelling.
Both work while the transport is playing: the rows ahead of the playhead play what
the restored song holds, while what the plug-in already sent to the host is not
rewound.

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
* Undo steps (default 50, range `0`–`200`): how many model states the undo keys
  (`Ctrl+Z` or `Alt+Z`) can walk back — changing a setting is itself an undoable
  action, so the row's own value rewinds too. `0` turns undoing off and stops the
  plug-in from holding states at all, and a 32 MB budget drops the oldest states
  of a pattern whose states do not fit that well, so a huge pattern keeps fewer
  of them than the row asks for

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
* Selection snaps to channel boundaries: a cell is one channel's visible
  fields, so selecting only one field of a channel — its volume alone, its
  command value alone — is not possible yet, and a paste of copied cells
  carries whole channels of cells rather than a single field's column. The
  paste filter can still be pointed at one field, and the fields the copied
  cells left empty clear the targets, which is how a pasted cell goes back to
  carrying.
* The pattern clipboard's plain `Ctrl+C` / `Ctrl+V` pair moved to `Ctrl+Shift+C`
  / `Ctrl+Shift+V` so the plain shortcuts could name cells instead; pasting the
  copied pattern as an instance, the choice Settings names, is now only in the
  timeline's and pool menus.
* Undo walks the plug-in's own state line, since the VST3 spec hands a plug-in
  no undo of its own, so it only reaches what the plug-in changed and what it
  can still hold. Hosts such as Bitwig and Reaper answer `Ctrl+Z`, `Ctrl+Shift+Z`
  and `Ctrl+Y` with their own global undo before the plug-in is offered the
  event, so in those hosts only the tracker's menu pair and the `Alt+Z` / `Alt+Y`
  pair reach the plug-in's undo, and that Alt pair is answered by whichever panel
  the host routed the press to rather than only by the editor, since JUCE sends a
  key press to the modal component and walks up only from there; a pair the
  editor alone owned went unheard while a right-click menu was open or while a
  pattern was being dragged from the pool, and nothing at all was heard once one
  of those parentless windows had closed, since closing it leaves no component
  focused at all and the walk then starts at the plug-in's top component, so the
  tracker, the timeline and the pool hand keyboard focus back to the tracker when
  their menus, their alerts or their drags close. What steps when the host answers those
  keys is still the host's own history, which hands the plug-in a state and starts
  a fresh line. That Alt pair has one spelling each, since `Alt+Shift+Z` is
  swallowed on the way to both hosts as well and the whole Alt+Shift family is
  left to the host rather than claimed by any panel. What the plug-in already sent to the host is not rewound: undoing a note
  at or behind the playhead changes the song but shows nothing until the
  transport runs over that row again, which is why the effect reads as nothing
  mid-song and as a change ahead of the playhead. The pattern clipboard, the
  selection, the cursor and both playheads stay where they are, so undoing
  changes what the song holds without moving where the user is working in it,
  and a state the host hands back starts a fresh line, so there is no undoing
  across a reload. The depth comes from Settings but a 32 MB budget also drops
  the oldest states, so a pattern wide or long enough keeps fewer of them than
  the row asks for.
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
Undo and redo are the plug-in's own (commit 6e51af9): the states every model
change reached are kept on the processor rather than in the editor, because a
host such as Reaper destroys the floating window and asks for a brand new
editor that has to find the same points; a state is the song's own ValueTree
XML, so one step back restores exactly what a reload of that state would have
and covers everything the song holds rather than only cell edits; the fields a
saved state does not carry (the pattern clipboard, the edit playhead, the
mirrored host position) are kept rather than reverted, so undoing rewinds the
song without moving where the user is working in it; undoing while the
transport runs goes through syncPlaybackModel with no engine.reset(), because
resetting would clear the engine's everPlayed stuck-note guard; redo is asked
for as Ctrl+Y and as Ctrl+Shift+Z and as the tracker menu's Undo / Redo pair;
Ctrl+Z closes a half-typed numeric buffer first, which makes one press one
effect; and the depth is the Settings tab's undoSteps row (default 50) with a
32 MB byte valve dropping the oldest states on top of it, so a huge pattern
holds fewer steps than the row asks for.
The Ctrl undo trio never reaches the plug-in in Bitwig or Reaper (the user's
interactive check found the tracker menu's Undo / Redo pair walking the line
while Ctrl+Z, Ctrl+Shift+Z and Ctrl+Y did nothing at all), because those hosts
answer it with their own global undo before the plug-in is offered the event, so
Alt+Z and Alt+Y are the spellings they leave free and walk the same line while
the Ctrl ones stay claimed as well (commit 30708bb), so a host that does forward
them keeps meaning the plug-in's undo rather than having one key mean two
different things per host; the letter is matched over the key code and over the
printed character, since a key held by Alt is handed over as the character that
press produced, and the tracker hands a key held with Alt up untouched rather
than typing the letter as a note before the editor gets the event. The Alt redo
alias Alt+Shift+Z that first pass added was dropped (commit 7f6ea6d) rather than
kept: the same check found that combination swallowed on the way to both hosts,
so claiming it would have made a key the user aims at the redo mean an undo
wherever it does arrive, and the whole Alt+Shift family is now left to the host,
which is why the Alt pair has one spelling each. That pair went inert right
after a right-click menu action or a pattern dragged from the pool onto the
timeline (the user's check found undo not working only after those actions, with
the menu's own Undo walking fine), and the reason is JUCE's modal key routing
rather than anything in the undo line: ComponentPeer::getTargetForKeyPress sends
a press to the modal component and handleKeyPress walks up only that chain, a
popup answers it by forwarding to the component its menu was attached to, and a
pattern the pool started dragging is held by the DragAndDropContainer with the
dragged item modal, so on those paths the editor was never consulted and the
tracker, which had declined Alt, swallowed the press. The line and pushUndoPoint
needed no change. The fix shape chosen was a per-panel pair rather than one
owner: Source/ui/UndoRedoKeys.h holds the matcher and the answer body every
panel shares, each panel declares keyPressed and undoStep / redoStep members, and
the editor injects its own performUndo / performRedo into all six of them with
one wireUndoRedo lambda, so whichever panel the host routed the press to walks
one point of one line, JUCE stopping at the first panel that answers is what
keeps one press from walking two, the answer claims a press only where that
panel holds the step so an event no panel can spend still reaches the editor, and
the tracker passes cancelPendingEdit so a half-typed number closes on the press
inside its own answer as well. That pair changed nothing in the hosts, and the
reason it could not is that the press never reached a panel at all: the popup and
the dragged item are parentless desktop windows, so the walk out of them is empty
and, once one had closed, nothing was focused any more — which deafened every key
the tracker owns rather than only the Alt pair, and only a click on a panel put
focus back. The shape the user chose keeps both of those windows where they are
and hands the focus back, so the parented-popup option was rejected: overriding
theme::LookAndFeel's getParentComponentForMenuOptions would have made every menu
a child of the plug-in's own window and laid it out inside those 1240x760 pixels
rather than over the host's UI, and the pool's startDragging keeps
allowDraggingToExternalWindows = true so the dragged pattern's cell still floats
across the panels. What the three panels that own a menu, an alert or a drag do
instead is call an injected focusRescue at the end of the menu callbacks and the
rename alert, ahead of the guard that refuses a change while the host plays in the
drop, and on the release of a drag; the editor hands focus to the tracker rather
than to the panel that asked, since focus resting on the panel would leave the
tracker, the only note entry there is, deaf the same way, and where the tracker is
hidden — the Settings and Macros tabs — the panel takes it so a press at least
walks up rather than stopping at nothing focused. Both hosts found this shape is
what they needed: the keyboard came back on its own after every one of the actions
that had deafened it, with no click in between, while the popups kept floating
where they were and the drag preview kept crossing the panels.
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
The plug-in binary loads on hosts whose libm is older (commit ee29e69): Bitwig Studio 6.1.4 refused to see it at all, its scan reporting

    /usr/lib/x86_64-linux-gnu/libm.so.6: version `GLIBC_2.43' not found
    (required by /home/corthax/.vst3/Corthax/Re.Trektor.vst3/Contents/x86_64-linux/Re.Trektor.so)

which also matches the exception the user quoted. Bitwig is a system Flatpak (/var/lib/flatpak/app/com.bitwig.BitwigStudio, its log naming /app/bin/BitwigPluginHost-X64-AVX2), and that is why the loader message quotes the Debian-family /usr/lib/x86_64-linux-gnu path that does not exist on this Arch host; probing it with flatpak run --command=sh showed the root ships glibc 2.42, so GLIBC_2.43 is genuinely missing there. The binary asked libm for 2.43 on exactly two symbols: objdump -R had two JUMP_SLOT relocations and the dynsym said sqrtf@GLIBC_2.43 and atan2f@GLIBC_2.43, while every other requirement topped out at GLIBC_2.38 (__isoc23_strtol/strtoll/sscanf, fmod), which a 2.42 host has. The references themselves are unversioned in the objects (nm -u says U sqrtf / U atan2f in the amalgamated juce_graphics, juce_gui_basics and juce_audio_basics translation units), so the 2.43 tag is chosen at link time by binding to this system's libm default, and the calls come from JUCE's geometry (std::atan2/std::sqrt on floats in PathStrokeType and Point.h) plus vendored HarfBuzz (sqrtf in hb-ft.cc): the requirement was not new to this rebuild, the Oct 30 build-instrument artefact carries it too, so the plug-in has never loaded in Bitwig rather than having regressed. Source/Portability.cpp defines both inside the plug-in target (added to its target_sources, so the instrument-fallback build inherits it): sqrtf compiles to the sqrtss instruction with no libm call at all and atan2f takes libm's double atan2, whose version is unchanged since GLIBC_2.2.5, so the newest version the binary asks for goes back to 2.38. Both names are aliased with .set in assembly with .hidden written by hand rather than declared directly, because GCC ignores a visibility attribute on a libc name ("visibility attribute ignored because it conflicts with previous declaration", a -Wattributes note against its own built-in declaration) and would otherwise export them and interpose on every other plug-in in the same host process: the definitions ended LOCAL in the built .so's dynamic symbol table. That one file skips LTO (set_source_files_properties COMPILE_FLAGS -fno-lto), because the LTO plugin resolves the aliases before it has seen rtr_sqrtf/rtr_atan2f and reports undefined references instead. A probe build linked both ways over a range of arguments and printed the two routines identically to libm's, so the geometry and the glyph metrics HarfBuzz derives keep the same values. Verified on both bundles after rebuilding: objdump -R names no sqrtf/atan2f relocation and readelf -VW tops out at GLIBC_2.38 with GLIBCXX_3.4.32, CoreTest 157, UiTest 127 and Smoke 13 checks all PASS, and the 900x560 --dump still lays the tools panel out at 156x488 with the interpolation combos at y=232/y=268 and the button at 8,294 140x22.
Transpose is one amount and a direction (commit 410b1fe): the right panel offered four buttons for -12, -1, +1 and +12, so the common octave move was one click but any other interval meant editing cells by hand. ToolsPanel::transposeAmount is the plug-in's usual number widget, a juce::Slider in IncDecButtons style with an editable text box like the bottom bar's OCT/STEP/CHANNELS and the Settings tab's sliders, so the drag buttons step a single semitone while typing the box sets the amount outright; it holds whole semitones from 1 to 48 and opens at 12, the octave the four buttons encoded, and a minimum of 1 keeps a direction button from being a no-op. transposeUp/transposeDown then only choose which way to move, so an amount typed once applies to every following click until it is changed, and the model layer's existing applyTranspose (int semitones) is untouched. arrowUpGlyph/arrowDownGlyph in ToolsPanel.h name the two button labels: juce::String takes a plain narrow literal as native bytes, so the raw UTF-8 arrows left in the sources came out on screen as an a-circumflex plus two control characters, and handing the explicit UTF-8 over made both arrows draw, so the font has both codepoints and the string was the problem; the --dump's mojibake on the older build was that same double-encoded text, not a font gap. UiTest finds the direction buttons through those helpers, which leaves one place that names the labels and means the test cannot pass against a mis-encoded label. The three controls stay on one row: at the 900x560 minimum window the field is 9,87 82x20 with value 12.00, a 46 px text box and two 16 px inc/dec buttons, the arrows sit at 93,87 and 121,87 both 26x20 inside the panel's 140 px content width, nothing in the panel is OUTSIDE its parent, and the row the old buttons' second line used moved Interpolate from 8,294 to 8,270 while the panel kept its 156x488. UiTest + 8 checks (the panel exposes one amount field, opens at 12 semitones, is a whole number of them from 1 to 48, the direction button reads the field so one amount moves both ways, the field's 5 raises 60 to 65, a note at 125 that would pass 127 stops at the edge, a `===` row has no number to move and stays a note-off as notes come down through it); the last three document the pre-existing applyTranspose guard c.note >= 0 && c.note <= 127 against the kEmpty -1 and kNoteOff 253 sentinels rather than a new rule, and the block restores the cells and the amount it touched so the later remap and interpolation blocks still see the pattern they expect. CoreTest 157, UiTest 135, Smoke 13 all PASS; the portability shim and the instrument-fallback bundle are untouched by this change.
Selection holds cells rather than rows (commit 292ff37): a selection was a band of whole rows, so every tool reached every channel of those rows and transposing one instrument channel moved the whole pattern. The tracker's selection is now a set of cells, which is what the user asked for: any cell can be selected on its own, a group of cells by a selection rectangle, Shift unions another rectangle into the set and Ctrl takes one away. State is one bit per (row, channel) cell — kMaxTrackerChannels = 127 makes kSelWords = (127 + 63) / 64 == 2, so a row is two uint64 words and the rectangle operations are word masks that union, subtract or replace whole words at a time, which also means the set can be any shape a sequence of gestures paints rather than only a rectangle. layoutFields records one channelRects entry per channel, the span its visible fields cover ending before the inter-channel gap, so a selected cell reads as its own column: drawRows tints each selected cell's column instead of the row band, and channelAtX plus the new public channelBoundsFor and addCellsToSelection let a gesture or a test name a cell without adding up fields. mouseDown parks the cursor as before and then applies the one-cell rectangle with the operation the mouse is holding, mouseDrag recomputes that operation on every event so a modifier pressed or released mid-drag changes what the sweep does from there, and gestureOp lets Ctrl win over Shift so a gesture is never adding and removing at once; the anchor of the gesture stays where the click started, so sweeping back shortens the rectangle instead of turning it around. A right-button modifier opens the context menu and on Linux JUCE's popupMenuClickModifier is the right button alone (commandModifier is the same flag as ctrlModifier), so a Ctrl+left click reaches the selection handlers rather than the menu. refresh() stopped clamping the selection to the visible length because isCellSelected and hasSelection report only cells inside the pattern that is showing, so a selection survives a shortened pattern or a pattern switch instead of being silently cut. The tracker's "Clear selection" menu item now clears only the selected cells. Pattern::interpolate moved to a predicate form at the model layer — interpolate (InterpField, const std::function<bool (int row, int ch)>& inCell) — because a cell selection has no bounds to hand over: anchors come only from cells inside the mask, only mask cells are written, and a cell outside the mask ends the anchor run, so a selection with a hole in it ramps in pieces rather than across the hole (this is what made the fill loop need no extra bounds check: a row outside the mask closed the run it would have used); an unset predicate returns 0 without dereferencing. ToolsPanel's rowRange/applyToRows became cellInScope/applyToCells and a new cellInScope that the wider scopes answer true to — the selection belongs to the tracker's view, so the wider scopes name whole patterns and ignore it — and applyRemap now walks every column from row 0 on purpose, since a carry has to be known before it can be pinned and pinning now means writing an explicit value only in a cell that is actually selected. Interpolation is still confined to the "Selection only" scope and its mask is the selection, so the panel's "All channels / Active channel" dropdown and its CHANNELS section are gone: the reach is the selection, and the getCursorChannel hook the dropdown used stays wired in ToolsPanel.h and PluginEditor.cpp as the panel's cursor hook even though nothing reads it now. UiTest drives the gestures with juce::ModifierKeys (mods) events, so the new makeEvent overload holding kShiftMod/kCtrlMod and the two-argument one delegating to it keeps the older call sites unchanged; both gesture blocks now name cells through channelBoundsFor (ch).getCentreX() and read the state back through isCellSelected. Verification: plugin and tests rebuilt clean with only the pre-existing warnings, CoreTest 164, UiTest 144, Smoke 13 all PASS; the 1240x760 --dump lists four ComboBoxes (scope 8,38, the remap pair 9,135 / 87,135, the interpolation field 8,208) and no CHANNELS row, with Interpolate at 8,234 instead of the earlier 8,270, and a 900x620 --select=6,1,9,1 shot crop-verified over the tracker at 176,188 568x398: the tint spans image x 393..545 and y 348..423, exactly channel 1's column of rows 6..9, with channel 0's cells in those rows and the rows above and below untouched.
Cell clipboard copies and paste filters (commit d103ccf): with the selection holding cells the next action that belongs to it is moving them rather than only acting on them, so Ctrl+C copies the selected cells and Ctrl+V pastes them at the cursor, and the tracker's right-click menu gained "Copy selection" (greyed out while nothing is selected) plus a "Paste" submenu built with JUCE's PopupMenu::addSubMenu (greyed out while the cell clipboard is empty) whose leaves are Everything / Note / Instrument / Volume / Pan / Command / Command Value, ids 7..13 mapped through a static pasteMasks table onto the paste bits. The pattern clipboard moved under Shift to make room, which is where the plain shortcuts used to sit: Ctrl+Shift+C is doCopy() and Ctrl+Shift+V is doPaste (true), the plain Ctrl+C/V pair now belongs to the cells and Ctrl+X is untouched. CellClipboard (Model.h, before Pattern because Pattern::pasteCells names it) is a vector of {row, ch, Cell} entries keeping their offset inside the copied rectangle measured from the top-left selected cell, so a Ctrl-drag hole simply never enters the clipboard and a paste cannot write through it; the clipboard is UI state (TrackerComponent::cellClipboard, not serialized, survives a pattern switch) like the selection is. copySelectionToClipboard normalizes the offsets onto that top-left cell and returns without touching the clipboard while nothing is selected, so a stray Ctrl+C never loses what the user copied earlier. Pattern::pasteCells (clip, anchorRow, anchorChannel, fieldMask) lives in the model layer so the algorithm can be tested without a GUI: the kPasteNote/kPasteInst/kPasteVol/kPastePan/kPasteCmd/kPasteCmdVal bits (kPasteAll = 63) pick which fields reach, the command pair splitting because "Command" names the number the column carries while "Command Value" names its argument, a field the copied cell left empty clears the target's so a pasted cell goes back to carrying whatever that column carried above it, and a target at or past length or channels is skipped, so a paste never grows the pattern in either axis while a jagged row just has no cell past its end; it returns the cells written, which is why pasteClipboardAtCursor only fires markChanged when that count is above zero. Paste is a cell edit, so unlike the timeline paste it stays available while the host is playing. Verification: CoreTest + 21 checks over a clipboard built the way the tracker builds it (a hole pastes nothing so the target keeps its own value, offsets land relative to the anchor, Everything rewrites all six fields including the command, each single-field filter leaves the target's other fields alone, the Command and Command Value options split the pair, an all-empty clipboard cell clears what it lands on, the column past the last channel is dropped while channelCapacity stays, and rows past a shortened end are dropped while length stays put); UiTest + 20 drive the real mouse gestures and the editor's keyPressed for Ctrl+C, Ctrl+V and the Shift pair, including a paste filtered to one field called directly through pasteClipboardAtCursor (kPasteNote), a 2-column block pasted at the last channel keeping only its first column, the same block pasted under a pattern shortened to 24 rows keeping only the row that fits, Ctrl+C on an empty selection keeping the clipboard of before, Ctrl+Shift+C setting song.clipboard.valid and Ctrl+Shift+V still landing blocked by the overlap at the playhead; EditorShot gained --paste-at=ROW,CH, which sends the --select= selection through copySelectionToClipboard() and lands it at a clicked cursor (channelBoundsFor (ch).getCentreX(), the usual 46 + 19 * row + 9.5 geometry) printing "paste wrote N cells at (row,ch)"; plugin + tests rebuilt, first pass failing only because the new pasteMenu needed juce::PopupMenu inside namespace rtr, CoreTest 185, UiTest 164, Smoke 13 all PASS, and --dump still lists the tracker at 176,188 908x538 with nothing new in the component tree since a popup is not part of the layout; the 1240x760 shots crop-verified over the tracker band of rows 18..24: --select=0,0,6,1 --paste-at=18,0 prints 14 and shows C-4 at row 18, D#4 at 20, G-4 at 22 and D#4 at 24 in the channel columns the copied offsets name with the rows between them blank, --paste-at=18,1 prints 7 with channel 0's command 74 64 and its 74 carries untouched and the block's second column gone, --channels=3 --paste-at=18,2 prints 7 with the block's first column landing in the third channel and nothing in a fourth, and --paste-at=60,0 prints 8, which is the row overflow being discarded (rows 60..63 fit while the block's rows 64..66 land past the pattern's end and are gone, and they sit below the rows the view shows, whose vertical position only follows the play row).
Cell menu groups its clears and hidden chips invert (commit 15e2513): two
readability complaints about the same view. The cell menu had four separate clear
items sitting ahead of the copy and paste ones, so the actions the user reaches
together (clear something, then paste the replacement) were on opposite ends of
the menu; the four clears now share one "Clear" submenu whose leaves read
Selection / Cell / Channel / Pattern, Selection first because it is the one that
needs the selection, which leaves the top level at three items - Clear, Copy
selection, Paste - and clearMenu is a second juce::PopupMenu fed to
menu.addSubMenu ("Clear", clearMenu), the one JUCE's own TableHeaderComponent
idiom. The ids did not change (Selection is still 4, Cell 1, Channel 2, Pattern
3, so the submenu is declared before menu's own items but presents them in the
order a reader wants), which is why the showMenuAsync callback below is
untouched apart from a comment naming 1..4 as the Clear submenu's leaves. The
second complaint was the chip colours: a hidden field's chip had letter, plate
and border all pushed to theme::outline, which sits a few levels above the panel
behind it, so a hidden column read as a chip that was simply not there rather
than as a field that was switched off, and finding which one to switch back
meant reading the dashes below instead. A hidden chip now inverts instead: the
plate takes theme::text and the letter is cut out in theme::panelLight, so both
states read at the same contrast and only the fill says which one the field is
in, while a shown chip keeps its dark plate and bright letter. Verification:
the chip is drawn, not queried, so no check moved - CoreTest 185, UiTest 164 and
Smoke 13 come back at exactly the previous commit's counts - and the shots do the
reading: 1240x760 --hide=0i0v0p0c against the plain shot, cropped 84x26+214+202
above channel 0's note column and blown up 8x, shows four filled light plates
with dark letters where the plain shot has four outlined dark ones, and the
wider 908x52 crop shows channel 1 keeping its shown chips beside them. The menu
structure is interactive-only in this environment (a popup is not part of the
component tree --dump prints and showMenuAsync needs a desktop), so the grouping
is for confirming in a host.
Undo and redo walk the plug-in's own state line (commit 6e51af9): the VST3
spec hands a plug-in no undo of its own, so every model change appends the
state it reached to a line the Re.TrektorAudioProcessor holds rather than the
editor, because a host such as Reaper closes a floating window by destroying
the view and asks for a brand new editor when the GUI is opened again and the
points the user stepped through have to be where that editor finds them too.
A state is song.toValueTree().toXmlString(), the same text a host's state
block carries, so restoring one goes through juce::ValueTree::fromXml and
song.fromValueTree, the path a reload takes, which is why one step back
retracts everything the song holds - a cell written, a clip dropped or
sliced, a pattern resized, a setting moved - instead of only the cell kinds
someone chose to record. What the tree does not carry is kept on purpose: the
pattern clipboard, song.editPlayheadBeats and song.transportMirrorBeats
survive a step, so undoing reads as a rewind of the song rather than as a
jump of where the user is working in it, and a state the host hands back
starts a fresh line because walking through states from before a reload would
mean nothing. Undoing follows the path a cell edit follows, syncPlaybackModel
with no engine.reset(), so it works while the transport runs and the rows
ahead of the playhead play what the restored song holds, while what the
plug-in already sent to the host cannot be taken back; the engine is not
reset because that would also clear its everPlayed stuck-note guard.
Settings gained undoSteps = 50 (kMaxUndoSteps = 200, clamped both ways with
the rest of the SETTINGS block) and trimUndoLine drops the oldest states at
that count and at a 32 MB budget counted as their UTF-8 size, never the state
the model is in and never one a redo would walk forward to, so lowering the
row takes effect at the next step and a pattern whose states do not fit that
comfortably keeps fewer of them; undoSteps 0 re-anchors the line, which is
what makes turning undoing off also stop the plug-in from holding states.
pushUndoPoint records nothing for an action that left the model as it was, so
a paste whose cells did not fit or a transpose that reached no cell leaves the
depth alone, and it forgets whatever the cursor had already walked back over,
which is what keeps Ctrl+Y retracing only the undone steps. Every model change
now ends in one funnel, PluginEditor::commitEdit(), which records the point
before refreshing, so the timeline, tracker, pool, tools, macros and settings
onModelChanged callbacks that used to call refreshAll() are rewired to it,
while the bottom bar's OCT / STEP / CHANNELS onValueChange handlers and
selectPattern() change the model without a callback and record their own
point. performUndo / performRedo step without recording one and also refresh
the Settings panel, whose rows hold their own values rather than reading them
off the model, so an undo that moved a setting pushes its row back. The line
itself is undoStates with undoCursor and undoBytes counted in UTF-8 bytes,
private to the processor, with anchorUndoLine, trimUndoLine, stepUndoHistory,
serialiseSong and recountUndoBytes as its helpers, undo() and redo() walking
behind the model lock and getUndoDepth / getUndoRedoCount naming what is still
walkable so the tracker's menu can grey its pair out: the menu's Undo and Redo
are ids 14 and 15, after the paste submenu's leaves since one callback answers
them by number alone, and they reach the editor through injected undoStep /
redoStep std::function members rather than the processor, so the menu walks the
same line the keys walk and refreshes every panel instead of only the grid.
cancelPendingEdit, public on the tracker, reports whether a numeric buffer was
half typed and closes it the way Esc does, which both the Ctrl+Z branch and the
menu's Undo consult before stepping, so one press stays one effect; Ctrl+Shift+Z
is tested before Ctrl+Z in PluginEditor::keyPressed, where the tracker's own
handler answers a key held by Ctrl with false and so leaves the shortcuts to the
editor. UiTest + 45
checks (209 total, PASS): a note entered at the cursor undone and redone with
the cursor left where it was, redo inert at the line's newest state in both
spellings, the line walked back to its oldest point and forward again with
both redo spellings on real steps, a clip placed from the pool undone
structurally and put back, a cell paste undone while the cell clipboard stays
alone, a transpose that reached nothing leaving the depth alone, the Settings
tab's Undo steps row capping the depth at 2 with a third Ctrl+Z inert and 0
turning undoing off, the bottom bar's CHANNELS slider widening the pattern on
the line, Ctrl+Z closing a pending digit buffer on the first press and
stepping on the second, and undoing while the mock transport runs leaving the
row it crosses without the note it played while a redo puts it back. That
last check failed on the first pass because it counted rows: the tracker had
scrolled to follow the cursor, so the row the click was meant to land on was
never written and the note it wanted was never in the pattern, so the check
now reads the row and channel back from the cursor and raises the record
octave to one the rest of the test never uses, which lets it name a single
note-on rather than compare event lists. EditorShot gained --undo=N /
--redo=N, which walk the line before the shot is rendered: --select=0,0,6,1
--paste-at=18,0 prints "undo/redo walked 1 of 1 steps, undo depth 0 redo 1"
with --undo=1 and a 300x220 crop at +176+500 over the tracker shows C-4 at
row 18, D#4 at 20, G-4 at 22 and D#4 at 24 in the plain shot against the same
rows empty next to it. CoreTest 185 and Smoke 13 come back unchanged, the
plugin rebuilt clean apart from the pre-existing warnings; the undo line
itself is what the user asked for, so the depth control, the key pair and the
menu pair still need a host to confirm.
The Alt spellings reach the undo pair where the Ctrl ones cannot (commit
30708bb): the user's interactive check split it, the tracker menu's Undo / Redo
items walk the line while Ctrl+Z, Ctrl+Shift+Z and Ctrl+Y do nothing at all,
which is what showed the state line itself was fine and only the keys were the
problem - Bitwig and Reaper answer those three with their own global undo before
the plug-in is offered the event, so no handler of ours can see them. Alt is the
spelling those hosts leave free, so the tracker's early guard now hands up a key
held with Alt as it already does a Ctrl or Cmd one, since a letter left to fall
through is typed as a note at the cursor before the editor sees the press, and
the editor answers Alt+Z, Alt+Y and Alt+Shift+Z after its Ctrl branches, matching
the letter over both the key code and the character the press printed because
that is what an Alt-held key arrives as over X11 and the Shift naming the redo
turns it uppercase. The Ctrl spellings stay claimed so a host that forwards them
keeps getting the plug-in's undo. UiTest + 19 checks (228 total, PASS): Alt+Z and
Alt+Y walk the same line over a note entered at the cursor, Alt+Shift+Z reaches
the state the plain redo does, a redo over the newest state is inert in the Alt
spelling too, and the lowercase spellings walk it as well, which is what covers
the case the letter arrives in rather than the one the code was written for; the
Alt pair also closes a half-typed digit buffer on the first press and steps on the
second. The one check that failed on the first pass was that buffer check, since
the Alt walking above moved the hex display along with the rest of the song and a
field's digit count follows that flag, so the display is now fixed there rather
than assumed. CoreTest 185 and Smoke 13 come back unchanged and the plugin rebuilt
with only the pre-existing warnings; the Alt pair is covered at the handler, so
what a host actually hands it still needs Bitwig and Reaper to confirm.
The Alt redo alias is dropped (commit 7f6ea6d): the check in the hosts came back
with Alt+Z and Alt+Y walking the line while Alt+Shift+Z does nothing at all in
either of them, so that combination is swallowed on the way to the plug-in just
as the Ctrl trio is, and an alias claimed on a key that never arrives is one the
hosts cannot use. Left to itself, such a press would have fallen into the plain
Alt undo, which is the opposite of what a user aiming at the redo means, so the
Alt block now refuses any Alt+Shift press and the whole family is handed to
whatever sits above the plug-in, leaving the pair at one spelling each. The Ctrl
spellings stay as they were, since a host that forwards them means the plug-in's
own undo there. UiTest + 2 checks (230 total, PASS): the alias check became one
that Alt+Y reaches the point the undo walked back, and the two new ones read the
Alt+Shift press back as the editor declining it and as neither walk moving.
CoreTest 185 and Smoke 13 are unchanged and the plugin rebuilt with only the
pre-existing warnings.
The Alt undo pair is answered by every panel rather than only by the editor
(commit 26ab9f6), which is what the pinned quirk needed: the undo line was
intact and the tracker's menu Undo walked it, so neither the line nor
pushUndoPoint was at fault, and what was at fault was how a press got to it.
JUCE sends a key press to the modal component and walks up only that chain
(ComponentPeer::getTargetForKeyPress, handleKeyPress), a popup answers a press
by forwarding it to the component its menu was attached to (juce_PopupMenu), and
a pattern dragged from the pool is held by the DragAndDropContainer with its
dragged item modal too, so after a menu paste, a clear, a slice-pattern or a
pool drop onto the timeline the editor was never consulted at all and Alt+Z was
swallowed by the panel the menu or the drag belonged to, while the same key
walked as soon as the panel had been clicked again. Moving the shortcut was not
an option, so a shared matcher in Source/ui/UndoRedoKeys.h gives all six panels
one body: matchUndoRedoKey reads Alt+Z / Alt+Y over both the key code and the
printed character and refuses Command, Ctrl and Shift, and answerUndoRedoKey
claims the press only when that panel holds the step, which leaves a press no
panel can spend free to walk up to the editor. The tracker hands cancelPendingEdit
as the closing hook, so a half-typed number still closes on the press inside the
tracker's own answer and one press stays one effect there too. The steps are the
editor's own rather than a copy of them: one wireUndoRedo lambda injects
performUndo and performRedo into the tracker, timeline, pool, settings, tools and
macros panels, so whichever panel holds the event walks one point of one line,
refreshes every panel and records no extra point of its own, and JUCE stopping at
the first panel that answers is what keeps one press from walking two. UiTest
+ 40 checks (270 total, PASS): the tracker's Alt checks now read back as
answering the pair and walking one step rather than declining it, a letter the
pair does not name and an Alt+Shift press stay refused there, and the timeline,
pool, settings, tools and macros panels each walk one point per press against the
editor's line, refuse that Alt+Shift press and come back to the line without net
effect. CoreTest 185 and Smoke 13 are unchanged, the plugin rebuilt with only the
pre-existing warnings, and the installed bundle at
/home/corthax/.vst3/Corthax/Re.Trektor.vst3 was refreshed to the new artefact
(sha256 41a0bd3026eca2e4e1d9ee8adf2e50d2e6445f9ebb66e4dcdca7f18762a4a139), still
topping out at GLIBC_2.38 with GLIBCXX_3.4.32 and naming no sqrtf or atan2f
relocation.
That answer changed nothing in the hosts, and the user's next check said why it
was broader than the pair: the plug-in lost keyboard input outright after a right-click
menu, even over a greyed item, stayed dead after a menu closed with Esc, and only came
back once a panel was clicked again. The fault sits one level down. A context menu's
window and a pattern the pool started dragging are both desktop windows with no parent
component — theme::LookAndFeel inherits LookAndFeel_V2's getParentComponentForMenuOptions,
which returns the options' own parent and no call site passes withParentComponent, so the
popup takes the addToDesktop branch, and the pool's startDragging passes
allowDraggingToExternalWindows = true — and closing one takes the keyboard focus with it,
so afterwards nothing is focused at all. getTargetForKeyPress then starts the walk at the
plug-in's top component, so only the editor's own shortcuts are answered while the note
entry, the cursor moves and the clears the tracker owns are unheard, which is why the whole
keyboard felt dead rather than only the Alt pair; JUCE's own hand-back after a menu
re-grabs focus on whatever component still holds it, which is nothing in that case, and a
drag has none at all. Grabbing focus also re-focuses the plug-in's own X11 window, which is
why a click woke the keyboard where a bare press had not. The shape the user chose keeps
the popups where they float and the drag preview crossing the panels, so the panels that
own a menu, an alert or a drag hand focus back themselves (commit 1c2e544): a focusRescue
std::function member injected by the editor into the tracker, timeline and pool beside the
undo pair's wireUndoRedo, called at the end of the tracker's and the timeline's context-menu
callbacks and of the pool's rename alert, ahead of the guard that refuses a change while the
host plays in the timeline's itemDropped, and on the pool's mouseUp, which ends a drag
wherever the pointer was. The editor hands focus to the tracker rather than to the panel
that asked, because focus resting on a panel would leave the tracker, the only note entry
there is, deaf the same way; where the tracker is hidden, which is the Settings and Macros
tabs, the panel takes it instead so a press still walks up rather than stopping at nothing
focused. UiTest + 15 checks (285 total, PASS): the hand-back is wired on the three panels,
runs at the drop and at the release ahead of both guards and while the host plays, spends
no point of the undo line, and the Alt pair heard straight after a pattern was dropped onto
the timeline takes that clip back and puts it again for one point per press, with the
pattern the drop made active coming back to what it was. CoreTest 185 and Smoke 13 are
unchanged, the plugin rebuilt with only the pre-existing warnings, and the installed bundle
at /home/corthax/.vst3/Corthax/Re.Trektor.vst3 was refreshed to the new artefact (sha256
fef0685afe97074e565b9c494a490ec5ddad1e5efdb1ee97553aa17edde38bd6), still topping out at
GLIBC_2.38 with GLIBCXX_3.4.32 and naming no sqrtf or atan2f relocation. Confirmed in
both hosts on the refreshed bundle: the keyboard is heard again on its own, so a note
typed, a cursor moved or a field cleared straight after a menu paste, a clear, a
slice-pattern, a greyed item picked, a menu closed with Escape or a pattern dragged
out of the pool and dropped onto the timeline lands where it was meant to with no
click in between, and Alt+Z / Alt+Y walk one point per press on each of those paths,
with the Alt+Shift family still refused rather than spent on a redo alias. That is
what settles the keyboard half of the pinned check and says the focus hand-back, not
the per-panel answer, is what the hosts needed.
Active
None.
Blocked
No Reaper installed; VST3 host testing limited to JUCE smoke test + Carla
availability. The modal routing that made the Alt pair inert cannot be
reproduced by the menu tests, which call the menu callback directly rather than
going through showMenuAsync or startDragging, so both the per-panel answer and the
focus hand-back are covered at the panel and at the drop and pinned by reading
ComponentPeer::getTargetForKeyPress, PopupMenuCompletionCallback and the MenuWindow
constructor rather than by a headless popup.
Next Move
Re-test in Reaper: moving a pattern from time 0 (the stuck-note scenario) is now guarded by the engine and covered by CoreTest, but not yet confirmed in Reaper itself.
Confirm in the DAW that a timeline click (left and right button) parks the playhead where a paste lands, that the parked playhead survives while the host is stopped, and that the follow resumes once the host transport moves.
Confirm interactively that the right-click menus now open beside the cursor in the timeline, pool and tracker views.
Verify pool drag-drop preview and context-menu paste/slice flows interactively in Carla; confirm Reaper compatibility of the MIDI effect (instrument fallback IS_SYNTH build compiles locally but is untested in Reaper).
Further command kinds: dedicated pitch bend CC105/CC106 pair command, tempo and song position commands.
Grid preview currently always starts at row 0 (no preview scrolling yet).
Confirm in the DAW that Ctrl+C and the cell menu's "Copy selection" copy the selected cells, that Ctrl+V and the menu's "Paste" submenu land them at the cursor starting at its row and channel, that the submenu's items filter to one field (and that "Command" moves the number while "Command Value" moves the argument), that a paste at the last channel or near the pattern's end drops what does not fit rather than growing it, and that Ctrl+Shift+C / Ctrl+Shift+V still reach the pattern clipboard.
Confirm in the DAW that the tracker menu now shows Clear / Copy selection / Paste at its top level, that the Clear submenu's leaves are Selection / Cell / Channel / Pattern and do what the old four items did, and that a hidden field's chip now reads as a filled plate with the letter cut out of it while a shown one keeps its dark plate, so the state of every chip is readable at a glance.
Confirmed in Bitwig and Reaper (the user's re-test against the refreshed bundle) that
the keyboard is heard again on its own: a note typed, a cursor moved or a field
cleared straight after a menu paste, a clear, a slice-pattern, a greyed item picked,
a menu closed with Escape or a pattern dragged out of the pool and dropped onto the
timeline lands where it was meant to with no click in between, and `Alt+Z` / `Alt+Y`
walk one point per press on each of those paths, with the Alt+Shift family refused
rather than spent on a redo alias — which is what says the focus hand-back rather than
the per-panel answer is what the hosts needed. Still to confirm in the DAW: that
`Alt+Z` takes back the last thing the plug-in changed for an action that never went
through a menu (a cell written, a clip dropped or sliced, a pattern resized, a
setting moved) and that `Alt+Y` puts it again, that the timeline pattern move still
undoes as normal, that the tracker menu's Undo / Redo pair walks the same line and
greys out at its ends, that the Settings tab's Undo steps row caps how far the walk
reaches and that 0 turns undoing off, that undoing while the transport runs changes
what the rows ahead of the playhead play while what was already sent to the host stays,
that undoing a note at or behind the playhead only shows when the transport runs over
that row again, and that the cursor, the selection, the clipboards and the playheads
stay where they were across an undo.
Confirm in the DAW that the volume column now emits CC7 (CC10 for pan), that a note-off row (`===`) with an explicit volume/pan still applies both controllers, that the Settings tab's Volume column CC / Pan column CC sliders repoint them, and that a channel's column chips hide only that channel's field.
Confirm interactively that dragging a pattern from the pool floats only that item (its grid preview when expanded, semi-transparent) under the pointer, that the grabbed spot stays under the cursor, and that the clip still lands at the drop position.
Confirm in the DAW that Remap moves only the instrument named in the source dropdown, that the dim carried rows follow it, and that All in the source box covers every instrument in the scope.
Confirm in the DAW that closing a floating Re.Trektor window and opening it again reopens it at the size it had before, and that a size dragged to the window's corner is what comes back.
Confirm in the DAW that the timeline ruler now shows only bar numbers, with the playback state, song position and bpm coming from the top bar's position label.
Confirm in the DAW that a command argument entered with the command number empty plays the command number carried down that column (the row shows the carried number dim next to the bright argument), that a column nothing was ever entered in still needs the explicit 0, and that a value below a macro call does nothing.
Confirm in the DAW that interpolation ramps only the blanks between the values that already exist inside the selected cells, that a cell left out of the selection ends the ramp there so a selection with a hole in it fills in pieces, and that Transpose and Remap still need the scope dropdown widened while nothing is selected.
Confirm in the DAW that a plain click selects the one cell under the cursor, that a Shift-drag unions another rectangle and a Ctrl-drag subtracts one, that the tint paints one channel column of a row rather than a band over the pattern, and that transpose, remap and interpolation then touch only the selected cells.
Confirm that the pinned bundle loads in Bitwig Studio 6.1.4, whose Flatpak root ships glibc 2.42, and that Reaper still loads it.
Confirm in the DAW that typing a number into the transpose amount field and clicking only a direction button moves the notes by that many semitones, that an amount past an octave still behaves, and that the two direction buttons draw as up and down arrows rather than boxes in Bitwig and Reaper.
Relevant Files
/media/data2/GameDev/Plugins/ReTrektor/CMakeLists.txt: JUCE plugin build (VST3, MIDI effect).
/media/data2/GameDev/Plugins/ReTrektor/Source/Model.h / Model.cpp: core data model, song/pattern/clip ops, snapping, slicing, paste, serialization, per-channel column visibility bytes (Pattern::vis), the volume/pan CC settings and the interpolation of one column between the values that already exist (InterpField, Pattern::interpolate taking a std::function<bool (int row, int ch)> mask so the selection can be any shape and a cell outside the mask ends the anchor run), and the cell clipboard (CellClipboard holding one {row, ch, Cell} entry per copied cell with its offset inside the copied rectangle, the kPasteNote/kPasteInst/kPasteVol/kPastePan/kPasteCmd/kPasteCmdVal bits and kPasteAll naming which fields reach, Pattern::pasteCells writing them at an anchor, clearing a field the copied cell left empty and skipping a target past length or channels so a paste never grows the pattern), and the Undo steps setting (Settings::undoSteps, default 50 with kMaxUndoSteps = 200 as the cap, written and read back clamped with the rest of the SETTINGS block so the depth a host reloads belongs to the song).
/media/data2/GameDev/Plugins/ReTrektor/Source/PlaybackEngine.h / PlaybackEngine.cpp: host-synced MIDI generation, carry/duplicate logic, explicit note-off panic, volume/pan driven by the Settings CC numbers, a value-only row using the command number carried in the column.
/media/data2/GameDev/Plugins/ReTrektor/Source/PluginProcessor.h / .cpp: processor, transport snapshot, state, lock, the size of the last GUI shown, and the undo line (undoStates holding the saved song XML with undoCursor and undoBytes counted in UTF-8 bytes, pushUndoPoint appending what a model change reached and skipping one that changed nothing, undo() / redo() stepping and refreshing the snapshot, resetUndoHistory anchoring a fresh line for a state the host hands back, getUndoDepth / getUndoRedoCount naming what is left to walk, trimUndoLine dropping the oldest states against the Settings depth and a 32 MB budget while never dropping the state the model is in or one ahead of the cursor, and stepUndoHistory keeping the pattern clipboard, the edit playhead and the mirrored transport position a saved state does not carry).
/media/data2/GameDev/Plugins/ReTrektor/Source/PluginEditor.h / .cpp: main window layout, tabs, shortcuts, bottom bar, starting a new editor at the remembered window size, wiring the tools panel's isCellSelected hook to the tracker's selection, and the shortcuts where Ctrl+C/V name the cell clipboard (copySelectionToClipboard / pasteClipboardAtCursor) while the pattern clipboard's pair moved to Ctrl+Shift+C / Ctrl+Shift+V, the undo funnel commitEdit() (record the point the model reached, then refresh, so every component's onModelChanged ends in it while the bottom bar's OCT / STEP / CHANNELS handlers and selectPattern() record their own), performUndo / performRedo stepping the plug-in's line and refreshing the Settings panel as well because its rows hold their own values, and the Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z shortcuts with the Alt+Z / Alt+Y spellings the hosts leave free answering the same two steps, one each since the whole Alt+Shift family is refused rather than spent on a redo alias the hosts swallow, the Alt branches matching the letter over the key code and the printed character, all of them reaching the editor while every panel answers the Alt pair as well through one wireUndoRedo lambda that injects performUndo / performRedo into the tracker, timeline, pool, settings, tools and macros panels, so whichever panel the host routed the press to walks one point of the same line rather than one the panel owns, and one wireFocusRescue lambda injecting the same shape's focusRescue member into the tracker, timeline and pool so the panel that closed a menu, an alert or a drag hands keyboard focus back to the tracker (and to itself where the tracker is hidden), which is what makes the whole keyboard heard again rather than only the editor's own shortcuts.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/TimelineComponent.h / .cpp: timeline, drag/drop, context menus, zoom, ruler (bar numbers only, no transport readout), and the Alt pair of undo and redo answered in its keyPressed, which is where a press aimed at undo has to be heard while the timeline's own clip menu is open or while a pattern is being dropped onto it, and a focusRescue member it calls at the end of both context menus and ahead of the playback guard in its drop, since both the popup and the dragged item are parentless desktop windows that take the keyboard focus with them when they close.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/TrackerComponent.h / .cpp: tracker grid, key entry, per-channel column visibility chips, field widths from fieldGlyphs, an entered command argument staying implicit over the command number carried in the column (carriedCommandNumber), cell selection (selBits one bit per (row, channel) in kSelWords = 2 uint64 words per row, applyCellRect unioning/subtracting/replacing word masks, gestureOp reading the held modifiers, channelAtX and channelBoundsFor naming a cell, per-cell painting and a hidden chip inverting its plate (theme::text plate, letter cut out in theme::panelLight) instead of dimming) and the cell clipboard (cellClipboard of CellClipboard type, copySelectionToClipboard putting the selected cells on it with their offsets inside the copied rectangle counted from the top-left selected cell and doing nothing while nothing is selected, pasteClipboardAtCursor writing at the cursor and only marking the model changed when it wrote something, hasCellClipboard greying out the menu's paste, and the cell menu's three top-level items - a "Clear" submenu whose leaves keep the old ids so the callback still answers them by id (4 Selection / 1 Cell / 2 Channel / 3 Pattern), "Copy selection", and a "Paste" submenu whose Everything / Note / Instrument / Volume / Pan / Command / Command Value leaves map onto the paste bits), and the undo pair (undoStep / redoStep std::function members the editor injects so the menu's Undo and Redo, ids 14 and 15 after the paste submenu's leaves and greyed out by the plug-in's own counts, walk the same line Ctrl+Z and Ctrl+Y walk and refresh every panel rather than only the grid, and cancelPendingEdit reporting whether a numeric buffer was half typed and closing it the way Esc does, which the Ctrl+Z and Alt+Z branches and the menu's Undo consult before stepping so one press is one effect, and its keyPressed now answering the Alt pair itself through answerUndoRedoKey, with cancelPendingEdit as the closing hook so a half-typed number still closes on that press rather than being walked over, while any other letter held with Alt is still handed up rather than typed as a note at the cursor), and a focusRescue member it calls at the end of the cell menu's callback, whether the press picked an item, a greyed one or dismissed the popup with Escape.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/PoolComponent.h / .cpp: pattern pool, drag source (the drag image is the item's cell), grid preview, rename, and the Alt pair answered in its keyPressed, which is what a press aimed at undo needs while the pool's own drag is still held by the DragAndDropContainer and has made that drag modal, and a focusRescue member it calls at the end of its own menu and of the rename alert, plus on the release of a drag, which ends whether or not the item ever landed on a target.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/ToolsPanel.h / .cpp: transpose as one semitone amount field (IncDecButtons slider with an editable text box, whole semitones 1-48 opening at 12) plus arrow direction buttons whose labels arrowUpGlyph/arrowDownGlyph build as explicit UTF-8, remap (source dropdown All/-0..-F -> target dropdown -0..-F), cell scope (cellInScope answers whether a cell is in reach and applyToCells walks the cells, so the wider scopes name whole patterns and only the "Selection only" scope consults the tracker through the isCellSelected hook), interpolation with its single field dropdown (Note/Instrument/Volume/Pan/CC number/CC value, ids following the InterpField enumerators, the mask being the selection so there is no channel dropdown any more), the getCursorChannel hook still wired but unread, the scope box defaulting to "Selection only", and the Alt pair of undo and redo answered in its keyPressed.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/SettingsPanel.h / .cpp: settings controls, including the Volume column CC and Pan column CC sliders and the Undo steps row (undoStepsSlider, 0-200 defaulting to 50, sitting with the input prefs ahead of the CC pair and pushed back by refresh() so an undo that moved the depth shows in the row), and the Alt pair answered in its keyPressed so a press aimed at undo is heard while a menu belongs to this panel.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/MacrosPanel.h / .cpp: macro editor (definitions; executed in playback via command 128-143) and the Alt pair answered in its keyPressed.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/UndoRedoKeys.h: the shared body every panel answers the Alt undo pair with (matchUndoRedoKey reading Alt+Z / Alt+Y over both the key code and the printed character and refusing Command, Ctrl and Shift so the whole Alt+Shift family stays the host's, answerUndoRedoKey claiming the press only when that panel holds the step, which leaves a press no panel can spend free to walk up to the editor, and closePending naming the half-typed numeric buffer the tracker closes on the press before stepping).
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/Theme.h: colors, fonts, LookAndFeel.
/media/data2/GameDev/Plugins/ReTrektor/Source/Portability.cpp: the sqrtf and atan2f defined inside the plug-in so its libm requirement stays portable, aliased hidden in assembly and built without LTO.
/media/data2/GameDev/Plugins/ReTrektor/tests/CMakeLists.txt: smoke/core/editor-shot test targets.
/media/data2/GameDev/Plugins/ReTrektor/tests/CoreTest.cpp, UiTest.cpp, SmokeTest.cpp, EditorShot.cpp: automated verification harnesses (CoreTest drives the interpolation predicate directly with rectangle masks and a holed mask and pastes a cell clipboard built the way the tracker builds it, UiTest drives the selection gestures and the cell clipboard through juce::ModifierKeys-carrying mouse events and the editor's keyPressed for Ctrl+C, Ctrl+V and the Shift pair, EditorShot can hold a selection with --select=r0,c0,r1,c1 and paste it at a cursor with --paste-at=row,ch, and can walk the undo line with --undo=N / --redo=N before it renders, so a shot shows what an earlier action left behind retracted).
/media/data2/GameDev/Plugins/ReTrektor/JUCE: vendored framework.
Important Context
Build: cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc) (build dir currently configured and passing).
Portability check: readelf -VW <Re.Trektor.so inside the bundle> | grep -oE "GLIBC_[0-9]+\.[0-9]+" | sort -uV | tail -3 must top out at GLIBC_2.38, and objdump -R must name no sqrtf/atan2f relocation; before the pin the only 2.43 pulls were exactly those two, this environment is Arch with glibc 2.43 while Bitwig's Flatpak root ships 2.42, and other plug-ins there load at 2.38/GLIBCXX_3.4.32 (NeuralNote.so fails identically over log10f@GLIBC_2.43).
Tests: cmake -S tests -B tests/build -DCMAKE_BUILD_TYPE=Release then build targets ReTrektorSmoke, ReTrektorCoreTest, ReTrektorEditorShot.
Smoke run: DISPLAY=:0 tests/build/ReTrektorSmoke_artefacts/Release/ReTrektorSmoke <bundle> [--screenshot=path].
Editor screenshot: DISPLAY=:0 tests/build/ReTrektorEditorShot_artefacts/Release/ReTrektorEditorShot /tmp/opencode/editor.png 1240 760.
Git (newest first): 1c2e544 the tracker, timeline and pool hand keyboard focus back once a menu, an alert or a drag has closed, since a parentless popup window or dragged item takes the focus with it and afterwards nothing is focused at all so the walk from a key press starts at the plug-in's top component and only the editor's own shortcuts are answered, which is why the whole keyboard went dead after a right-click menu or a pool drop and why only a click woke it, the injected focusRescue handing focus to the tracker rather than to the panel that asked so the note entry is heard again while the popups and the floating drag preview stay where they were; 26ab9f6 answer the Alt undo pair in every panel rather than only in the editor, since JUCE sends a key press to the modal component and walks up only that chain so the editor is never consulted while a right-click menu is open or while the pool's pattern drag is held by the DragAndDropContainer, one shared matcher in Source/ui/UndoRedoKeys.h giving all six panels the same body and claiming a press only where that panel holds the step, and the editor injecting its own performUndo / performRedo into all of them so one press walks one point of one line; 7f6ea6d the Alt redo alias Alt+Shift+Z is dropped since both Bitwig and Reaper swallow that combo before the plug-in sees it, the Alt block now refusing every Alt+Shift press so the Alt pair keeps one spelling each while the Ctrl spellings stay claimed; 30708bb undo and redo also answer Alt, which is the pair Bitwig and Reaper leave free since their own undo swallows the Ctrl trio before the plug-in sees it, the letter matched over the key code and the printed character because X11 hands an Alt-held key as the character it printed; 6e51af9 undo and redo walk the states the model changes made, the depth capped by the Settings tab's Undo steps row plus a 32 MB budget, the pair also in the tracker menu and the undo line living on the plug-in so a reopened editor finds the same points; 15e2513 group the tracker's four clears into one Clear submenu and let a hidden chip invert its plate instead of dimming; d103ccf copy the selected cells and paste them at the cursor with a field filter, the paste filter's submenu living in the cell menu and the pattern clipboard's pair moving under Shift; 292ff37 selection holds cells rather than rows so the tools stop hitting every channel; e8f3049 document that transpose reads a semitone amount and a direction; 410b1fe transpose is an editable semitone amount plus a direction button instead of four fixed steps; ef3417f document that the plug-in binary is pinned so an older libm can load it; c3e2f36 document the interpolation tool, its dropdowns and its default scope; ee29e69 pin the two float routines the plug-in takes through libm's newest symbol versions so a host with an older libm loads the binary; 2e67b16 interpolation fills the blanks between the values that already exist, with field and channel dropdowns and the panel opening on the selection-only scope; 40345cc document that a command argument uses the carried command number; 76115ec a command argument without a command number uses the command carried in the column; 205cfe1 document that the timeline ruler keeps only bar numbers; d2f672e the timeline ruler drops the transport readout; 8d077f7 document that a reopened editor keeps the window size; c12431b a reopened editor comes back at the size the closed one had; ece35e2 document that a pool drag floats one item, not the pool panel; df03559 a pool drag floats only the item's cell at the cursor; 3f14d6f volume/pan controllers precede the note-off branch; cd3fc2c volume column drives CC7, Settings name the volume/pan CC numbers, chips hide one field of one channel; f9acf12 instrument remap reads a source and a target dropdown; 08af633 anchor right-click menus at the cursor; 32499ef document that a timeline click parks the playhead; 02c0ca3 jump the playhead to a timeline click; 117744f document pool copy actions and the removed redundant clip menu item; ce6422d drop the redundant "Copy pattern data (unique)" clip menu action; ae95cac scrollbar + --scroll docs; 97f50ea tracker horizontal scrollbar; 93dd668 channel/limit docs; 0716702 hidden channel columns + constant 127 channel cap; 121ecd0 pattern resize preserves hidden rows; 9ffaaa4 EditorShot
--channels=N flag; a5e6e2d bottom-bar/settings channel docs; b02e52a CHANNELS slider direct edit; 1173682 stuck-note docs; b1725b1 stuck-note fix for hosts that skip MIDI-effect calls while stopped; de11909 pool grid preview; c4a8422 README macro-playback update; 49ec706 macro playback wiring; c36c6c7 ReTtrektor typo fix; 4263853 instrument fallback option; f89454e headless UI interaction test; c168e8b panels fit minimum window; a2ec7c9 Re.Trektor sources; ccc50f2 vendored JUCE 8.0.9.
DISPLAY=:0 / wayland-0 available; no Reaper; /usr/bin/carla exists.
Missing xcb-util-devel/xcb-util-wm-devel were installed via sudo dnf earlier (leftover from iPlug2 attempt, harmless).
iPlug2 and its VST3 SDK clone deleted from workspace; .gitmodules removed.
