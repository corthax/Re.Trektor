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

* `ReTrektorCoreTest` – model, sequencing, snapping, arpeggiator macros, slicing,
  paste, state, including that an older saved state which still carries properties
  the current build no longer reads, the dropped paste row's and the dropped hex
  row's, reloads its other settings, keeps its pattern and clip counts and does
  not carry them on, a transport re-arm putting
  every channel's all-notes-off ahead of the note-ons queued for that same block
  sample rather than after them, so the note whose row sits exactly at a start or
  exactly at a loop start is not released the moment it is struck, sending no
  All Sound Off there at all, playing the row a wrapped pass's block boundary
  landed inside once at that block's first sample with the stuck-key guard's
  note-off ahead of it and nothing closing it again inside that block, re-arming
  that row exactly once over the 50 blocks after it rather than twice, playing a
  row once at the first sample when the reported start sits a hair past it rather
  than a whole block inside it and playing it once only there, and
  keeping a fresh start inside a row and a seek forward into it honouring the
  mid-row setting while the wrap ignores it, the volume/pan columns' controller numbers, per-channel column
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
  discarded, so a paste never grows the pattern in either axis, and the paste's
  additive mode spelling masking each copied cell's own non-empty fields under
  that filter instead, so the target keeps its own value under every field the
  buffer left empty, an entry carrying nothing at all is skipped rather than
  written blank and a one-field additive paste only counts the entry that really
  carries that field), and the arpeggiator macros at the layer below a host:
  the grow-only table's defaults, which now arrive as eight steps that sound
  nothing, and its `firstUndefArpId` / `lastDefArpId` pair, a run whose row's own
  note is its first step and whose eight seeded steps land a sixth of a row apart
  in order, spilling past the row that called them and
  played by the blocks that reach them, a looped run cycling past that row while
  its cycle boundaries stay exact, a finished unlooped run leaving its last
  step's note held rather than repeating it, a row naming only the argument
  under a carried call switching to the macro that id names and restarting at its
  first step, an undefined id under that carried call sounding nothing while the
  run above it is still stopped there, the switched macro stepping its own offsets
  off the note the column named rather than off the note the run above was left
  sounding, the note that run left holding reaching the host as a note-off ordered
  ahead of any of the new run's note-ons, a call the column has no note for — under
  a note-off — sounding nothing rather than stepping off a note from above it while
  the call carried onto the note entering below it steps from that one, an `A 0`
  call releasing the note the run holds as a plain note-off rather than an
  All Sound Off, the release leaving the column calling nothing rather than
  calling its id so a further row there that names only an argument answers
  nothing and the run above is stopped rather than restarted, and a call whose
  row also holds the column's note-off being answered rather than ignored, its
  steps measured against the note that row is releasing while that note-off
  still takes the note away for the rows below it and reaches the host as a
  note-off ordered ahead of the run's first note-on at one sample, an empty step
  sustaining the step before it, a step landing off
  the keyboard snapping back by whole octaves to either end, the clip end closing
  a run that was still looping rather than the row that called it, the steps
  growing adds sounding nothing rather than a ramp the model no longer has, and the
  macro state round-tripping its id, its loop flag and all 64 signed steps — with an
  older state's `MACROS` section dropped outright, an older row's command `140`
  landing on the letter `A` with its old argument naming the id instead, and a
  ramping command number stopping at the highest CC rather than inventing a letter, and
  an undo handing an earlier state's own steps, loop flag and absence back rather
  than the steps the macro still had in place of them; the pitch bend letter and the
  wheel its argument names at that same layer: the bend's command value sitting above
  the ones an older build could have written so an older state's macro calls at both
  ends of that range land on the call letter rather than turning into bends, the
  argument's centre naming the wheel's exact centre rather than a position a step off
  it, the letter and its argument surviving the saved form unchanged, one standard
  pitch wheel pair moved ahead of the note-on of the row that asked for it at the
  same block offset so the note that row strikes is bent from its start, the note of
  that row reaching the MIDI channel its instrument names, the wheel moved once down a
  column and returning only at a note-off that also releases the note the channel was
  holding rather than being straightened by anything in between, argument-only rows
  under a carried letter moving that same wheel in turn through the wheel's bottom and
  back to the centre and to the first point again, and a row that bends without striking
  anything keeping the wheel bent through the clip's own rows with only that clip's end
  taking it back while its instrument names the channel both the move and the take-back
  reach.
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
  with nothing selected leaving the clipboard of before alone, the Alt+C / Alt+V
  / Alt+X steps the hosts leave free doing the same at every panel rather than
  only at the editor, since those hosts answer the Ctrl pair with their own
  clipboard, each panel copying what the tracker holds selected and pasting it
  at a fresh anchor
  at the cursor with everything so the whole copied cell lands there with its
  empty fields clearing the target and the second copied cell keeping its offset
  below it, that paste leaving exactly one point of the undo line for that one
  press, Alt+X cutting the same two cells so the data they held is already on the
  clipboard and the cells come back empty while the painted selection and the
  cursor stay where they were, the cut leaving one point of the line of its own
  and the following paste moving the cut cells to the cursor rather than
  repeating them there, the Alt held letter entering no note of its own at the
  cursor where a bare c would have typed a C and a bare x would have typed a D
  sharp, the steps taken by the lowercase spellings X11 hands over as well, and
  the Ctrl spellings of the copy and the paste left refused by every panel rather
  than claimed there so a host that does forward them still finds the plug-in's
  clipboard, while its Ctrl x spelling is not answered anywhere at all any more
  and neither is a Ctrl+Shift pair, since all three of those belong to the hosts
  and the slice reads Alt+S instead, the pattern clipboard's keyboard pair dropped
  outright with the pattern clipboard's own state and the clip count shown staying
  exactly as the timeline's and the pool's menus had left them, the paste
  submenus' whole-buffer and one-field additive leaves driven through the same
  paste call their callback makes so only the fields the buffer really carries
  land on top of what the target already held and its own survive under the empty
  rest, a one-field additive leaf skipping a copied cell that does not carry that
  field at all, while a copied
  cell carrying nothing at all leaves no change at all and so no point of the
  undo line, and a section placing one fresh clip of a fresh pattern for every
  panel's Alt+S press and splitting it at its own middle row so one press makes
  one clip two over two new patterns and records exactly one point of the line
  however far the press was routed, types no C sharp at the cell the cursor parks
  on for that held s, takes the lowercase spelling X11 hands it over, refuses an
  Alt+Shift s and a Ctrl s, and still claims the press while the transport runs
  without splitting anything, the Settings row that named the pattern paste's
  default dropped outright so the panel exposes no toggle under that name and the
  saved settings block no longer carries the property the undo line and the host's
  blocks save,
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
  note it played and redoing it putting it back, the Command column's letter `A`
  claiming the command number while a `10` typed into that field still types CC 10
  there, an `A` typed last of a run of digits claiming the field on its own and
  dropping the number it interrupts, and an argument-only row under a carried call
  staying implicit and naming an id that
  sounds nothing when nothing defined it, that same call being answered at a row
  that holds the column's note-off rather than skipped there and that release's
  `0` being drawn `OFF` with the column below it carrying neither the letter nor
  an argument, read through the tracker's own `carriedCommandNumber` while the
  rows above the release still carry it, the argument the column carries following
  the newest one its rows entered beside that letter, read through the tracker's
  own new `carriedCommandValue` above and under such a row, and a release written
  as only its argument under a carried call ending it there so both reads come
  back empty under one, and the Macros tab's one length field and
  looped tick per macro that exists, its `+ Add macro` and `- Remove selected` pair,
  the `- Remove selected` one clearing the macro the step cursor sits on rather than
  the last one a row can name, a number typed one character at a time at the drawn
  cursor so a sign and three digits land as one value on one step and stay one point
  of the undo line rather than one per press, `Escape` dropping a number half typed
  rather than writing half of it, the tab filled with 25 macros through the model and
  a refresh rather than through its own buttons so the undo line keeps the length the
  sections below lean on, clicked through the `MACROS` and `TRACKER` tab buttons since
  a tab that is not the one showing has no bounds to grab, the wheel moving its
  scrollbar and a click of the strip's column jumping the drawn thumb to the pointer
  while a drag carries it back to the head, a drag of a card rather than of the strip
  picking a step cursor up and leaving the list where it was, the strip's own
  shading changing every group of "Subticks per row" steps rather than every
  line of sixteen cells it wraps at, read through the panel's `stepBand`, and a
  drag of a length
  field or a click of the looped tick each taking one point of the undo line with
  `Ctrl+Z` handing the state's own steps back, the pitch bend letter claiming the same field as the bend rather than
  as a number and leaving the wheel's centre as the argument it had to write beside
  it, so a row naming only the letter bends nowhere, the argument typed next naming
  a point on that wheel and playing where that point is read in steps of 128, an
  argument-only row under a carried letter staying implicit and moving the same wheel
  again, an 11 typed as its own three decimal digits still giving CC 11 rather than
  the letter and the B typed after them claiming the field and dropping that number, a
  typed
  `144` giving the highest CC rather than inventing the bend letter, and that letter's
  claim being one undoable action the editor's `Ctrl+Z` and `Ctrl+Y` take back and put
  again, with the undo checks reading the cell back through the song rather than through
  a held reference since restoring a state replaces the pattern objects) and confirms
  which
  MIDI channel and which controller value the notes and columns end up on
* `ReTrektorEditorShot` – renders the editor headlessly and can dump the
  component layout (`--dump`, optionally `--tab=settings|macros|tools` to show a
  whole tab at once, the Settings tab's rows all being the decimal ones now its
  row offering another spelling has gone and the tools panel's instrument dropdowns
  naming their choices `-0 .. -15`,
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
  every value being drawn in that one decimal spelling, and `--drag-preview=IDX`
  compositing the image JUCE floats at the cursor while pool item IDX is dragged
  out of the pool, so a shot can show that only that item's cell follows the
  pointer), `--cmd-carry` to write a row that only names a command argument
  below the demo's command at row 20 so the rows under those two carry that
  argument rather than the command's own, and a call above a row naming only its
  `0` in the other channel over an explicit `0` argument in a channel that never
  named one, so a shot can show the dim carried number over the bright argument,
  the newest argument drawn dim on the rows under it, the release read `OFF`
  beside the letter carried into it with neither letter nor argument drawn over
  the rows under it, and the explicit zero beside all that, and `--select=6,1,9,1`
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
  the line's end rather than failing the shot), and `--arp=0,+4,-8` to write signed
  semitone steps into arpeggiator id 1 (a lone `.` naming a step that sounds nothing,
  so a Macros-tab shot shows a step strip of its own length rather than one that is
  all empty), `--arp-loop` to mark that macro looped so its card shows the looped tick,
  and `--macros=N` to have the song define N macros, ids 1..N all of them fresh, which
  is what makes a Macros-tab shot long enough that the cards do not fit and the tab has
  to draw the scrollbar and thumb it offers, and `--arp-call=ROW` to name an arpeggiator
  call in tracker channel 0 at that row, which is what paints the command column's orange
  `A` and its decimal id, `--arp-call-id=VAL` naming the id that call writes so `0` makes
  it the release, drawn `A OFF` with the rows under it carrying no letter and no argument,
  and `--arp-note-off=ROW` holding the column's note-off in channel 0 at that row, which
  is what puts a call beside a `===` rather than beside a note or an empty cell, and
  `--bend=ROW` with `--bend-arg=VAL`, 96 by default, to name
  a pitch bend at that row, which paints the lilac `B` and the point on the wheel it bends
  to in decimal beside it, so one shot can show both command letters and both of their
  decimal arguments)

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
  digits like the pan column. The command cell's two halves each take room for
  their own widest spelling rather than splitting the cell in half: the number's
  half takes its three CC digits with a two-pixel inner trim and the argument's
  half its three glyphs with a wider pad, so a macro id, a point on the wheel and
  a release's `OFF` all keep their last glyph rather than one half being cut tight
  by halving a wider total. The rows field above the line
  numbers sets
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
  menu's "Copy selection" — `Alt+C` for the hosts that answer the plain pair
  with their own clipboard — puts them on a cell clipboard that lives in the
  tracker view beside the selection — it is not saved with the song either,
  and it survives a pattern switch — keeping each cell's place inside the
  copied rectangle counted from its top-left cell, so the cells a Ctrl
  gesture left out stay missing from the clipboard and a paste never writes
  through them. The selected cells can also be cut rather than only copied: the
  cell menu's "Cut selection" item and the `Alt+X` key take that same copy and
  then empty exactly the cells they took, leaving the painted selection and the
  cursor where they were, so a following paste moves those cells to the cursor
  instead of repeating them there, and a cut on its own is the menu's Clear /
  Selection with the data kept. The clip
  slice at the playhead, which the timeline used to answer with `Ctrl+X`, moved to
  `Alt+S` rather than keeping a Ctrl spelling the hosts spend on their own cut. `Ctrl+V`
  or the cell menu's "Paste" submenu — `Alt+V` the
  same way — then writes them at the cursor, starting at its active row and
  channel, either all of it
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
* **Macros tab** – the arpeggiator macros the Command column's letter `A` calls.
  Nothing is painted above the cards but the tab's title and the one label row
  naming what its own rows hold (`MACRO`, `LENGTH`, `LOOPED`, `STEPS`), since what
  the rules are belongs to the Commands map and this file rather than to a memo the
  cards would have to be read under. One card per defined macro, named by the call a
  row writes (`A 1`) in orange: a `LENGTH` field (1–64 steps, the plug-in's usual
  number widget) and a `LOOPED` tick sit on the card's control line, and the macro's
  steps are drawn beneath it, 16 to a line, each as its signed semitones away from the
  base note (`--` being a step that sounds nothing), the strip shading every second
  group of "Subticks per row" of them - six at the default, so steps 0-5 sit on one
  shade and 6-11 on a darker one - which is where a step stops belonging to the row
  that called it, that group living in the Settings row rather than in the 16 cells
  a line wraps at, so a macro longer than its row reads as row spans rather than as
  one long run of numbers; the step cursor the arrows move
  and a click puts on one of the drawn steps is what enters a step at the keyboard, and
  a number typed there is built up in the cell before any of it is written, so a sign
  and up to three digits land as one value and the cell shows it as it is entered while
  it lasts, with `Escape` dropping it rather than writing half of it. `+ Add macro`
  defines the lowest id that has none yet with the default eight steps that all sound
  nothing, so a macro's shape is only ever where it was entered and lengthening one
  gains the same rather than a ramp at its tail; `- Remove selected` clears the macro
  the step cursor sits on rather than the last one a row can name, falling back to that
  highest one while there is no cursor yet. A macro of `LENGTH` steps has a card of
  `26 + ceil (LENGTH / 16) * 20 + 4` px, so a list of them longer than the panel's
  height is what the tab scrolls rather than grows without bound: a vertical strip on
  its right edge, 10 px in from that edge, whose thumb is as tall as the share of the
  track the cards that fit take, so the thumb reads as the window that is showing, and
  is turned by the mouse wheel as well as grabbed by the mouse, which is what the mouse
  alone does when a host gives it no wheel. A click anywhere in that strip's column
  puts the thumb's middle under the pointer and holds it there while the button is held,
  so the list can be dragged to a chosen place; a drag started anywhere else is a drag
  of a card, which picks a step cursor up rather than moving the list.
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
* The tracker cell menu keeps four items at its top level, each carrying its
  own choices: a "Clear" submenu (Selection / Cell / Channel / Pattern, with
  Selection greyed out while nothing is selected), "Copy selection" (greyed out
  while nothing is selected), "Cut selection" (greyed out the same way) which
  takes that copy and then empties exactly the cells it took, and a "Paste"
  submenu (greyed out while the cell clipboard is empty) whose items are
  Everything / Note / Instrument / Volume / Pan / Command / Command Value. What
  the cursor's last click parked is what the clears work on and what a paste
  starts at, and a cut leaves that same selection painted so the paste after it
  still starts there. The menu's Undo and Redo items sit
  after that quartet and walk the same line the undo and redo keys walk, so they
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
(Selection / Cell / Channel / Pattern), offers "Copy selection" and "Cut
selection", both greyed out while nothing is selected and the cut taking that
copy and then emptying exactly the cells it took so the selection stays painted
and the cursor stays parked where it was, and two sibling paste submenus, both
greyed out while the cell clipboard is empty and both carrying the same items
Everything / Note / Instrument / Volume / Pan / Command / Command Value, the plain
"Paste" one writing every field the buffer carries, empty ones included, and the
"Paste (Additive)" one letting only the fields the buffer really carries reach, so
the target keeps its own value wherever the buffer was empty; either menu paste
writes at the cursor the last click parked, at its row and channel, the same place
`Ctrl+V` and `Alt+V` write at, the menu's cut writing nothing but leaving those
cells empty, and the two items after them are the Undo / Redo pair.

The Macros tab's own strip is grabbed the same way its horizontal twin is: a click
anywhere in that strip's column, 10 px in from the tab's right edge and level with the
cards rather than with the foot holding its two buttons, puts the thumb's middle under
the pointer and carries it along while the button is held, so a list of macros longer
than the tab can be dragged to a chosen place by mouse alone, which is what a user has
to work with where a host gives the plug-in no wheel at all; the wheel turns that same
list as well. A click in a card is what picks a step cursor up there, so a drag started
away from the strip moves the cursor rather than the list, and the tab answers a pointer
in the strip's column ahead of its card hit-tests, which is why a grab of the strip never
reads as a click on the card behind it.

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
| `Ctrl+Z` | undo the last change |
| `Ctrl+Y` / `Ctrl+Shift+Z` | redo the last undone change |
| `Alt+Z` | undo, for the hosts that answer `Ctrl+Z` with their own undo, in whichever panel the host routed the press to |
| `Alt+Y` | redo, the same hosts, the same panel |
| `Alt+C` | copy the selected cells, for the hosts that answer `Ctrl+C` with their own clipboard, in whichever panel the host routed the press to |
| `Alt+V` | paste them at the cursor with everything, the same hosts, the same panel |
| `Alt+X` | cut them — the same copy followed by emptying exactly the cells it took, so `Alt+V` after it moves them there rather than repeating them; the painted selection and the cursor stay where they were, the same hosts and the same panel |
| `Alt+S` | slice the clip under the plug-in playhead at it, the clip becoming two over two patterns holding its halves; the hosts' own `Ctrl+X` cut never reaches the plug-in, so this is the shortcut's only keyboard spelling, shared by every panel, and the timeline's "Slice at playhead" menu item is its only other route |
| `F5` | switch to the Tracker tab |

The field filters (`Everything`, `Note`, `Instrument`, `Volume`, `Pan`, `Command`,
`Command Value`) sit in both of the tracker's right-click paste submenus, the plain
"Paste" one and the "Paste (Additive)" one, so the keyboard paste always writes
everything and clears the fields the buffer left empty — the `Alt+V` spelling the
hosts leave free included. The whole `Paste (Additive)` submenu is the paste that
never clears: inside each copied cell only the fields the buffer actually carries
land, on top of what the target already holds, so an empty field leaves the
target's own value in place rather than clearing it and a cell the buffer left
entirely empty writes nothing at all and leaves the undo line alone; a leaf of that
submenu and the matching leaf of the plain one differ only in that mode. The
pattern clipboard has no keyboard spelling of its own at all: the
`Ctrl+Shift+C` / `Ctrl+Shift+V` pair spent on it was swallowed on the way to both
hosts the way the plain `Ctrl` trio is, and it was dropped rather than moved again,
so copying and pasting a whole pattern is only what the timeline's clip menu, its
empty-area menu and the pool's menu do, each of which names its own instance or
unique choice.

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

The cell clipboard's steps belong to the same Alt spelling for the same two reasons:
Bitwig and Reaper answer `Ctrl+C`, `Ctrl+V` and `Ctrl+X` themselves, with their own
clipboard and cut, before the plug-in is offered the event, so the cells this plug-in
copies, pastes and cuts are unreachable there, and where the plug-in would be offered
them at all, the panel a menu or a drag belongs to is what answers the press and the
editor never is. `Alt+C` copies what the tracker is holding selected, `Alt+V` pastes it
at the cursor with everything, which is the plain spelling's meaning rather than one
field at a time, and `Alt+X` cuts, which is that same copy followed by emptying exactly
the cells it took: the painted selection and the cursor stay where they were, so a cut
on its own is the menu's Clear / Selection with the data kept and a cut and a following
paste move the cells to the cursor. The clip slice at the playhead, which the
timeline used to answer with `Ctrl+X`, moved to `Alt+S` instead of keeping a Ctrl
spelling the hosts spend on their own cut, since that letter was found in both hosts
to change nothing about the plug-in there, and it is answered by every panel with the
editor's own slice injected the way the clipboard's steps are, so a press routed to a
menu or a pool drag still splits the clip under the playhead at the snapped beat and
records one point of the undo line, and the timeline's "Slice at playhead" menu item
is the slice's only other route. All four letters are claimed by a panel whenever that
panel holds the step rather than only when the step did something, since c, v, x and s
are four of the note-entry letters and an Alt held one that fell through would type a
C, a D sharp, a G or a C sharp at the cursor beside doing its own work, so the tracker
answers the slice ahead of its own note entry, which would otherwise read this very s
as a C sharp at the cell the cursor sits on. `Ctrl+X`, `Ctrl+Shift+C` and `Ctrl+Shift+V`
are not answered by the plug-in at all now, so a host that spends them keeps its own
cut and its own duplication, while `Ctrl+C` and `Ctrl+V` keep the plain cell clipboard
the Alt letters reach, with the Shift spellings refused rather than left to read as the
plain pair.

Every numeric field is decimal only: the plug-in has no other spelling on offer
and no row that offers one. Instrument values are the MIDI output channel
(`0`–`15`), drawn with the column's dash as `-0` to `-15`, the same labels the
tools panel's remap dropdowns use. Volume and Pan take `0`–`127` as up to three
digits, displayed right-aligned like the pan column. The Command field accepts
`0`–`127` and the two letters `A` and `B`: an `A` typed as the command number
field's first character claims it and means `128`, the arpeggiator call, whose
argument names the macro id, and a `B` claims it and means `144`, the pitch bend,
whose argument names a point on the wheel. The CC `10` and CC `11` a column asks
for are typed `10` and `11`, and a letter typed after a run of digits only claims
the field, the number being typed having been dropped rather than read as a
command, so nothing about a number's spelling reaches one of the letters. A
letter writes the command value it names rather than a number that gets clamped,
since the number field tops at the highest CC `127` and only the letters
themselves reach above it: typing `144` into that field gives the highest CC
rather than the bend letter, and an interpolation ramp climbs toward it rather
than inventing a command. Each letter has to leave an argument behind, so a call
starts at the release id `0` and a bend starts at the wheel's centre `64`, which
is what makes a row naming only the `B` bend nowhere rather than lock the wheel
to its bottom. The call pair is drawn orange and the bend pair lilac, every
value in the view being decimal, and either letter a row carries from above
draws dim like any other carried command. So does the argument carried beside
it, and that argument is the newest one that column's rows entered rather than
the one the letter itself named, which is what an argument typed under a
command already in force changes: the rows under it are drawn with it beside
the letter still carried there. An argument of `0` typed under a carried call
is the arpeggiator release on its own, that row reading a bright `OFF` beside
the letter carried into it and the rows under it carrying neither the letter
nor an argument.

The Macros tab's steps are typed the same way rather than one press at a time: the
arrows and Page Up/Down move the step cursor over the cards it draws, with `Tab` joining
the down arrow, and a sign and up to three digits then build one step's number in the
cell the cursor sits on, which draws that number while it lasts rather than the step's
own value, so the digits are read as they are entered. The press that is not another
character of that number is the one that lands it, so `Enter` and any move of the cursor
commit it while `Escape` drops it and leaves the step's own value alone, which is what
keeps one number at one point of the undo line rather than one per keystroke. The sign
leads whatever digits were typed, a repeat of the sign the number already carries changes
nothing, one of the other side moves the number to the other side of the base note, and
a number that is nothing but a sign means the base note itself; the value lands clamped
to the jump the field can hold. `Delete` and `Backspace` make a step sound nothing, which
sustains the step before it rather than the base note.

## MIDI model

* Each pattern row has 5 columns per tracker channel: Note, Instrument,
  Volume, Pan and Command.
* Instrument selects the **MIDI output channel** (0–15). When unset, the last
  value used on that tracker channel is reused. The same carry applies to
  volume, pan and command, the command carrying its letter and its argument
  together and that argument being the newest one the column's rows entered
  rather than the one the letter itself named. Carried values are drawn
  dimmer; volume, pan and instrument only carry when a note exists, command
  carries even on empty
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
  column carries down from above, and the letter and the argument the column
  carries are both drawn dim over a row's own bright pair. The argument that
  row entered is the one its rows under it carry, rather than the argument the
  letter above called with, so entering a new argument under a command already
  in force moves what is drawn dim below it along with the row's own bright
  one; a row that named a command of its own answers with that row's own
  argument, which draws nothing beside a letter that had no argument of its
  own. Only a column that has never named a command gets the explicit `0` the
  entry has to write,
  and a column that carries nothing plays nothing. Command
  `128`, entered as the letter `A`, calls an arpeggiator macro and its
  argument names the macro id, `1`–`127`, with `0` releasing the note the run
  holds and leaving the column calling nothing rather than calling its id, so
  the rows under one carry neither the letter nor an argument and that release's
  `0` is drawn `OFF` instead - a row under a carried call naming only that `0`
  answering as the same release, its own row still drawing the letter carried
  into it beside the bright `OFF` and its rows under it carrying neither; the
  pair is drawn orange, and since decimal is the
  one spelling the whole view has, that argument's cell keeps the three glyphs a
  macro id, a point on the wheel and the release's `OFF` all need. An arpeggiator:

  * sounds one step per subtick, a subtick being a row over the Settings row
    "Subticks per row" (default 6, so a step is a sixth of a row);
  * holds up to 64 steps, 8 by default, each one the signed semitones to sound
    away from the base note and clamped to ±`127`, so a macro can be longer than
    the row that called it and the steps past that row's end play on while the
    blocks below it run; a macro that has just been defined holds eight steps that
    all sound nothing rather than a shape the plug-in invented, which is why a call
    of a macro nothing has been entered into plays the note its column named
    and a macro's shape is only ever where it was entered, growing one included;
  * reads a step that holds no value (`--`) as nothing to sound, which sustains
    the step before it, and a step landing off the keyboard snaps back by whole
    octaves rather than cutting, which is what keeps `+100` from a C still
    reading as a shape rather than as nothing;
  * loops when its `LOOPED` tick is set, starting back over at its first step
    after its last and keeping going until the clip that owns it ends, a note-off
    takes the column's note away or an `A 0` call ends it; a looped cycle is
    closed at its own foot by the next cycle's first step releasing the note the
    step before it left, at that step's subtick, which is why an empty step at a
    cycle's head keeps the note before it sustained rather than opening a gap. An
    unlooped macro leaves its last step's note held until the clip closes it
    rather than repeating it, which is why a finished run sounds nothing more;
  * steps away from the note its own column named rather than off whatever
    note the channel was left sounding: the call row's own note where it named
    one, otherwise the note that column's rows above last named, so one note at
    the head of a column drives every call down it and a second call lower in
    the column steps from the same note the one above stepped from rather than
    from whatever note that run happened to stop on. A note entering further
    down the column re-bases a run that is still going, and a column that has
    named no note, or whose note a note-off took away, has nothing to step
    from, so its steps sound nothing until a note enters below it;
  * answers a call whose row also holds the column's note-off (`=== A 1`) rather
    than ignoring it, the run measuring from the note that row is releasing while
    the note-off still takes that note away for the rows under it, which is what
    hands the host the release as a note-off ordered ahead of that run's first
    note-on at one sample and leaves a run the rows above left in force silent
    below the note-off until a note enters again;
  * stops the run its call replaces: the note that run's last step entered is
    released at the row that switched, ordered ahead of the new run's first
    step, so a call never keeps a note another run was left sounding under it.
    The release follows only a note the run itself entered, which is what a
    macro whose steps all sound nothing cuts nothing of its own and leaves a
    note the column struck there alone, and what a row that struck a note of
    its own needs no close for is already covered by that strike. A run that
    simply walked out its steps still leaves its last one held until something
    below closes it;
  * answers a row that names only the argument below a carried `A` as a call of
    that id, restarting the run at the new macro's first step, which is also why
    such a row invents no controller — and an id nothing defined sounds nothing
    at all, while still stopping the run above it rather than leaving its last
    note held under nothing; and a row under an `A 0` release has no carried call
    to answer, since a release leaves the column calling nothing rather than
    calling its id, so the argument such a row names reaches nothing and the run
    above one is stopped there rather than restarting under it.

* Command `144`, entered as the letter `B`, moves that tracker channel's pitch
  wheel and its argument names a point on the 14-bit wheel in steps of 128: `0`
  at the bottom, `64` at the wheel's exact centre 8192 and `127` near its top at
  16256, clamped to the wheel's own top 16383, so the pair the host is handed is
  the standard pitch bend message sent to the MIDI channel that row's instrument
  names, like every other command row. The wheel moves ahead of everything else
  that row asks for, which is what bends the note the same row strikes from its
  start rather than leaving it straight until the row below it, and an argument
  the row left empty is the centre, which bends nowhere. A bend is carried state
  the way a volume or a pan is, so the notes below the row that asked for a point
  stay bent until something releases them, and a row that names only an argument
  under a carried `B` moves that same wheel to a new point — which is also how a
  row further down puts a bend back after a note-off had taken it. The wheel
  returns to the centre at a note-off, at the end of the clip whose rows last
  moved it, which is what a row that bent without striking a note needs since no
  note-off of that channel was ever going to close it, at a transport re-arm and
  at the editor's own all-notes-off.
* A tracker pattern can use up to 127 tracker channels, independent of the 16
  MIDI output channels. Identical notes triggered on multiple tracker channels
  in the same row are sent only once.
* Stuck-note safety net: hosts like Reaper never call a MIDI effect while the
  transport is stopped, so note-offs emitted at stop time may never reach the
  synth. The engine remembers every pitch it has ever sent a note-on for, per
  MIDI channel (survives transport re-arms), always precedes a later note-on
  of such a pitch with a matching note-off, and closes clip ends with a
  tolerance for hosts that report block bounds slightly short.
* Transport re-arm: a fresh start or a transport jump releases what the synth
  may still be holding from the position the transport left, at that block's
  first sample, and those releases are ordered ahead of the note-ons queued for
  that same sample — all-notes-off sorts before a note-off, which sorts before a
  note-on — so a note whose row sits exactly at a playback start or exactly at a
  looped region's start survives the re-arm that follows it rather than being
  released the moment it is struck. Only CC 123 leaves the live paths: it is
  Bitwig that honours it, which is where the cut note at a start and the note
  missing at a loop's repeat came from, while Reaper's receivers ignore it. No
  All Sound Off (CC 120) is sent there, since nothing turns a channel that reads
  it back on and it would silence the volume and pan columns' output and the
  macros on the new pass; CC 120 goes out only with the editor's own "all notes
  off".
* There is no audio processing; the plug-in outputs MIDI and passes incoming
  MIDI through.

## Settings

* Ignore mid-row transport start (default on): with it on, a fresh playback start
  inside a row, or a seek that comes in from behind it, does not play that row,
  since the transport is only moving and the row is not being repeated; a
  transport that came back to an earlier position, which is a loop wrap or a
  scrub, plays the row its block boundary fell inside whatever this says, because
  the host meant that position and the row it fell inside belongs to the new pass
  — which is what lets a loop whose block boundary landed a little past its start
  still play the note sitting exactly on that start
* Subticks per row (default 6): how many equal parts one row is divided into,
  which is what names an arpeggiator step's width — a macro sounds one step per
  part, so 6 puts a step at a sixth of a row and lets the eight steps a fresh
  macro holds reach past the row that called the letter `A`; those eight sound
  nothing until entered, so it is their width rather than their number that a
  call of a fresh macro can hear
* Rows per beat (default 4, i.e. one row per 16th note)
* New pattern length in rows (default 64, maximum 1024) — resizing an existing
  pattern never discards rows: shortening hides the tail and enlarging brings it
  back; only rows inside the visible length are played
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

* Arpeggiator macros are named by id rather than by position, so clearing one
  leaves only the rows that called that id silent, and `- Remove selected` clears
  the macro the step cursor sits on rather than moving the ones below it — the ids
  a row can name stay put. Which macro that is comes from where the cursor was last
  put, by a click of one of its drawn steps or by the arrows, so a click of the button
  with no cursor aimed anywhere yet falls back to the highest id defined, and taking a
  macro back parks the cursor on another one rather than leaving it on the card that
  just went. What an older song cannot keep is its own macro section:
  the build has one command letter for that half of the column, so an older
  state's `MACROS` block is dropped outright and an older row's command value
  `128`–`143` lands on the letter `A` while the argument it holds stays
  the argument it was, which for the older encoding was the ignored value, so
  the macro that call now names — usually id `0`, the release — has to be
  re-entered by hand. This build's own bend letter is a command value `144`
  that older build could not have written, so no older row turns into a bend
  and none of those calls becomes one.
* The base note a call steps from belongs to the column rather than to the
  channel, so a call in a column that has named no note, or whose note a note-off
  took away, sounds nothing rather than stepping off whatever note another clip
  left held on that channel — the note has to be entered in that column above the
  call or in the call's own row. The run a call replaces is stopped at that row,
  its last step's note released there rather than kept under the new macro's
  subticks, and that release follows only a note the run itself entered, so a
  note the column struck under a macro whose steps all sound nothing is left to
  whatever ends it below rather than cut at the call.
* A call in a row that also holds the column's note-off (`=== A 1`) is answered
  there rather than ignored, its steps measured from the note that row releases
  while the note-off still takes the note away for the rows under it, so one such
  row reads as one release followed by a new run rather than as a row that
  sounds nothing; an `A 0` release ends the column's call as well as the run, so
  the letter and its argument stop being drawn over the rows below it, the
  release's own `0` reads as `OFF` and a further row naming only an argument
  there answers nothing. Both are the plug-in's own reading rather than a host's,
  and where a host puts that release and the run's first note-on at one sample
  is still known only from the hosts.
* A pitch wheel left bent by a channel whose clip was deleted outright is taken
  back only at the next release point or transport re-arm rather than by the loop
  that closes the notes and the arpeggiator runs of a clip the playhead is no
  longer over: such a channel has no note of its own to release and no clip end
  of its own to close, so a row that only bent keeps its wheel bent across that
  gap.
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
* Command kinds beyond plain CC and the arpeggiator call (a dedicated pitch
  bend CC105/CC106 pair, tempo, song position) are future work.
* Selection snaps to channel boundaries: a cell is one channel's visible
  fields, so selecting only one field of a channel — its volume alone, its
  command value alone — is not possible yet, and a paste of copied cells
  carries whole channels of cells rather than a single field's column. The
  paste filter can still be pointed at one field, and the fields the copied
  cells left empty clear the targets, which is how a pasted cell goes back to
  carrying.
* The pattern clipboard has no keyboard spelling of its own at all: the
  `Ctrl+Shift+C` / `Ctrl+Shift+V` pair spent on it was swallowed on the way to
  both hosts the way the plain `Ctrl` trio is, so it was dropped completely rather
  than moved to a third letter, and copying or pasting a whole pattern is only
  what the timeline's clip menu, its empty-area menu and the pool's item menu do,
  each of which names its own instance or unique choice. The clip slice likewise
  gave up the `Ctrl+X` spelling the timeline had answered with, since that letter
  is the hosts' own cut and was found to change nothing about the plug-in in
  either of them, so it reads `Alt+S`, one spelling shared by every panel rather
  than a Ctrl letter per host, since a slice has no per-host meaning to keep, and
  the timeline's "Slice at playhead" menu item is its only other route; the letter
  is one the note entry types as C sharp, so a panel holding the slice claims the
  press before its own key entry can leave a note at the cursor, which is also
  what keeps an Alt held letter spent there from falling through when the slice
  itself refuses. The cells keep their plain `Ctrl+C` / `Ctrl+V` pair, now with
  the Shift spellings refused rather than left to read as the plain one, so a host
  that spends `Ctrl+Shift+C` keeps its own meaning, and the only paste that never
  clears, now a whole "Paste (Additive)" submenu of the cell menu rather than one
  leaf of the plain "Paste" one, has no keyboard spelling of its own: `Ctrl+V` and
  `Alt+V` both write everything and clear the fields the
  buffer left empty. The Settings row that used to name that pattern paste's
  default, "Paste as unique pattern by default", lost its only reader with that
  pair and was then dropped outright rather than left shown and saved: the panel
  offers no toggle under that name, the settings struct holds no such member and
  the saved settings block no longer writes or reads the property, so a song
  reloaded from an older block simply finds nothing to read there, which leaves
  the paste of a pattern named by whichever menu asked for it, the timeline's,
  the timeline's empty area's and the pool's each naming their own instance or
  unique choice.
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
Columns: Note, Instrument, Volume, Pan, Command (+ hidden-able); --/--- when empty (Renoise style), instrument shown -0..-15, every value in the one decimal spelling the plug-in has.
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
That reasoning gives the cell clipboard's pair the same treatment, which is what the
user's next check found: Ctrl+C and Ctrl+V are the hosts' own clipboard, so Bitwig and
Reaper answer those two before the plug-in is offered the event and the cells this
plug-in copies and pastes are unreachable there, and where the plug-in would have been
offered them the panel a menu or a drag belongs to answers the press instead of the
editor. The spellings chosen are Alt+C copying what the tracker holds selected and Alt+V
pasting it at the cursor with everything, which is the plain spelling's meaning rather
than one field at a time; the Ctrl spellings were left as they are rather than moved
again, so a host that does forward them still finds the plug-in's own clipboard and
neither spelling changes meaning per host, and the pattern clipboard's Ctrl+Shift pair
was left under Shift rather than mirrored to Alt, since only the plain spellings had to
move to make room for the cells. Unlike the undo pair, this one has to claim a press
whose step has nothing to do as well as one whose step did, since a note is entered at
the tracker by letters and the two letters the pair names are two of them, so an Alt
held c would otherwise type a C at the cursor beside copying, and a paste answered both
at a panel and at the editor would walk the undo line twice for one press. The shared
header grew the pair next to the undo one and was renamed UndoRedoKeys.h to AltShortcutKeys.h
rather than a second header added, since it now names both Alt pairs the hosts leave
free instead of only the undo one, so the earlier references to that file's name below
are the same header under its old name. Both hosts found the pair works at these spellings
without anything further: the copy and the paste are heard straight after the actions that
had deafened the keyboard, at every tab rather than only over the grid, and one press does
it once, so the per-panel answer plus the claim that keeps an Alt held letter out of the
note entry and a paste out of landing twice are the whole shape the hosts needed.
Cut is the clipboard's third step rather than a new shortcut family: Alt+X was picked
as the spelling the same reasoning leaves free, and it takes the copy the Alt+C step
already makes and then empties exactly the cells that copy took, so the cut needs no data
and no paste filter of its own and the two halves are one action the cell menu's "Cut
selection" item reaches as well. A cut leaves the painted selection and the cursor where
they were rather than clearing either, since the painting is what says where the next
paste anchors and dropping it would make the pair mean two different things depending on
whether a paste followed it. Ctrl+X stayed the clip slice at the playhead it already was,
so the Ctrl letter x keeps one meaning inside the plug-in while the hosts' own Ctrl+X cut
stays theirs, and the cell menu kept its ids around the new item rather than renumbering
what the callback answers, ids 1..4 the Clear submenu's leaves, 5 the top-level copy, 6
the cut right after it, 7..13 the paste submenu's leaves and 14/15 the undo pair still
last, with the new item greyed while nothing is selected. The wipe shares one
clearSelectedCells loop with the menu's Clear / Selection so the two do not drift apart,
and a cut records exactly one point of the undo line, the wipe's alone, since the copy it
makes changes nothing in the saved song.
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
The user's next check hit the cell clipboard's pair with the same cause: Ctrl+C and
Ctrl+V did nothing at all, which the user named as the same issue as the Ctrl undo
trio. The pair is inert for two reasons stacked on top of each other, and both were
already known: Bitwig and Reaper answer Ctrl+C and Ctrl+V themselves, with their own
clipboard, before the plug-in is offered the event, so the cells this plug-in copies
and pastes were never reachable there, and where the plug-in would have been offered
them the panel a menu or a drag belongs to answers the press rather than the editor,
which is what the Alt undo pair needed the panel members for. The spellings the user
asked for are the ones the same reasoning gives: Alt+C copying what the tracker holds
selected and Alt+V pasting it at the cursor with everything, the plain spelling's
meaning rather than one field at a time, while the Ctrl spellings were left exactly as
they were so a host that does forward them still finds the plug-in's clipboard and no
spelling changes meaning per host, and the pattern clipboard's Ctrl+Shift pair was not
mirrored since nothing had reached for it. The shared header grew the pair next to the
undo one and was renamed UndoRedoKeys.h to AltShortcutKeys.h (commit 63d7b46) rather
than a second header added, since it now names both Alt pairs the hosts leave free: a
ClipboardKey, a matchClipboardKey reading C and V over the key code and the printed
character the undo matcher already reads, and an answerClipboardKey the six panels
chain after their answerUndoRedoKey. The clipboard pair claims a press whenever the
panel holds the step rather than only when the step did something, which is where the
undo pair's rule alone stops being enough: the two letters it names are two of the
note-entry letters, so an Alt held c would otherwise type a C at the cursor beside
copying, and a paste landed at a panel and answered again at the editor would walk the
line twice for one press. The pair reuses the wiring shape the undo pair had: copyStep
/ pasteStep std::function members on all six panels, injected by the editor with one
wireClipboard lambda giving every panel the tracker's own copySelectionToClipboard and
pasteClipboardAtCursor (kPasteAll), so the clipboard lives in one place, the paste that
wrote anything records its point of the undo line through the tracker's own change
whichever panel asked, and the editor's own keyPressed answers the pair through the
shared matcher as well, which is why its first line now includes the renamed header.
UiTest + 105 checks (390 total, PASS): all six panels hold both steps, and each of them
copies what the tracker holds selected for an Alt+C that records no point of the line,
enters no note at the cursor where a bare c would have typed one and leaves the cursor
where it was for the paste; each of them then pastes at a fresh anchor with everything,
so the whole copied cell lands at the cursor with its empty fields clearing the target,
the second copied cell keeps its offset below it, and that one press leaves exactly one
point of the line; each of them leaves an Alt+Shift combination, a letter neither pair
names and the Ctrl spelling refused rather than claimed, and the editor still answers
that Ctrl spelling with the same cells; the lowercase spellings X11 hands a key held
with Alt arrive as are taken by the tracker too. CoreTest 185 and Smoke 13 are
unchanged, the plugin rebuilt with only the pre-existing warnings, and the installed
bundle at /home/corthax/.vst3/Corthax/Re.Trektor.vst3 was refreshed to the new artefact
(sha256 8419a0a4d49d3a08a01800c2cb6ae98862874535cfeec4a2c94aa6d5c2ad428c), still topping
out at GLIBC_2.38 with GLIBCXX_3.4.32 and naming no sqrtf or atan2f relocation. Confirmed
in both hosts on the refreshed bundle: Alt+C copies what the tracker holds selected and
Alt+V pastes it at the cursor with everything, heard straight after a menu paste, a clear
or a slice-pattern and straight after a pattern was dragged out of the pool and dropped
onto the timeline with no click in between, at every tab rather than only over the grid,
and one press does it once rather than twice. The Ctrl spellings were left claimed, so in
those two hosts Ctrl+C and Ctrl+V still belong to the host's own clipboard, which is what
the Alt spelling works around rather than something the confirmation contradicts.
Alt+X cut selection and the cell menu's "Cut selection" item (commit 4e67f5f): the
clipboard's third step, taking the copy the Alt+C step makes and then emptying exactly the
cells that copy took, so a cut on its own is the menu's Clear / Selection with the data
kept and a cut and a following Alt+V move the cells to the cursor instead of repeating
them there. The shared header grew a cut enumerator on the ClipboardKey, the x mapped
over the key code and the printed character like the other two letters, and answerClipboardKey
a third step, with a cutStep member beside copyStep and pasteStep on all six panels and
one wireClipboard lambda injecting all three of the tracker's own steps, so whichever
panel the host routed the press to reaches the one cell clipboard. The tracker grew
cutSelectionToClipboard (guard on the selection, copy, clear, markChanged, repaint) and
the clearSelectedCells loop the cell menu's Clear / Selection now calls as well rather
than owning a second copy of it, and the cell menu gained id 6 "Cut selection" right after
"Copy selection", greyed while nothing is selected, leaving the paste submenu's 7..13 and
the undo pair's 14/15 where they were. Ctrl+X stayed the clip slice at the playhead, so
the Ctrl x keeps one meaning in the plug-in and the hosts' Ctrl+X cut stays theirs. A cut
records one point of the undo line, the wipe's, since the copy changes nothing in the
saved song. UiTest + 75 checks (465 total, PASS): each of the six panels cuts the
selection for an Alt+X that leaves the data on the clipboard, brings exactly the two
cells it took back to empty ones, records one point, leaves the selection painted and the
cursor unmoved so the paste after lands the cut cells at the anchor with their offsets,
types no D sharp where a bare x would have typed one, and refuses an Alt+Shift x and the
Ctrl x spelling; the lowercase x X11 hands over is taken as well, and the fixture's data
now comes through the clipboard and a recorded paste rather than a silent write into the
model, since a cut that returns the model to the state the cursor already sits on is
deduped and would leave no point to count. CoreTest unchanged at 185 and Smoke at 13, the
installed bundle refreshed to sha256 4da98f401f9c272ddae6d6627aef84754f4b1dfa4e76f4148e28af4d33083b34,
still topping out at GLIBC_2.38 with GLIBCXX_3.4.32 and naming no sqrtf or atan2f
relocation.
Alt+S clip slice at the playhead, the pattern clipboard's keyboard pair dropped and the
cell menu's "Additive (everything)" paste leaf (commit cf2ad7d), all three asked for by
the user's check in one go. That check found that Ctrl+X changed nothing about the
plug-in in either host and that Alt+Shift+X changed nothing there either, since the
letter is the host's own cut, so the slice's shortcut moved to Alt+S, the free letter,
rather than keeping a Ctrl spelling the hosts spend on their own cut, and the Alt+Shift
half of that finding was already the whole family's meaning rather than a cut of the
cells. The pattern clipboard's Ctrl+Shift+C / Ctrl+Shift+V pair was dropped completely
rather than moved to a third letter, since a pair found unreachable in the hosts has no
meaning left to move elsewhere and the timeline's clip menu, its empty-area menu and the
pool's item menu each already name their own copy and paste choice, which made both
doCopy() / doPaste (bool forceUnique) members and the editor's Ctrl+X and Ctrl+Shift
branches go away with them; the cells' Ctrl+C / Ctrl+V branches then had to learn to
refuse Shift, since a plain ctrl && keyCode () test would otherwise have swallowed the
Shift spelling into the cell clipboard and left the hosts' own Ctrl+Shift+C spent twice
over. The slice has one spelling shared by every panel rather than a Ctrl letter per
host, because a clip slice has no per-host meaning to keep, so the shared header gained
matchSliceKey / answerSliceKey beside the clipboard trio with the s read over the key
code and over the printed character like the other letters, an Alt+Shift or Ctrl spelling
refused, and the press claimed whenever a panel holds the step rather than only when the
step did something, which is what keeps this very letter from falling through as a note
entered at the cursor, s being the note entry's C sharp, so the tracker answers the slice
ahead of its own key entry. All six panels gained a sliceStep member beside cutStep and
the editor one wireSlice lambda injecting its own doSlice into each of them, so whichever
panel the host routed the press to splits the same clip at the same snapped beat through
the one funnel that records the slice's single point of the undo line and refreshes all
six views, and the editor answers the same step through answerSliceKey next to its
clipboard branch, which leaves the timeline's "Slice at playhead" menu item the slice's
only other route. The paste came in as kPasteAdditive (Model.h, a mode bit added to a
filter rather than one more field bit, so pasteCells kept its four arguments and its
callers kept their shape) with Pattern::pasteCells clearing that bit from the working
mask and then masking each copied cell's own non-empty fields under it, so an empty field
of the buffer leaves the target's own value in place rather than clearing it, a cell
carrying nothing at all is skipped rather than written blank, and a paste that wrote
nothing leaves the model unchanged and records no point of the undo line, the shape the
paste's filters already had, while a filter and the mode compose rather than replace each
other. Its menu leaf is id 16 "Additive (everything)" presented right after "Everything"
so the two all-field choices read together, kept out of the 7..13 run the static pasteMasks
map through and answered on its own by pasteClipboardAtCursor (kPasteAll | kPasteAdditive),
which leaves the submenu's one-field 8..13 and the undo pair's 14/15 where they were, and
the keyboard's Ctrl+V / Alt+V keep the clearing spelling, so the mode has one route, the
menu's. The pair's only other casualty is the Settings row "Paste as unique pattern by
default", whose sole reader was the dropped doPaste: nothing consults it any more, the
menus name their own instance or unique choice, and it is still saved with the song and
still shown, which is what leaves dropping or repointing that row a question rather than
a change made here. CoreTest 194 checks, +9 over the clipboard the clearing checks already
build, so the same two entries land their own note, volume, instrument and command pair on
top of what the target carried while its pan keeps the target's value where the plain
Everything had just cleared it, a one-field additive paste counts only the entry that
really carries that field and leaves the other's own value in place, and an all-empty
clipboard cell reports nothing written and leaves the target's whole cell alone where the
plain spelling still writes it. UiTest 525 checks, +60 over the same fixture: the menu's
paste driven straight at a clicked cursor, the buffer's empty fields leaving the target's
own command and instrument in place, the clipboard's own hole still skipped, an all-empty
cell additively leaving no change and no point of the line while the plain spelling still
writes it, the dropped Ctrl+Shift pair refused at the editor with the pattern clipboard's
valid flag and the clip count left exactly as they were, and a new section holding the
sliceStep in all six panels, placing one fresh clip of a fresh pattern for every press and
splitting it at its own middle row so one press turns one clip into two over two new
patterns and records exactly one point of the line however far the press was routed, with
the cell the cursor sits on reading back untouched, which is the Alt held s typed as no C
sharp; that section also takes the lowercase Alt+s X11 hands over, refuses an Alt+Shift s
and a Ctrl s, leaves the editor silent for Ctrl+X and the dropped pair, and still claims
the press while the transport runs without splitting anything. The Ctrl+X checks inside the
Alt clipboard section had to be reworded to name the host rather than the editor, since the
editor no longer answers that letter, and the fixture's rows 25..27 had to go back to empty
once the additive paste had filled them, since hidden rows survive a shortening only while
they hold data, which is what the shortening checks below read. Smoke 13 checks against the
installed bundle, all PASS; the bundle at /home/corthax/.vst3/Corthax/Re.Trektor.vst3 was
refreshed by rsync to sha256 8189f0ca32ff74a998ab71272eac407dd603c937e56569b7b2ca2c9bf3a280c0
and matches the built artefact, still topping out at GLIBC_2.38 with GLIBCXX_3.4.32 and
naming no sqrtf or atan2f relocation.
the cell menu's additive paste as its own submenu rather than one leaf (commit 12b1b88),
which is what the user asked for once the leaf was in: the additive mode presented "the
same way as regular paste menu with the same sub-menus", a "Paste" branch and a "Paste
(Additive)" branch each carrying its own leaves. the "Paste" submenu went back to just its
seven leaves and their ids, Everything / Note / Instrument / Volume / Pan / Command /
Command Value at 7..13, and a sibling "Paste (Additive)" submenu repeats exactly those
seven labels under a run of ids of its own, 17..23, greyed out by the same hasCellClipboard()
the first submenu is greyed by, so the mode is now something the user picks after the field
rather than one extra all-field choice sitting beside Everything. the old id 16 leaf and its
answering branch went away, 16 stays unused, and the undo pair keeps its 14/15, which leaves
every other id of this menu where it was; the callback's static pasteMasks moved up above the
if-chain so both runs map through it, the plain leaves by pasteMasks[result - 7] and the
additive ones by pasteMasks[result - 17] | kPasteAdditive, which is what keeps one array for
two submenus rather than a copy of its seven bits. nothing else moved: kPasteAdditive was
already a mode bit added to a filter rather than a field bit, Pattern::pasteCells already
cleared that bit from the working mask and masked each copied cell's own non-empty fields
under it, and pasteClipboardAtCursor keeps its one int fieldMask argument, so a leaf and its
additive twin differ by that one bit and the model layer stayed exactly as it was. UiTest + 3
(528 total, PASS): a one-field additive leaf driven through the same paste call the callback
makes, kPasteVol | kPasteAdditive, where the copied cell whose own volume is empty is skipped
whole so the target keeps its volume and gets nothing else of that cell while the copied cell
that does carry one lands at its offset below the anchor and writes neither its note nor any
other field, and still records its one point of the line; the paste stays at the cursor the
block above left rather than moving the cursor back up the pattern, since moving it makes
ensureCursorVisible scroll the view and the blocks below map their clicks through that view.
CoreTest unchanged at 194, its kPasteAll | kPasteAdditive, its one-field and its all-empty
checks already covering the mode at the layer below, and Smoke unchanged at 13 against the
bundle refreshed by rsync to sha256 d122990de58b5164895e83caaa8c9dcd5804131c2d75ed5b7e889444c821565c,
which matches the built artefact and still tops out at GLIBC_2.38 with GLIBCXX_3.4.32 and
names no sqrtf or atan2f relocation.
the Settings tab's "Paste as unique pattern by default" row dropped outright (commit 66606a7),
which is what the user settled about the row the dropped Ctrl+Shift pattern pair had left ownerless:
it was saved, shown and rewound by an undo while nothing consulted it, and rather than repointing
it at one of the menus that already name their own instance or unique choice it goes away on all
three levels, the Settings panel no longer adding the toggle and no longer re-reading it in its
refresh, the Settings struct losing the member, and the saved settings block losing the property
on both the write and the read back, which is what makes a song saved before the drop reload with
nothing to read there instead of carrying a value no one asks for and takes the row off the panel
rather than leaving a grey row behind. nothing else read the member and no behaviour had to move,
since the pattern clipboard's own menus are the only route to pasting a pattern and each of them
names its own choice, and the panel counts its rows rather than naming them by index, so the rows
after it shift up one and the window still fits. UiTest + 2 (530 total, PASS): the panel exposes
no toggle under that name, and the saved state, the form the undo line and the host's blocks use,
no longer carries the property, so the drop is shown on both the shown and the saved side rather
than only at the panel. CoreTest unchanged at 194, no instance or unique paste check ever went
through the setting, and Smoke unchanged at 13, its serialised-state check coming down from 2429
to 2405 bytes, which is the property that left the saved form; the bundle refreshed by rsync to
sha256 89808362688396eb234910f08e374e465d567bba5bf7e6aba2efdda1082ff530 matches the built artefact
and still tops out at GLIBC_2.38 with GLIBCXX_3.4.32 and names no sqrtf or atan2f relocation.
the older saved state that still carries the dropped property reloads cleanly (commit ef04bf5),
which is the half of the drop the plug-in cannot see from its own side: a song saved while the row
still existed carries a SETTINGS property nothing reads any more, and that is the state a host
hands back on a reload, so the read side of the round trip now has a check rather than a claim.
CoreTest takes the state the current song serialises, puts the dropped property back into its
SETTINGS child the way an older song's block holds it, and reads that whole state back: the other
settings reload, the pattern and clip counts come back the same, and re-serialising the reloaded
song does not carry the property on, which is what makes an older saved state a one-way read
rather than a thing the plug-in quietly keeps alive again. it is the same layer the undo line
restores through, since a step back reads one of these saved strings through song.fromValueTree,
so an older host block and an older undo point behave alike. CoreTest 197 checks, +3 over the
state round trip the volume/pan CC checks already build; UiTest unchanged at 530, which already
shows the panel exposing no toggle under that name and the current song's saved state carrying
no such property, and Smoke unchanged at 13 against the same bundle, its sha256 unchanged at
89808362688396eb234910f08e374e465d567bba5bf7e6aba2efdda1082ff530, since the drop is what that
bundle was built from.
the Bitwig-only transport re-arm cut fixed on the engine (commit 6056397), which is what a
note sitting exactly at a playback start being cut shortly and the note sitting exactly at a
looped region's start being skipped outright on its repeat both came down to, Reaper showing
neither because its MIDI receivers do not act on what the plug-in puts at those positions: at
a justStarted or jumped block the channel-wide release the engine queues and the note sitting
at that same sample go into the same buffer and are sorted by (offset, order), and the release
had been given order 1000 and 1001 while a note-on is order 1, so it reached the host after the
note-on it belonged ahead of and a host honouring an all-notes-off, which Bitwig does while
Reaper's receivers ignore it, released the note the moment it was struck. all-notes-off is now
order -2 and all-sound-off -1, ahead of the note-offs' 0, the note-ons' 1 and the volume and pan
columns' 2, and all-sound-off is emitted only where the whole channel is meant to go silent for
good, which is the editor's panic and no live path, since CC 120 has no companion message turning
a channel back on and would take the volume and pan columns' output, the pitch wheel and the
macros of a pass the transport is only moving into; the flag is a defaulted parameter, so the
transport-stop flush and the re-arm read as before and only panic passes it. the row sitting at
the start was being dropped as well: the row-boundary guard only let a row behind a block start
through when the run was near-continuous, which a jump is not, so the row the host left its block
boundary inside fell between that guard and the re-arm branch, which only owns a row strictly
behind the start; the guard now splits by continuity, a continuous run keeping the drift dedup
unchanged, which is what keeps a row from being played by two neighbouring blocks, and a
transport that is not continuous taking a row within the new named kRowAlignEps of its start as
the row the host meant and playing it at offset 0 rather than dropping it. the re-arm branch's
gate went from any jump to jumpedBack, a jump whose start is below the position the engine last
saw, so a fresh start and a forward seek into a row still honour the Settings row about a mid-row
start while a loop wrap or a scrub plays the row its boundary landed inside whatever that says,
which is what lets a loop whose boundary fell past its start play the note sitting on it on every
repeat; narrowing it that way was required rather than chosen, since the UiTest's playRow
fixtures park their transport a hair before a row, which is a forward jump, and letting those
re-arm the row they sit inside made two of its instrument and volume checks stop answering.
CoreTest 215 checks, +18 through a raw event recorder the harness already collects at, which
reads one block's events in the order the host is handed them so the checks name the ordering
and the wrap arithmetic at the layer below rather than what a host does with them, and it also
shows a start a hair past the row playing it once only and a seek forward into the row still
honouring the setting; UiTest unchanged at 530, which is the answer the narrowing had to keep,
and Smoke unchanged at 13 with its serialised state still 2405 bytes against the refreshed
bundle, sha256 b00c12f53451a63fc3ced750720a26aab97267e56c08d7a3fec73064dcd08f45, which still
tops out at GLIBC_2.38 with GLIBCXX_3.4.32 and names no sqrtf or atan2f relocation.
the arpeggiator's letter A and its macros replace the M0-M15 macro kinds and their command
values 128-143 outright (commit 04f13bb), which leaves the Command column's upper half one
command letter rather than sixteen macro slots: an A typed as the first character of a command
number claims that field as 128, the arpeggiator call, whose argument names the macro id, while
a number that merely has an a in it keeps its own spelling, which is what still lets the hex
CC 10 a column asks for be typed 0A with hex on rather than turning into a call; the pair is
drawn orange in decimal even where the rest of the view is hex and the A a row carries down
draws dim like any other carried command, and a row that names only the argument under a
carried A answers as a call of that id, restarting the run at the new macro's first step and
inventing no controller since a macro has none to answer. What the run does is the tracker's
own language rather than three hard-coded shapes: up to 64 steps, 8 by default, each one the
signed semitones to sound away from the note the channel holds and clamped to +-127, sounded
one step per subtick, a subtick being a row over the Settings row "Subticks per row" (default
6), so a macro can run past the row that called it and the steps beyond that row's end play on
while the blocks below it run; an empty step sustains the step before it, a step landing off
the keyboard snaps back by whole octaves rather than cutting so a step of +100 off a C keeps
its shape, id 0 releases the note the run holds rather than starting one, an id nothing defined
sounds nothing at all, a looped macro starts over after its last step and runs until the clip
that owns it ends, a note-off takes its base away or an A 0 call ends it, and an unlooped one
leaves its last step's note held until the clip closes it rather than repeating it. The macros
live in one grow-only table indexed by id - 1 beside the pattern rows and channels the build
already treats that way, a macro with no steps is not defined and only the macros that exist
hold a card, so the tab's + Add macro / - Remove last pair names firstUndefArpId and lastDefArpId
and the ids below a cleared macro keep naming the same macros; the tab itself is rewritten to
that shape, a card per defined macro with a LENGTH field the plug-in's usual number widget for
1-64 steps and a LOOPED tick on its control line, the steps drawn beneath 16 to a line as their
signed semitones with -- marking the step that sounds nothing and the step cursor the arrows and
a click move being what enters one at the keyboard. Two of the round's changes are fixes rather
than features: dragging a length field and clicking the looped tick changed the model without
giving the undo line a point of their own, so a later Ctrl+Z stepped back over a length change
without saying so and the panel now answers both through onModelChanged like every other change
it makes; and an undo hands back the song the changes are already in rather than a fresh one,
so the state's steps were being appended behind the ones the macro still held and the resize
that follows kept the model's own rather than the state's, leaving a macro's steps, its loop
flag and a macro the state does not name still in place after the walk back - the ARPS read now
clears the table ahead of filling it, since the section names every macro that exists, which is
what makes walking the undo line a true rewind of the macros as well as of the rows, and the
two were found together, since one UiTest action that changes nothing, a transpose at the edge
of an empty selection, was adding a point because the model no longer matched the state its own
cursor sat on. The old format is only half kept: an older state's MACROS section is dropped
outright and an older row's command 128-143 is clamped to the letter A while the argument it
holds stays the argument it was, which for the older encoding was the ignored value, so the
macro that call now names has to be re-entered by hand. CoreTest 240 checks, +25 over the re-arm
round, at the layer below a host: the grow-only table's defaults and the firstUndefArpId /
lastDefArpId pair, a run whose row's own note is its first step with eight steps a sixth of a
row apart in order spilling past the row that called them, a looped run cycling past that row
with its cycle boundaries exact, a finished unlooped run leaving its last step's note held
rather than repeating it, a row naming only the argument switching to the id it names and
restarting, an undefined id silent, an A 0 call releasing as a plain note-off with only its own
stuck-key guards around it, an empty step, the octave snap at both ends, the clip end closing a
run that was still looping rather than the row that called it, and the macro state round-tripping
its id, its loop flag and all 64 signed steps with an older state's MACROS section dropped and an
older row's command 140 landing on A keeping its old argument, plus the undo read taking an
earlier state's steps, loop flag and absence of a macro back; UiTest 564 checks, +34, driving the
letter's entry and its hex 0A neighbour through the tracker's own keyPressed, the panel's length
slider, looped tick, + Add macro and - Remove last and the step cursor typing a step, and showing
a drag of the length or a click of the tick as one undoable action each with Ctrl+Z taking the
state's own steps back; Smoke unchanged at 13 with the serialised state at 2396 bytes against the
refreshed bundle at sha256 2a8cc889ce52a717dd4d315c4363d37b06004da36a1bc576708ccb19890c5218,
matching the built artefact and still topping out at GLIBC_2.38 with GLIBCXX_3.4.32 and naming no
sqrtf or atan2f relocation; EditorShot gained --arp= / --arp-loop / --arp-call= so the Macros tab
shot shows a step strip of a length its own rather than the default ramp, and a tracker shot can
name a call and show the orange A and its decimal id in a channel.
the pitch bend letter B beside the arpeggiator's letter A, so the Command column's upper
half names two command kinds rather than one (commit 74cb0a1): a row writing B and an
argument sends the standard pitch wheel pair to the MIDI channel that row's instrument names,
its argument naming a point on the 14-bit wheel in steps of 128, 0 at its bottom, 64 at its
exact centre 8192 and 127 near its top at 16256 rather than the wheel's own top 16383; the
bend's command value 144 sitting deliberately above the command values 128-143 an older build
wrote for its M0-M15 macro kinds, which is what lets the read send every older row in that
range to the letter A with the argument it had and no older row becoming a bend, since 144 is
a value that build could not have written, and why the argument's centre 64 and the wheel's
centre 8192 are named as two constants in two spaces, kBendCentreVal in the model and kBendCentrePos
in the engine with a static_assert saying the first times kBendStep is the second - the model's
own having to be renamed since kBendCentre was already the engine's name for the position the two
would otherwise have clashed over, and the engine's kBendFull going away with the position
constants moved into its header. The two letters claim the command number as its first character
rather than being typed as a number, which is what keeps the hex CC 10 and CC 11 a column asks
for as their own 0A and 0B spellings, and the letter writes the cell it claims directly rather
than going through applyValue and clampValue, because the number field now tops at the highest
CC 127 in both clampValue and the interpolation's fieldTop: a typed digit run and a ramp can
only reach a plain CC, which is what makes typing 144 into that field give the highest CC rather
than invent the bend letter and stops a ramp from inventing either letter, the letters themselves
being the only route to a command kind. An entry has to leave an argument behind, so a call
starts at the release id 0 as before and a bend starts at the wheel's centre 64, which is what
makes a row naming only the letter B ask for nothing rather than lock the wheel to its bottom,
and bendTo reads an empty argument as that centre defensively as well. The pair is drawn in the
lilac theme::cmdOther, the theme's one command colour that had been going unused so Theme.h did
not change, the argument keeping its decimal spelling where the rest of the view is hex the way
a macro id does, and the pool's grid preview paints that command's tick in the same colour.
The wheel moves at event order 0 rather than the 3 a command message had, which is what puts it
ahead of everything else that row asks for and so bends the note the same row strikes from its
start rather than leaving it straight until the row below it, while a note-off that had just
taken the wheel back still sits ahead of the move, so the host sees the centre and then the new
point. The re-centre that startNote used to do went away with that ordering: a bend is carried
state like a volume, a pan or an instrument, so the notes below the row that asked for a point
stay bent until something releases them, and the re-centres stay where a release happens - the
note-offs of allNotesOff and stopNote, a transport re-arm, the editor's own all-notes-off, and a
new take-back at the end of the clip whose rows last moved the wheel, keyed on the ChannelState's
own new bendClip, which is what a row that bent without striking a note needs since no note-off
of that channel was ever going to close it. Under a carried B an argument-only row names a new
point on the same wheel, which is also how a row further down re-asserts a bend a note-off had
just taken back. One gap is left deliberately rather than fixed: a wheel left bent by a channel
whose clip was deleted outright has no note of that channel to release and no clip end to close,
so the loop that closes such a clip's notes and arpeggiator runs has no wheel of its own to
return and that channel keeps its wheel bent until the next release point or re-arm. Two of the
round's changes are to the harness rather than to the plug-in: its RawEvent grew a kind 5 for
the pitch wheel with the position read back as its pitch, so a check can name the order a host
is handed the pair in, and its ordering checks now find events by kind rather than assuming the
first raw event is the row's, since the host's re-arm sends sixteen all-notes-off ahead of the
first block's row events; and the existing interpolation check for the command column moved with
fieldTop, its anchors 120 and 140 now clamping to the highest CC so the ramp reads 121, 124 and
126 between them and its guard names kCcTop rather than the call. UiTest had two hazards of its
own to work around: the bend's writes stay at rows 20..23 of the front pattern, since a write
below the pattern's visible length grows its row capacity and the shortening checks further down
expect the capacity the clipboard fixture had left, and the section ends with a Home press so
ensureCursorVisible brings the view back to the top for the fixed-y clicks of the sections after
it, which a cursor parked at rows 24..28 had scrolled away. CoreTest 255 checks, +15 at the layer
below a host: the model taking the command value above the older build's range, the argument's
centre naming the wheel's centre exactly, the letter and its argument surviving the saved form
and an older state's M0, M12 and M15 calls landing on A rather than turning into bends; playback
showing one standard pair at 12288, the note of that row on the channel its instrument names and
the wheel ahead of that note-on at the same block offset; the wheel carried down a column moved
once and returning only at the note-off that also releases the note the channel was holding;
argument rows under a carried letter moving that same wheel through 12288, 4096, its bottom, the
centre and 12288 again, a note-off returning it and a further argument putting the bend back; and
a bend of a row that strikes nothing keeping the wheel bent through the clip's own rows with only
that clip's end taking it back, nothing of that channel having ever played and the row's instrument
naming the channel both the move and the take-back reach; UiTest 588 checks, +24, driving the
claim writing 144 and leaving the centre as the argument, the argument typed next naming a point
and playing where it is read in steps of 128, an argument-only row under the carried letter staying
implicit and moving the same wheel, the hex 0B still typing CC 11, a typed 144 giving the highest
CC and the claim being one undoable action the editor's Ctrl+Z and Ctrl+Y take back and put again,
with a sub-block in the dedicated undo section taking the letter back over a row that already held
an argument and reading the cell back through the song rather than a held reference since restoring
a state replaces the pattern objects; Smoke unchanged at 13 with the serialised state still 2396
bytes, a bend riding the command fields the state already had, against the bundle refreshed by rsync
to sha256 376b44a5aa292c2cf9155d4a6ce8bea9360b6669c17e7eda8d5181175af7ad81, matching the built
artefact and still topping out at GLIBC_2.38 with GLIBCXX_3.4.32 and naming no sqrtf or atan2f
relocation; and EditorShot gained --bend=ROW and --bend-arg=VAL, 96 by default, so a tracker shot
can show the lilac B 96 and the orange A 1 in the same channel, the shot at /tmp/opencode/bendcall.png
rendered at 1240x760 with --bend=2 --bend-arg=96 --arp-call=4 --arp=0,+4,-8,+12 --rows=32 --channels=4
cropped to the tracker band and read back at 2x showing the lilac letter and its decimal point at
row 2, the orange pair at row 4 and both letters carried dim below their own rows.
the Macros tab's five asks are answered in the tab rather than anywhere else: its description
memo is gone, the tab keeping its title and the one label row naming MACRO, LENGTH, LOOPED and
STEPS, that row moving up from y 76 to y 48 and kContentTop taking the cards up from 92 to 70,
since what the rules are belongs to the Commands map and to this file rather than to something
a scrolled list has to be read under; a newly defined macro arrives holding steps that sound
nothing rather than the ramp it used to be born with, kArpDefaultJump deleted outright, ArpMacro
::setDefault writing kArpEmptyStep for every step and Song::setArpLength growing on the same
value, so a macro's shape is only ever where it was entered and lengthening one never quietly
gains a ramp at its tail; a step's number is typed rather than committed one press at a time, the
value living only in stepText until the press that closes it arrives so a sign and up to three
digits build one value that reaches the jump the field can hold, the cell the cursor sits on
drawing that number while it lasts rather than the step's own value, and commitStepText firing
only when the incoming press is neither a further character of that number nor Escape, which is
what lets the arrows and a click of another card close it through setStepCursor as well and keeps
one number at one undo point rather than one per keystroke; the sign rules keep one leading sign,
a repeat of it changing nothing, the other sign moving the number to the other side of the base
note and a sign typed after the digits belonging to them; - Remove last becomes - Remove selected
and clears the macro the step cursor sits on, falling back to lastDefArpId while there is no
cursor and putting the cursor at 0 before rebuild parks it again; and the drawn scrollbar is
grabbed as well as turned, its geometry living in one set of helpers, scrollbarX, thumbHeight,
thumbTop and scrollToPointer, so paint and the drag cannot drift apart and the thumb a drag
centres on is the one that is visible, with mouseDown answering a pointer in the column 10 px in
from the tab's right edge ahead of the card hit-test loop, keeping the grab in draggingThumb
through mouseDrag while mouseUp clears it, and JUCE spelling that callback mouseDrag rather than
the mouseDragged the panel first declared, an override of which does not compile. The tests found
three hazards of their own to work around rather than accept: CoreTest's playback fixtures no
longer trust a ramp the model does not have, so a file-scope seedSteps (Song&, int id, int jump)
helper says for itself which offsets the six of them are listening for; the UiTest's undo checks
had to keep the length of the line the sections below lean on, since trimUndoLine caps the line at
the Settings' 50 points by erasing the oldest states, which is what made that section's step-typing
checks reworked rather than added, a digit typed and then dropped with Escape writing nothing and
taking no point of the line and the closed number being one undoable action, so the section still
makes exactly one edit and the additive-paste depth check further down stays at 49 before its paste
rather than saturating one check earlier; its arpeggiator playback fixture hand-seeds its macro and
hands it over with proc.syncPlaybackModel (), because the engine processes *playbackSong rather
than the live song the fixture writes to; and its new scrollbar section fills the song with 25 macros
through defineArp and a refresh rather than through the tab's own + Add macro button, which keeps
them a fixture rather than 24 edits, and clicks the MACROS and TRACKER juce::TextButton pair around
it, since a tab that is not the one showing is addChildComponent-hidden and gets no bounds at all,
0x0, so there is nothing for a grab of its strip to hit; the panel lays out at 908x688 there, 25 cards
of 50px each with 12 of them fitting, which is the panel the strip's thumb is drawn in at x about
1053. EditorShot gained --macros=N for exactly that, and its --arp= bullet says what it replaces now
that the tab has no ramp to replace - a strip rather than one that is all empty; the three shots of
the tab read back as the memo gone with the label row alone above the cards and the cursor highlight
on the first cell at /tmp/opencode/macros.png, 12-plus cards with the thumb as the light bar at the
top of the track at /tmp/opencode/macrosmany.png and its crop /tmp/opencode/macrosmany_c.png and its
strip's own column at /tmp/opencode/strip.png, and a fresh macro as eight -- at /tmp/opencode/macrosempty.png.
CoreTest 256 checks, +1 for the steps growing adds sounding nothing; UiTest 606, +18; Smoke unchanged
at 13 with the serialised state still 2396 bytes against the bundle refreshed by rsync to sha256
c9b5b9bf1fa7207bb4049a3a46d1416560eecd914cc958f2a14a433ac443a433, matching the built artefact and
still topping out at GLIBC_2.38 with GLIBCXX_3.4.32 and naming no sqrtf or atan2f relocation.
The arpeggiator's base note moved from the channel to the column (commit 19e4278): a second
call lower in one column used to take its base off the note the channel was left holding
rather than off the note that column named, so a column that entered 60 at its head, let a
macro walk to 88 and called a second macro further down stepped that one off 88 rather than
off the 60 - the reported symptom being that each new call took its octave off wherever the
run above happened to be sounding rather than off what the rows themselves asked for. In
ChannelState, arpBase is gone and columnNote takes its place, the note this channel's column
last named: processRow writes it for a note cell, clears it at the note-off where the run in
force used to lose its arpBase instead and leaves a cell that names no note alone, so the
note carries down the column the way an instrument, a volume or a pan does. flushArp reads a
step's offset off that note, which is what re-bases a run that is still going by a note
entering further down without the engine having to be told it is one, and arpCommand has no
base of its own left to work out, the run it starts measuring from the note the column named -
the call row's own note where it named one, the note its rows above left named where it did
not, and nothing at all where the column never named one, so the rule a note-off already used
for a run in force now covers the call itself rather than only that run. A call also stops the
run it replaces: closeArp releases the note that run last entered, tracked in the new arpNote
that endArp clears beside the run's other fields, at that row's first subtick, so the new run
starts above a release rather than keeping the note the run above was left sounding under its
first step - which is what a macro whose steps all sound nothing needed, one of those having
used to hold the previous run's note for the rest of the block, and what a run that had already
walked out its steps and left its last one sustained needed as well. closeArp releases only a
note the run itself entered, which is what leaves a note the column struck under such a run to
whatever ends it below rather than cutting it at the call, and arpCommand skips the close on a
row that struck a note of its own, that strike having released what was held above it already
and a second release having cut the row's own note at its first subtick instead. The one rule
that did not move is the sustaining one: a run that walks out its steps still leaves its last
note held, and a looped cycle is still closed by the next cycle's first step releasing the note
before it at that step's own subtick, which is what an empty step keeps sustaining the step
before it rather than opening a gap at a cycle's head. The tests found one fixture that encoded
the old rule rather than the new one, the value-only switch of a column whose 60 sits at its
head, whose switched macro now steps at 72, 84, 96 and 108 off that 60 rather than at 88, 100,
112 and 124 off the note the run was left sounding, its count of note-ons staying at 12 since
both bases sound four steps, and its two named offsets moving to the 96 and 108 only the new
base can reach; that fixture gained a check that the 88 the run left holding is released at the
row that switched ahead of the new run's first step, read through the harness's raw event order
the same way the re-arm checks read it, and a new fixture names the other half of the rule, a
call under a note-off in a column that names no note having nothing to step from while the
same call carried onto a note entering below it steps from that one. CoreTest 259 checks, +3;
UiTest unchanged at 606, its arpeggiator playback fixture hand-seeding its macro and handing it
over with proc.syncPlaybackModel () unchanged since a call that names a note had already
stepped from that note and nothing in that suite makes a second call in one column; Smoke
unchanged at 13 with the serialised state still 2396 bytes against the bundle refreshed by
rsync to sha256 3d20e3015718609b02ce7d6fed8d82fc0933b9dd1dc5e95a0cf80f6cf64c3503, matching the
built artefact and still topping out at GLIBC_2.38 with GLIBCXX_3.4.32 and naming no sqrtf or
atan2f relocation. The tracker reads back with the call in a column whose own row names no
note of its own at /tmp/opencode/arpbase2.png and its crop at /tmp/opencode/arpchan.png, the
call's own A drawn bright orange with its decimal macro id at a row whose note cell is empty
while the note column above named its note four rows up, and the A the rows below carry from
it drawn dim with no argument of their own.
The arpeggiator's display and its note-off row came answering in both layers (commit
6a7c41e): a call written in a row that also holds the column's note-off used to be
dropped with the note, since processRow's note-off branch released what the channel
was holding, took the column's note away and walked on through the row with a
continue, so the command written beside a === never ran and the user's report that
a setting of the command on a row with a note-off is ignored held until now. That
branch no longer returns early: the row's two facts are computed up front, noteOff
for the cell holding the note-off and noteHere for one holding a note, and the row's
command is answered after them. The note-off still releases the held note before the
call runs, so the run the rows above left in force is closed by that note-off rather
than by the command, and the call is answered against the note the column still had
named - the note that row is releasing - which the run keeps in the new arpCallNote
for as long as the column has none of its own, flushArp reading a step's base as
columnNote where it has one and arpCallNote where it does not. The note-off's
clearing of columnNote moved below the command blocks for that reason, and it is what
keeps the two earlier fixtures green rather than a third rule: the call the column has
no note for still sounds nothing, and the call carried onto a note entering below the
note-off still steps from that note. A duplicate note on a row now skips only its own
strike, so a note already sounding on the row still gets its command, and endArp takes
arpCallNote back beside the run's other fields while a further note-off below takes the
answer away from the rows under it. One release now agrees with the view: a row naming
the letter A and the argument 0 releases the note the run holds, arpCommand clearing
lastCmd and lastCmdVal when it ends it, so the column below one calls nothing rather
than calling id 0 and a row there naming only an argument answers nothing while the
run above is stopped rather than restarted - which is also what the tracker stopped
drawing a carried letter and a 0 over those rows, through the new file-local
commandLeavesCarry applied to both the display's carry table and carriedCommandNumber.
Two reads went with them: the display's Eff struct grew cmdVal, so the argument a
column carries is drawn as well as its letter and drawn dim like every other carried
value rather than left as a placeholder under a row that named neither, and the value
cell spells an arpeggiator's 0 as what it does rather than as a bare digit, an A 0
reading as A OFF. That spelling found the cell too narrow in a hex view, and had been
before this round as well: the argument's half was two glyphs there while a macro id
and a point on the wheel already kept their decimal spelling, so an id of 100 and a
bend above the wheel's middle clipped their last digit, while the release's three
letters needed the room too. fieldGlyphs now gives the argument three glyphs whatever
the view is, a plain CC's hex argument padding in the first two of them as the pan
column already does, and the command cell splits its two halves by their own glyph
counts rather than halving the whole cell, each half taking its own pad, which is what
puts a CC's two digits back inside a half that the wider total's halving had cut too
tight. The Macros tab shades its drawn step strip in row spans rather than in the
sixteen cells a line wraps at: a step is one subtick and a subtick is a row over the
Settings row "Subticks per row", so a step's group is its index over that many through
the panel's new stepBand, and the strip paints every second group at 0.58 alpha of
whatever colour the step's own state asks for, which is what makes a macro reaching
past the row that called it read as spans rather than as one long run of numbers. The
accessor sits public beside getScrollY for the same reason that one does, so a test
reads the boundary rather than a pixel. CoreTest 264, +5: a new fixture answers a call
beside a note-off, its steps walking the note that row was releasing, its note-ons
counted and its release read through the raw event order as a note-off ordered ahead
of that run's first note-on at the same sample; another names the release leaving the
column carrying nothing, the macro a further argument under one would have switched to
staying unplayed and the run above stopping rather than restarting below it. UiTest 609,
+3: the release drawn as its letter's OFF with the column under it carrying neither
letter nor argument while the rows above still carry the call, read through
carriedCommandNumber, and the strip's shading changing at six steps rather than at
sixteen cells, read through stepBand - the tracker writes of that first check going
through the model and being put back rather than typed, which is what keeps the undo
line the length the sections below lean on. EditorShot gained --arp-call-id=VAL and
--arp-note-off=ROW beside its --arp-call=, and its usage header now spells --tab=settings
as its parser already required. The five shots read back as the 12-step strip in two
shaded bands of six at /tmp/opencode/arpbands.png, the release drawn A OFF with the
rows under it carrying nothing at all at /tmp/opencode/arpbase-off.png and its crop
/tmp/opencode/off-crop.png, a call at a row holding the column's === bright there with
the rows under it carrying it dim at /tmp/opencode/arpbase-onoffrow.png and
/tmp/opencode/onoff-crop.png, a row's own bright A 1 over the dim carried ones and a
bright CC over its dim carried pair at /tmp/opencode/arpbase-dimcarry.png and
/tmp/opencode/dimcarry-crop.png, and the hex view's A OFF whole rather than clipped at
/tmp/opencode/arphex2.png; Smoke unchanged at 13 with the serialised state still 2396
bytes against the bundle refreshed by rsync to sha256
65a894722bc4249da197cd7672c2a619c17b69a3e14726c88572054639836ef6, matching the built
artefact and still topping out at GLIBC_2.38 with GLIBCXX_3.4.32 and naming no sqrtf
or atan2f relocation.
The values have one decimal spelling and there is no other to be offered (commit
d978758): the user asked for the hexadecimal display option to be dropped and decimal
kept, and that is what the plug-in already drew for a macro id and for a bend's point
on the wheel, so the row that offered the other spelling was a choice a host could
second-guess rather than one that bought anything. SettingsPanel stops adding the
"Hexadecimal values" toggle and the panel loses its hexToggle member, so its refresh
guard moved onto the mid-row toggle that is now the tab's first toggle - which is what
leaves the tab one row shorter and opening at "Ignore mid-row start" rather than at a
spelling row. Model.h loses the Settings::hexDisplay member and Model.cpp neither
writes nor reads that property in the SETTINGS block, so an older state that still
spells the choice reloads with nothing to read it into, re-serialising it does not
carry the property on, and the older state stays a one-way read rather than something
the plug-in quietly keeps alive again - the same shape the dropped paste row's
property already showed, and the two are now checked together in one fixture. The two
drawing helpers lost their hex branch rather than gaining a flag: valueString takes
only a digit count and pads a shorter decimal number with spaces so its digits land
in the same columns as a longer one's, and instString takes only the instrument and
writes the dash the column draws beside it. TrackerComponent lost its isHexDigitChar
helper outright and both numeric-key paths now take digits only, so nothing about a
number's spelling can reach one of the command letters any more - the CC 10 and the
CC 11 a column asks for are typed 10 and 11, and a letter typed after a run of digits
claims the field on its own with that number dropped rather than read, which is the
half of the earlier rule the tests had to keep rather than lose. maxDigitsFor and
fieldGlyphs lost their hex halves and name one spelling: the instrument's two digits,
three glyphs with the column's dash, and volume, pan, the command number and the
command argument three each, so the release's OFF, a macro id of 100 and a bend's
point above 99 keep their last glyph in the view rather than in one spelling's favour,
and the command cell still splits its two halves by their own glyph counts because the
halves' pads differ - the number's taking a two-pixel inner trim and the argument's
the wider kFieldPad - rather than by a wider total being halved. The row gutter, the
value cells, the command pair and the empty fields all draw in the one decimal
spelling, with the release still spelled OFF, and the tools panel's remap dropdowns
now label their instruments -0 .. -15, the decimal spelling that column shows them in.
The tests moved with the drop: CoreTest's state round trip writes and reads the mid-row
switch rather than a hex one, and its older-state fixture puts that dropped hex property
back into the SETTINGS child beside the dropped paste one to show both reload, both
leave the pattern and clip counts alone and neither comes back on a re-serialise; UiTest
drives the three decimal digits a command field takes, an A typed last of a run of digits
claiming the field and dropping them at one row and a B doing the same at another, the
remap dropdowns naming their instruments -0 .. -15 and the Settings tab exposing no
toggle under that old name - and its Settings-section and undo-reverts-Settings undoable
actions spending the tab's Snap to grid row instead of the row that used to switch the
spelling, each of them still exactly one point of the undo line, which is what keeps the
undo saturation the later sections ride at its cap where it was rather than moving it by
one; and EditorShot lost its --hex flag, its usage header's mention of it and the parse
block that read it, along with the setting it wrote. CoreTest 265 checks, +1, UiTest 614,
+5, and Smoke 13 all PASS, and Smoke's serialised state came down from 2396 to 2376 bytes,
which is the property that left the saved form - the undo line, the undo caps and the
state's other properties otherwise unchanged. The plug-in builds clean and the bundle
refreshed by rsync --checksum to sha256 bd6de09edc6280312122738ec87630bb8cea91a64841b4a1f2c3b7078e6a7537
matches the built artefact and still tops out at GLIBC_2.38 with GLIBCXX_3.4.32 and names
no sqrtf or atan2f relocation. The shots read back with nothing about a spelling left in
the view: the Settings tab opening at "Ignore mid-row start" with the row gone above it
rather than greyed and twelve rows beside it at /tmp/opencode/hexgone-settings.png with its
crops /tmp/opencode/settings-crop.png and /tmp/opencode/settings-rows.png, the release drawn A
OFF with the rows under it carrying nothing at all at /tmp/opencode/hexgone-off.png and its
crop /tmp/opencode/off-crop2.png, a call in a row holding the column's === drawn bright there
with the rows under it carrying it dim at /tmp/opencode/hexgone-onoffrow.png and its crop
/tmp/opencode/onoff-crop2.png, a macro id of 100 keeping all three digits of it over the dim
carried pair at /tmp/opencode/hexgone-id100.png and its crop /tmp/opencode/id100-crop.png, the
Macros tab's 12-step strip still in two shaded bands of six at /tmp/opencode/hexgone-macros.png,
and the tools panel's remap dropdown opening at All -> -0 at /tmp/opencode/hexgone-tools.png and
its crop /tmp/opencode/tools-crop2.png. The shot that showed the other spelling is not re-taken:
/tmp/opencode/arphex2.png belongs to a view this build no longer has and is cited nowhere.
The carried argument follows the newest argument the column entered (commit 9cda2f5): the
user's report was that entering a command value where the command was empty or carried left
the rows below showing the argument the letter above had called with rather than the one just
entered, so the view's carried argument had to follow the newest argument entered under a
command in force rather than only the letter. The drawing and the reads now step the column
through one helper so they cannot drift apart: a file-local stepCommandCarry sits beside
commandLeavesCarry and moves the letter and its argument together - a row that named a
command leaves that one and its own argument, an A 0 leaving neither, and a row that named
only an argument belongs to the command already in force above it, so it keeps that letter
carried and leaves the argument it named itself in force for the rows below rather than the
argument the letter above called with. An argument of 0 read under a carried call is the
release as much as its letter and its 0 are, clearing both, which is what the engine already
did there: arpCommand answers that row and clears lastCmd and lastCmdVal, so the view now
answers the same row the same way rather than carrying the call on over the rows under it.
drawRows fills its carry table from what the column carries into a row before that row steps
it, which is what a release written as only its argument still reads its letter on its own
row and draws nothing over the ones under it, and the value cell answers a row that named a
command of its own with that row's own argument, so a letter that had no argument of its own
keeps drawing nothing beside it rather than picking an argument up from above. carriedCommandNumber
walks the column down from its top over the rows above rather than up from the row, because
what a row leaves in force also depends on the arguments the rows above it entered - which an
up-from-the-row walk could not see, and which had left a letter and an argument drawn over the
rows under an argument-only release - and the new public carriedCommandValue reads the argument
the same walk leaves, so a test reads the carry rather than a pixel. Nothing moved in the
engine or in the saved form: processRow already answered a row naming only an argument with
that row's own value, and ChannelState's lastCmdVal was written there but never read, the view
having had its own carry table all along. The tests moved with the rule: UiTest 619, +5, reads
the carried id as 1 above the argument-only row the section writes and as the 7 that row
entered under it, the letter still the one carried beside it, and both reads back empty under
the argument-only 0 the section writes further down, the reads' fixtures written through the
model and put back rather than typed, which is what keeps the undo line the length the sections
below lean on; CoreTest 265 unchanged, its command-argument fixture already naming the engine's
answer of such a row; and EditorShot's --cmd-carry fixture grew the other channel's A 3 call
above a row naming only its 0 there, its usage header naming the flag now, so the shot reads
74 64 bright at row 20 with 74 dim beside a bright 99 at 21 and 74 99 both dim at 22 and 23 in
one channel, and A 3 with A OFF and then neither letter nor argument over the rows under it in
the other, at /tmp/opencode/cmdcarry.png with its crops /tmp/opencode/cmdcarry-ch01.png,
/tmp/opencode/cmdcarry-rows.png and /tmp/opencode/cmdcarry-ch1.png. Smoke unchanged at 13 with
the serialised state still 2376 bytes, since nothing about a row's shape or the saved form
changed, against the bundle refreshed by rsync --checksum to sha256
2f0b356850b19708de675c3806af200d487ca4a44a39616b27a0d3c31e5ce615 matching the built artefact and
still topping out at GLIBC_2.38 with GLIBCXX_3.4.32 and naming no sqrtf or atan2f relocation.
Active
Nothing is open in the code: this round's one change, the carried command argument
following the newest argument the column entered rather than the argument the letter above
it called with, is landed, built, installed and tested, and the decimal spelling and the
five asks before it are landed as they were with only the hosts hearing them still open.
The tracker's command column steps its letter and its argument together through one file-local
stepCommandCarry that the carry table, carriedCommandNumber and the new carriedCommandValue
all walk, so the view and the reads cannot disagree: a row that named a command answers with
that row's own argument, which leaves nothing drawn beside a letter that had none, and a row
that named only an argument keeps the letter carried above it and leaves its own argument
carried for the rows under it, an argument of 0 under a carried call clearing both because
that argument is the release the engine's arpCommand already answers there. The carry table
is filled from what the column carries into a row before that row steps it, which is what an
argument-only release still reads its letter on its own row and draws neither letter nor
argument over the ones under it, and carriedCommandNumber now walks down from the top of the
column rather than up from the row, since what a row leaves in force depends on the arguments
the rows above it entered. Nothing moved in the engine, whose lastCmdVal was written and never
read, or in the saved form. CoreTest answers 265 checks, UiTest 619 and
Smoke 13 all PASS, with Smoke's serialised state still 2376 bytes against the bundle
refreshed by rsync to sha256 2f0b356850b19708de675c3806af200d487ca4a44a39616b27a0d3c31e5ce615,
matching the built artefact and still topping out at GLIBC_2.38 with GLIBCXX_3.4.32 and
naming no sqrtf or atan2f relocation, so what is left of this round is the user seeing the
carried argument follow a row that entered a new one, and an argument-only release ending what
the column carries, in Reaper and Bitwig rather than anything still open in the code.

The Macros tab's five fixes stand landed as before it: the tab paints no description memo, keeps its title and the one MACRO / LENGTH /
LOOPED / STEPS label row at y 48 with kContentTop taking the cards up to 70; a newly defined
macro arrives as eight steps that all sound nothing, ArpMacro::setDefault and Song::setArpLength
writing kArpEmptyStep rather than the ramp kArpDefaultJump used to name, so a macro's shape is
only ever where it was entered; a step's number is typed rather than committed one press at a
time, the value living only in stepText until commitStepText lands it, so a sign and up to three
digits reach one value inside ±127, the cell the cursor sits on shows it while it lasts, the
arrows and a click of another card close it and Escape drops it; - Remove selected clears the
macro the step cursor sits on rather than the last one a row can name; and the drawn scrollbar
is grabbed as well as turned, its geometry living in the scrollbarX / thumbHeight / thumbTop /
scrollToPointer helpers one set of so paint and the drag cannot drift apart, with mouseDown,
mouseDrag and mouseUp carrying the grab. The pitch bend round stands landed as before it: the letter B claims the command number as 144 and leaves the wheel's centre 64 as the argument
it has to write beside it, bendTo sends the standard pitch wheel pair at the order that puts the
move ahead of the note its own row strikes, the read sends an older state's command 128-143 to the
letter A with the argument it had while only 144 reads back as a bend, and the clip-end take-back
keyed on the ChannelState's own new bendClip returns a wheel a row left bent without striking. The
arpeggiator round likewise stands: the letter A claims its half of the column, the macro table and
its ARPS section read and write, the Macros tab holds one card per defined macro with its LENGTH
field, LOOPED tick and drawn step strip, and the engine sounds one step per subtick and counts steps
rather than positions so a looped run's cycle boundaries stay exact. What the two letters leave the
engine with is one rule rather than two: a bend is carried state like a volume, a pan or an instrument,
so the re-centre that used to sit inside startNote is gone and the notes below the row that asked
for a point stay bent, the wheel returning to its centre only where a release happens - the note-offs
of allNotesOff and stopNote, a transport re-arm, the editor's own all-notes-off, and the end of the
clip whose rows last moved it. The gap left deliberately is the one release point that cannot be had:
a wheel left bent by a channel whose clip was deleted outright has no note of that channel to release
and no clip end of its own to close, so it is taken back only at the next release point or re-arm
rather than by the loop that closes such a clip's notes and arpeggiator runs. The old macro kinds are
still not reconciled: an older row's command 128-143 lands on the letter A keeping the argument it had,
which for the older encoding was the ignored value, so the macro those calls name has to be re-entered,
and the older state's own macro definitions are dropped outright - while this build's bend letter is
a command value that build could not have written, so no older row turns into a bend and none of those
calls becomes one. The new empty steps cost the older encoding nothing of its own: a fresh macro still
holds eight of them under the same length and the same saved base64 field, only read as nothing until
entered. The loop fix's residual is unchanged: the re-arm recovers the row a wrapped transport's block
start fell inside, so a wrap whose boundary landed more than one row past its start can still leave
the row sitting exactly at the start unplayed, and Bitwig was reported leaving its boundary inside
the first row, which is the case that now plays; a wider miss would need the re-arm to reach back
over every row the wrap skipped rather than over the one the start fell inside. Whether a mid-row
start reached by a forward seek should play the row as a wrap does is the user's to settle, and the
current build keeps that case honouring the Settings row.
Blocked
No Reaper installed; VST3 host testing limited to JUCE smoke test + Carla
availability. The modal routing that made the Alt pair inert cannot be
reproduced by the menu tests, which call the menu callback directly rather than
going through showMenuAsync or startDragging, so both the per-panel answer and the
focus hand-back are covered at the panel and at the drop and pinned by reading
ComponentPeer::getTargetForKeyPress, PopupMenuCompletionCallback and the MenuWindow
constructor rather than by a headless popup. The cell clipboard's Alt pair is covered
the same way rather than more strongly: it is pinned at each panel's keyPressed and at
the editor's own branch, so what a host does with Ctrl+C and Ctrl+V before the plug-in
sees them is still only known from the hosts, and no headless test holds a clipboard
of its own that the plug-in could be shown losing. The cut is covered in that same shape
rather than in a stronger one: it is pinned at each panel's keyPressed and in the editor's
branch, while the cell menu's "Cut selection" item and the callback that answers id 6 are
only read, since no test opens a popup through showMenuAsync or drives that callback, so
what a host does with its own Ctrl+X before the plug-in is offered it is still known only
from the hosts. The Alt+S slice and the two paste submenus are covered in that
same weak shape rather than in a stronger one: the slice is pinned at each panel's
keyPressed and in the editor's own branch, and the submenus' leaves by the paste
calls their callback makes rather than by the popup that would have selected one
of the new 17..23 ids, so the menu's own selection of a leaf, the fact that the
additive submenu really opens as its own submenu beside the plain one, and what a
host spends its Ctrl+X and Ctrl+Shift+C / Ctrl+Shift+V on before the plug-in is
offered them are still read-only. The Settings row the user settled by dropping
is the one question that no longer needs the hosts or a headless popup: UiTest
shows the panel exposing no toggle under that name and the saved settings block
carrying no such property, so what is still unknown about the Settings tab is
only what a host spends the keyboard spells it swallows before the plug-in sees
them. The transport re-arm is the same shape of cover rather than a stronger one: CoreTest names
the ordering of the channel-wide releases against the note-ons queued for the same block sample,
that no CC 120 leaves the live paths, the row a wrapped pass's block boundary landed inside being
played once at that block's first sample and the wrap's arithmetic over the blocks after it, but
what Bitwig does with an all-notes-off it receives and where it leaves a loop's block boundary
inside a row are known from its behaviour and the user's reports rather than from a host the
tests can drive. The arpeggiator is the same shape of cover rather than a stronger one: UiTest
drives the letter's entry, a 10 typed as its own three decimal digits, the A typed after them
claiming the field and dropping them, a row naming only an argument under a carried
call, the panel's length slider, looped tick, + Add macro and the - Remove selected one, the step
cursor and a number typed at it one character at a time, the scroll a wheel leaves on a taller
panel and a click and a drag of the drawn strip's own column, and CoreTest names the step widths,
the loop's exact boundaries, the empty step, the octave snap at both ends, an A 0 release, the
base a call takes from the column it is in rather than from the note the channel was left
sounding, the run a switched call leaving its last note released ahead of the new run's first
step, a call the column has no note for sounding nothing rather than leaning on a note from
above a note-off, the clip end closing a run that was still looping, the steps growing adds
sounding nothing and the undo read taking an earlier state's steps back; but a second call in
one column is named at the engine rather than shown heard in a host, the Macros tab's own
context menu and a drag made on a card's length bar rather than its value written through
setValue are still only read, and a number the tests hand over one KeyPress at a time is only
known to reach a host's keyboard in that shape, so what a host spends a key of its own on before
the plug-in is offered it hides that whole step rather than only the sign rules. Where a host's
MIDI receiver puts a note-on that lands a subtick rather than a row apart, how it spends a note-off
and a note-on that arrive at one sample, which is the shape a switched call hands it, how far it
carries a step's note while the run is still going, and what it does with the letter A before the
plug-in is offered the press remain known only from the hosts. The pitch bend is
the same shape of cover rather than a stronger one: CoreTest names the standard pair and the wheel
position its argument names, that the move leaves at the same block sample as the note-on of the
row that asked for it and ahead of it, that the argument's centre reads the wheel's centre exactly
rather than a step off it, that the wheel is moved once down a column and is not straightened by
anything between that row and the note-off which also releases the note the channel was holding,
that argument-only rows under a carried letter move that same wheel in turn through its bottom and
back, and that a clip's own end returns a wheel a row left bent while it struck nothing; and UiTest
drives the letter's claim and the centre argument it writes with it, the argument typed next, an
argument-only row under the carried letter, an 11 typed as its own three decimal digits and
the B typed after them claiming the field and dropping the number, a typed 144 landing on the highest
CC rather than inventing the letter and the claim being one point of the undo line the editor walks
back and forward; but where a host's MIDI receiver maps a 14-bit position to a sounding pitch, how
far it honours a wheel it was left with while the transport is stopped or across a block boundary,
whether it wants the standard pair at all rather than one wider message, how long a host holds a
bend for notes its own synth plays from a re-armed position, and what a host does with the letter B
before the plug-in is offered the press are known only from the hosts.
This round's five asks are the same shape of cover rather than a stronger one: CoreTest
names a call beside the row's note-off being answered there, its steps measured from the
note that row was releasing, its note-ons counted against the run and its release read
through the raw event order as a note-off ordered ahead of that run's first note-on at
the same sample, and names the release leaving the column carrying nothing so a macro
an argument under one would have switched to never sounds; UiTest reads the tracker's
own carriedCommandNumber as holding the call above a release and nothing at all under
one, with that release's argument drawn OFF, and reads the Macros tab's own stepBand as
changing at six steps rather than at the sixteen cells a line wraps at; and EditorShot
draws each of the three shapes in a shot - the dim carried letter beside a dim carried
argument, the A OFF with nothing carried under it and the call bright in a row holding
the column's note-off. What is still known only from the hosts is where their MIDI
receivers put a note-off and a note-on handed at one sample, which is the shape a call
beside a note-off sends them, how long a host holds the note that release cut, whether
a host spends its own reading of the letter A and the argument 0 before the plug-in is
offered the press, whether a host keeps the plug-in's own three-glyph argument cell rather
than restyling the column, and where the bands move when the Settings
row "Subticks per row" is set to anything but its default, which the tests only read at
the default.
The decimal round's one change is the same shape of cover rather than a stronger one: the suites
show the Settings tab exposing no toggle under the name the row used to have, the saved
state carrying no such property and an older state that still carries one reloading with
nothing to read it into and not carrying it on, the three decimal digits a command field
takes at the keyboard with a letter typed after them claiming that field and dropping them,
the remap dropdowns naming their instruments -0 .. -15, and EditorShot having no flag that
would ask for another spelling to render. What is still known only from the hosts is whether
a host restyles the numbers it shows at all - its own theme, its own font, its own digit
width - and where it puts the plug-in's own field widths once it has no spelling setting to
read, which is the whole of what a view the tests cannot drive can add.
This round's one change is the same shape of cover rather than a stronger one: the suites read
the argument a column carries through the tracker's own new carriedCommandValue rather than at a
pixel, so UiTest shows the carried id following the newest argument an argument-only row entered
under a carried call rather than the argument that call itself named, and both reads coming back
empty under a release written as only its argument, while EditorShot's --cmd-carry fixture grew a
call in the other channel above a row naming only its 0 there, so the shot shows the release read
OFF beside the letter carried into it with neither letter nor argument drawn over the rows under
it rather than the call being carried on. What is still known only from the hosts is whether one
sees the dim argument the rows under an argument-only row carry move to the newest one entered
rather than staying the one the letter above called with, whether an argument-only 0 under a
carried call reads OFF on its own row and leaves the rows under it drawing neither letter nor
argument, whether the row itself stays implicit rather than becoming an explicit call there, and
whether the undo line is unchanged by an argument entered under a command already in force, since
the suites only read the carry through the accessors and only render the shape to a PNG the hosts
never see.
Next Move
Confirm in Reaper and Bitwig that entering only an argument in the command column, with the
command number left empty under a command the column already carries, moves the dim argument
drawn on the rows under it to the newest one entered there rather than leaving it the one the
letter above called with, beside the letter still carried there, and beside the row's own bright
new argument; that an argument of 0 entered that way under a carried arpeggiator call reads
`OFF` on its own row beside the carried letter and leaves the rows under it drawing neither the
letter nor an argument, the run being released rather than restarted; that the row itself keeps
its cell implicit, the command number staying empty rather than becoming an explicit call, so
the undo line the editor's Ctrl+Z and Ctrl+Y walk is unchanged; and that a row that named a
command of its own still answers with that row's own argument, a bare letter keeping nothing
drawn beside it rather than picking an argument up from above.
Confirm in Reaper and Bitwig that the values have one decimal spelling and nothing else to
offer: that the Settings tab opens at "Ignore mid-row start" with the row that used to switch
the spelling gone rather than greyed and one row fewer in the tab beside it, and that no host
keyboard spelling swallows a row that is not there; that a CC 10 and a CC 11 are typed 10 and
11 in the command number and that typing 144 there still gives the highest CC rather than a
letter; that a letter typed last of a run of digits claims that field on its own and drops the
number it was interrupting rather than reading it as a command, the letters themselves being
the only route to a command kind; that the volume, pan and the command pair take the three
decimal digits they now do and the empty fields still draw --- rather than a shorter dash; that
the instrument column shows its -0 to -15 and the tools panel's remap dropdowns name those
same labels, so a remap choice reads as the column it acts on; that the command cell still fits
the argument's three glyphs - the release's OFF, a macro id of 100 and a bend's point above 99
all keeping their last glyph - without the number's half being cut beside it, the two halves
keeping their own pads; that the row gutter still counts its rows in decimal three digits wide;
and that undoing is as it was, the Settings tab's Snap to grid row taking one point of the line
back and putting it again as the undoable action the tests spend there.
Confirm in Reaper and Bitwig that a row holding the column's note-off and a call beside
it plays that call rather than nothing, its first step measured from the note that row was
releasing, and that the release and that step reach the host in that order at one sample
rather than as a note-off and a note-on of the same pitch the host doubles, reorders or
drops; that the rows under such a note-off still sound nothing of that run until a note
enters the column again; that a row naming only an argument under an `A 0` release sounds
nothing there rather than switching or restarting anything, while one under a real call
still switches as it did; that the tracker draws no orange letter and no argument over the
rows under a release and still draws the letter and the argument dim over the rows under a
call that named one; that the release's own argument reads `OFF` rather
than its `0`, that a macro id of 100 and a bend's point above 99 keep all three digits of the
one decimal spelling rather than losing their last one, and that a plain CC's argument pads in
that same three; and that the Macros tab's step strip changes shade every six steps at the default
"Subticks per row" rather than every sixteen cells, a macro longer than its row reading as
spans - the bands following that row where a host sets it to another value.
Confirm in Reaper and Bitwig that a column entering one note and calling two macros down it
plays both of them off that one note rather than the second one off wherever the first had
walked to - the reported symptom being that a new call took its octave off the note the run
above was left sounding, so heard again the two calls should sit over the same base and only
their own step offsets should differ; that the note the run above left holding is released at
the row that switched rather than kept on under the new macro's first subticks, which is what
an empty first step, or a macro nothing has entered into, makes audible as a cut there rather
than as a note held under the new run; that the release and the new run's first step reach the
host in that order at one sample when the call's row names a note of its own, so nothing is
heard doubled or cut short at the switch; that a call in a column that named no note, or whose
note a note-off took away, sounds nothing at all rather than stepping off whatever note the
channel happened to be holding from another clip, and that entering a note below it gives the
carried call a base again; that a note entering further down a column still re-bases a run
that is still going there rather than leaving it on the note above; and that a row naming a
note and a call in one cell still has that note as the run's first step, the run picking up
at the next one.
Confirm in Reaper and Bitwig that the Macros tab has no description memo over its cards any more,
only its title and the one label row naming MACRO, LENGTH, LOOPED and STEPS, so a list scrolled to
its foot is read against the cards rather than under a memo; that a macro the + Add macro button
just defined shows eight -- rather than a ramp, plays the note its column named when a row
calls it before anything was entered, and gains nothing at its tail when its LENGTH field is
dragged up; that a step's number can be more than one digit and can be negative - a sign, then two
or three digits, then Enter landing one value on one step rather than only its last figure, the cell
the cursor sits on drawing that number while it lasts so the digits can be read as they are entered,
Escape dropping it rather than writing half of it, and a sign typed after the digits it belongs to
leading them rather than trailing them; that - Remove selected clears the macro the step cursor sits
on rather than the last one a row can call, so clicking one of a macro's drawn steps and then the
button takes that macro back and leaves the ids above and below naming the same macros; and that the
tab's vertical scrollbar can be dragged with the mouse rather than only turned with the wheel - a
click in the strip's own column puts the drawn thumb under the pointer, holding and moving carries it
to a chosen place, a drag started away from the strip picks a step cursor up instead of moving the
list, and the wheel still works beside it.
Confirm in the DAW that a row writing the letter B and an argument moves the wheel to where that
argument is read in steps of 128, 64 bending nowhere and 96 sitting in the wheel's upper quarter,
that the standard pitch bend pair reaches the MIDI channel that row's instrument names, and that it
goes out ahead of the note that same row strikes so the note that row hits is already bent from its
start rather than straight until the row below it; that the bend carries down the column the way a
volume, a pan or an instrument does, so notes entered further down play bent until a note-off, the
end of the clip whose rows asked, a transport re-arm or the editor's own all-notes-off takes the
wheel back, and that a row naming only an argument under a carried B moves that same wheel to a new
point rather than inventing a controller of its own; that the letter draws lilac and its argument
is drawn in the view's one decimal spelling, that the B a row carries from above draws dim beside
the bright argument a row below writes, that an 11 typed there still gives CC 11 rather than the
letter, that typing 144 into the command number gives the highest CC rather than
inventing a bend, and that the pool's grid preview paints that command's tick lilac too; that a
Ctrl+Z or Alt+Z taken right after the letter's claim takes the command and the centre argument it
wrote with it back for one point of the line and Ctrl+Y or Alt+Y puts both again; and that a wheel
left bent by a clip whose rows only bent, with no note of that channel, is taken back at that clip's
own end rather than bending the notes of the clip below it.
Confirm in the DAW that a row writing the letter A and an argument starts a run at the note that
row holds, one step at a time a sixth of a row apart with the Settings row "Subticks per row" at
its default 6, and that the steps past the row that called them play on over the rows below rather
than being cut at its end; that the letter and its macro id draw orange in the view's one
decimal spelling, that the A a row carries from above draws dim beside the bright argument a
row below writes, and that typing 0 into a command number still types CC 0 while a 10 types
CC 10 rather than a call; that a row naming only the argument under a carried A
restarts the run at the macro that id names at its first step, sends no controller of its own and
sounds nothing at all when that id has no macro; that an A 0 call releases the note the run is
holding the way a note-off does rather than sending an All Sound Off; that a step drawn -- sounds
nothing and keeps the step before it held, and that a step of +100 off a C plays a note three
octaves below where the step would land, snapped there by whole octaves rather than sounding
nothing at all; that the Macros tab holds one card per macro that
exists with its LENGTH field reaching 64 steps, its LOOPED tick and its drawn step strip 16 to a
line, that a drag on the length bar keeps the steps already entered where they were and pads the
grown part with steps that sound nothing rather than with the base note, that + Add macro names the
lowest id that has none and arrives empty rather than with a ramp, and that - Remove selected takes
the macro the step cursor sits on back leaving the ids above and below naming the same macros, that
the arrows
and a click on the drawn strip move the step cursor and a typed sign and digits land on that step;
that a Ctrl+Z or Alt+Z taken right after a length drag, a click of the tick or a typed step takes
that one action back, the macro's steps with it, and that Ctrl+Y or Alt+Y puts it again; that a
looped macro keeps cycling while the clip that owns the call runs and closes its last held note at
the clip end rather than at the row that called it, while an unlooped one leaves its last step
held; and, in a project saved before this round, that its older macro calls land on the letter A
keeping the argument they had, that nothing about their sound survives and that re-entering the
macro the call now names is what brings it back.
start plays in full on every repeat rather than being skipped outright, that a note whose row
sits exactly at a manual transport start or at a seek plays in full rather than being cut
shortly, that notes still close at clip ends and nothing stacks up on a seek so Reaper's
stuck-note answer is intact, that a note a row or more past a start still plays normally on a
fresh start and on a wrap, and whether a mid-row start reached by a forward seek should also
play the row, since the current build keeps that case honouring the Settings row rather than
re-arming it the way a wrap does.
Confirm in the DAW that Alt+X cuts what the tracker holds selected in Bitwig and Reaper,
heard straight after a menu action or a pool drop and at every tab rather than only over
the grid, one press doing it once: the data the selection held going onto the cell
clipboard, exactly those cells coming back empty while the painted selection and the
cursor stay where they were, a following Alt+V moving the cut cells to the cursor rather
than repeating them there, and an Alt+Shift x or the Ctrl x spelling leaving the cells
alone while Ctrl+X is no longer answered by the plug-in at all, the slice having moved to
`Alt+S`. Still to confirm there for the cell menu too: that its "Cut selection" item
reaches the same step as the key and greys out while nothing is selected.
Confirm in Bitwig and Reaper that `Alt+S` slices the clip under the plug-in playhead at it
straight after a menu action or a pool drop and at every tab rather than only over the
grid, one press doing it once, the clip coming back as two over two patterns holding its
halves, its own cell types no C sharp at the cursor for that held s, the lowercase spelling
arrives as that one too, a press still makes one point of the undo line however far the host
routed it, and `Alt+Shift+S` and `Ctrl+S` leave the clips alone since both belong to the host;
the same re-test should show `Ctrl+X`, `Ctrl+Shift+C` and `Ctrl+Shift+V` changing nothing about
the plug-in while `Ctrl+C` and `Ctrl+V` keep its cell clipboard. For the cell menu, confirm that
the "Paste" submenu opens as two sibling submenus, "Paste" and "Paste (Additive)", each showing
the same seven leaves Everything / Note / Instrument / Volume / Pan / Command / Command Value,
that a leaf of "Paste (Additive)" lands only the fields the buffer really carries so the target
keeps its own values under the empty ones while the matching leaf of the plain "Paste" submenu
clears them, that both submenus grey out whole while the cell clipboard is empty, that a paste
of an all-empty cell changes nothing and leaves the undo line alone additively while the plain
spelling still writes it, and that the one-field leaves of both submenus filter as before,
"Command" moving the number the column carries and "Command Value" its argument; and in the
Settings tab that the row naming the pattern paste's default is simply gone rather than still
shown, with a song saved before the drop reloading without it and with no trace of the property
left in the state the undo line and the host's blocks save.
Re-test in Reaper: moving a pattern from time 0 (the stuck-note scenario) is now guarded by the engine and covered by CoreTest, but not yet confirmed in Reaper itself.
Confirm in the DAW that a timeline click (left and right button) parks the playhead where a paste lands, that the parked playhead survives while the host is stopped, and that the follow resumes once the host transport moves.
Confirm interactively that the right-click menus now open beside the cursor in the timeline, pool and tracker views.
Verify pool drag-drop preview and context-menu paste/slice flows interactively in Carla; confirm Reaper compatibility of the MIDI effect (instrument fallback IS_SYNTH build compiles locally but is untested in Reaper).
Further command kinds: dedicated pitch bend CC105/CC106 pair command, tempo and song position commands.
Grid preview currently always starts at row 0 (no preview scrolling yet).
Confirmed in Bitwig and Reaper that Alt+C copies what the tracker holds selected and Alt+V pastes it at the cursor with everything, heard straight after a menu paste, a clear or a slice-pattern and straight after a pattern was dragged out of the pool and dropped onto the timeline with no click in between, at every tab rather than only over the grid, with one press doing it once rather than twice. Still to confirm in the DAW for the cell clipboard: that the cell menu's own "Copy selection" and "Paste" items reach the same pair, that the "Paste" submenu's items filter to one field (and that "Command" moves the number the column carries while "Command Value" moves its argument), that a paste at the last channel or near the pattern's end drops what does not fit rather than growing the pattern, that Ctrl+C and Ctrl+V still reach the plug-in's own clipboard in a host that does forward
them rather than the host's, and that the pattern clipboard, now without any keyboard
spelling of its own, only moves through the timeline's and the pool's menus.
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
/media/data2/GameDev/Plugins/ReTrektor/Source/Model.h / Model.cpp: core data model, song/pattern/clip ops, snapping, slicing, paste, serialization, per-channel column visibility bytes (Pattern::vis), the volume/pan CC settings and the interpolation of one column between the values that already exist (InterpField, Pattern::interpolate taking a std::function<bool (int row, int ch)> mask so the selection can be any shape and a cell outside the mask ends the anchor run), and the cell clipboard (CellClipboard holding one {row, ch, Cell} entry per copied cell with its offset inside the copied rectangle, the kPasteNote/kPasteInst/kPasteVol/kPastePan/kPasteCmd/kPasteCmdVal bits and kPasteAll naming which fields reach plus kPasteAdditive = 64 as a mode bit added to a filter rather than one more field, Pattern::pasteCells writing them at an anchor and, when that bit is set, masking each copied cell's own non-empty fields under the working mask so an empty field of the buffer leaves the target's value in place and a cell carrying nothing at all writes nothing, still skipping a target past length or channels so a paste never grows the pattern), and the Undo steps setting (Settings::undoSteps, default 50 with kMaxUndoSteps = 200 as the cap, written and read back clamped with the rest of the SETTINGS block so the depth a host reloads belongs to the song, the same block having lost the pattern paste's default property on both the write and the read back along with its Settings member, which leaves a song saved before the drop reloading with nothing to read there, this block also losing the hex display's property on both the write and the read back with its Settings member and the Settings tab's row for it gone, which is what lets an older state that still spells that choice reload with nothing to read it into and leaves every value the model holds the one decimal spelling valueString and instString draw it in, a shorter number padded with spaces so its digits land in the same columns as a longer one's), and the arpeggiator macros (ArpMacro holding signed semitone steps off the base note, kArpCmd = 128 as the command value the letter A enters and kMaxArpMacros = 127 naming the id cap with id 0 the release call, kMaxArpSteps = 64, kArpEmptyStep = -128 as the step that sounds nothing and, this round, as the value every step of a newly defined macro holds and every step growing one adds, kArpDefaultSteps = 8 naming only how many of them a macro starts with while kArpDefaultJump was deleted outright since nothing builds a shape any more, Song::arps one grow-only table indexed by id - 1 beside the pattern rows and channels the build already treats that way so clearing a macro keeps the room its steps took and defining it again restores them, findArp growing it while peekArp reads it without growing it so the UI, the engine and the writer never invent a macro, arpDefined / arpLooped / firstUndefArpId / lastDefArpId / defineArp / clearArp / setArpLength clamped 1..64 and padded with kArpEmptyStep rather than with the base note, so a macro's shape is only ever where it was entered / setArpStep / setArpLooped naming it, the ARPS section writing a macro's id, loop flag, length and its steps as one base64 block rather than one property per step, an older state's MACROS section dropped outright on the read since those kinds are gone, an older row's command in the 128-143 range landing on kArpCmd while the argument it holds stays the argument
it was, and the ARPS read clearing the whole table ahead of filling it, since that section names every macro that exists, which is what hands an undo an earlier state's own steps, loop flag and absence rather than the steps the macro still held with the state's added behind them), and the pitch bend letter
(kCcTop = 127 as the highest plain CC number a column can hold, kBendCmd = 144 as the command value
the letter B enters and kBendCentreVal = 64 as the bend argument that sits at the wheel's centre,
that 144 chosen to sit above the command values 128-143 an older build could have written so the one
value this build owns for a bend is a value that build could not have written and no older row
becomes one while all of its macro calls still land on the letter they now mean, the read naming
that one value and mapping nothing else above the highest CC there, kCcTop naming the number
field's top so an interpolation ramp climbs toward the highest CC rather than inventing a letter,
and the Cell::cmd comment naming the three kinds a column can hold - a plain CC, the call and the
bend - beside a value-space comment saying that a bend's argument names a point on the 14-bit wheel
in steps of 128 rather than a position on it).
/media/data2/GameDev/Plugins/ReTrektor/Source/PlaybackEngine.h / PlaybackEngine.cpp: host-synced MIDI generation, carry/duplicate logic, explicit note-off panic, volume/pan driven by the Settings CC numbers, a value-only row using the command number carried in the column, and the transport re-arm: its
channel-wide releases are ordered ahead of everything queued at that same sample (all-notes-off
at order -2 and all-sound-off at -1, against the note-offs' 0, the note-ons' 1 and the volume and
pan columns' 2), the sound-off half only asked for by panic since CC 120 has nothing turning a
channel back on, the row-boundary guard split by continuity so a continuous run keeps its drift
dedup while a transport that is not one takes a row within the named kRowAlignEps of its start at
offset 0 instead of dropping it, and the branch that plays the row a start fell inside is gated
on jumpedBack rather than on any jump, so a fresh start and a forward seek still read the
mid-row setting and only a transport that came back to an earlier position ignores it, and the
arpeggiator run its letter A starts: a ChannelState holds arpId / arpCount / arpOrigin / arpNote / arpCallNote / arpClip / arpRowEnd and an inline endArp() that clears the note the run entered
and the note a row holding the column's note-off answered its call against, arpCallNote, with the
rest of them while leaving the column's own note alone, beside the new columnNote the run's steps
measure from - the note this channel's column last named, which processRow writes for a note
cell, clears at a note-off after that row's command has been answered rather than before it, so a
call written beside a === runs there and answers against the note that row is releasing, and leaves
alone for a cell that names none, so the note carries down
the column the way an instrument, a volume or a pan does -; the run counting steps rather than
accumulating positions so a step sits at arpOrigin + arpCount * stepPpq and a looped macro's cycle
boundaries stay exact rather than drifting, arpCommand answering the command value kArpCmd with
the argument as the macro id, id 0 releasing the note the run holds and clearing lastCmd and
lastCmdVal so the column under one calls nothing rather than calling id 0 and a row there naming
only an argument answers nothing, an id nothing defined
sounding nothing at all while still closing the run above it, and the run it starts having no base
of its own to work out since flushArp reads a step's offset off columnNote where the column has one
and off arpCallNote where it does not but a call at a row holding the column's note-off answered
there - the call row's own note where it named one, the note its rows above left named where it did
not and nothing at all where the column never named one and no such row answered it - and a note entering further down the column re-basing a run
that is still going by being that note rather than by a re-base of the engine's own; flushArp
putting one step's note on a subtick (a row over the Settings row "Subticks per row") at a time
with the steps of a macro longer than its row played on by the blocks that reach them, an empty
step sustaining the step before it rather than the base note, a step landing off the keyboard
snapped back by whole octaves so the step keeps its shape, a run still going flushed ahead of the
next clip's rows, capped at the row end per row, re-flushed when the transport re-arms and closed
by endArp at the clip end rather than at the row that called it, and closeArp releasing the note
the run a call replaces last entered at that row's first subtick, ordered ahead of the new run's
first note-on and skipped on a row that struck a note of its own since that strike had already
released what was held above it, releasing only a note the run entered rather than whatever the
channel happens to hold, and a finished unlooped run leaving its last step's note sustained until
the clip closes it rather than repeating it, and the pitch wheel its letter B moves:
kBendStep / kBendCentrePos / kBendTopPos naming the wheel's own positions in the header with a static
assert that the model's kBendCentreVal times the step is that centre exactly rather than a position a
step off it, a ChannelState holding the wheel position it last left in force and the clip whose rows
last moved it with an inline endBend() taking both back, bendTo answering the command value kBendCmd
by reading the row's argument as a point in those steps, an argument the row left empty as the centre
so a letter-only row bends nowhere, and the standard pitch wheel pair queued at order 0 rather than a
command's 3 so the wheel is where the row asked before the note that same row strikes is queued and
the note it strikes is bent from its start while a note-off that had just taken the wheel back still
reaches the host ahead of it, the re-centre that startNote used to send gone altogether since a bend
is carried state like a volume, a pan or an instrument and the notes below the row that asked for a
point stay bent until something releases them, the re-centres kept at the note-offs of allNotesOff and
stopNote and at a transport re-arm and the editor's own all-notes-off, a new take-back at the end of
the clip whose rows last moved the wheel keyed on that bendClip - which is what a row that bent without
striking a note needs, since no note-off of that channel was ever going to close it and only the clip's
own end can - sent to the channel that bend row's own instrument named, an argument-only row under a
carried kBendCmd moving that same wheel to a new point rather than inventing a controller, which is also
how a further row re-asserts the bend a note-off had taken back, and the command number's clamp moved
from the call to kCcTop on both the explicit and the carried path so no value above a plain CC can be
mistaken for one of the letters' own kinds.
/media/data2/GameDev/Plugins/ReTrektor/Source/PluginProcessor.h / .cpp: processor, transport snapshot, state, lock, the size of the last GUI shown, and the undo line (undoStates holding the saved song XML with undoCursor and undoBytes counted in UTF-8 bytes, pushUndoPoint appending what a model change reached and skipping one that changed nothing, undo() / redo() stepping and refreshing the snapshot, resetUndoHistory anchoring a fresh line for a state the host hands back, getUndoDepth / getUndoRedoCount naming what is left to walk, trimUndoLine dropping the oldest states against the Settings depth and a 32 MB budget while never dropping the state the model is in or one ahead of the cursor, and stepUndoHistory keeping the pattern clipboard, the edit playhead and the mirrored transport position a saved state does not carry).
/media/data2/GameDev/Plugins/ReTrektor/Source/PluginEditor.h / .cpp: main window layout, tabs, shortcuts, bottom bar, starting a new editor at the remembered window size, wiring the tools panel's isCellSelected hook to the tracker's selection, and the shortcuts where Ctrl+C/V name the cell clipboard (copySelectionToClipboard / pasteClipboardAtCursor) while the pattern clipboard's Ctrl+Shift+C / Ctrl+Shift+V pair is refused rather than answered, its two doCopy() / doPaste (bool forceUnique) members and its Ctrl+X slice branch gone altogether with that pair, the undo funnel commitEdit() (record the point the model reached, then refresh, so every component's onModelChanged ends in it while the bottom bar's OCT / STEP / CHANNELS handlers and selectPattern() record their own), performUndo / performRedo stepping the plug-in's line and refreshing the Settings panel as well because its rows hold their own values, and the Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z shortcuts with the Alt+Z / Alt+Y spellings the hosts leave free answering the same two steps, one each since the whole Alt+Shift family is refused rather than spent on a redo alias the hosts swallow, the Alt branches matching the letter over the key code and the printed character, all of them reaching the editor while every panel answers the Alt pair as well through one wireUndoRedo lambda that injects performUndo / performRedo into the tracker, timeline, pool, settings, tools and macros panels, so whichever panel the host routed the press to walks one point of the same line rather than one the panel owns, and one wireFocusRescue lambda injecting the same shape's focusRescue member into the tracker, timeline and pool so the panel that closed a menu, an alert or a drag hands keyboard focus back to the tracker (and to itself where the tracker is hidden), which is what makes the whole keyboard heard again rather than only the editor's own shortcuts, and one wireClipboard lambda injecting the cell clipboard's Alt steps as copyStep / pasteStep / cutStep members into all six panels with the tracker's own copySelectionToClipboard, pasteClipboardAtCursor (kPasteAll) and cutSelectionToClipboard, and one wireSlice lambda injecting the editor's own doSlice as a sliceStep member into all six of them beside those clipboard steps, so an Alt+C / Alt+V / Alt+X reaches one cell clipboard and an Alt+S reaches one clip slice at one snapped beat whichever panel the host routed the press to, and one press of either leaves one point of the undo line rather than two, the editor's own keyPressed answering those steps through the shared matchClipboardKey and answerSliceKey next to a Ctrl+C / Ctrl+V pair that refuses Shift so the hosts keep their own Ctrl+Shift+C / Ctrl+Shift+V meaning and no Ctrl+X branch left to it at all, and its first lines including the renamed header so no panel and the editor can disagree about what the Alt letters name.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/TimelineComponent.h / .cpp: timeline, drag/drop, context menus, zoom, ruler (bar numbers only, no transport readout), and the Alt pair of undo and redo answered in its keyPressed, which is where a press aimed at undo has to be heard while the timeline's own clip menu is open or while a pattern is being dropped onto it, and a focusRescue member it calls at the end of both context menus and ahead of the playback guard in its drop, since both the popup and the dragged item are parentless desktop windows that take the keyboard focus with them when they close, and the Alt clipboard steps answered after the undo one through copyStep / pasteStep / cutStep members the editor injects, chained through the 4-argument answerClipboardKey so an Alt+C / Alt+V / Alt+X pressed while this panel routes the press reaches the tracker's cell clipboard rather than the host's, and the clip slice answered last of all through answerSliceKey with a sliceStep member of the same shape, so an Alt+S heard while this panel holds the press splits the clip under the playhead that the timeline's own "Slice at playhead" menu item reaches rather than leaving the Ctrl+X spelling the host already spent on its own cut.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/TrackerComponent.h / .cpp: tracker grid, key entry, per-channel column visibility chips, field widths from fieldGlyphs, an entered command argument staying implicit over the command number carried in the column (carriedCommandNumber), cell selection (selBits one bit per (row, channel) in kSelWords = 2 uint64 words per row, applyCellRect unioning/subtracting/replacing word masks, gestureOp reading the held modifiers, channelAtX and channelBoundsFor naming a cell, per-cell painting and a hidden chip inverting its plate (theme::text plate, letter cut out in theme::panelLight) instead of dimming) and the cell clipboard (cellClipboard of CellClipboard type, copySelectionToClipboard putting the selected cells on it with their offsets inside the copied rectangle counted from the top-left selected cell and doing nothing while nothing is selected, pasteClipboardAtCursor writing at the cursor and only marking the model changed when it wrote something, cutSelectionToClipboard taking that copy and then emptying exactly the cells it took through the clearSelectedCells loop the cell menu's Clear / Selection calls as well so the two do not drift apart, marking the model changed for the wipe alone since the copy changes nothing in the saved song, hasCellClipboard greying out both of the menu's pastes, and the cell menu's four top-level items - a "Clear" submenu whose leaves keep the old ids so the callback still answers them by id (4 Selection / 1 Cell / 2 Channel / 3 Pattern), "Copy selection" (id 5), "Cut selection" (id 6 right after the copy, greyed while nothing is selected and answered by the same cutSelectionToClipboard the Alt+X key reaches), and two sibling paste submenus carrying the same seven leaves, Everything / Note / Instrument / Volume / Pan / Command / Command Value, the plain "Paste" one at ids 7..13 and the "Paste (Additive)" one at ids 17..23, both greyed by hasCellClipboard and both answered through one static pasteMasks the callback hoists above its if-chain so the field and the mode compose rather than a leaf owning the mode, pasteClipboardAtCursor (pasteMasks[result - 7]) and pasteClipboardAtCursor (pasteMasks[result - 17] | kPasteAdditive), 16 left unused between the two runs and the undo pair's 14/15 still last), and the undo pair (undoStep / redoStep std::function members the editor injects so the menu's Undo and Redo, ids 14 and 15 after the paste submenu's leaves and greyed out by the plug-in's own counts, walk the same line Ctrl+Z and Ctrl+Y walk and refresh every panel rather than only the grid, and cancelPendingEdit reporting whether a numeric buffer was half typed and closing it the way Esc does, which the Ctrl+Z and Alt+Z branches and the menu's Undo consult before stepping so one press is one effect, and its keyPressed now answering the Alt pair itself through answerUndoRedoKey, with cancelPendingEdit as the closing hook so a half-typed number still closes on that press rather than being walked over, while any other letter held with Alt is still handed up rather than typed as a note at the cursor), and a focusRescue member it calls at the end of the cell menu's callback, whether the press picked an item, a greyed one or dismissed the popup with Escape, and the Alt clipboard steps answered through the 4-argument answerClipboardKey with copyStep / pasteStep / cutStep members of the same shape the editor injects, placed ahead of the hand-up that keeps other Alt letters out of the tracker's own key entry, since the three letters they name are three of the note-entry letters and an Alt held c or x that fell through would type a C or a D sharp at the cursor next to copying, pasting or cutting, and the Alt+S slice answered the same way through answerSliceKey with a sliceStep member of the same shape the editor injects, this very letter being the note entry's C sharp, so the slice is claimed before the key entry can leave a C sharp at the cursor whether or not there was a clip to split, and the command letters: the A and the B branch sitting in the path that owns a value field's first character, so either letter claims the command number as its own command value while a number that merely has an a or a b in it keeps its own spelling and the CC 10 and CC 11 a column asks for are typed as their own 10 and 11, a letter typed after a run of digits claiming the field and dropping that number rather than reading it, the pair painted in theme::cmdMacro with every value drawn in the view's one decimal spelling, the A a row carries from above drawn dim through carriedCommandNumber beside the bright argument a row below writes, and clampValue's command number capped at kCcTop rather than at the call, both letters' claim writing the command value it names straight into the cell rather than as a number that gets clamped there, so a typed digit run and an interpolation ramp can only reach a plain CC and typing 144 into the field gives the highest CC rather than inventing a letter - the letters being the only route to a command kind - beside each letter leaving its own argument behind, a call at the release id 0 and a bend at the wheel's centre kBendCentreVal so a row naming only the letter bends nowhere rather than locking the wheel to its bottom, and the bend pair painted in the lilac theme::cmdOther that was going unused with that argument drawn in the view's one decimal spelling like every other value and that letter carried down a column drawn dim like the call's is beside the bright argument a row below writes, the command cell now splitting its two halves by their own glyph counts rather than halving the whole cell and each half taking its own pad, with fieldGlyphs giving the number's half three glyphs and the argument's half three more in the view's one decimal spelling so a macro id of 100, a bend's point above 99 and an arpeggiator release's OFF all keep their last glyph while the instrument's half keeps room for its dash and two digits, and maxDigitsFor taking the three decimal digits at the keyboard everywhere so no letter can be invented by a number's spelling, valueString and instString drawing every value decimal with no setting and no row offering another spelling, the display's carry table and the two reads walking the command column together through one file-local stepCommandCarry so they cannot disagree - a row that named a command of its own leaves that one and its own argument, the value cell answering such a row with that row's own argument so a letter that had none draws nothing beside it rather than picking an argument up from above, and a row that named only an argument keeps the letter carried above it and leaves the argument it named itself carried for the rows below rather than the one that letter called with, an argument of 0 under a carried call clearing both since that argument is the release the engine's arpCommand already answers there; drawRows fills a row's entry from what the column carries into that row before that row steps it, which is what an argument-only release still reads its letter on its own row and draws neither letter nor argument over the ones under it, and carriedCommandNumber walks the column down from its top over the rows above rather than up from the row, since what a row leaves in force also depends on the arguments the rows above it entered, beside a new public carriedCommandValue reading the argument the same walk leaves so a test reads the carry rather than a pixel, while the letter A with the argument 0 still releases and leaves the column under it carrying neither a letter nor an argument and the value cell spells that release OFF rather than a bare 0.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/PoolComponent.h / .cpp: pattern pool, drag source (the drag image is the item's cell), grid preview, rename, and the Alt pair answered in its keyPressed, which is what a press aimed at undo needs while the pool's own drag is still held by the DragAndDropContainer and has made that drag modal, and a focusRescue member it calls at the end of its own menu and of the rename alert, plus on the release of a drag, which ends whether or not the item ever landed on a target, and the Alt clipboard steps answered after the undo one through copyStep / pasteStep / cutStep members the editor injects, since the same reason that made the undo pair unreachable gives the clipboard's steps to the host's own clipboard and cut as well, and the clip slice answered last of all through answerSliceKey with a sliceStep member of the same shape the editor injects, so an Alt+S is heard even while the pool's drag is still modal and the editor is never offered the press at all, and its grid preview's command tick now painted lilac for the pitch bend beside the teal a plain CC and the orange a call keeps.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/ToolsPanel.h / .cpp: transpose as one semitone amount field (IncDecButtons slider with an editable text box, whole semitones 1-48 opening at 12) plus arrow direction buttons whose labels arrowUpGlyph/arrowDownGlyph build as explicit UTF-8, remap (source dropdown All/-0..-15 -> target dropdown -0..-15, the labels being the decimal ones the tracker's instrument column writes them in), cell scope (cellInScope answers whether a cell is in reach and applyToCells walks the cells, so the wider scopes name whole patterns and only the "Selection only" scope consults the tracker through the isCellSelected hook), interpolation with its single field dropdown (Note/Instrument/Volume/Pan/CC number/CC value, ids following the InterpField enumerators, the mask being the selection so there is no channel dropdown any more), the getCursorChannel hook still wired but unread, the scope box defaulting to "Selection only", and the Alt pair of undo and redo answered in its keyPressed, as is the Alt clipboard steps through copyStep / pasteStep / cutStep members the editor injects, so an Alt+C / Alt+V / Alt+X aimed at the cells is heard while a menu belongs to this panel, and so is the Alt+S slice, answered last of all through answerSliceKey with a sliceStep member of the same shape the editor injects.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/SettingsPanel.h / .cpp: settings controls, the row that used to switch the values between hex and decimal having been dropped outright along with its toggle member, its setting and its saved property, so every value the tab moves is decimal and its refresh guard now starts at the mid-row toggle, including the Volume column CC and Pan column CC sliders and the Undo steps row (undoStepsSlider, 0-200 defaulting to 50, sitting with the input prefs ahead of the CC pair and pushed back by refresh() so an undo that moved the depth shows in the row), and the Alt pair answered in its keyPressed so a press aimed at undo is heard while a menu belongs to this panel, as is the Alt clipboard steps through the same copyStep / pasteStep / cutStep members the editor injects, and the Alt+S slice through a sliceStep member of the same shape answered by answerSliceKey, since a popup opened over this panel is what routes the press here rather than the editor, and the "Paste as unique pattern by default" row dropped along with the Settings member it read and the property it saved, which leaves the panel counting one row fewer and its refresh re-reading the rest in their order.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/MacrosPanel.h / .cpp: the arpeggiator macros the Command column's letter A calls, rewritten from the old name/kind/two-parameter editor: one card per macro that exists, named by the call a row writes (A 1) in orange, the tab painting nothing above the cards but its title and the one MACRO / LENGTH / LOOPED / STEPS label row at y 48 rather than the four dim caption lines that used to sit between the title and the cards, kContentTop taking them up from 92 to 70, since what the rules are belongs to the Commands map and to the README rather than to something a scrolled list has to be read under; each card's control line holding a LENGTH field, the plug-in's usual IncDecButtons Slider over 1..kMaxArpSteps with an editable text box, and a LOOPED ToggleButton, and the macro's own steps drawn beneath it 16 to a line as their signed semitones off the base note, arpStepText drawing -- for kArpEmptyStep, +N for a positive one and 0 for the base note, -- being the step that sounds nothing and sustains the one before, and a newly defined macro arriving with eight of them that all sound nothing rather than with the ramp kArpDefaultJump used to name; the step cursor (stepId / stepIndex) the arrows and a click on a drawn step move is what enters a step at the keyboard, and a step's number now living only in stepText until commitStepText lands it rather than being written by every press, so a sign and up to three digits accumulate into one value, the cell the cursor sits on draws that number while it lasts rather than the step's own value, commitStepText fires from keyPressed only when the incoming press is neither a further character of that number nor Escape, which is what lets setStepCursor, the arrows and a click of another card close it as well, one leading sign being kept with a repeat of it changing nothing, the other sign moving the number rather than trailing it and one typed after the digits it belongs to leading them, a number that is nothing but a sign meaning the base note, the value landing clamped to ±127 as one point of the undo line rather than one per keystroke and Escape dropping it altogether rather than leaving half of it behind; the bottom of the panel holding + Add macro (defineArp at firstUndefArpId) and - Remove selected rather than - Remove last, that one clearing the macro the step cursor sits on, falling back to lastDefArpId while there is no cursor yet and taking the cursor off the macro it cleared before rebuild parks it on another one, its bounds widened to 132 so the longer label fits, so the ids above and below a cleared macro keep naming the same macros; the vertical scrollbar the tab draws on its right edge having its geometry in one set of helpers, scrollbarX at getWidth() - 10, thumbHeight, thumbTop and scrollToPointer, so paint and the drag cannot drift apart and the thumb a drag centres on is the one that is visible, with mouseDown answering a pointer in that column ahead of the card hit-test loop, keeping the grab in draggingThumb through mouseDrag while mouseUp clears it - JUCE spelling that callback mouseDrag rather than the mouseDragged the panel first declared, an override of which does not compile - and mouseWheelMove still turning the list beside it, which is what the mouse alone needs where a host gives the plug-in no wheel, and a public getScrollY () so a test can read the scroll back rather than only see it painted, beside a public stepBand (which) naming a step's group as its index over the Settings row "Subticks per row" - six at the default - named the same way rather than by the 16 cells a drawn line wraps at, the strip's paint shading every second group at 0.58 alpha of whatever colour that step's own state asks for, theme::cmdMacro for a step the macro holds or the cursor pending, else theme::noteOff, which is what makes a macro longer than the row that called it read as row spans rather than as one run of numbers and is what the accessor is public for, so UiTest reads the boundary at steps 0, 5, 6 and 12 rather than a pixel; rebuild / syncValues / cardHeight / blockTop / contentHeight / clampScroll laying the cards out against the scrollY that wheel and the drawn thumb move, and every change the panel makes answering through onModelChanged so the undo line holds a point for a drag of a length field, a click of the looped tick, a typed step and a cleared one as it does for the two buttons, which is what lets Ctrl+Z and Alt+Z take each of them back rather than stepping over them in silence; both Alt pairs answered in its keyPressed, the undo one and the clipboard one through copyStep / pasteStep / cutStep members of the same shape answered by the 4-argument answerClipboardKey, since the macros panel's own context menu is what routes a press to it rather than to the editor, and the Alt+S slice answered after the clipboard one through answerSliceKey with a sliceStep member of the same shape.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/AltShortcutKeys.h: the shared body every panel answers the Alt pairs the hosts leave free with, renamed from UndoRedoKeys.h once it grew the second pair (the references to that name above are this header under its old name). matchUndoRedoKey / answerUndoRedoKey as before (reading Alt+Z / Alt+Y over both the key code and the printed character and refusing Command, Ctrl and Shift so the whole Alt+Shift family stays the host's, claiming the press only when that panel holds the step, and closePending naming the half-typed numeric buffer the tracker closes on the press before stepping); matchClipboardKey / answerClipboardKey the new steps added to it (a ClipboardKey of none, copy, paste or cut, the letters read the same two ways, Alt+C copying what the tracker holds selected, Alt+V pasting at the cursor with everything and Alt+X cutting, which is that copy followed by emptying exactly the cells it took so the paste after it moves them, and the answer claiming the press whenever that panel holds the step rather than only when the step did something, since the three letters they name are three of the note-entry letters and an Alt held c or x that fell through would type a C or a D sharp at the cursor, next to copying, and a paste answered twice would walk the undo line twice); matchSliceKey / answerSliceKey the clip slice at the playhead added beside them, the s read the same two ways over the key code and the printed character, an Alt+Shift or Ctrl spelling refused so the whole Alt+Shift family and the host's own save stay the host's, and the press claimed whenever the panel holds the slice rather than only when the slice did something, since s is one of the note-entry letters and an Alt held s that fell through would type a C sharp at the cursor beside splitting a clip, while there is no Ctrl spelling of the slice left for a host and the plug-in to disagree about; matchSliceKey / answerSliceKey the clip slice at the playhead added beside them, the s read the same two ways over the key code and the printed character, Alt+Shift and Ctrl refused so the whole Alt+Shift family and the host's own save stay its, and the press claimed whenever the panel holds the slice rather than only when the slice did something, since s is one of the note-entry letters and an Alt held s that fell through would type a C sharp at the cursor beside splitting a clip, and there is no Ctrl spelling of the slice left in any panel for a host and the plug-in to disagree about.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/Theme.h: colors, fonts, LookAndFeel, unchanged this round: the Command column's three kinds already had the teal of theme::cmdCC and the orange of theme::cmdMacro, and theme::cmdOther, the lilac that had been going unused, is what the pitch bend's letter, its decimal argument and its carried spelling are now painted in.
/media/data2/GameDev/Plugins/ReTrektor/Source/Portability.cpp: the sqrtf and atan2f defined inside the plug-in so its libm requirement stays portable, aliased hidden in assembly and built without LTO.
/media/data2/GameDev/Plugins/ReTrektor/tests/CMakeLists.txt: smoke/core/editor-shot test targets.
/media/data2/GameDev/Plugins/ReTrektor/tests/CoreTest.cpp, UiTest.cpp, SmokeTest.cpp, EditorShot.cpp: automated verification harnesses (CoreTest drives the interpolation predicate directly with rectangle masks and a holed mask and pastes a cell clipboard built the way the tracker builds it, UiTest drives the selection gestures and the cell clipboard through juce::ModifierKeys-carrying mouse events and the editor's keyPressed for Ctrl+C, Ctrl+V and the Shift pair, and drives every panel's own keyPressed for the Alt+C / Alt+V / Alt+X steps, so each of the six panels is shown copying, cutting and pasting the tracker's selection at a fresh anchor with the cut leaving exactly the cells it took empty, the selection still painted and the cursor unmoved so the paste after it moves the cut cells to the anchor, and each of them refusing an Alt+Shift spelling and the Ctrl spellings instead; its fixture now puts the data into the selected cells through the clipboard and a paste rather than a silent write, since a cut that returns the model to the state the undo cursor already sits on is deduped and would leave no point for the checks to count; CoreTest drives the additive spelling of the same mask over the same clipboard, so the entries that carry something land their own fields on top of what the target carried while the fields the cell left empty keep the target's value, a one-field additive paste counts only the entry that really carries that field, and an all-empty clipboard cell reports nothing written where the plain spelling still writes it, UiTest drives that submenu pair's whole-buffer and one-field additive leaves through the same paste calls their callback makes at a clicked cursor, kPasteAll | kPasteAdditive and kPasteVol | kPasteAdditive, the one-field one showing a copied cell that does not carry that field skipped whole so the target keeps its value there while the copied cell that does carry one lands at its offset below the anchor and writes nothing else of its own, gives every panel the editor's sliceStep and splits one fresh clip of a fresh pattern for every Alt+S press at its own middle row so one press turns one clip into two over two new patterns and leaves exactly one point of the line, reads the cell the cursor sits on back again to show the Alt held s typed no C sharp, takes the lowercase Alt+s X11 hands over, refuses an Alt+Shift s and a Ctrl s and the editor's Ctrl+X and dropped Ctrl+Shift pair while the pattern clipboard and the clip count stay put, and still claims the slice while the transport runs without splitting, its fixture putting the rows the additive paste filled back to empty once it is done with them, since hidden rows survive a shortening only while they hold data and the shortening checks below read that, and its additive one-field paste staying over the cursor the block above left rather than moving the cursor up the pattern, since moving it lets ensureCursorVisible scroll the view and the blocks below map their clicks through it; EditorShot can hold a selection with --select=r0,c0,r1,c1 and paste it at a cursor with --paste-at=row,ch, and can walk the undo line with --undo=N / --redo=N before it renders, so a shot shows what an earlier action left behind retracted; CoreTest names the arpeggiator at the layer below a host, the grow-only table's defaults arriving as eight steps that sound nothing and the firstUndefArpId / lastDefArpId pair, the steps growing adds sounding nothing rather than a ramp the model no longer has, a run whose row's own note is its first step with its eight seeded steps landing a sixth of a row apart in order and spilling past the row that called them — six of its playback fixtures now say for themselves which offsets they are listening for through a file-scope seedSteps (Song&, int id, int jump) helper rather than trusting a ramp the model no longer builds — a looped run cycling past that row with its cycle boundaries exact, a finished unlooped run leaving its last step's note held, a row naming only the argument under a carried call switching to the macro that id names and restarting at its first step while an undefined id sounds nothing but still stops the run above it, the switched macro stepping its own offsets off the note the column named rather than off the note the run was left sounding with its two named offsets moved to the two only that base can reach, the note the run above left holding reaching the host as a note-off ordered ahead of any of the new run's note-ons, a call under a note-off in a column that names no note having nothing to step from while the same call carried onto a note entering below it steps from that one, an A 0 call releasing as a plain note-off with only the stuck-key guard's own extras around it and no All Sound Off, a call written in the note-off's own row being answered there rather than
dropped with the note, its steps measured off the note that row was releasing while its note-ons
are counted and its release read through the RawEvent order as a note-off arriving ahead of that
run's first note-on at one sample, and the release leaving the column carrying no call so a macro
an argument written further down would have switched to is never played and the run above stopped
rather than restarted below it, an empty step, the octave snap at both ends of the keyboard, the clip end closing a run that was still looping and the macro state round-tripping its id, its loop flag and all 64 signed steps with an older state's MACROS section dropped outright and an older row's command 140 landing on the letter A keeping its old argument, and an undo read taking an earlier state's steps, loop flag and absence of a macro back rather than what the macro still held; UiTest drives the letter's entry, a 10 typed as its own three decimal digits, the A typed after them claiming the field and dropping that number, and an argument-only row under a carried call through the tracker's own keyPressed, the letter and the
carried argument both drawn dim under a row that named neither, that release drawn as OFF and the
column read through the tracker's own carriedCommandNumber as still carrying the call above it and
nothing at all under it - that read written into the pattern through the model and put back
afterwards rather than typed, which is what keeps the undo line the length the sections below lean
on - the Macros tab's one length field per defined macro, its LOOPED tick, its + Add macro and - Remove selected pair with that one clearing the macro the step cursor sits on, and a number typed one character at a time at that cursor so a sign and three digits land as one value on one step and stay one point of the undo line rather than one per press while Escape drops a half-typed one — that section's typing checks reworked rather than added to, since trimUndoLine caps the line at the Settings' 50 points by erasing the oldest states and one more edit would have pushed that saturation one check earlier into the additive-paste section below — its own arpeggiator playback fixture handing its hand-seeded macro to the engine through proc.syncPlaybackModel () since the engine processes *playbackSong rather than the live song the fixture writes to, and its new scrollbar section filling the song with 25 macros through defineArp and a refresh rather than through the tab's own + Add macro button so they stay a fixture rather than 24 edits and clicking the MACROS and TRACKER juce::TextButton pair around it since a tab that is not the one showing is addChildComponent-hidden and gets no bounds at all, 0x0, so there is nothing for a grab of its strip to hit, that panel laying out at 908x688 there with 25 cards of 50px each, and covering the wheel moving the list, the drawn strip's own shading read through the panel's stepBand
as changing at six steps rather than at the sixteen cells a line wraps at, a click of the strip's
column jumping the drawn thumb to the pointer rather than rolling a fixed amount, a drag carrying it back to the head, a drag of a card rather than of the strip leaving the list where it was and the fields the fill had built for the macros now gone keeping their room but staying hidden; and EditorShot gained --arp=0,+4,-8 / --arp-loop / --macros=N / --arp-call=ROW / --arp-call-id=VAL /
--arp-note-off=ROW, a lone dot naming an empty step and --macros=N having the song define ids 1..N
all of them fresh, the id defaulting to 1 so --arp-call-id=0 makes the shot's call the release drawn
A OFF with nothing carried under it and --arp-note-off=ROW putting the column's note-off in channel
0 at that row so a shot can show a call beside a === rather than only beside a note or an empty
cell, so a shot can show a step strip of its own length, a tab whose cards do not fit and the
scrollbar and thumb it draws there, and a channel's orange A with its decimal id rather than only a
ramp the model no longer has; the bend at the layer below a host, its RawEvent growing a kind for the pitch wheel with the position read back as its pitch so a check can name the order the host is handed the pair in, and its ordering checks finding events by kind rather than assuming the first event of a block is the row's, since the host's own re-arm sends sixteen all-notes-offs ahead of them; a row that names the letter and an argument moving the wheel once to where that argument is read in steps of 128 and the note that same row strikes reaching the host after the wheel at the same block offset, a wheel moved once down a column not being straightened by anything before the note-off that also releases the note the channel was holding, argument-only rows under a carried letter moving that same wheel in turn, a note-off returning it to the centre and a further argument putting the bend back, a row that bends without striking keeping the wheel bent through the clip's own rows with only that clip's end taking it back while its instrument names the channel both the move and the return reach, and the read sending an older state's macro calls at both ends of the 128-143 range to the letter A rather than to the new bend value above it; UiTest driving the claim and the centre argument it leaves beside it, the argument typed next naming a point and playing where that point is read, an argument-only row under a carried letter staying implicit and moving the same wheel, an 11 typed as its own three decimal digits still giving CC 11, a B typed after them claiming the field and dropping the number, a typed 144 giving the highest CC rather than the letter, the claim being one point of the undo line walked back and forward through the editor's Ctrl+Z and Ctrl+Y and a further sub-block taking the claim over a row that already held an argument, its undo checks reading the cell back through the song rather than through a held reference since restoring a state replaces the pattern objects, its writes kept at rows 20..23 so they do not grow the front pattern's row capacity the shortening checks below read, and the section ending with a Home press so the fixed-y clicks of the later sections land on the rows they name rather than on rows the view had scrolled past; and EditorShot gaining --bend=ROW / --bend-arg=VAL so one tracker shot can show the lilac B and its decimal point on the wheel beside the orange A and its macro id).
This round they moved with the values' one spelling: CoreTest's state round trip writes the mid-row
switch and reads it back rather than a hex one, its older-state fixture putting that dropped hex
property back into the SETTINGS child to show it reloads and does not come back; UiTest drives the
three decimal digits a command field takes, an A typed after a run of digits claiming the field and
dropping them at one row and a B doing the same at another, the Settings tab exposing no toggle under that old
name, and the Settings tab's Snap to grid row standing in for the row that used to switch the
spelling on one undoable action and back on another - each of them still one point of the undo line,
which is what keeps the undo caps the later sections ride where they were; and EditorShot lost the
--hex flag its usage header and parse block used to carry.
/media/data2/GameDev/Plugins/ReTrektor/JUCE: vendored framework.
Important Context
Build: cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc) (build dir currently configured and passing).
Portability check: readelf -VW <Re.Trektor.so inside the bundle> | grep -oE "GLIBC_[0-9]+\.[0-9]+" | sort -uV | tail -3 must top out at GLIBC_2.38, and objdump -R must name no sqrtf/atan2f relocation; before the pin the only 2.43 pulls were exactly those two, this environment is Arch with glibc 2.43 while Bitwig's Flatpak root ships 2.42, and other plug-ins there load at 2.38/GLIBCXX_3.4.32 (NeuralNote.so fails identically over log10f@GLIBC_2.43).
Tests: cmake -S tests -B tests/build -DCMAKE_BUILD_TYPE=Release then build the prefixed targets
ReTrektorCoreTest / ReTrektorUiTest / ReTrektorEditorShot / ReTrektorSmoke (a bare CoreTest target
name fails to resolve), each binary at tests/build/<Target>_artefacts/Release/<Target>.
UiTest and Smoke need DISPLAY=:0, and Smoke takes the bundle's path as argv[1].
Two facts the UiTest has to work with rather than assume: the engine processes *playbackSong rather
than the live song, so a test writing at the model layer hands it over with proc.syncPlaybackModel (),
and trimUndoLine caps the undo line at the Settings' undoSteps by erasing the oldest states, so the
depth checks that ride that cap move with every edit made upstream of them.
Editor screenshot: DISPLAY=:0 tests/build/ReTrektorEditorShot_artefacts/Release/ReTrektorEditorShot /tmp/opencode/editor.png 1240 760.
Git (newest first): 9cda2f5 the carried argument follows the newest argument the column entered
rather than the one the letter above it called with: a row that names only an argument under a
command already in force answers with that command, so the argument its rows under it carry are the
one that row entered rather than the one the letter itself called with, which the view had kept
drawing again below every row that entered a new one. The drawing and the two reads now step the
column through one file-local stepCommandCarry beside commandLeavesCarry so they cannot disagree: a
row that named a command leaves that one and its own argument, an A 0 leaving neither, and a row
that named only an argument belongs to the command already in force above it, so it keeps that
letter carried and leaves the argument it named itself in force for the rows below rather than the
argument the letter above called with; an argument of 0 read under a carried call is the release on
its own, clearing both the letter and the argument, which is what the engine's arpCommand already
does there by clearing lastCmd and lastCmdVal. drawRows fills its carry table from what the column
carries into a row before that row steps it, so an argument-only release still draws its carried
letter on its own row and nothing on the ones under it, and the value cell answers a row that named
a command of its own with that row's own argument, so a letter that had no argument of its own keeps
drawing --- beside it rather than picking one up from above. carriedCommandNumber was rewritten to
walk down from the top of the column over the rows above rather than up from the row, since what a
row leaves in force also depends on the arguments the rows above it entered and an up-from-the-row
walk could not see them, and a new public carriedCommandValue reads the argument the same walk
leaves so a test reads the carry rather than a pixel. UiTest 619, +5, reads the carried id as 1
above the argument-only row its arp fixture writes and as the 7 that row entered under it, the
letter still the carried kArpCmd beside it, and both carriedCommandNumber and carriedCommandValue
back empty under the argument-only 0 it writes further down, the reads' fixtures written through the
model and put back rather than typed so the undo line keeps the length the sections below lean on;
the --cmd-carry shot grew the other channel's A 3 call above a row naming only its 0 there so both
halves of the rule are visible rather than only read, its usage header naming the flag. Nothing
moved in the engine, whose ChannelState::lastCmdVal was written at processRow but never read and
whose processRow already answered a row naming only an argument with that row's own value, or in the
saved form; CoreTest 265 and Smoke 13 unchanged, the serialised state still 2376 bytes; d978758 the
values' one decimal spelling and the hex display option gone
outright, the Settings tab, the drawing, the typing and the saved state agreeing that there is
nothing left to choose: SettingsPanel stops adding the "Hexadecimal values" toggle and loses its
hexToggle member with its refresh guard moving onto the mid-row toggle that is now the tab's first
toggle, which is what leaves the tab one row shorter and opening at "Ignore mid-row start" rather
than at a spelling row; Settings loses its hexDisplay member, the SETTINGS block neither writes
nor reads that property, and an older state that still spells the choice reloads with nothing to
read it into, leaves its pattern and clip counts alone and does not carry it on, the same one-way
read the dropped paste row's property already showed and the two now checked in one fixture, which
is what took Smoke's serialised state from 2396 to 2376 bytes; valueString and instString lost
their hex branch rather than gaining a flag, valueString padding a shorter decimal number with
spaces so its digits land in the same columns as a longer one's, and maxDigitsFor and fieldGlyphs
naming one spelling - the instrument's two digits with room for the column's dash beside them and
volume, pan, the command number and the command argument three glyphs each - so the release's OFF,
a macro id of 100 and a bend's point above 99 keep their last glyph in the cell rather than in one
spelling's favour, and the command cell's two halves still split by their own glyph counts since
their pads differ rather than by a wider total being halved, the number's half taking a two-pixel
inner trim and the argument's the wider kFieldPad; isHexDigitChar is gone and both numeric-key
paths take digits only, so the CC 10 and CC 11 a column asks for are typed 10 and 11 and a letter
typed after a run of digits claims the field with that number dropped rather than read, the
command letters' own claim at a value field's first character being the only route to a command
kind and clampValue still capping a typed number at kCcTop, the row gutter and every value cell
drawing decimal only and the tools panel's remap dropdowns labelling their instruments -0 .. -15
like the tracker's column does; CoreTest 265 checks, +1, reading the mid-row switch back rather
than a hex one and the older state's two dropped properties, UiTest 614, +5, driving the three
decimal digits a command field takes, an A typed last of a run of digits claiming it and dropping
them at one row and a B doing the same at another, the dropdown labels and the Settings tab
exposing no toggle under that old name - its Settings-section and undo-reverts-Settings undoable
actions spending the tab's Snap to grid row instead of the row that used to switch the spelling,
each of them still exactly one point of the undo line, which is what keeps the undo saturation the
later sections ride at its cap where it was rather than moving it by one - and Smoke unchanged at
13; EditorShot losing --hex, its usage header's mention of it and the parse block that read it,
along with the setting it wrote; the bundle refreshed by rsync --checksum to sha256
bd6de09edc6280312122738ec87630bb8cea91a64841b4a1f2c3b7078e6a7537 matching the built artefact and
still topping out at GLIBC_2.38 with GLIBCXX_3.4.32 and naming no sqrtf or atan2f relocation, and
the shots reading the Settings tab's twelve rows with the row gone above it rather than greyed, the
release drawn A OFF with the rows under it carrying nothing, a macro id of 100 keeping all three
glyphs over its dim carried pair, the Macros tab's 12-step strip still in two shaded bands of six
and the remap dropdown opening at All -> -0; 6a7c41e an arpeggiator call answered at a row holding
the column's note-off, an
arpeggiator release leaving the column calling nothing and the view agreeing with both: processRow's
note-off branch stopped walking the row away with a continue after releasing and clearing columnNote,
so a command written beside a === never ran, and now computes the row's two facts up front, noteOff
for the cell holding the note-off and noteHere for one holding a note, releases first so the run the
rows above left in force is closed by that note-off rather than by the command, and answers the
command after them against the note the column still had named, the note that row is releasing, which
the run keeps in the new arpCallNote while flushArp reads a step's base as columnNote where the
column has one and arpCallNote where it does not; the note-off's clearing of columnNote sits below
the command blocks, which is what keeps the fixture saying a call the column has no note for sounds
nothing and the one saying a call carried onto a note entering below a note-off steps from that note
green rather than a third rule, a duplicate note on a row now skipping only its own strike rather than
the whole branch so the note already sounding there still gets its command, and endArp taking arpCallNote
back beside the run's other fields while a further note-off below takes the answer away again; arpCommand's
release clears lastCmd and lastCmdVal so the column under an A 0 calls nothing rather than calling id 0,
a row there naming only an argument answers nothing and the run above is stopped rather than restarted,
and the tracker agrees through the new file-local commandLeavesCarry, applied both to the display's carry
table and to carriedCommandNumber so the letter and its argument stop being drawn over those rows and stay
carried above them; the display's Eff grew cmdVal, so the argument a column carries is drawn beside its
letter and dim like every other carried value rather than left as a placeholder under a row that named
neither, and the value cell spells an arpeggiator's 0 as what it does rather than as a bare digit, an A 0
reading A OFF, which found the command cell too narrow in a hex view and had been before this round as
well: fieldGlyphs now gives the argument three glyphs whatever the view is, a plain CC's hex argument
padding in the first two of them as the pan column already does, and the cell splits its two halves by
their own glyph counts with their own pads rather than halving the whole cell, which is what puts a CC's
two digits back inside a half the wider total's halving had cut too tight; the Macros tab shades its drawn
step strip in row spans rather than in the sixteen cells a line wraps at, a step's group being its index
over the Settings row "Subticks per row" through the new public stepBand and every second group painted at
0.58 alpha of whatever colour the step's own state asks for, which is what makes a macro reaching past its
row read as spans rather than as one long run of numbers and what makes that accessor public beside
getScrollY, so a test reads the boundary rather than a pixel; CoreTest 264 checks, +5 over a fixture naming
a call beside the row's note-off being answered there, its steps walking the note that row was releasing,
its note-ons counted and its release read through the raw event order as a note-off ordered ahead of that
run's first note-on at one sample, and one naming the release leaving the column carrying nothing so a
macro an argument below one would have switched to stays unplayed and the run above stops rather than
restarting, UiTest 609, +3 with the release drawn as its letter's OFF and the column read through
carriedCommandNumber as carrying the call above it and nothing at all under it, those two writes going
through the model and being put back rather than typed so the line stays the length the sections below
lean on, and the strip's shade read through stepBand as changing at six steps rather than at the sixteen
cells a line wraps at, and EditorShot gaining --arp-call-id=VAL and --arp-note-off=ROW so a shot can name
the release, drawn A OFF with nothing under it, and hold the column's note-off so a call sits beside a ===
rather than only beside a note or an empty cell, its usage header now spelling --tab=settings as its parser
already required, with Smoke unchanged at 13 and the serialised state still 2396 bytes against the bundle
refreshed to sha256 65a894722bc4249da197cd7672c2a619c17b69a3e14726c88572054639836ef6; 19e4278 an arpeggiator
call steps from the note its column named rather
than off whatever note the channel was left sounding, ChannelState's arpBase gone and taken by
columnNote, the note that channel's column last named, which processRow writes for a note cell,
clears at the note-off where the run in force used to lose its base instead and leaves alone for
a cell that names none so the note carries down the column like an instrument, a volume or a pan
does; flushArp reading a step's offset off it, which is what re-bases a run a note entering
further down is still going, and arpCommand having no base of its own left to work out, so a call
steps from the note its column named - the call row's own where it named one, the note its rows
above left named where it did not, and nothing at all where the column never named one, which is
what a second call used to take its octave off wherever the run above happened to be sounding;
closeArp new releasing the note the run a call replaces last entered, tracked in the new arpNote
that endArp now clears beside the run's other fields while leaving the column's own note alone,
at that row's first subtick ahead of the new run's first step, skipped on a row that struck a note
of its own since that strike had released what was held above it already and a second close would
have cut that note at its first subtick, and releasing only a note the run entered so a note the
column struck under a macro whose steps all sound nothing is left to whatever ends it below rather
than cut at the call; the one rule that did not move the sustaining one, a run that walks out its
steps still holding its last note until the clip closes it and a looped cycle still closed by the
next cycle's first step releasing the note before it at that step's own subtick, which is why an
empty step keeps sustaining the step before it rather than opening a gap at a cycle's head; CoreTest
259 checks, +3, its value-only switch fixture now naming the offsets the switched macro steps off
the column's 60 at 96 and 108 rather than the 100 and 112 only a base of 88 could reach while its
count of note-ons stays at 12 since both bases sound four steps, a new check that the 88 that run
left holding reaches the host as a note-off ordered ahead of all four of the new run's note-ons
read through the harness's raw event order the same way the re-arm checks read it, and a new
fixture for a call the column has no note for, under a note-off, sounding nothing while the same
call carried onto a note entering below it steps from that one; UiTest unchanged at 606 since a
call that names a note had already stepped from that note and nothing in that suite makes a second
call in one column; Smoke unchanged at 13 with the serialised state still 2396 bytes against the
bundle refreshed by rsync to sha256 3d20e3015718609b02ce7d6fed8d82fc0933b9dd1dc5e95a0cf80f6cf64c3503,
still topping out at GLIBC_2.38 with GLIBCXX_3.4.32 and naming no sqrtf or atan2f relocation; the
tracker read back with the call at a row whose own note cell is empty, its A and decimal macro id
bright there and the A the rows below carry from it drawn dim with no argument of their own, at
/tmp/opencode/arpbase2.png and its crop at /tmp/opencode/arpchan.png; e4671c2 the Macros tab's five asks answered in the tab itself rather than
anywhere else: its description memo gone, the tab keeping its title and the one MACRO / LENGTH /
LOOPED / STEPS label row moved up from y 76 to y 48 with kContentTop taking the cards up from 92 to
70, since what the rules are belongs to the Commands map and to this file rather than to a memo a
scrolled list has to be read under; a newly defined macro arriving holding steps that sound nothing
rather than the ramp it used to be born with, kArpDefaultJump deleted outright, ArpMacro::setDefault
writing kArpEmptyStep for every step and Song::setArpLength growing a macro on the same value so a
macro's shape is only ever where it was entered and lengthening one never quietly gains a ramp at
its tail; a step's number typed rather than committed one press at a time, the value living only in
stepText until the press that closes it arrives so a sign and up to three digits build one value
reaching the ±127 the field can hold, the cell the cursor sits on drawing that number while it lasts
rather than the step's own value, commitStepText firing only when the incoming press is neither a
further character of that number nor Escape which is what lets the arrows, a click of another card
and setStepCursor close it as well, one leading sign kept with a repeat of it changing nothing and
the other sign moving the number rather than trailing it, a number that is nothing but a sign meaning
the base note and one left half typed dropped outright by Escape rather than written half, which is
also what keeps one number at one point of the undo line rather than one per keystroke; - Remove last
becoming - Remove selected and clearing the macro the step cursor sits on, falling back to lastDefArpId
while there is no cursor yet and taking the cursor off the macro it cleared before rebuild parks it
again, its bounds widened to 132 so the longer label fits; and the drawn scrollbar grabbed as well as
turned, its geometry living in one set of helpers, scrollbarX at getWidth() - 10, thumbHeight, thumbTop
and scrollToPointer, so paint and the drag cannot drift apart and the thumb a drag centres on is the
one that is visible, with mouseDown answering a pointer in that column ahead of the card hit-test loop,
the grab kept in draggingThumb through mouseDrag while mouseUp clears it - JUCE spelling that callback
mouseDrag rather than the mouseDragged the panel first declared, an override of which does not compile -
and mouseWheelMove still turning the list beside it, plus a public getScrollY () so a test can read the
scroll back; CoreTest 256 checks, +1 for the steps growing adds sounding nothing, and its six playback
fixtures now saying for themselves which offsets they are listening for through a file-scope seedSteps
(Song&, int id, int jump) helper rather than trusting a ramp the model no longer has; UiTest 606, +18,
its typing checks reworked rather than added so the section still makes exactly one edit and the undo
line the additive-paste section below leans on stays at 49 rather than saturating at the Settings' 50
one check earlier, its Macros-tab fill going through defineArp and a refresh rather than through the
tab's own + Add macro button so 25 macros stay a fixture rather than 24 edits, its strip grabbed only
after a click of the MACROS and TRACKER tab buttons since a tab that is not the one showing is hidden
and gets no bounds at all, 0x0, and EditorShot gaining --macros=N for a list long enough that the cards
do not fit and the tab has to draw the scrollbar and thumb it offers, with the tab read back at
/tmp/opencode/macros.png, /tmp/opencode/macrosmany.png and /tmp/opencode/macrosempty.png showing the
memo gone with the label row alone above the cards, a fresh macro as eight -- and the thumb as the light
bar at the top of the track; 74cb0a1 the Command column's letter B and the pitch wheel it moves, beside
the arpeggiator's letter A rather than replacing it, kBendCmd = 144 as the command value the letter
enters and kBendCentreVal = 64 as the argument that sits at the wheel's centre, that 144 chosen to
sit above the command values 128-143 an older build wrote for its M0-M15 macro kinds so an older state's
macro calls at both ends of that range land on the letter A they now mean with the argument they had and
no older row becomes a bend, since 144 is a value that build could not have written; a row naming the
letter and an argument sending the standard pitch wheel pair to the MIDI channel its instrument names,
its argument naming a point on the 14-bit wheel in steps of kBendStep = 128 with 0 at the bottom, 64 at
the wheel's exact centre 8192 and 127 near its top at 16256 rather than the wheel's own top 16383, and a
static_assert saying the argument's centre times that step is the wheel's centre exactly rather than a
position a step off it - the two centres named kBendCentreVal in the model and kBendCentrePos in the engine
since kBendCentre was already the engine's name for the position and the two would otherwise have clashed
over it, and the engine's kBendFull going away with the position constants moved into its header; both
letters claiming the command number as its first character rather than being typed as a number, which is
what keeps the hex CC 10 and CC 11 a column asks for as their own 0A and 0B spellings, and the claim writing
the value it names straight into the cell rather than through applyValue and clampValue, because the number
field now tops at the highest CC 127 in both clampValue and the interpolation's fieldTop so a typed digit
run and a ramp can only reach a plain CC: typing 144 gives CC 127 and a ramp climbs toward the highest CC
rather than inventing either letter, the letters themselves being the only route to a command kind; each
letter leaving an argument behind, a call at the release id 0 as before and a bend at the centre 64, which
is what makes a row naming only the letter ask for nothing rather than lock the wheel to its bottom while
bendTo reads an empty argument as that centre defensively as well, and the pair drawn in the lilac theme::cmdOther
the theme had left unused so Theme.h did not change, a bend's argument keeping its decimal spelling where the
view is hex the way a macro id does and the pool's grid preview painting that command's tick in the same colour;
the wheel moving at event order 0 rather than the 3 a command message had so it is already where the row asked
before the note that same row strikes is queued, which is what bends that note from its start rather than leaving
it straight until the row below, while a note-off that had just taken the wheel back still reaches the host ahead
of it; the re-centre that used to sit in startNote gone since a bend is carried state like a volume, a pan or an
instrument so the notes below the row that asked for a point stay bent until something releases them, and the
re-centres kept where a release happens - the note-offs of allNotesOff and stopNote, a transport re-arm, the
editor's own all-notes-off, and a new take-back at the end of the clip whose rows last moved the wheel keyed on
the ChannelState's own new bendClip, which is what a row that bent without striking a note needs since no note-off
of that channel was ever going to close it; an argument-only row under a carried letter naming a new point on the
same wheel rather than inventing a controller, which is also how a further row re-asserts a bend a note-off had
just taken back; one gap left deliberately rather than fixed, a wheel left bent by a channel whose clip was deleted
outright having no note of that channel to release and no clip end to close so it is taken back only at the next
release point or re-arm rather than by the loop that closes such a clip's notes and arpeggiator runs; the harness
growing a kind 5 for the pitch wheel with the position read back as its pitch so a check can name the order a host
is handed the pair in, and its ordering checks finding events by kind rather than assuming the first event of a
block is the row's, since the host's re-arm sends sixteen all-notes-off ahead of the first block's row events, while
the existing interpolation check for the command column moved with fieldTop so its anchors 120 and 140 now clamp to
the highest CC and read 121, 124 and 126 between them with the guard naming kCcTop rather than the call; CoreTest
255 checks, +15 at the layer below a host, UiTest 588, +24, its bend writes kept at rows 20..23 of the front pattern
since a write below its visible length grows the row capacity the shortening checks further down read, and its new
section ending with a Home press so ensureCursorVisible brings the view back to the top for the fixed-y clicks of the
sections after it, and Smoke unchanged at 13 with the serialised state still 2396 bytes since a bend rides the command
fields the state already had, against the bundle refreshed by rsync to sha256 376b44a5aa292c2cf9155d4a6ce8bea9360b6669c17e7eda8d5181175af7ad81
matching the built artefact and still topping out at GLIBC_2.38 with GLIBCXX_3.4.32 and naming no sqrtf or atan2f
relocation, and EditorShot gaining --bend=ROW / --bend-arg=VAL, 96 by default, so one tracker shot can show the lilac
B and its decimal point on the wheel beside the orange A and its macro id; 04f13bb the Command column's letter A and its arpeggiator macros in place of
the M0-M15 macro kinds and their command values 128-143, an A typed as a value field's first
character claiming the command number as 128 while a number that merely has an a in it keeps its
own spelling so the hex CC 10 is still typed 0A, that pair drawn orange in decimal and the A a row
carries from above drawn dim, and a row naming only the argument under a carried A answering as a
call of that id, restarting at its first step and inventing no controller since a macro has none
to answer; a macro holding up to 64 signed semitone steps off the note the channel is holding, 8 by
default sounded one per subtick, a row over the Settings row "Subticks per row" at its default 6, so
a run reaches past the row that called it and the blocks below play the rest, an empty step sustaining
the step before it, a step landing off the keyboard snapping back by whole octaves, id 0 releasing
the note the run holds rather than starting one, an id nothing defined sounding nothing at all, a
looped macro starting over after its last step and running until the clip that owns the call ends,
a note-off takes its base away or an A 0 call ends it while an unlooped one leaves its last step's
note held until the clip closes it; the macros living in one grow-only table indexed by id - 1 like
the pattern rows and channels already do, with the ARPS section writing a macro's id, loop flag and
steps as one base64 block, an older state's MACROS section dropped outright and an older row's
command 128-143 clamped to the letter while its argument stays the argument it was, which for the
older encoding was the ignored value so the macros those calls name have to be re-entered; the
Macros tab rewritten to one card per macro that exists, a LENGTH field the plug-in's usual number
widget over 1-64 and a LOOPED tick on the card's control line with the steps drawn beneath 16 to a
line, the step cursor the arrows and a click move being what enters one at the keyboard, and + Add
macro / - Remove last naming firstUndefArpId and lastDefArpId so the ids below a cleared macro keep
naming the same macros; and two fixes found by the UiTest rather than by the feature, the length
drag and the tick click answering with their own point of the undo line rather than changing the
model behind a silent line, and the ARPS read clearing the table ahead of filling it so an undo hands
back the state's own steps, loop flag and absence instead of the state's steps added behind the ones
the macro still held, which is what had left a transpose that changes nothing adding a point because
the model no longer matched the state its own cursor sat on; CoreTest 240 checks, UiTest 564 and
Smoke 13 with the serialised state at 2396 bytes against the bundle refreshed by rsync to sha256
2a8cc889ce52a717dd4d315c4363d37b06004da36a1bc576708ccb19890c5218, matching the built artefact and
still topping out at GLIBC_2.38 with GLIBCXX_3.4.32 and naming no sqrtf or atan2f relocation, while
EditorShot gained --arp= / --arp-loop / --arp-call= so the tab's step strip and a channel's orange
letter can be shown rather than only the default ramp; 6056397 a transport that re-arms no longer cutting the note that sits at its
new start, the Bitwig-only half of the symptom the user reported, a note whose row sits exactly at
a playback start being cut shortly and the note sitting exactly at a looped region's start being
skipped outright when the region repeats while Reaper shows neither since its receivers ignore the
release: the channel-wide all-notes-off queued at a justStarted or jumped block had been given
order 1000 and 1001 while a note-on is order 1 and the pending events are sorted by (offset, order),
so it reached the host after the note-on it belonged ahead of and a host honouring it released the
note the moment it was struck, and on the other half the row sitting at the start was being dropped
by the row-boundary guard, which only let a row behind a block start through when the run was
near-continuous and a jump never is, leaving it out of reach of the re-arm branch that owns only a
row strictly behind the start; the release is now order -2 with all-sound-off at -1 and only where
panic asks for it, since CC 120 has no companion message turning a channel back on and would take
the volume and pan columns, the pitch wheel and the macros of a pass the transport is only moving
into, the guard is split by continuity so a continuous run keeps its drift dedup while a transport
that is not one snaps a row within the newly named kRowAlignEps of its start to offset 0 rather
than dropping it, and the branch that plays the row a start fell inside is gated on jumpedBack, a
jump whose start is below the position the engine last saw, rather than on any jump, so a fresh
start and a forward seek into a row still honour the Settings row about a mid-row start while a
loop wrap or a scrub plays the row its block boundary landed inside whatever that setting says,
which is what lets a loop whose boundary fell past its start play the note sitting on that start
on every repeat, the narrowing to backwards jumps not a nicety but the answer the UiTest's playRow
fixtures, which park their transport a hair before a row and so are forward jumps, forced to keep;
CoreTest 215 checks, +18 through a raw event recorder the harness now fills from the same loop that
collects its typed events so a check can name the order a host is handed them in, UiTest unchanged
at 530, which is that answer it had to keep, and Smoke unchanged at 13 with its serialised state
still 2405 bytes against the refreshed bundle, sha256 b00c12f53451a63fc3ced750720a26aab97267e56c08d7a3fec73064dcd08f45,
which still tops out at GLIBC_2.38 with GLIBCXX_3.4.32 and names no sqrtf or atan2f relocation;
ef04bf5 an older saved state that still carries the property the dropped
Settings row used to have reloads cleanly, which is the half of that drop the plug-in cannot see
from its own side, a song saved while the row still existed holding a SETTINGS property nothing
reads any more and being exactly the state a host hands back on a reload: CoreTest takes the state
the current song serialises, puts that property back into its SETTINGS child the way an older
song's block holds it, and reads the whole state back, showing the other settings reloading, the
pattern and clip counts coming back the same and re-serialising the reloaded song not carrying the
property on, which is what makes the older state a one-way read rather than something the plug-in
quietly keeps alive again, and it is the same layer the undo line restores through, since a step
back reads one of these saved strings through song.fromValueTree, so an older host block and an
older undo point behave alike; CoreTest 197 checks, +3 over the state round trip the volume/pan
CC checks already build, UiTest unchanged at 530 and Smoke unchanged at 13 against the unchanged
bundle; 66606a7 the Settings tab's "Paste as unique pattern by default" row is
dropped outright rather than left saved and shown, since the user settled what to do about the
row its own dropped Ctrl+Shift pattern paste pair had left ownerless: the Settings panel stops
adding the toggle and stops re-reading it in its refresh, the Settings struct loses the member,
and the saved settings block loses the property on both the write and the read back, which is
what makes a song saved before the drop reload with nothing to read there rather than carry a
value no one asks for, takes the row off the panel rather than leaving a grey one behind, and
leaves the panel counting one row fewer with the rows after it shifted up, since the panel counts
its rows rather than naming them by index. nothing else read the member and no behaviour moved:
the pattern clipboard's own menus are the only route to pasting a pattern and the timeline's clip
menu, its empty-area menu and the pool's item menu each already name their own instance or unique
choice. UiTest + 2 (530 total, PASS) for the panel exposing no toggle under that name and for the
saved state, the form the undo line and the host's blocks use, no longer carrying the property,
so the drop is shown on the shown side and on the saved one rather than only in the panel; CoreTest
unchanged at 194, no instance or unique paste check ever went through the setting, and Smoke
unchanged at 13 with its serialised-state check coming down from 2429 to 2405 bytes, which is the
property that left the saved form; the bundle refreshed by rsync to sha256
89808362688396eb234910f08e374e465d567bba5bf7e6aba2efdda1082ff530 matches the built artefact and
still tops out at GLIBC_2.38 with GLIBCXX_3.4.32 and names no sqrtf or atan2f relocation; 12b1b88 the cell menu's additive paste becomes its own submenu rather
than one leaf inside the plain one, since the user wanted it presented the way the plain paste
is, with the same sub-menus: the "Paste" submenu went back to just its seven leaves and their
ids, Everything / Note / Instrument / Volume / Pan / Command / Command Value at 7..13, and a
sibling "Paste (Additive)" submenu repeats exactly those seven labels under a run of ids of its
own, 17..23, greyed out by the same hasCellClipboard the first submenu is greyed by, so the
mode is now something the user picks after the field rather than one extra all-field choice
sitting beside Everything; the old id 16 leaf and its own answering branch went away, 16 stays
unused, and the undo pair keeps its 14/15, which leaves every other id of this menu where it
was. the callback's static pasteMasks moved up above the if-chain so both runs map through one
array by one subtraction each, the plain leaves by pasteMasks[result - 7] and the additive ones
by pasteMasks[result - 17] | kPasteAdditive, rather than a second copy of its seven bits or a
leaf that owns the mode. nothing else moved: kPasteAdditive was already a mode bit added to a
filter rather than a field bit, Pattern::pasteCells already cleared that bit from the working
mask and masked each copied cell's own non-empty fields under it, and pasteClipboardAtCursor
keeps its one int fieldMask argument, so a leaf and its additive twin differ by that one bit
and only the menu and its callback changed. UiTest + 3 (528 total, PASS) for a one-field
additive leaf driven through the same paste call the callback makes, kPasteVol |
kPasteAdditive, where the copied cell whose own volume is empty is skipped whole so the target
keeps its volume and gets nothing else of that cell while the copied cell that does carry one
lands at its offset below the anchor and writes neither its note nor any other field and still
records its one point of the line; that paste stays at the cursor the block above left rather
than at a cursor moved back up the pattern, since moving it makes ensureCursorVisible scroll
the view and the blocks below map their clicks through it. CoreTest unchanged at 194, whose
kPasteAll | kPasteAdditive, one-field and all-empty checks already cover the mode at the layer
below, and Smoke unchanged at 13 against the bundle refreshed by rsync to sha256
d122990de58b5164895e83caaa8c9dcd5804131c2d75ed5b7e889444c821565c, matching the built artefact
and still topping out at GLIBC_2.38 with GLIBCXX_3.4.32 and naming no sqrtf or atan2f
relocation; cf2ad7d the clip slice moves to Alt+S, the pattern clipboard's Ctrl+Shift+C / Ctrl+Shift+V keyboard pair is dropped completely and the cell menu's "Paste" submenu gains an "Additive (everything)" leaf at id 16 right after Everything, since the user's check found Ctrl+X and Alt+Shift+X changed nothing about the plug-in in either host and both Ctrl+Shift letters were swallowed the same way: the shared header gains matchSliceKey / answerSliceKey beside the clipboard trio with the s read over the key code and the printed character and the press claimed whenever a panel holds the slice, since s is the note entry's C sharp and an Alt held s that fell through would leave a note at the cursor, all six panels gain a sliceStep member beside cutStep and the editor one wireSlice lambda injecting its own doSlice into each of them so whichever panel the host routed the press to splits the same clip at the same snapped beat and records one point of the undo line, the editor answers that step through answerSliceKey next to its clipboard branch and keeps a Ctrl+C / Ctrl+V pair that refuses Shift so the hosts' Ctrl+Shift+C / Ctrl+Shift+V keep their own meaning, both doCopy() / doPaste members and the Ctrl+X branch going away with the dropped pair and leaving the Settings row "Paste as unique pattern by default" with no reader at all, and kPasteAdditive coming in as a mode bit beside the paste fields so Pattern::pasteCells keeps its four arguments, clears that bit from the working mask and masks each copied cell's own non-empty fields under it, which is what leaves the target's own value under every empty field of the buffer, skips a cell that carries nothing rather than writing it blank, and leaves the undo line alone when nothing was written, id 16 kept out of the 7..13 run the static pasteMasks map through so the leaf is answered on its own while the submenu's one-field 8..13 and the undo pair's 14/15 stay where they were; 4e67f5f Alt+X cuts the selection in every panel, taking the copy the Alt+C step makes and then emptying exactly the cells that copy took, so a cut and a following Alt+V move the cells to the cursor rather than repeating them there, a cut on its own is the menu's Clear / Selection with the data kept, and the painted selection and the cursor stay where they were because the painting is what says where the next paste anchors, the ClipboardKey gaining a cut enumerator with the x read over the key code and the printed character and answerClipboardKey gaining a third step and every panel a cutStep member the one wireClipboard lambda injects with the tracker's own cutSelectionToClipboard, so the wipe and the menu's Clear / Selection answer through one shared clearSelectedCells loop rather than two copies of it and one cut records one point of the undo line since only the wipe changes the saved song, the cell menu gaining id 6 "Cut selection" right after "Copy selection" rather than renumbering the ids its callback answers, Ctrl+X staying the clip slice at the playhead since that is not a cut of the cells and both hosts answer their own Ctrl+X before the plug-in sees it, and the clipboard's steps claiming a press even when their step has nothing to do because the three letters they name are three of the note-entry letters and an Alt held x would otherwise type a D sharp at the cursor; 63d7b46 the cell clipboard answers Alt+C and Alt+V in every panel rather than only at the editor, since Bitwig and Reaper answer Ctrl+C and Ctrl+V with their own clipboard before the plug-in is offered the event, which leaves the cells this plug-in copies and pastes unreachable there, and a press is answered by the panel a menu or a drag belongs to rather than by the editor, so Alt is the spelling they leave free: Alt+C copies what the tracker holds selected and Alt+V pastes it at the cursor with everything, the plain spelling's meaning, the Ctrl spellings stay claimed so a host that does forward them still finds the plug-in's own clipboard and the pattern clipboard's Ctrl+Shift pair stays under Shift rather than being mirrored, the shared header renamed UndoRedoKeys.h to AltShortcutKeys.h as it grew that pair next to the undo one, the six panels gaining copyStep / pasteStep members chained after their undo answer with one wireClipboard lambda injecting the tracker's own copySelectionToClipboard and pasteClipboardAtCursor (kPasteAll) into all of them so one press walks one effect whichever panel answered, and the clipboard pair claiming a press whose step does nothing rather than only one whose step did, since the two letters it names are two of the note-entry letters and an Alt held c that fell through would type a C at the cursor beside copying; 1c2e544 the tracker, timeline and pool hand keyboard focus back once a menu, an alert or a drag has closed, since a parentless popup window or dragged item takes the focus with it and afterwards nothing is focused at all so the walk from a key press starts at the plug-in's top component and only the editor's own shortcuts are answered, which is why the whole keyboard went dead after a right-click menu or a pool drop and why only a click woke it, the injected focusRescue handing focus to the tracker rather than to the panel that asked so the note entry is heard again while the popups and the floating drag preview stay where they were; 26ab9f6 answer the Alt undo pair in every panel rather than only in the editor, since JUCE sends a key press to the modal component and walks up only that chain so the editor is never consulted while a right-click menu is open or while the pool's pattern drag is held by the DragAndDropContainer, one shared matcher in Source/ui/UndoRedoKeys.h giving all six panels the same body and claiming a press only where that panel holds the step, and the editor injecting its own performUndo / performRedo into all of them so one press walks one point of one line; 7f6ea6d the Alt redo alias Alt+Shift+Z is dropped since both Bitwig and Reaper swallow that combo before the plug-in sees it, the Alt block now refusing every Alt+Shift press so the Alt pair keeps one spelling each while the Ctrl spellings stay claimed; 30708bb undo and redo also answer Alt, which is the pair Bitwig and Reaper leave free since their own undo swallows the Ctrl trio before the plug-in sees it, the letter matched over the key code and the printed character because X11 hands an Alt-held key as the character it printed; 6e51af9 undo and redo walk the states the model changes made, the depth capped by the Settings tab's Undo steps row plus a 32 MB budget, the pair also in the tracker menu and the undo line living on the plug-in so a reopened editor finds the same points; 15e2513 group the tracker's four clears into one Clear submenu and let a hidden chip invert its plate instead of dimming; d103ccf copy the selected cells and paste them at the cursor with a field filter, the paste filter's submenu living in the cell menu and the pattern clipboard's pair moving under Shift; 292ff37 selection holds cells rather than rows so the tools stop hitting every channel; e8f3049 document that transpose reads a semitone amount and a direction; 410b1fe transpose is an editable semitone amount plus a direction button instead of four fixed steps; ef3417f document that the plug-in binary is pinned so an older libm can load it; c3e2f36 document the interpolation tool, its dropdowns and its default scope; ee29e69 pin the two float routines the plug-in takes through libm's newest symbol versions so a host with an older libm loads the binary; 2e67b16 interpolation fills the blanks between the values that already exist, with field and channel dropdowns and the panel opening on the selection-only scope; 40345cc document that a command argument uses the carried command number; 76115ec a command argument without a command number uses the command carried in the column; 205cfe1 document that the timeline ruler keeps only bar numbers; d2f672e the timeline ruler drops the transport readout; 8d077f7 document that a reopened editor keeps the window size; c12431b a reopened editor comes back at the size the closed one had; ece35e2 document that a pool drag floats one item, not the pool panel; df03559 a pool drag floats only the item's cell at the cursor; 3f14d6f volume/pan controllers precede the note-off branch; cd3fc2c volume column drives CC7, Settings name the volume/pan CC numbers, chips hide one field of one channel; f9acf12 instrument remap reads a source and a target dropdown; 08af633 anchor right-click menus at the cursor; 32499ef document that a timeline click parks the playhead; 02c0ca3 jump the playhead to a timeline click; 117744f document pool copy actions and the removed redundant clip menu item; ce6422d drop the redundant "Copy pattern data (unique)" clip menu action; ae95cac scrollbar + --scroll docs; 97f50ea tracker horizontal scrollbar; 93dd668 channel/limit docs; 0716702 hidden channel columns + constant 127 channel cap; 121ecd0 pattern resize preserves hidden rows; 9ffaaa4 EditorShot
--channels=N flag; a5e6e2d bottom-bar/settings channel docs; b02e52a CHANNELS slider direct edit; 1173682 stuck-note docs; b1725b1 stuck-note fix for hosts that skip MIDI-effect calls while stopped; de11909 pool grid preview; c4a8422 README macro-playback update; 49ec706 macro playback wiring; c36c6c7 ReTtrektor typo fix; 4263853 instrument fallback option; f89454e headless UI interaction test; c168e8b panels fit minimum window; a2ec7c9 Re.Trektor sources; ccc50f2 vendored JUCE 8.0.9.
DISPLAY=:0 / wayland-0 available; no Reaper; /usr/bin/carla exists.
Missing xcb-util-devel/xcb-util-wm-devel were installed via sudo dnf earlier (leftover from iPlug2 attempt, harmless).
iPlug2 and its VST3 SDK clone deleted from workspace; .gitmodules removed.
