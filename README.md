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
  note is its first step — sounded at the pitch the first step moved it to at that
  row itself, the run's first note-on being at the call's own subtick rather than at
  the subtick below it and the note as it was typed never reaching the host when that
  step names a jump, and as it was typed when the first step holds nothing — and
  whose eight seeded steps land a sixth of a row apart from that first one, spilling
  past the row that called them and
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
  All Sound Off and striking the note that run measured from again at its own row,
  that base being the last note the fixture hears rather than the run's last step,
  the release leaving the column calling nothing rather than
  calling its id so a further row there that names only an argument answers
  nothing and the run above is stopped rather than restarted, and a call whose
  row also holds the column's note-off being answered rather than ignored, its
  steps measured against the note that row is releasing while that note-off
  still takes the note away for the rows below it and reaches the host as a
  note-off ordered ahead of the run's first note-on at one sample, an empty step
  sustaining the step before it, a step entered as the note column's own note-off
  opening whatever the run was holding there rather than sounding one of its own -
  the run's first step being the note the call row entered, so a step mid-run orders
  its release between the note-ons either side of it, one at a run's head opening the
  note the column had entered above a call whose own row holds no note, that opening
  landing at the step's own subtick rather than being the re-articulation of the step
  under it and being the run's own release rather than one the clip's end takes later,
  one entered with nothing before it but an empty step opening that first note
  whether the call's own row entered it or the column's rows above did, that empty
  step having already sounded it as it was typed, and a looped cycle stepping back
  onto that first step and through its note-off steps, the note the cycle was
  holding opened once at each of them and once more where a stepped step leaves
  it, the cycle then sounding its next note past that rest - and
  that the release such a step opens is only ever a note that run entered, its first
  step being the note the call row or the column's rows above named, so a step never
  cuts a note another column or another run was holding, a step landing off
  the keyboard snapping back by whole octaves to either end, the clip end closing
  a run that was still looping rather than the row that called it, the steps
  growing adds sounding nothing rather than a ramp the model no longer has, and the
  macro state round-tripping its id, its loop flag and all 64 signed steps — now saved
  as two bytes a step so a step beyond a one-byte jump survives, with the reader telling
  the two spellings apart by the length the block declares and so an older state's one-byte
  steps widening unchanged rather than losing their jumps — and that a note-off step
  round-trips through the wider block as the same opening step, with an older state's
  steps keeping their jumps, their steps that sound nothing and their absence intact —
  with an
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
  reach, and the two slide letters with them: their command values sitting above the
  highest one an older build could write so no older row can already be a slide, a
  slide's argument naming how many rows its extreme sits below the row that named the
  letter rather than where on the wheel that extreme is, both letters and their row
  counts surviving the saved form unchanged while an older state's calls at either end
  of its range still land on the call letter, the row that named a slide leaving the
  wheel where it already was and sending nothing of its own so that row's own note
  plays straight, the sub-rows under it moving the wheel along the grid the Settings row
  names rather than one whole position per row, landing on the extreme exactly over the
  span's own last row, the `0` the entry leaves beside a slide letter spelling that
  letter's release rather than a slide over no rows at all and the narrowest span
  taking one row, an
  argument-only row under a carried letter re-arming a ramp of its own from where the
  wheel had got to on those sub-rows while a further one over a wheel already at the
  extreme moves nothing, a row naming a command that is not a slide ending the ramp
  that was still climbing, a note-off ending a ramp partway down its steps with the
  wheel having already moved on the sub-rows of the row above it, and a slide wider
  than the clip that named it having its ramp put away at that clip's end even though
  its own wheel never left the centre, and the argument field's two letter spellings
  with them: the `STP` and `OFF` sitting above the highest number that field can spell
  so a run of typed digits cannot invent either of them and both survive the saved form
  unchanged as the argument's own numbers, an `STP` read beside a carried or named wheel
  letter stopping a ramp partway up its steps and leaving the wheel holding the point
  those sub-rows had reached until a note-off below takes it back, that letter still
  being carried on below the row that asked to hold, an `OFF` spelled by a bare `0` read
  under a carried slide letter putting that wheel back at its centre there and leaving
  the column carrying nothing under it so a further argument-only row answers nothing,
  the `0` beside the bend letter's own row still naming the wheel's bottom while an `A`
  call on a column whose wheel a bend row had left bent puts that wheel back at its centre
  at its own row rather than leaving it bent under the run, the run's own steps below that
  row saying nothing about the wheel and being measured from the note the column named, a
  bare slide row closing the arpeggiator run that column was in by releasing the note its
  last step entered there - that note being released three times over, once by the step
  above and once by the stuck-key guard, and the release ahead of the note the column
  strikes below the ramp being what leaves nothing of that run sounding under it - and a
  slide in another tracker channel of the same MIDI channel leaving that run stepping on
  below it, the two commands cooperating rather than one switching the other off with
  every note of both of them reaching the one MIDI channel, and the arpeggiator's release
  re-triggering the note it measured from: an `A 0` kill sounding the base the run stepped
  from again at its own row, that note being the last one the column sounds, and a release
  spelled by an `o` under a carried call leaving that column sounding its own base note
  rather than silent below it, and one pattern placed twice back to back having
  one release of that pitch ahead of the note the lower clip enters at its row 0
  rather than two — that clip's own end being the only thing queued there ahead of
  that note-on, the stuck-key guard adding no second note-off of a pitch that
  release had already opened at that very sample of that block, which is what a
  host answering the sample rather than the order inside one needs so it does not
  hear that note as opened at once, and nothing of that pitch closing it again over
  the rows below, and the vibrato macros and the wobbles they run at that same layer:
  the grow-only table's one default macro at the plain sinusoidal shape at half full
  pitchbend over a cycle of eight subticks - so the VIB MACRO tab has a card the way
  the ARP MACRO tab has steps - its `firstUndefVibId` / `lastDefVibId` pair naming the
  same gap, naming the top id growing the grow-only table to the id limit while id
  `0` stays the release call and there is no entry above the limit, naming an id
  extending it with absent entries rather than moving the ones already in it so a
  fresh macro still takes the lowest gap, clearing one leaving its room and the shape,
  amplitude and cycle its knobs had while the entry answers for nothing of its own -
  which is what makes the engine stop a wobble whose macro was taken away rather than
  swing it at nothing - and defining it again starting over from the defaults rather
  than from what the cleared one had still held; the setters refusing an absent macro,
  so an id nothing entered never answers with a shape, an amplitude or a cycle of its
  own, and the three settings clamping at their own tops rather than wrapping, which is
  what leaves the cycle never at zero subticks - it clamps up to one - and the amplitude
  never past full pitchbend; the vibrato letter's command value sitting above both the
  highest value an older build could write and the highest plain CC the number field can
  spell, so no older row and no typed digit run can already be a vibrato, the letter and
  its `OFF` and `STP` spellings surviving the saved form as the argument's own numbers
  above the field's top and a macro's three settings arriving as their own properties,
  which is what a saved state brings the knob positions back at, the saved form writing
  one entry per macro that is present rather than one per id the table has room for so a
  round trip with a gap between two defined macros brings the two back at the ids they
  were at and leaves the gap a gap, a state that had the default macro taken away reads
  back without it - which is what an undo that removed a vibrato leaves it removed rather
  than the reset putting one back - and an older state's arriving as no section at all
  leaves the read with the one default macro a fresh song defines rather than an empty tab;
  a wobble's grid - a cycle 6 subticks wide being exactly one row wide at the default
  Settings, so each of the three rows a note is held over draws one cycle of eight points
  and the note-off below the last of them takes the wheel back to its centre, and the full
  amplitude reaching the wheel's own extremes rather than one of them leaned against further
  than it is: the height from the centre is the centre's own value, so the peak above would
  land one position past the top and is clamped to it while the trough below lands exactly
  on the bottom - the amplitude being a share of the wheel's own half height rather than a
  number of its own, so a knob at half swings half as far each way and keeps the wheel's own
  centre as the point its cycle passes through, and a knob turned down to off moves the wheel
  nowhere at all, the arming row's first point already sitting on the base and every point of
  the cycle sitting on it too, which is what makes such a macro still a macro rather than no
  macro at all; the default macro's cycle of eight subticks being one point per sub-row step
  and four thirds of a row wide so its cycle crosses the rows it sits in rather than starting
  on each of them, a cycle 12 subticks wide being two rows wide and drawn with 12 points of
  it rather than the 8 it is asked for at least, so its curve is cut at the twelfths of a
  cycle, and a cycle one subtick wide still drawn with the 8 a cycle is never drawn with fewer
  of, so the wheel moves 8 times inside the sub-row grid rather than once over it, 6 cycles of
  it land inside one row, each of them repeating the same 8 points, and its last point leaves
  the wheel at 6744 rather than back at the base, which is what makes the 48th message the
  note-off row below taking that point back to the centre rather than one more point of the
  cycle; the six shapes and their Up and Down spellings drawing their cycle as the points of
  it the amplitude names, the sinusoidal one curving and the triangle one ramping, the Up
  spellings having the cycle's second half inverted so the wobble never reaches below its base
  and stays on one side of the pitch it swings about and the Down spellings having its first
  half inverted so it never rises above it, the other half being the one the plain shape swung
  to the far side in - at full amplitude those lean on the wheel's ends, the sinusoidal Up
  climbing to the wheel's top twice a cycle with nothing below the base to go to, the fold
  landing on the peak the plain shape's second half would have reached below, and the sinusoidal
  Down landing on the wheel's bottom twice a cycle and never rising above the base, that spelling
  being the plain shape's cycle read through the base instead; the point a wobble swings about
  being wherever the wheel already is rather than the centre, so one armed a row under a bend
  row swings about the point that row left the wheel at, its peak clamped at the wheel's own top
  where the base has no room above it for the full swing, that row above sending the point it
  named ahead of the swing's own first point and the two being one pitch rather than a jump since
  the row bent the wheel at the same sample as the note it struck there, so the pitch the wobble
  later swings about is already in force at that note rather than one the row below it asks for,
  and a bend row under a running wobble moving the point the swing measures about to the pitch it
  asks for rather than ending the swing or starting a new one, the row's own point and the
  wobble's being one pitch and the points below reaching over the new base, that bend row
  neither striking a note nor releasing the one the row above it held so the swing carries on
  across it; a release row taking the wobble and the wheel it had left swinging away on its own
  row - the last point of the cycle having moved the wheel to 2399 just above the bottom and the
  row's own message naming the centre rather than a point of the cycle, so no pitchbent wheel is
  left over the rows below the release - while the note the row above struck is still held to the
  note-off below, the release being about the wheel rather than about the note, that centre
  arriving at the row that released rather than being saved for the note-off below; the `OFF`
  the entry writes for an `o` doing what a `0` beside the letter does, the argument field's own
  top being one below what the spelling needs, and a row naming only an argument below that
  release arming nothing, the release having left the column calling nothing rather than calling
  id `0` the way a run's and a slide's release leave nothing below them; an `STP` argument row
  asking for nothing and leaving the swing swinging over the rows below it, the row under that
  naming another macro id re-arming the swing over that macro, its cycle counting again from that
  row rather than from where the first stretch had got in its cycle, and that re-armed swing
  measuring its quieter cycle from the point the first stretch's last point had got the wheel to
  rather than from where that stretch was armed, which is what keeps the retarget from jumping to
  the quieter macro's own base; a row that names the letter itself and an `STP` argument ending
  that row's own swing rather than re-arming one, a wobble having no move of the wheel to hold,
  and leaving the wheel sitting at the point its last point had got to until the note-off below
  takes it back, that letter's own row leaving the column calling it so a bare argument one row
  below re-arms the swing from the point the ended one had left the wheel at - its fuller
  amplitude reaching over the base by more than the base has room for, the points of it that
  would land below the wheel's own bottom all sitting on it rather than going past it - and that
  re-armed swing saying nothing at its own arming row, whose first point sits on that base, which
  is what makes the first message it sends one part way up its cycle rather than a jump to one of
  its extremes; a macro taken away while its wobble swings ending the swing at its next point
  rather than at the row that took it and saying nothing of its own, the wheel keeping whatever
  point of its own the swing had got to, whose last point sits above the base so the wheel is
  left bent rather than one already at its centre, and a release row below is what puts that
  wheel away, the note-off taking the point the swing had left back to its centre; and the clip
  those rows live in running out under a wobble that armed no note of its own, a swing that clip's
  single cell armed having no rows below the clip left to keep swinging over so its cycle stops at
  the clip's end and the wheel it had left swinging comes back to its centre there - 16 bends of
  the two cycles of eight points it holds over the clip's two rows, the wobble armed with no note
  of its own having nothing to release so the wheel being all that is left to put away, and the
  only all-notes-offs there the ones the transport's own start asks for, one per channel, which is
  what leaves the clip's end sending only the wheel's take-back rather than releasing anything of
  its own.
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
  timeline, pool, settings, tools, ARP MACRO and VIB MACRO panels each walk one point per
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
  back empty under one, and the ARP MACRO tab's one length field and
  looped tick per macro that exists, its `+ Add macro` and `- Remove selected` pair,
  the `- Remove selected` one clearing the macro the step cursor sits on rather than
  the last one a row can name, a number typed one character at a time at the drawn
  cursor so a sign and three digits land as one value on one step and stay one point
  of the undo line rather than one per press, `Escape` dropping a number half typed
  rather than writing half of it, the tab filled with 25 macros through the model and
  a refresh rather than through its own buttons so the undo line keeps the length the
  sections below lean on, clicked through the `ARP MACRO` and `TRACKER` tab buttons — the
  tab buttons now spelling `ARP MACRO` and `VIB MACRO` rather than the one `MACROS` the
  vibrato tab's card was reached through before it had a tab of its own, and the two macro
  tabs' buttons being 10 px wider than the plain ones so their two-word names are spelled
  rather than shrunk — since
  a tab that is not the one showing has no bounds to grab, the wheel moving its
  scrollbar and a click of the strip's column jumping the drawn thumb to the pointer
  while a drag carries it back to the head, a drag of a card rather than of the strip
  picking a step cursor up and leaving the list where it was, the strip's own
  shading changing every group of "Subticks per row" steps rather than every
  line of sixteen cells it wraps at, read through the panel's `stepBand`, the note-off
  step its own copy of the note column's note-off key writes — a press of the backtick
  at the cursor the `+ Add macro` button left parked there spelling that step rather than
  typing a number into it, claiming the press, spending exactly one point of the undo
  line for that one press and leaving the cursor on the step it opened the way `Delete`
  and `Backspace` do, that step then read through the panel's own `stepLabel` as the word
  the shot draws beside a jump's sign, `Ctrl+Z` taking the step back to the one that
  sounds nothing and `Ctrl+Y` putting the opening back, and `Backspace` clearing an
  opened step to the one that sounds nothing at a point of its own — and a
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
  again, and the two slide letters entered the same way but past every undo-depth check,
  since a claim there also spends a point of a line the checks above had already counted
  to its cap and would otherwise be read as one of the line's last points: the letter `U`
  claiming the command number and leaving the default span of 8 rows beside it, 2 typed
  over that argument naming the two rows over whose sub-rows the wheel climbs, a row under
  the carried letter naming only an argument staying implicit and giving a span of its own,
  a `D` typed after an `11` claiming the field and dropping that number while a typed `146`
  gives the highest CC rather than inventing the slide letter, the same letter still
  entering a D natural where the note column is asked for, and the whole climb read back
  as one wheel move on every sub-row of the two rows it takes, twelve of them under a clip
  and a pattern of the section's own rather than as one jump per row or as a jump the ramp
  could not be played across, since a transport jump releases
  whatever the synth was holding and that release is what ends a slide the host jumped out
  of, and the argument field's two letter spellings typed there beside them: a `0` typed
  beside a slide letter landing the `OFF` argument the entry has to leave rather than a
  slide over no rows, the read under that row coming back as no carried slide at all, the
  `o` the entry answers writing that same spelling under a carried letter with that row
  staying implicit and the read under it still coming back empty, an `s` landing the `STP`
  beside a wheel letter with that letter and that argument still carried on over the rows
  below it, a letter neither claim owns left alone so the cell keeps the value it had and
  no note is typed in the note column beside it, the `s` beside an arpeggiator call asked
  for by nothing and refused rather than written while the argument the entry left stays
  in the cell, and the `o` beside a call landing the release's `OFF` spelling with the
  column under it carrying no call, these reads going through the tracker's own new
  `commandKindAt` beside its `carriedCommandNumber` / `carriedCommandValue` pair, with the
  undo checks reading the cell back through the song rather than through
  a held reference since restoring a state replaces the pattern objects, and the VIB MACRO
  tab's own card, its tab button clicked and the panel refreshed: the default song having the
  one macro a row can name and the tab one card for it, that macro getting its own dropdown and
  its two knobs, the two knobs told apart by the span they walk rather than by the order they
  were added in so a check about one of them is about that one however the card was built, the
  amplitude turning off to full pitchbend in hundredths - the same hundredths the model keeps -
  and the cycle counting whole subticks one cycle takes from one to the limit a row's argument
  can name, the card opening at the amplitude and the cycle a newly defined macro starts at, the
  dropdown offering the six shapes a wobble runs, opened at the one the macro holds, the dropdown's
  own ids being one above the model's own and its items read back as spelled by the panel's own
  `vibShapeLabel` rather than as a second copy of what the tab draws, the dropdown's own change
  writing the shape to the macro - which is what a row already calling that id swings by from
  there - and spending one point of the undo line of its own, a knob turned writing its own
  setting: the share of full pitchbend and the subticks the cycle takes, six here, and the
  amplitude spelled as that share rather than as the hundredth the model keeps, read back through
  the panel's own `vibAmplitudeLabel`, each knob turned leaving a point of its own so the card's
  three settings are three separate actions and once the line is full one of them drops out of it
  at the far end, which is why those depth checks read the line back as the depth the Settings
  tab's Undo steps row asks for rather than as one more than it can hold, `+ Add macro` taking the
  lowest id that has none yet and moving the gap below it down the way the ARP MACRO tab adds one,
  the fresh macro arriving at the shape, amplitude and cycle the model gives a newly defined one
  rather than at what the card below it had, and `- Remove selected` putting away the macro the
  last gesture picked out and leaving the ids naming the same macros below it, the same pairing the
  ARP MACRO tab uses, the macro that was never picked out keeping the settings the knobs had left
  it at - the card being about which macro there is rather than about what it holds - and a macro
  defined and a macro taken away being one action each, the card's three settings being three more
  above them, the keyboard focus - no Button anywhere in the editor's tree takes it, so an
  Enter pressed only to confirm a value that was typed into a field is never spent on
  whatever the pointer touched last and kept doing so for every further press: not one of
  the four tab names, not a Settings row's checkbox, not a card's `+ Add macro` / `-
  Remove selected` pair, and not the two `+`/`-` buttons a knob builds inside itself, the
  theme having taken it away from the pair while the look was in force and the editor's own
  sweep taking it away from the ones its panels made before that was - while the knobs and
  the ARP MACRO, VIB MACRO and Settings tabs want it instead, being the controls whose own
  text box that value is typed into, and each of those three panels spending an Enter
  rather than handing it up so repeating the press changes no checkbox, adds or clears no
  macro and leaves the undo line alone, and a knob dragged along its own row moving the
  number that row shows and feeding the model behind it, so the Settings tab's Undo steps
  row can be asked for by dragging it rather than only by typing into it - the drag being
  heard over the `+`/`-` pair a Slider builds beside its text box, since that style draws
  no thumb to grab and that pair answered a click alone until the drag was asked for, which
  is what had left a sideways drag over a row doing nothing at all - and that drag being
  one action of the undo line rather than one point per mouse event along the row, so the
  checks grab a drag as several mouse events and ask that one Ctrl+Z takes the whole drag
  back to the number the row had before it, that a second drag back along the row is a
  second action of its own rather than the events between the two numbers, that the row's
  own `+` button still costs one step of its interval and one undo point, and that the row
  leaves the depth where the round's other checks read it, the tab being put back on the
  tracker for the sections below) and confirms which MIDI channel and which
  controller value the notes and columns end up on
* `ReTrektorEditorShot` – renders the editor headlessly and can dump the
  component layout (`--dump`, optionally `--tab=settings|macros|vibs|tools` to show a
  whole tab at once - the shot keeps its short aliases, `--tab=macros` naming the
  ARP MACRO tab the older shots asked for by name and `--tab=vibs` naming the VIB
  MACRO one, since the tab buttons now spell those two-word names out - the Settings tab's rows all being the decimal ones now its
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
  the word `off` or the note column's own backtick naming a step that opens whatever the
  run was holding there, so a Macros-tab shot can show the `OFF` cell among the jumps,
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
  to in decimal beside it, and `--slide=ROW` to name a wheel slide up in tracker channel
  0 at that row, which paints the lilac `U`, `--slide-down=ROW` naming a slide down there
  instead, the letter `D`, whose command value is one the number field cannot be typed as
  and only the letter itself can name, and `--slide-arg=VAL` saying how many rows that
  slide takes to reach its extreme so the argument is drawn in decimal beside the letter,
  12 by default rather than the 8 the letter's own entry leaves, the two arguments the
  entry writes as letters rather than as numbers being nameable here by their own values
  since the field cannot spell either: `--slide-arg=128` is the `OFF` an `o` writes, so
  a shot shows the lilac `U OFF` with the rows under that row carrying nothing, and
  `--slide-arg=129` is the `STP` its `s` writes, so a shot shows the lilac `D STP` with
  that letter and that spelling carried dim over the rows under it, and the narrowest
  slide a row can ask for taking one row rather than the `0` spelling a slide over none),
  `--vib-shape=`, `--vib-amp=` and `--vib-cycle=` to set the three settings a wobble runs on
  vibrato macro 1 - the shape one of the VIB MACRO tab's own six spellings (Sin, Sin Up, Sin
  Down, Triangle, Triangle Up, Triangle Down, with their spaces and dashes optional) or the
  number the model holds for it, the amplitude the hundredths of full pitchbend the model keeps
  so 25 is the knob spelled 0.25, and the cycle counted in subticks one sin or triangle cycle
  takes on the same grid an arpeggiator's steps and a slide's ramp are measured over - which is
  what makes a `--tab=vibs` shot show a card whose shape is picked out and whose two knobs are
  turned rather than one left at what a fresh macro starts at; `--vib-macros=N` to have the song
  define N of them, ids 1..N all of them fresh, which is what makes such a shot long enough that
  the cards do not all fit and the tab has to draw its scrollbar and thumb - the dump reading the
  panel back as `VibMacrosPanel` at 176,38 908x688 holding 25 dropdowns and 50 knobs for a 25-macro
  song, 22 of the cards laid out and visible while the three that do not fit are hidden rather
  than drawn past the window, and the shot showing the strip's thumb at the far end of its track
  with `V 25`'s own card clipped at the bottom edge; `--vib-call=ROW` to name a vibrato call in
  tracker channel 0 at that row, which paints the command column's lilac `V` and its decimal
  macro id and leaves that letter and that argument carried dim over the rows under it in that
  channel only, and `--vib-call-id=VAL` naming the id that call writes, so `0` makes it the
  release, whose argument is drawn `OFF` rather than as a `0` and which leaves the rows below it
  carrying no letter and no argument at all)

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

* **Top bar** – `TRACKER` / `ARP MACRO` / `VIB MACRO` / `SETTINGS` tabs, snap
  toggle, grid selector, zoom out/in, host position. The two macro tabs are 10 px
  wider than the plain ones, which is what their two-word names are spelled in
  rather than shrunk below the other tabs' letter.
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
  lines, and a teal/orange/lilac tick where a plain CC, an arpeggiator call or one of
  the four wheel letters is present). "Copy pattern"
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
  half its three glyphs with a wider pad, so a macro id, a point on the wheel, a slide's
  span, a release's `OFF` and a hold's `STP` all keep their last glyph rather than one
  half being cut
  tight
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
  states at all. A knob dragged along its own row is one action of this line
  rather than one point per movement the host hears along it, so one press of
  the undo keys takes a whole drag back to the number the row had before it
  rather than leaving the number part-way along the drag, and a drag that
  ended on the number the row began at leaves no point of the line at all —
  and that row, being the depth itself, can be asked for by dragging it, which
  is what then caps how far a following Ctrl+Z can reach. An undo reaches the
  playback snapshot the way a cell edit does, so while the transport runs the
  rows ahead of the playhead play what the restored song holds. `Ctrl+Z` or
  `Alt+Z` while a number is still half typed closes that buffer instead of
  stepping, which is what makes one press one effect.
* **ARP MACRO tab** – the arpeggiator macros the Command column's letter `A` calls.
  Nothing is painted above the cards but the tab's title and the one label row
  naming what its own rows hold (`MACRO`, `LENGTH`, `LOOPED`, `STEPS`), since what
  the rules are belongs to the Commands map and this file rather than to a memo the
  cards would have to be read under. One card per defined macro, named by the call a
  row writes (`A 1`) in orange: a `LENGTH` field (1–64 steps, the plug-in's usual
  number widget) and a `LOOPED` tick sit on the card's control line, and the macro's
  steps are drawn beneath it, 16 to a line, each as its signed semitones away from the
  base note (`--` being a step that sounds nothing and `OFF` being a step that opens
  the note the run was holding, spelled the way the note column spells its own note-off
  so the cell reads as the release rather than as a jump of a number the table has no
  row for), the strip shading every second
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
* **VIB MACRO tab** – the vibrato macros the Command column's letter `V` calls, added
  to and removed from exactly as the arpeggiator macros are: `+ Add macro` defines the
  lowest id that has none yet at the shape, amplitude and cycle the model gives a fresh
  one — the plain sinusoidal wobble at half full pitchbend over a cycle of eight subticks,
  so the card shows a wobble the way the ARP MACRO tab's card has steps to show — and
  `- Remove selected` clears the macro the last click of a card or the arrows picked out,
  falling back to the highest id a row can name while nothing is picked out and leaving the
  ids below the removed one naming the same macros. Clearing says a macro has none rather
  than erasing the shape, the amplitude and the cycle its knobs had, which is what re-defining
  it then starts over from. Nothing is painted above the cards but the tab's title and the one
  label row naming what its own rows hold (`MACRO`, `SHAPE`, `AMPLITUDE (of full bend)` and
  `CYCLE (subticks per swing)`), since the rules belong to the Commands map and this file
  rather than to a memo the cards would have to be read under. One card per defined macro,
  named by the call a row writes (`V 1`) in the lilac the wheel's own commands are read in
  rather than in the orange of an arpeggiator call, which is what makes the two macro tabs
  tell the two letters apart at a glance, and marked in its own line by a cursor rule drawn
  left of that name rather than over one of its settings. A card holds the three settings a
  wobble runs on one line: the shape as a dropdown over the six spellings `Sin`, `Sin Up`,
  `Sin Down`, `Triangle`, `Triangle Up` and `Triangle Down` — the sinusoidal one curving and
  the triangle one ramping in four straight sides whose corners sit on the cycle's quarters
  rather than bunched at its ends, the `Up` spellings having the cycle's second half folded
  back over the base so the pitch only ever goes above it and the `Down` spellings having its
  first half folded so it only ever goes below — then the plug-in's usual number widgets for
  the amplitude, spelled as the share of full pitchbend rather than as the hundredth the model
  keeps and turning from off to full in hundredths, and for the cycle, counted in whole subticks
  one sin or triangle cycle takes, from one to the 127 a row's argument can name, the same
  grid an arpeggiator's steps and a slide's ramp are measured over. A card exists whether or
  not a knob is turned, so a macro whose amplitude is at off still has a card and still
  swings — at nowhere of its own — rather than being no macro at all, and each of the three
  settings is an action of its own, so a card whose shape was picked and whose two knobs were
  turned leaves three points of the undo line rather than one. A card is 22 px tall plus a 4
  px gap, so a list of them longer than the panel's height is what the tab scrolls rather than
  grows without bound: the same vertical strip on its right edge, 10 px in from that edge,
  whose thumb is as tall as the share of the track the cards that fit take so the thumb reads
  as the window that is showing, is turned by the mouse wheel as well as grabbed by the mouse,
  which is what the mouse alone does when a host gives it no wheel, and a click anywhere in
  that column puts the thumb's middle under the pointer and holds it there while the button is
  held so the list can be dragged to a chosen place, the tab answering a pointer in that
  strip's column ahead of its card hit-tests, which is why a grab of the strip never reads as
  a click on the card behind it. A click on the card's own name — the part left of the
  dropdown — is what picks that macro out for the remove button and the arrows to answer
  against, the settings themselves being turned by the controls sitting over the card rather
  than by a click of the card.
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

The ARP MACRO tab's own strip is grabbed the same way its horizontal twin is: a click
anywhere in that strip's column, 10 px in from the tab's right edge and level with the
cards rather than with the foot holding its two buttons, puts the thumb's middle under
the pointer and carries it along while the button is held, so a list of macros longer
than the tab can be dragged to a chosen place by mouse alone, which is what a user has
to work with where a host gives the plug-in no wheel at all; the wheel turns that same
list as well. A click in a card is what picks a step cursor up there, so a drag started
away from the strip moves the cursor rather than the list, and the tab answers a pointer
in the strip's column ahead of its card hit-tests, which is why a grab of the strip never
reads as a click on the card behind it.

Every number widget the plug-in carries - the Settings rows' sliders, the bottom bar's
OCT / STEP / CHANNELS, the transpose amount and the two card knobs - answers a drag
along its own row now as well: that style draws no thumb to grab, so the whole area beside
the row's text box is the pair of `+`/`-` buttons the look and feel builds for it, and
those buttons answered a click alone until the drag was asked for, which is what had left
a sideways drag over a row doing nothing at all and left the number on whatever the last
click had stepped it to. Asking for the horizontal drag puts the pointer's path along the
row onto that pair, and on the row's own width, onto the value: the number moves with the
pointer, the button the drag runs towards lights up, and the drag reaches the model behind
the row, so the Settings tab's Undo steps row can be asked for by dragging it rather than
only by typing into it. A click of either button still costs one step of the row's interval
and one point of the undo line of its own, and typing the box still sets the value outright,
so the row keeps both ways of moving it besides the drag. One drag along a row is one action
of the undo line rather than one point per movement the host hears along it: a Slider sends
a value notification for every mouse event it moves over, and each of those reaches the row's
handler and, through it, the editor's change of the model, so a drag that stepped over eight
numbers used to earn eight undo points and one `Ctrl+Z` left the row still part-way along the
drag rather than off it. The line hears the drag's two ends from the knob itself, keeps the
one point the drag's first change made, moves it along for the changes after it and, once the
gesture is over, drops that point when the drag ended on the number it began at, which is what
a knob carried out and back leaves nothing to undo - a drag that dipped through that number on
its way to another one is left alone, since the row really did move, and two drags out and back
are two actions of the line, since the pointer left the row between them and each mouse-down/up
pair is one gesture. The depth the Settings tab's Undo steps row asks for is what caps how far
that one `Ctrl+Z` can then reach.

The VIB MACRO tab is grabbed the same way again, its own strip living 10 px in from that
tab's right edge and level with its cards rather than with the foot holding its `+ Add
macro` and `- Remove selected` pair, the wheel turning that list and a click of that
column taking the drawn thumb to the pointer for as long as the button is held, while a
drag started anywhere else picks a card out rather than moving the list — a click on the
card's own `V 1` name being what picks it, since a pointer over one of the three settings
lands on the control sitting there instead. That dropdown is what a drag over its six
spellings answers, and its change reaching the macro is what makes a row already calling
that id swing by the new shape from there rather than holding the one the row that armed
it had; the two knobs drag, take a typed value in their text box and answer the
`+` and `-` buttons beside them like the plug-in's other number widgets, the amplitude
walking off to full pitchbend in hundredths and the cycle walking whole subticks.

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
| `` ` `` | note-off (`===`), and in the ARP MACRO tab the note-off step drawn `OFF` |
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

An Enter pressed to confirm a value typed into a field is not a click and fires nothing
the pointer happened to touch last. A Button takes the keyboard focus by default and
answers the Return key while it holds it, so before this build one Enter kept repeating
whatever the last click had done for as long as the pointer stayed put: a tab's name
switched the tab back, a Settings row's checkbox flipped itself again, `- Remove selected`
cleared a second macro, and a knob's own `+`/`-` buttons stepped the knob. The focus now
rests on the control whose text box the value is typed into instead: the ARP MACRO, VIB
MACRO and Settings tabs and every knob want it, the tracker grabs it the way it always
did, and no Button in the editor's tree holds it at all - neither the four tab names, a
Settings row's checkboxes, a card's `+ Add macro` / `- Remove selected` pair and the two
`+`/`-` buttons each knob builds inside itself, that pair being made only by the look and
feel, so the theme's own `createSliderButton` takes the focus away while that look is in
force and the editor sweeps it away from the buttons its panels built before it was. A
ComboBox keeps its own Enter, opening its popup, which is a choice being made rather than
a value being confirmed. Alert windows are left alone too: one is a window of its own with
no parent component, so the sweep never reaches it, and its buttons answering a Return on
their own is what closes one by keyboard.

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
`0`–`127` and the five letters `A`, `B`, `U`, `D` and `V`: an `A` typed as the command number
field's first character claims it and means `128`, the arpeggiator call, whose
argument names the macro id, a `B` claims it and means `144`, the pitch bend, whose
argument names a point on the wheel, a `U` or a `D` claims it and means `145` or
`146`, a slide of the wheel to its top or to its bottom, whose argument says how many
rows below the one that named the letter its extreme sits over rather than where on
that wheel it goes, and a `V` claims it and means `147`, the vibrato, whose argument names
which of the VIB MACRO tab's macros the column swings. The claim is only the command
number field's, which is what keeps `V` a note letter where the note column is asked for:
typed there it still types an F natural, the way the slide letter `D` still types a D
natural. The CC `10` and CC `11` a column asks for are typed `10` and `11`,
and a letter typed after a run of digits only claims
the field, the number being typed having been dropped rather than read as a
command, so nothing about a number's spelling reaches one of the letters. A
letter writes the command value it names rather than a number that gets clamped,
since the number field tops at the highest CC `127` and only the letters themselves
reach above it: typing `146` into that field gives the highest CC rather than the
slide letter, an older build's `143` cannot be typed back at all, and an interpolation
ramp climbs toward that highest CC rather than inventing a command. Each letter has to
leave an argument behind, so a call and a vibrato start at the release id `0`, a bend starts at the
wheel's centre `64` and a slide starts at the default span of `8` rows, which is what
makes a row naming only the `B` bend nowhere rather than lock the wheel to its bottom,
a row naming only a slide letter take its extreme over 8 rows rather than over
none at all and a row naming only the `V` read as `V OFF` rather than run the default macro.
The call pair is drawn orange and the three wheel pairs and the vibrato pair lilac, every
value in the view being decimal, and either letter a row carries from above
draws dim like any other carried command. So does the argument carried beside
it, and that argument is the newest one that column's rows entered rather than
the one the letter itself named, which is what an argument typed under a
command already in force changes: the rows under it are drawn with it beside
the letter still carried there, and under a carried slide letter that newest
argument is the span the rows below are still taking toward the extreme rather
than a new point on that wheel. An argument of `0` typed under a carried call
is the arpeggiator release on its own, that row reading a bright `OFF` beside
the letter carried into it and the rows under it carrying neither the letter
nor an argument, and the same `0` read beside a named or a carried vibrato letter
lands that vibrato's own release there rather than a call of macro `0`, since id
`0` calls nothing — the entry writing the `OFF` a row spells for an `o` being the
other way of spelling it, both of them drawing `OFF` and both of them leaving the
column carrying neither the letter nor an argument below, and the same `0` read beside a named or a carried slide letter
lands that release there rather than a slide over no rows at all, since the
narrowest span the field can ask for takes one row. The argument field answers
two letters rather than digits, since neither is a number the field could spell:
an `o` or an `O` lands the `OFF` the drawing reads, switching the pitchbending
off and putting the wheel back at its centre beside a wheel letter, going
the arpeggiator run off beside a call and taking a vibrato's swing off beside the
vibrato letter, and an `s` or an `S` lands the `STP`, which stops whatever move the
wheel was making and leaves it sitting on the point it had got to, beside a wheel letter only — an `s` aimed at the vibrato letter
is refused rather than written, since a wobble has no move of the wheel to stop,
the cell keeping the argument it had and the swing above still swinging over the
rows below it. A letter neither of those two
claims is ignored rather than written, the cell keeping the value it had, and
it types no note in the note column beside it either, so an `s` aimed at a call,
at a vibrato or at a plain CC asks nothing of a run or a swing that has no wheel
to stop.

The ARP MACRO tab's steps are typed the same way rather than one press at a time: the
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
to the jump the field can hold. The note column's own note-off key spells a step the same
way here: the backtick (and its shifted tilde, the pair the note entry answers alike) opens
the step the cursor sits on rather than writing a number into it, so the cell reads `OFF`,
the run's own release, and the cursor stays where it was the way `Delete` and `Backspace`
leave it. `Delete` and `Backspace` make a step sound nothing, which
sustains the step before it rather than the base note.

The VIB MACRO tab has no numbers to type, so its own keyboard does only what its cards
need: the arrows and `Tab` move a macro cursor from card to card, `PageUp` and `PageDown`
go to the list's own ends — the nearest defined macro at or below the top rather than one
card up — and where there is no cursor yet the arrows start from the highest id a row can
name, which is what the remove button answers against as well. The three settings on a card
are turned by JUCE's own controls rather than typed into the cell: the shape dropdown is
dragged over its six spellings, its change writing the shape to the macro so a row already
calling that id swings by it from there and leaving one point of the undo line for that one
pick, and the two knobs take a typed amount in their text box as well as a drag, each of
them leaving a point of its own. Defining a macro and taking one away are one action each
as well, so a card whose three settings were all touched and that was then defined and
removed leaves five points of the line rather than one.

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
  argument names the macro id, `1`–`127`, with the release spelled two ways,
  the id `0` a row names beside the letter and the `OFF` the entry writes for
  an `o`, either going the run off, re-triggering the note the run measured
  from at its own row rather than only cutting the sound and leaving the
  column calling nothing rather than calling its id, so the rows under either
  carry neither the letter nor an argument and both read `OFF` rather than the
  number - a row under a carried call naming only an `o` or only a `0`
  answering as the same release, its own row still drawing the letter carried
  into it beside the bright `OFF` and its rows under it carrying neither; the
  pair is drawn orange, and since decimal is the
  one spelling the whole view has, that argument's cell keeps the three glyphs a
  macro id, a point on the wheel and the release's `OFF` all need. An
  `OFF` and an `STP` sit above the field's own top as the argument's own
  numbers rather than as numbers it can spell, so a run of typed digits cannot
  invent either of them and both survive the saved form unchanged. An
  arpeggiator:

  * sounds one step per subtick, a subtick being a row over the Settings row
    "Subticks per row" (default 6, so a step is a sixth of a row), the first of them
    being the note entered on the row that made the call: the macro is checked before
    that note is sounded, and the note goes out at the pitch the first step moved it
    to at that row's own subtick rather than as it was typed at the row below, which
    is why the run's first note-on sits at the call itself, an empty first step
    sounding the note as it was typed and a first step of `OFF` sounding none of it
    at all - the run's own release there opening whatever the channel was left
    holding, while the column still names the note entered at that row so the steps
    under the rest measure from it;
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
  * opens what it was holding at a step entered as the note column's own note-off,
    drawn `OFF`: the step releases the note the run last entered and, where the run
    has sounded no step of its own yet, the note it is measuring from - the call row's
    own, or the one the column's rows above last named - at that step's subtick and
    then sounds nothing, so a step of `OFF` at a run's head is a rest over the note
    the rows above had left held rather than a release of nothing, that opening being
    the run's own rather than one the clip's end takes lower down and the column still
    naming that note so the steps under the rest measure from it again, which is also
    why the note a step of `OFF` opened is not re-sounded by the steps under it until
    one of them names a note again. Only a note that run entered is ever opened there,
    so a step of `OFF` cuts nothing another column or another run was holding;
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
  * goes off when its argument names nothing to call, spelled by the id `0`
    beside the letter or by the `OFF` the entry writes for an `o`: the note the
    run was left sounding is released at that row and the note the run measured
    from is struck there again, so a row turning a run off goes back to the
    column's own note rather than cutting the sound and leaving nothing under
    it, that re-struck note being owned on by a note-off below or by the clip's
    end the way a call row's own note was. A row that struck a note of its own
    had already struck that base, and a row naming the column's note-off is
    what took it away, so neither re-strikes, and an `STP` beside a call means
    nothing, a run having no wheel to stop;
  * closes and is closed by what shares its column rather than by what shares
    its MIDI channel: the row under a carried call that names a slide letter
    ends the run there, the note that run's last step entered being released at
    that row and the slide letter arming a ramp of its own below it, while a
    slide written in another tracker channel of the same MIDI channel leaves
    that run alone, its steps still walking below that row and being played
    against the wheel that other channel's ramp is moving;
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
  row further down puts a bend back after a note-off had taken it, the `OFF` the
  entry writes for an `o` naming the centre there rather than a number above the
  field's own top and an `s` landing the `STP`, which asks for nothing at all and
  so leaves the wheel sitting on the point the rows above had left it at. A `0`
  beside the bend letter's own row keeps naming the wheel's bottom rather than
  that release. The wheel
  returns to the centre at a note-off, at a row naming the arpeggiator call
  rather than a wheel command, a run having no wheel of its own to put right and
  that call's row being the only one able to put it back, at the end of the clip whose rows last
  moved it, which is what a row that bent without striking a note needs since no
  note-off of that channel was ever going to close it, at a transport re-arm and
  at the editor's own all-notes-off.
* Commands `145` and `146`, entered as the letters `U` and `D`, slide that wheel to
  its top 16383 or to its bottom 0 instead of holding a point of it, and their argument
  says how many rows below the one that named the letter its extreme sits over rather
  than where on that wheel it goes: the empty argument taking the default span of `8`
  rows, the widest span the field can naming being 127 rows and the narrowest taking one
  row, since the `0` a row's entry leaves beside a slide letter and an `o` typed there are
  that letter's release rather than a slide over no rows at all, the wheel being put back at
  its centre on that row, no ramp armed there and the letter leaving the column carrying
  nothing below it. The row that named the slide is the ramp's own
  start, so the wheel sits where it already was there and that row's own note plays
  straight with nothing sent of its own, while every sub-row under it keeps the wheel
  climbing along the grid the Settings row "Subticks per row" names rather than jumping
  to a new point once on each row under it, the step being counted from the ramp's own
  start in transport position rather than read off the row — which is what a transport
  jump that makes this block play a row again leaves it on the same step of the ramp
  rather than advancing it twice, and what leaves a row's own sub-row steps waiting for
  the block that reaches them the way an arpeggiator run's do — and the step that sits
  over the span's own last row lands on the extreme exactly, the steps before it holding
  the whole wheel position that far along the straight line between where the ramp left
  the wheel and that extreme, the wheel then sitting at a point of the column's own like
  a bend row left it. A row under a carried `U` or `D` that names only an argument re-arms
  a ramp of its own from wherever the wheel's sub-rows had got to, which is how a slide is
  retargeted while it runs, and a ramp re-armed over a wheel already at the extreme moves
  nothing. An `OFF` read under a carried letter, spelled by the `o` the entry writes or by
  a `0` that row typed, answers the same way the naming row does, and an `STP` one stops
  whatever move the wheel was making and leaves it holding the point its sub-rows had
  reached, the letter being carried on below it rather than put away, since that spelling
  asks nothing of the wheel's own state. A row naming a command that is not a slide ends
  the ramp that was still climbing, and where that command is the arpeggiator call the
  wheel is put back at its centre there rather than left where that ramp had got to, a run
  having no wheel of its own to right and that call's row being what can; any other such
  row leaves the wheel where it is and asks for a point of its own rather than a climb.
  The two halves of a column end one another rather than overlap, a slide row under a
  carried call closing the run that column was in by releasing the note its last step
  entered at that row and arming a ramp of its own below it the way the call's own row
  ends the ramp above it, so a column holds either a run or a wheel move and never both,
  while a slide written in another tracker channel of the same MIDI channel leaves that
  run alone, its steps still walking below the slide's row and being played against the
  wheel that channel's ramp is moving, since a ramp and a run belong to the tracker
  channel that named them while the host hears the one MIDI channel both were sent to.
  A slide is
  a state the column carries down like a bend rather than one its own row only, so a
  note-off below the row that named one ends it partway down its steps, the wheel
  having already moved on the sub-rows inside the row that named the note-off, which
  is why nothing keeps climbing underneath that note-off, and at the end
  of the clip whose rows armed it: a ramp still climbing there has no rows below that
  clip left to finish it even when its own wheel never left the centre, so only the
  ramp needs putting away and no message is what takes it back.
* Command `147`, entered as the letter `V`, runs one of the VIB MACRO tab's macros and its
  argument names that macro's id, `1`–`127`, the release spelled the same two ways an
  arpeggiator call spells it — the id `0` a row names beside the letter and the `OFF` the
  entry writes for an `o`, both drawn `OFF` — which puts the pitchbending away and takes the
  wheel back to its centre at that row, the last point of the cycle having left it somewhere
  else, so the rows under a release sit under a straight pitch rather than at whatever point
  of the swing they would have got to. A release leaves the column calling nothing rather than
  calling id `0`, which is why a further row there that names only an argument arms nothing,
  while a row under a carried `V` naming another macro's id re-arms the swing there: the cycle
  counts again from that row and the new stretch measures its shape, its amplitude and its cycle
  from wherever the previous one's last point had got the wheel rather than jumping to the new
  macro's own base. An `STP` one asks nothing at all, a wobble having no move of the wheel to
  stop, and leaves the swing above still swinging over the rows below it — the editor refuses the
  `s` spelling beside the vibrato letter rather than writing it, so that argument is one a saved
  state or a shot's own flag puts there. The pair is drawn lilac like the wheel's own rather than
  in an arpeggiator call's orange, since a vibrato is a wheel move, and as with a slide row the
  run that column was in is closed here rather than kept: the note its last step entered is
  released at that row and the run ended, while a run another tracker channel of the same MIDI
  channel was in is left alone, its steps still walking below.
* What that wobble does with the wheel:
  * it swings about whatever point the wheel is already at where it was armed — a bend row's own
    point, a slide's extreme or the centre — rather than about the centre, which is what lets the
    letter that bent the pitch, a slide's extreme and a plain bend argument all be the base a
    wobble swings from. The swing is clamped at the wheel's own top or bottom where the base has
    no room above or below it for the full amplitude. A bend row under a swing that is still
    running moves the point it measures about to the pitch that row asks for rather than ending
    the swing or starting a new one, and neither strikes a note nor releases the one the row above
    it held, so the swing carries on across it, its own point and that row's being one pitch
    rather than a jump;
  * it re-reads the macro's three settings at every point of its cycle rather than once at the
    arming row, which is what leaves a shape, an amplitude or a cycle turned on the tab while a
    wobble was swinging changing that wobble from where it is rather than holding what the arming
    row had still in force, and what ends a swing whose macro was taken away at its next point
    rather than at the row that took it, saying nothing of its own and leaving the wheel at
    whatever point of its own the swing had got to, a release row below being what puts that away;
  * its cycle is the macro's own, counted in subticks on the grid the Settings row "Subticks per
    row" names, and it is never drawn with fewer than 8 points of it, so a cycle 12 subticks wide
    is two rows wide cut at the twelfths of a cycle rather than the eighths it asked for at least,
    a cycle 8 subticks wide at the default 6 subticks to a row is four thirds of a row wide with
    one point per sub-row step and crosses the rows it sits in rather than starting on each of
    them, and one a single subtick wide still moves the wheel 8 times inside that grid rather than
    once over it, so 6 cycles of it land inside one row and its last point leaves the wheel off
    the base rather than back at it;
  * its points are counted from the arming row in transport position and its phase from that count,
    the way a run's steps and a ramp's are rather than read off the rows or carried round from the
    point before, which is what a transport jump that makes this block play a row again leaves on
    the same point of the cycle instead of advancing it twice and what keeps a swing that cycled
    many times over many blocks exact rather than drifted by its own fractions;
  * it sends the standard pitch bend pair, CC105 and CC106, to the MIDI channel that row's
    instrument names like every other command row: the position is the wheel's centre 8192 plus
    the shape's share of the macro's own height, and that height is the amplitude's share of the
    wheel's own half-height 8192 — so a knob at full reaches the wheel's own extremes from a swing
    sitting at the centre, the peak above landing one position past the wheel's top 16383 and
    clamping to it while the trough below lands exactly on its bottom — counted in whole wheel
    positions rather than in the argument's steps of 128, which is what leaves nothing rounded
    where the swing's own curve crosses the centre. Only a point that changes the wheel's position
    says anything, which is what leaves the arming row's own point, sitting on the base, sending
    nothing of its own and a row that armed a wobble under a bend row leaving that row's own point
    ahead of the swing's first, and what leaves a macro whose amplitude knob is at off holding the
    pitch it is at over its cycles rather than moving the wheel at every point of them — such a
    macro still being a macro rather than no macro at all, its cycle still turning and its points
    still counted even though none of them moves the wheel.
  * the six shapes are two curves and their folds: Sin curves and Triangle ramps in four straight
    sides whose corners sit on the cycle's quarters rather than bunched at its ends — climbing to
    its far top a quarter of the way in, back through the base by halfway and to its own bottom
    three quarters in — and the `Up` spellings of each have the cycle's second half inverted, so
    the wobble never reaches below its base, while the `Down` spellings have its first half
    inverted, so it never rises above it; at full amplitude that has the sinusoidal `Up` landing on
    the wheel's top twice a cycle with nothing below the base to go to and the sinusoidal `Down`
    landing on its bottom twice, the fold being the half the plain shape swung to the far side in;
  * it never ends by itself: a cycle repeats for as long as the column carries the letter, so what
    puts a swing away is the row that released it, the end of the clip whose rows armed it — where
    that clip armed the wobble with no note of its own there is nothing to release and no note-off
    to close it, so the clip's end takes the wheel back and nothing else, the all-notes-offs being
    the ones the transport's own start asks for rather than any of the clip's — the loop that closes
    a column whose clip was deleted or is no longer under the playhead, which names the clip the
    wobble's own rows lived in since a column that armed a swing without striking a note has no note
    whose clip says where it sits, and a swing whose next point sits more than a row behind where
    this block starts, which is what a scrub or a jump leaves a swing ended at where it had got to
    rather than one swinging late at that point.
* A tracker pattern can use up to 127 tracker channels, independent of the 16
  MIDI output channels. Identical notes triggered on multiple tracker channels
  in the same row are sent only once.
* Stuck-note safety net: hosts like Reaper never call a MIDI effect while the
  transport is stopped, so note-offs emitted at stop time may never reach the
  synth. The engine remembers every pitch it has ever sent a note-on for, per
  MIDI channel (survives transport re-arms), always precedes a later note-on
  of such a pitch with a matching note-off, and closes clip ends with a
  tolerance for hosts that report block bounds slightly short. The release that
  channel already put at that same sample is the exception rather than an extra
  beside it: it is recorded with the pitch, the MIDI channel and the sample of
  the block it was queued in, and the guard asks that record before adding an
  opening of its own, which is what a note entered at the very sample a clip's
  end released that pitch at — one pattern placed twice back to back, holding
  one pitch at its foot row and at the row below that boundary — has ahead of
  it: one note-off of that pitch rather than two. Two releases of one pitch
  around one note-on at one sample is what a host that answers the sample
  rather than the order inside one reads as the new note being opened at once,
  while the one release a note change inside one clip already puts ahead of its
  note works everywhere.
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
  which is what names an arpeggiator step's width, a slide wheel's next move and how
  wide one vibrato macro's cycle is -
  a macro sounds one step per part, so 6 puts a step at a sixth of a row and lets
  the eight steps a fresh macro holds reach past the row that called the letter
  `A`; those eight sound nothing until entered, so it is their width rather than
  their number that a call of a fresh macro can hear, and a slide of 8 rows over
  that same 6 walks the wheel in 48 whole positions across those eight rows rather
  than in 8 jumps of one whole position each, so the grid names how far apart the
  wheel's own moves are rather than how many rows its extreme sits over, and says as
  much about the two spellings that ask nothing of a span: the `0` or the `o` a row
  writes beside a slide letter puts the wheel back at its centre from the point that
  letter's ramp had reached up to the end of the row above, and the `s` that asks to
  hold leaves the wheel sitting on that same point, since a row's own sub-row steps
  are flushed only after that row's command has answered and are left waiting there
  for an ended ramp rather than moving the wheel before the answer. A wobble counts its
  cycle the same way rather than over the rows: the VIB MACRO tab's cycle knob spells
  whole subticks one sin or triangle cycle takes, so a cycle of 6 is exactly one row
  wide at the default 6 while a cycle of 8 is four thirds of a row wide and one of
  12 is two rows wide, and it is that width over the 8 points a cycle is never drawn
  with fewer of that says how far apart the wheel's own moves are - one point per
  sub-row step at a cycle of the default 8, and 8 points inside those parts rather than
  one move over them at a cycle narrower than that
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
  action, so the row's own value rewinds too, however it was moved: a drag along
  the row, a click of the row's own `+`/`-` pair, which still costs one step of
  its interval each, and a value typed into the row's text box all reach the
  model. One drag is one action of the undo line rather than one point per
  movement the host hears along the row, so one `Ctrl+Z` takes a whole drag off
  the row at once rather than leaving the number part-way along it, and a drag
  that ended on the number the row began at leaves no point at all. `0` turns
  undoing off and stops the plug-in from holding states at all, and a 32 MB
  budget drops the oldest states of a pattern whose states do not fit that well,
  so a huge pattern keeps fewer of them than the row asks for

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
  re-entered by hand. This build's own bend letter, its two slide letters and its
  vibrato letter are command values `144`–`147` that an older build could not have
  written, so no older row turns into a bend, a slide or a wobble and none of
  those calls becomes one; an older state has no `VIBS` section either, since
  nothing older than the letter `V` had a vibrato to save, so the read arrives
  with the one default macro a fresh song defines rather than with an empty tab.
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
  sounds nothing; an `A 0` release or an `A OFF` one ends the column's call as well
  as the run, so the letter and its argument stop being drawn over the rows below
  it, either spelling reading `OFF` and a further row naming only an argument
  there answering nothing. Going off strikes the note the run measured from again
  at its own row rather than only cutting the sound, so the column goes back to
  sounding its own base note until a note-off below or the clip's end owns it, and
  that re-strike carries the guard's own extra note-off of the pitch it re-articles,
  which is one note-off more than a bare release of the run's last step. Both are
  the plug-in's own reading rather than a host's, and where a host puts that
  release, that re-struck note-on and the run's first note-on at one sample is
  still known only from the hosts.
* The argument field's two letter spellings sit above the numbers that field can
  spell, so an older state's argument of `128` or `129` beside a call reads as the
  release and as nothing at all rather than as one of the ids the older encoding
  ignored: the release one going the run off and re-striking its base, the `STP`
  one being refused since a run has no wheel to stop, and an older argument of
  `130`–`143` still landing on the highest macro id as before. Nothing about the
  saved form says which build wrote the number, and the older state's own macro
  definitions are dropped outright, so those calls still have to be re-entered
  whichever way they now read.
* A pitch wheel left bent by a channel whose clip was deleted outright is taken
  back only at the next release point or transport re-arm rather than by the loop
  that closes the notes, the arpeggiator runs and the wobbles of a clip the playhead is no
  longer over. That loop now names the clip an arpeggiator's or a wobble's own rows
  lived in when the column has no note whose clip says where it sits, since a row
  that armed a run or a swing without striking a note leaves no note to point at
  one: their being gone is what ends them, and what returns the wheel such a
  swing had left off the centre there rather than leaving it bent over the clips
  that are still under the playhead, the way the clip a wobble was armed by is
  what puts its swing away at its own end. The wheel's own letters are what that
  loop still has nothing to name, so a row that only bent keeps its wheel bent
  across that gap - and a slide, whose ramp belongs to the tracker channel
  rather than to the clip that armed it, keeps climbing: only that clip's own end
  puts a ramp that is still climbing away, since it is the only one whose rows ran
  out, so a ramp armed by the rows of a clip that is gone keeps the wheel climbing
  over the sub-rows of any further clip of that channel until a release point below
  takes it back - the ramp's own steps being a sub-row wide rather than a row wide,
  which is what makes a wheel a release point is taken back from a point of that line
  rather than from one of the row's own.
* Where the asks named only the wheel, its readings at the other kinds are the
  plug-in's own: an `s` beside an arpeggiator call is refused rather than written,
  a run having no wheel to stop and the row sounding nothing and leaving the run
  above it as it was, an `s` beside a vibrato letter refused the same way, a swing
  having no move of the wheel to stop either and leaving the swing above still
  swinging, and an `s` beside a plain CC is refused the same way, and a `0`
  beside the bend letter's own row keeps naming the wheel's bottom while the `o`
  there spells the release. The `STP` hold asks nothing of the note column, so the
  note that row or a row above it struck is still held under it bent at the point
  being held, and the letter stays carried below that row so a further row under it
  can re-arm a ramp, hold the wheel again or put it away. What only the hosts can
  say is whether they hear a wheel held at a point by an `STP` as one pitch over the
  rows below, how many of a ramp's sub-row positions a host lets through before it
  hears an `OFF` that had been climbing as one move back to the centre, whether a
  host reads an `o` or an `s` in the argument column as its own before the plug-in
  is offered the press, whether an `s` typed in the note column still enters a C sharp
  there, the claim being only the argument field's, and where a host puts a release, the
  note it re-triggers at that row and the following note-ons when they all land at one
  sample.
* A wobble never ends by itself: a cycle repeats for as long as the column carries the
  letter `V`, so what puts the pitchbending away is a release row — spelled by the id
  `0` beside the letter or by the `OFF` the entry writes for an `o`, the two reading
  the same way an arpeggiator call's release does — the end of the clip whose rows
  armed the swing, which is what a swing a clip armed with no note of its own needs
  since no note-off of it was ever going to close it and nothing of it was ever
  going to release anything, the all-notes-offs being the ones the transport's own
  start asks for, or a transport re-arm. A swing whose macro was taken away ends at
  its next point rather than at the row that took it, says nothing of its own and
  leaves the wheel at whatever point of its own the swing had got to, so a release
  row below is still what puts that away. An `STP` spelling beside the vibrato letter
  is a number a saved state or a shot's own flag leaves there rather than something
  the editor offers, since the entry refuses the `s` there — a wobble has no move of
  the wheel to stop, so such a row leaves the swing above still swinging over the
  rows below it. What only the hosts can say for a wobble is how wide a pitch they
  hear it swing: the amplitude is a share of the plug-in's own 14-bit wheel rather
  than of a host's pitchbend range, so a knob at full reaches that wheel's own
  extremes and means whatever the host's own range says that is, and how many CC105
  and CC106 pairs in a row a host lets through before it hears the swing as one
  pitch rather than as a shape — the plug-in sends one per point of a cycle that
  changes the wheel's position and no more, that being one every sub-row step at a
  cycle 8 subticks or wider and never more than 8 points over a cycle at a narrower
  one.
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
  same pitch, at the cost of one extra note-off, and that extra one is the only
  place the shape goes wrong: a second note-off of a pitch beside the note-on
  one sample later is what a host answering that sample rather than the order
  inside it reads as the new note being opened right away, which is the reported
  row-0 note cut at a clip boundary — one pattern placed twice back to back, its
  foot's note and the next clip's row 0 holding one pitch, a different pitch there
  having never shown it since that boundary only ever released the other one. That
  duplicate is now suppressed by recording the release rather than by moving an
  event, pinned by the CoreTest "A clip boundary re-entering the pitch the
  clip's foot entered" section at one release ahead of that note-on rather than
  two and at nothing of that pitch closing the note again after it. That boundary
  has since been asked again, one pattern placed twice back to back, in the host
  the report was seen in, and no abrupt cut was heard there, so the report's own
  shape is gone in that host rather than only in the fixture; what that section
  still covers at the engine alone is the moved-clip-from-time-0 scenario, which
  has not been asked in a host at all.
* Command kinds beyond plain CC, the arpeggiator call, the pitch wheel and the
  vibrato that wobbles it (a dedicated
  pitch bend CC105/CC106 pair rather than the standard pitch wheel message, tempo, song
  position) are future work.
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
A clip boundary re-entering the pitch the clip above left playing sends one release there rather
than two: the release a channel queued at a sample is recorded as that pitch, that MIDI channel and
that sample of the block it was queued in rather than an event being moved or a note-on being started
later, and the stuck-key guard asks that record before it adds an opening of its own, since two note-offs
of one pitch around one note-on at one sample is what Reaper read as the note entered at the row below
the boundary being opened right away while the one release a note change inside one clip already puts
ahead of its note-on reads right everywhere. The record only reaching as far as the block being queued,
its three numbers cleared at that block's start by the flush loop over every tracker channel, is what
keeps a note entered at a same-numbered sample of the block below still asking the guard for its own
opening, and allNotesOff's own per-channel note-offs not being recorded at all is what keeps a transport
re-arm and a loop wrap re-articulating exactly as the fixtures above pin, that block being the one a host
hands over while the transport is stopped and so may never deliver at all.
The vibrato letter is command value `147` (commit 579e210): the bend, slide and vibrato letters sit
above `kOlderCmdTop = 143` so an older state's argument of `130`-`143` still lands on the highest
macro id rather than turning into one of these, and `128` and `129` still read as the OFF and STP
sentinels rather than as ids, so nothing an older build could write becomes a wobble and nothing a
digit run can type reaches the letter. Only the command number field claims the letter, so `V` typed
where the note column is asked for still types F natural, the way the slide letter `D` still types a
D natural, and the note key `v` stays a note key rather than being spent twice. A macro is a
`VibMacro { present, type, amplitude, frequency }` in a grow-only `std::vector<VibMacro> vibs`, since
clearing one leaves its room and the shape, amplitude and cycle its knobs had rather than erasing the
settings, which is what a macro that goes off still answers at and re-defining then starts over from;
the `present` flag is what says a macro exists, so a card exists whether or not a knob is turned and
the table only ever grows, keeping ids in place the way the arp table does. `VIBS`/`VIB` with `id`/`type`/
`amp`/`freq` props is the saved form, one entry per macro that is present rather than one per id the
table has room for, and the read empties `present` over the whole table ahead of it the way the `ARPS`
read does, which is what an undo that took a vibrato away leaves it undefined rather than holding the
knob positions it had. A wobble swings about `c.lastBendPos` rather than the wheel's centre, so a bend
row's own point, a slide's extreme and the centre can all be the base it swings from, and it is measured
on `vibPoints = max (cycle, kVibCyclePoints = 8)` subticks rather than the row's own `subticksPerRow`
steps, so the curve is cut at the cycle's own fractions (a cycle 6 wide is one row wide, one 12 two rows
and one 8 four thirds of a row at the default Settings), a cycle of 12 is drawn with 12 points rather
than the 8 it asked for at least, and a cycle of 1 still moves the wheel 8 times inside that grid instead
of once over it, with the phase counted from the arming row's transport position the way an arp run's
steps and a slide's ramp are, so a transport jump re-delivering a row does not advance the swing twice.
A wobble never ends by itself: what puts it away is the row that released it, the end of the clip whose
rows armed it (which is what a swing a clip armed with no note of its own needs, since no note-off was
ever going to close it), the clip-gone loop, or a swing whose next point sits more than a row behind
where this block starts. The gone-clip loop had to learn to name a run's or a swing's own clip rather
than only `c.activeClip`, which a command-only row never sets, so when a column has no active note it
falls back to `c.arpClip` then `c.vibClip` (commit 579e210) - without that fallback that `else` branch
ended the swing and sent nothing of the wheel at all, so the 16 bends the clip-runs-out fixture pins
came back as an empty run rather than as the swing's own points. The UI's tab names had to be spelled
out rather than reused: the ARP MACRO and VIB MACRO buttons are 10 px wider than the plain ones, since
a tab's own text is what a test compares against and the vibrato card needed a tab of its own before
it could be reached at all. The ComboBox was preferred over a hand-drawn selector because JUCE's own
already reaches the model through onChange, which is what lets a dropdown pick be one point of the
undo line and a knob turned two more rather than a card being one action of its own.
An arpeggiator run's first tick had two questions open in what was reported: whether the note entered
beside a call is that run's first step or only the note the run is measured from, and what a step entered
as the note column's own note-off lets go of. The note entered at the call's own row was made that run's
first step, since what was reported was a call beside a note not answering until the subtick below: the
note handling in processRow reads the call that row arms through a new file-local arpCallArmedAtRow (const
Cell&, int lastCmd) and its first step through a new arpFirstStepOf (const Song&, int id), the first taking
the plain lastCmd the row had already left in force rather than the channel itself, since ChannelState is
a private nested type of the engine and no helper outside its own file can name it, so the note entered
here cannot disagree with the arpCommand answered below about what that call is about to do to it. The
note goes out as arpSteppedNote (cell.note + first step) at the call's own subtick while arpCount still
starts at 1 there, and arpSteppedNote had to leave the step loop as a helper of its own rather than stay
inline in it, since the first step above and the steps below cannot both spell one octave snap without
one of them drifting. An empty first step leaves the note where it was typed, a first step of OFF sounds
none of it at all, and the note as it was typed never reaches the host at all when that step names a jump
- the column still names it, so the run measures from it and an A 0 call written there still re-strikes
it. A note-off step was made to open whatever the run holds at its own subtick rather than only the note
the step above entered: flushArp releases c.activeNote under the guard c.activeNote >= 0 rather than the
older c.arpNote >= 0 && c.activeNote == c.arpNote, which could not say anything at a run's head where
arpNote had never been set, and it sets c.arpNote = -1 rather than ending the run, so the steps under the
rest keep stepping and measure from the note the column still names. Both answers were needed for that
rest - the run's own note and the column's - so the engine keeps the second in c.columnNote and c.arpCallNote
rather than in the run's own state, which is what the two new fixtures at a call whose row has no note of
its own pin at a run's first tick and at its second. A first step entered as that note-off sounds none of
the note entered there at all, the run opening instead whatever the channel was left holding at that row,
which is why the README names that case apart from the one where a later step opens the note the run itself
entered.
The Enter key's other half: a JUCE Button wants the keyboard focus by default and answers the Return key
while it holds it, so one click anywhere left every later Enter repeating whatever the pointer had last
done, for as long as the pointer stayed put - the tab names switching the tab back, the top bar's snap
checkbox and its zoom pair, the tools' arrows, a Settings row's checkbox, a card's + Add macro / - Remove
selected pair, a knob's own +/- pair - and the report asked that a press meant only to confirm a typed
value fire none of them. The fix moves the focus rather than the handlers, so every control keeps its
click: no Button in the editor's tree wants the focus at all, and the controls whose own text box a value
is typed into want it instead, which is every knob - Slider::init forces a Slider not to want that focus,
so setWantsKeyboardFocus (true) had to be put back at each creation site rather than once - and the three
panels that own typed fields, the ARP MACRO, VIB MACRO and Settings tabs, whose focus the editor's setTab
grabs for the tab that is showing, mirroring what it already did for the tracker. Those three panels spend
the returnKey in their own keyPressed rather than handing it up, since their own fields had answered it
already and handing it on would let the editor's shortcuts and whatever sits above them hear a press that
was only meant to close a number. A Slider in the IncDecButtons style builds its own +/- buttons through
the look and feel, so nothing outside a Slider can reach them and the theme's own createSliderButton
override takes their focus away - but that override only sees the sliders built while the plugin's own
look is in force, and the editor's constructor puts that look in force as the first line of its body, so
the knobs the panels carry as their own members - the Settings rows', both macro tabs' cards and the tools'
transpose amount - were built before it and got the default look instead, while the bottom bar's three
knobs and the fresh cards a tab rebuilds as its macros are added to and taken away are built after it.
The editor therefore also sweeps its own component tree for Buttons in setTab, which catches the tab
buttons, the top bar's snap checkbox and zoom pair, and every knob pair those panels built under the
default look, on every tab change rather than only at construction. The tools
panel keeps no focus of its own, since it stays on screen on every tab and focus resting there would leave
the tracker, the only note entry there is, deaf. A ComboBox keeps its own Enter, opening its popup, which
is a choice being made rather than a value being confirmed, and an AlertWindow's buttons are left alone
the same way: it is a TopLevelWindow with no parent component, so the sweep never reaches it, and answering
a Return on their own is what closing one by keyboard needs. The three focus checks UiTest had leaned on
getCurrentlyFocusedComponent for turned out to be unobservable in that harness rather than failing -
Component::takeKeyboardFocus no-ops when getPeer () is empty and that harness creates no desktop window,
so nothing is ever focused there - and were replaced by structural ones: no Button inside the editor's
tree or inside a showing panel wants the focus, the knobs want it, a knob's own +/- pair is there and
holds neither, and each of the three panels spends a repeat of the Enter leaving no checkbox flipped, no
macro added or cleared and the undo line alone.
The Undo steps row's knob being made draggable rather than the row being given a widget of its own: every
number widget the plug-in carries is a juce::Slider in the IncDecButtons style, that style draws no thumb to
grab so the whole area beside a row's text box is the pair of +/- buttons the look and feel builds for it, and
Slider's own default has incDecButtonMode at incDecButtonsNotDraggable
(JUCE/modules/juce_gui_basics/widgets/juce_Slider.cpp:1336), which leaves incDecDragDirectionIsHorizontal ()
false and a horizontal drag over the row worth zero delta - the number leaving whatever the last click had
stepped it to and the row never reaching the undo line at all. Asking each of the six creation sites for
incDecButtonsDraggable_Horizontal rather than restyling them keeps the +/- pair, its one-step-per-click cost
and the typed text box intact, and JUCE's own trade is left as it is: a draggable pair gets an
addMouseListener rather than the setRepeatSpeed (300, 100, 20) the non-draggable one repeats on, so a held
button lights up instead of auto-repeating. The gesture's state living on the plug-in rather than beside each
knob, because pushUndoPoint () is reached from deep inside the panels' callbacks and by the bottom bar's
handlers, which call it directly, so the only place that knows both where a change came from and which gesture
a change belongs to is the processor: it holds the dragBeingHeard and dragPointMade pair the beginKnobDrag /
endKnobDrag pair sets and clears, and pushUndoPoint hands a gesture's second and later events to
replaceUndoPoint. Those two moves of the gesture's point taken with the model lock already held rather than
taking it again, since modelLock is a juce::CriticalSection and pushUndoPoint is what calls replaceUndoPoint
from inside its own ScopedLock - a second ScopedLock on the same thread would deadlock that spin lock. The
net-zero drop decided at the gesture's end rather than mid-gesture, which is what leaves a drag that dips
through the number it began at on its way to another one alone rather than erasing the point at the moment
that number came back: the row was still moved by the gesture, so the user would walk back into a state that
never was. Two drags out and back are two actions rather than one, since the pointer left the row between them
and each mouse-down/up pair is one gesture; only a gesture ending where it began leaves no point. The
transpose amount's row asked for the drag but not for the line's two ends, since its number belongs to the
arrows' next click rather than to the model and a drag over it makes no point of the line to coalesce.
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
A macro step learned the note column's own note-off (commit 5a14699): an arpeggiator run could
sound a step that named a note, sustain the note it was holding, or sound nothing at all, but could
never open what it held from inside the run - the only release a call could make was the row's whole
`A 0`, which gave up the run rather than one step of it, so the user's ask that a macro step spell
the note-off the way the note column spells it could not be read at all while a step held one int.
The step widened instead of a flag beside it: ArpMacro::steps went to std::vector<std::int16_t>,
kArpEmptyStep stayed at -128 as the step that sounds nothing, the new kArpNoteOffStep took -129
below it and the new kArpTopStep capped a typed jump at ±127 so the two spellings below -128 are
the only values a step can hold that are not jumps - the sentinel passes through the setter and the
octave snap untouched on both sides, which is what leaves the opening a shape rather than a landing
the snap would have folded. The engine answers it inside flushArp rather than as another kind of row:
at that subtick it stops only the note the run itself entered, only while the state still holds that
very note, then sounds nothing and keeps counting the step, so the run stays where it was, its cycle
boundaries stay exact beside the loop and what a column, another run or the row itself was holding is
never cut by a step. The saved steps block went two bytes a step little endian so the opening survives
a state, and the reader tells the two spellings apart by the length the block declares - size ==
declared * 2 reads wide, otherwise one byte sign extended - so an older state's one-byte steps widen
unchanged with their jumps, their steps that sound nothing and their absence intact. The Macros tab
shares one spelling rather than keeping a second: the panel's file-local stepText lambda became the
public MacrosPanel::stepLabel, which the strip's paint and the tests read, so OFF, the note column's
own word for a release rather than ===, which is not a step, is what a cell that was opened draws in
the jump's orange and a cell that sounds nothing draws -- in the release's own dim; the backtick and
its shifted tilde write the opening at the step cursor, claim the press, spend one point of the undo
line for it and leave the cursor where it was the way Delete and Backspace do rather than stepping
the run past the step just written, and the value a half-typed number commits now passes through the
setter's own clamp rather than being clamped a second time at the panel, which leaves one rule.
The tests moved with all three: CoreTest 279, +14, names an opening mid-run ordering its release
between the note-ons either side of it, one at a run's head releasing nothing while the tail of the
same run closes what the next step entered, a looped cycle stepping through its openings and sounding
itself again after the one release, and that an opening cuts nothing a column or another run was
holding; the round trip putting an opening at a step and reading it back through the wider block, and
a hand-built older ARP record of four one-byte steps widening with its jumps, its empty steps and its
absence intact; UiTest 629, +10, drives the panel's own `+ Add macro` leaving the cursor on the fresh
macro's first step, the backtick spelling that step, claiming the press, spending exactly one point of
the line for that one press and leaving the cursor there, reads that step and an empty one back through
the panel's own stepLabel as OFF and --, walks Ctrl+Z and Ctrl+Y across the spelling, and clears the
opened step to the one that sounds nothing at a point of its own - that section sitting after the undo
line's depth checks, since a step written earlier in the run would otherwise have moved the paste and
clipboard depths the later checks ride, the trap the earlier sections had already shown. EditorShot's
--arp= grew the token off, spelled the same way as the note column's own backtick so one flag can name
either, so the Macros-tab shot at /tmp/opencode/arp-off.png - 1000x640, --tab=macros --arp=+0,off,+4,-8,
off,+12 --macros=4 - reads the OFF cells among the jumps in the jump's own orange rather than as dashes
under the band, against /tmp/opencode/arp-empty.png with dots in the same places whose different sha256
is the cheap proof the word really is drawn there. Smoke unchanged at 13 with its serialised state at
2392 bytes, since only a step's width grew, against the bundle refreshed by rsync --checksum to sha256
e792e2e1af46e657217d2193119290af33da9e4b5ec652c9cffe4e67ef78cbfc matching the built artefact and still
topping out at GLIBC_2.38 with GLIBCXX_3.4.32 and naming no sqrtf or atan2f relocation.
The Command column's two new wheel letters and the ramp they drive: a `U` and a `D` claim the
command number as 145 and 146 and leave the default span of 8 rows as the argument their entry has
to write beside them, so a row naming only a letter takes its extreme over 8 rows rather than over
none at all, and that argument says how many rows below the naming one the extreme sits over rather
than where on the wheel it goes. The row that named the slide is the ramp's own start: the wheel
stays where it already was there and nothing is sent of that row, which is what lets the note that
row strikes play straight, while the rows under it move the wheel one whole position at a time and
the row the span names lands on the extreme exactly, the positions being whole ones by truncating
slideFrom + (slideTo - slideFrom) * steps / jmax (1, span) rather than carrying a float run, so the
endpoints are exact, an argument of 0 reaches the extreme on the naming row itself and 127 is the
widest span the field can name. A row under the carried letter that names only an argument re-arms a
ramp of its own from wherever the wheel had got to, which is how a slide is retargeted while it runs,
and one re-armed over a wheel already at the extreme moves nothing of its own; a row naming a command
that is not a slide ends the ramp that was still climbing while leaving the wheel where that row left
it, so the point such a row asks for stays a point rather than becoming a climb. The ramp is ROW-based
rather than counted, so it is idempotent under a row this block already advanced - slideStep reads the
row it is asked for rather than counting steps, which is what a transport jump that makes this block
play a row again leaves it on the same point of the ramp rather than advancing it twice - and a
slide's pitch wheel uses the bend's own order 0, so it precedes the note-on of the row that asked for
it and a note entering while the wheel climbs is bent by where the climb had got to rather than by the
point above it. The state grew slideOn / slideRow0 / slideSpan / slideLastRow / slideFrom / slideTo /
slideClip beside a slideRunning () guard and an inline endSlide (), every release point that ends a
bend now ending a slide too - the note-offs of allNotesOff and stopNote gained an else if (c.slideRunning ())
c.endBend (), since a slide is a state the column carries down like a bend rather than one its own row
only, so a note-off below the row that named one ends it even before its first step moved the wheel -
and the clip-end take-back went from bendClip == clip.uid && wheelBent to (bendClip == clip.uid && wheelBent)
|| (slideClip == clip.uid && slideRunning ()), the centring message being sent only where the wheel really
is out of place, which is what a ramp still climbing at a clip's end needs: it has no rows below that clip
left to finish it, so when its own wheel never left the centre only the ramp needs putting away and no
message is what takes it back. kBendBottomPos = 0 came in beside kBendTopPos as the wheel's own ends
rather than as a literal 0 at two call sites. The model gained kOlderCmdTop = 143, kSlideUpCmd = 145,
kSlideDownCmd = 146 and kSlideDefaultSpan = 8, and the older-command read rule went from rawCmd >= kArpCmd
to rawCmd >= kArpCmd && rawCmd <= kOlderCmdTop, so an older state's macro calls at either end of its range
still land on the call letter while 144, 145 and 146 read back as themselves and no older row can already
be a slide; the number field's typed clamp stays at kCcTop = 127, so a run of digits cannot invent either
letter while the note column's own D keeps entering a D natural there. Both letters draw in the lilac
theme::cmdOther the bend pair already had, with their arguments in decimal beside them and the entry
claiming the field's first character, and the pool's grid-preview command tick now paints that lilac for
the two new values as well. The tests moved with all three layers: CoreTest 296, +17, six new sections
past the pitch bend ones - the model's two values above kOlderCmdTop and kCcTop, both letters and their
row counts surviving the saved form, an older state's calls at either end of its range still landing on
the call letter rather than turning into a bend or a slide and this build's own three wheel values reading
back as themselves, the climb to the top over a span of 4 going 10239, 12287, 14335 and kBendTopPos
exactly and returning only at a note-off below, D 0 reaching kBendBottomPos on the naming row itself, a
row naming only an argument re-arming a span of 2 from where the wheel had got to - 9215 then 12799 then
the top - while a further one over a wheel already at the extreme moves nothing, a note-off one row under
the naming row ending a slide before its first step with nothing sent at all, and a slide of 8 rows over
a clip of only four climbing 9215, 10239 and 11263 and being put away at that clip's end; UiTest 652, +23,
one self-contained section after every undo-depth check, since a U claim there also spends a point of a
line the checks above had already counted to their cap and would otherwise read as one of the line's last
points - it gives itself a pattern and a clip of its own parked at 4400 beats clear of every other clip,
since the pattern the tracker holds by that point sits on a clip the shared play helper never reaches, and
reads the whole twelve-row climb back as one continuous run of 64-sample blocks, { 8874, 9557, 10239, 10922,
11604, 12287, 12970, 13652, 14335, 15017, 15700, kBendTopPos }, under that clip rather than through the
jumped playRowAt, whose transport jump releases what the synth was holding and that release is exactly
what ends a slide the host jumped out of, that run being primed by a clear past the end of every clip the
sections above parked so it answers only with its own rows, and its D-natural check naming the octave it
enters at since the sections above leave recordOctave at 9; and EditorShot gained --slide=ROW, --slide-down=ROW
and --slide-arg=VAL, 12 by default rather than the 8 the letter's own entry leaves. Two shots read the two
letters: /tmp/opencode/slide-u.png at --slide=6 --slide-arg=12, the lilac U 12 carried down from that row,
and /tmp/opencode/slide-d.png at --slide-down=10 --slide-arg=3, the lilac D 3, whose differing sha256 is the
cheap proof the two letters really are drawn there. Smoke stayed at PASS with 0 failures and 13 checks, its
serialised state still 2392 bytes since a slide costs the saved form nothing of its own - the two values and
their arguments living in the cells a pattern already saves - against the bundle refreshed by rsync -a
--checksum over the bundle's DIRECTORY rather than its single .so, the built and installed binary both sha256
3d0755c45500d341907af8c0fca8d9e6304fcce08575c59f9f7894e77f759bc9 and the sha the previous build carried gone,
the portability read still topping out at GLIBC_2.38 with GLIBCXX_3.4.32 and naming no sqrtf or atan2f, the
slide's arithmetic being integer throughout so the only float names the binary keeps are JUCE's own powf and sinf.
The slide's ramp interpolated over the Settings' sub-row grid rather than stepped one whole
position per row: the wheel is a state the column carries like a volume, a pan or an arpeggiator
run, and the letters only arm a ramp, so what moves it belongs to the position between the rows
rather than to each row of its own - flushSlide now sits beside flushArp at all three of process()'s
flush sites and walks the ramp over sub-rows a row over the Settings row "Subticks per row" wide,
the same grid an arpeggiator's steps are counted on, its steps counted from the ramp's own start in
transport ppq rather than read off the row, its limit being the end of the row the engine last owned
so a row's own steps wait for the block that reaches them rather than being bunched onto one sample,
and a ramp whose next step sits more than a row behind where the block starts ended outright the way
an unplaceable run's is, which is what a scrub or a jump leaves behind; a step's wheel position is
slideFrom + (slideTo - slideFrom) * slideCount / juce::jmax (1, slideTotal), the count scaled before
the division so the step sitting over the span's own last row lands on the extreme exactly and the
ones before it hold the whole position that far along the straight line between where the ramp left
the wheel and that extreme rather than a staircase of one whole position per row - a downward wheel
sitting a hair above the exact line, since integer division truncates toward zero - and a step whose
whole position is the one the wheel already sits at left unspoken, which is why slideCommand became
arm-only at (ch, rowPpq, argument, up, clipUid, song) and slideStep was deleted with the trailing
call in processRow: the naming row's own step at count 0 is exactly that, so even a span of 0 has its
extreme sent by flushSlide on the naming row's own sample at count 0 rather than by the arm of its
own. ChannelState's slideRow0 / slideLastRow became slideOrigin / slideRowEnd beside slideTotal /
slideCount beside the existing slideOn / slideSpan / slideFrom / slideTo / slideClip, the whole ramp
re-counted as slideSpan * subticks so an argument-only row that re-arms mid-climb still lands its own
extreme that many rows below where it armed rather than where the old row grid had put it; slideRunning
() went from comparing rows to slideCount <= slideTotal with slideOn cleared only after the ramp's last
step, which is what left the existing else if (c.slideRunning ()) c.endBend () release guards in allNotesOff
and stopNote and the (bendClip == clip.uid && wheelBent) || (slideClip == clip.uid && slideRunning ())
clip-end take-back unchanged, a ramp that reached its extreme still leaving the wheel at a point of the
column's own for those same points to take back. Each step is queued at the pitch wheel's own order 0
so it precedes the note-on of the row it sits inside, which is what a note entering while the wheel
climbs is bent by the point the climb had got to rather than by the one above it, and the position
arithmetic stays integer throughout so the wheel stays 14-bit while only the step's time is a double.
What the fixtures now show is the row's own sub-row steps landing inside a block boundary: a U 4 over
four rows sends 25 wheel messages rather than 5, its row-boundary values 10239, 12287, 14335 and kBendTopPos
intact beside the 341, 682 and 1023 filling in between them; its retarget fixture's two argument-only
rows moved to rows 1 and 3 so they sit inside their own sub-row ramps and re-arm from 11604 the sub-rows
had reached rather than from the whole-row point above it; its note-off a row below a D 4 now ends a
ramp that had already sent 7851, 7510, 7168, 6827 and 6486 rather than one that had sent nothing at all;
and its slide the clip runs out of sending 23 of its 48 sub-row steps rather than 3 of its 8 rows,
the clip's own end at 1.0 ppq falling between the naming row's own sub-row steps, that still-climbing
ramp being put away there exactly as before. CoreTest stayed at 296 checks with the six slide sections' asserted arrays
recomputed - one of them printing the actual ramp once to read the values back rather than guess them,
its expected 8945 beside the real 9045 being a mis-copy of the array rather than an engine miss - and
UiTest stayed at 652 with its slide section retyping the argument from 12 to 2 so its 12-value climb
stays the same twelve values over two rows rather than twelve rows, which is what proves the sub-row
timing while keeping every check count the sections around it lean on; the tracker's drawing, the typing
and the Settings rows were untouched, so both slide shots came back with the sha256 they had the round
before, /tmp/opencode/slide-u.png still c19a018bf14bb9e1ae5b306c2ba8a88aadda0ff4ef3c301e674d242c2c6382fc
at --slide=6 --slide-arg=12 and /tmp/opencode/slide-d.png still e1a76f3baf61481d2ee462be4c183d1d9551dbc00dd0588b8935fa217b9fe149
at --slide-down=10 --slide-arg=3, the two pictures being unchanged rather than a cheap proof of a new
one. Smoke stayed at PASS with 0 failures and 13 checks, its serialised state still 2392 bytes since
the ramp costs the saved form nothing of its own, against the bundle refreshed by rsync -a --checksum
over the bundle's DIRECTORY rather than its single .so, the built and installed binary both sha256
6d9b4362fcf76ca3c859c7617a977fbaa7984bbaa774b195acc2c31c7d6d61f0 with the sha the previous build carried
gone, as expected since the engine changed rather than the saved form, the portability read still
topping out at GLIBC_2.38 with GLIBCXX_3.4.32 and naming no sqrtf or atan2f relocation, the only
single-precision names the binary keeps being JUCE's own atan2, cosf, expf, logf, powf, sincosf and
sinf.
The argument field's OFF and STP round answers the asks about that field's own letters: an
s entered as a value stops the pitchbending and holds the wheel on the pitch it is at,
spelled STP rather than as a number, an 0 or an o entered as a value disabling the pitchbending
and resetting it, spelled OFF, and anything else that is not a number ignored rather than
written, leaving the cell's value as it was. An A call on the same tracker channel as a
pitchbend stops and resets that wheel rather than leaving it bent under the run, a U or a D
on that same channel after a call closes the run that channel was in while the same two
commands written on separate tracker channels of one MIDI channel work together rather than
one switching the other off, and an arpeggiator release entered as 0 or o goes off and
re-triggers the last base note at its own row rather than only cutting the sound. The
argument field took two sentinels above its own top rather than two more numbers inside it,
kArgOff = 128 and kArgStp = 129 in Model.h sitting above kCcTop = 127 and below kOlderCmdTop
= 143 beside the wheel's three command values, and cmdVal is already saved as a juce::int16
and read back with no clamp, so both spellings survive the saved form unchanged while the
read sends an older state's 128 and 129 to whatever they now mean to the call letter they
land on; clampValue (Field::cmdVal) stayed jlimit (0, 127, ...), maxDigitsFor and fieldGlyphs
stayed 3 for that field and the interpolation's own top for the argument stayed 127, so a
run of typed digits and a climbing command number can neither of them invent a spelling only
a letter can write. The engine took one new release point rather than a rule per kind:
PlaybackEngine::endWheel (events, ch, midiCh, offset) puts the wheel back at its centre when
it is bent and only puts a still-climbing ramp away when it is not, the note-offs of allNotesOff
and stopNote calling it instead of their own pair of guards, a row naming the arpeggiator
call in processRow calling it ahead of that call - which is what puts the wheel the same
column had left bent back at its centre there, a run having no wheel of its own to put right
- and a file-local answerBend lambda shared by the row's own B and by an argument-only row
under a carried B mapping an OFF argument to kBendCentreVal and returning for an STP one,
which is what leaves the wheel sitting on the point it is at for the rows under that row. The
const int midiCh hoisted above the command block is the channel every command answer takes.
slideCommand grew from an arm-only (ch, rowPpq, argument, up, clipUid, song) into
(events, ch, offset, rowPpq, argument, up, noteOnRow, clipUid, song) and works out that channel
from the instrument its own column carries, since an OFF row has to send a message of its own:
it closes the arpeggiator run that column was in first, closeArp plus endArp ahead of
everything else so only a bare slide row releases the note the run's last step entered while a
row that struck a note of its own had released it as that strike, answers an OFF with endWheel
plus clearing lastCmd and lastCmdVal and returning so nothing at all is carried below it the
way an arpeggiator's release leaves nothing, answers an STP with endSlide plus returning so the
letter keeps being carried and the point it holds needs no message of its own, and otherwise
arms the ramp with jlimit (1, 127, argument) rather than jlimit (0, ...) since the span 0 now
means the OFF above it. arpCommand gained a bool noteOffRow and a const bool releaseCall = id
== 0 || id == kArgOff read before id is clamped to kMaxArpMacros, since clamping first would
have landed that sentinel on the highest macro instead of on the release, and its release
branch reads the base it re-strikes from c.columnNote or c.arpCallNote before endArp takes them
away and strikes it again with startNote when the row is neither one that struck a note nor one
naming the column's note-off, that re-struck note's clip being the rows this clip owns. What
the tracker does with the same two spellings: a file-local commandArgumentReleases (cmd, cmdVal)
now names an OFF of any kind and a 0 read under a call or under a slide letter but never a 0
beside the bend letter, and both commandLeavesCarry and the implicit half of stepCommandCarry
ask it, so the column stops drawing a carried letter under a release however that release was
reached and keeps drawing one under an STP; a new commandKindAt (row, ch) names the kind a
row's argument belongs to, the row's own command where the row named one and carriedCommandNumber
(row, ch) where it did not, and applyValue (Field::cmdVal) rewrites a typed 0 to kArgOff when
that kind is a slide letter. The letter claim sits in the value-field entry ahead of the digit
path, an o or an O landing kArgOff beside a wheel or an arp kind and an s or an S landing kArgStp
beside a wheel kind only, a claimed write doing markChanged, advanceRow (jmax (1, stepSkip)) and
repaint the way a committed number does, and a letter neither claim owns falling through to
return false so the cell keeps its value and no note is typed in the note column beside it. The
drawing reads OFF for an arp release, for a kArgOff and for a 0 read under a slide letter, reads
STP for kArgStp, and keeps the colours it had, the three wheel kinds still sharing the lilac
theme::cmdOther. The new fixtures name the point rather than the shape: CoreTest went from 296
checks to 314 with sections of their own for an STP read under a carried U 4 stopping the ramp
after eleven sub-row steps at 11946 and holding the wheel there until the note-off below sends
the centre, an OFF spelled by a bare 0 read under that same carried letter sending the same
eleven values and leaving an argument-only row further down answering nothing, both spellings
read back out of the saved form and both sitting above kCcTop, an A 1 call on a column whose B
100 row had left the wheel at 12800 sending the centre as its own next message while the run's
steps below that row keep being measured from the note the column named, a bare U 2 row closing
the A 1 run its column was in with 12 note-ons of that run above the closing row, the note 72
its last step entered released three times over - the step above's transition, the stuck-key
guard's and the close at that row - and the release ahead of the note the column strikes below
the ramp being what leaves nothing of that run under it, and a U 2 in the second tracker channel
of the same MIDI channel leaving that run stepping on with every note of both commands reaching
the one channel the harness reads back as 1; the two arpeggiator-release fixtures moved to the
re-trigger's own counts rather than losing their shape, the A 0 kill sounding 19 note-ons
rather than 18 with h.notes.back() being the base 60 it re-struck rather than the run's last
step 64, and the release row hearing 13 note-ons rather than 12 with ons.back() being that same
60. UiTest went from 652 to 676 with the slide section's new sub-block typing the spellings
rather than reading pixels: a 0 beside a named D landing kArgOff with carriedCommandNumber (25,
0) coming back empty, commandKindAt (24, 0) naming that row's own letter, an s landing kArgStp
with the letter and that argument still carried over the rows below, an o under a carried U
landing kArgOff at a row that stays implicit with the read under it still empty, an unclaimed q
leaving both the cell and the note column alone, an s beside a call refused rather than written
while the argument the entry left stays in the cell, an o beside a call landing the release's
OFF spelling with no call carried below it, and every one of those writes spending exactly one
point of the undo line. EditorShot gained no flag since --slide-arg writes the cell's value
directly, so --slide-arg=128 and --slide-arg=129 name the two sentinels and its doc now says so
and that the narrowest slide a row can ask for takes one row; the two shots the round before
pinned came back byte-identical at c19a018bf14bb9e1ae5b306c2ba8a88aadda0ff4ef3c301e674d242c2c6382fc
at --slide=6 --slide-arg=12 and e1a76f3baf61481d2ee462be4c183d1d9551dbc00dd0588b8935fa217b9fe149
at --slide-down=10 --slide-arg=3, and this round made two new 1240x760 ones of its own, 5386fb80f9ccd0434cf122048522bb9e36e19eec98b8be9e50003b222afe5d26
at --slide=6 --slide-arg=128 showing the lilac U OFF painted at that row with the command
column under it carrying nothing and c1fc0fa130b2d570389be930639820e04f1655f1c1126be872751fda952a19bd
at --slide-down=10 --slide-arg=129 showing D STP named there with the letter and that spelling
carried dim over the four rows under it, both read back off crops of the tracker band rather
than off the whole-window pictures. Smoke stayed at PASS with 0 failures and 13 checks and its
serialised state at 2392 bytes, since the two spellings cost the saved form nothing of their own
and the argument was already a two-byte number, against the bundle refreshed by rsync -a --checksum
over the bundle's DIRECTORY rather than its single .so, the built and installed binary both
18450614c5e9862582472bd0a56e2f7261954d4ba86fe87877151ab4083c30a6 with the 6d9b4362fcf76ca3c859c7617a977fbaa7984bbaa774b195acc2c31c7d6d61f0
the previous round carried gone, as expected since the engine and the entry changed rather than
the saved form, and the portability read still topping out at GLIBC_2.38 with GLIBCXX_3.4.32 and
naming no sqrtf or atan2f relocation, the only single-precision names the binary keeps being
JUCE's own cosf, expf, floorf, logf, powf, roundf, sincosf and sinf; the build still warns only
of the PluginEditor.cpp -Wshadow pair and the hidden processBlock (double&) overload.
The row-0 note cut at a clip boundary is the stuck-key guard's second release of one
pitch beside the note-on one sample later, and that is the whole difference between the
shape that works and the one that does not: the report "sometimes row 0 note in pattern
being cut shortly if previous pattern ends with the same note playing. no cut if its
different note" is heard in Reaper, the note entered at that row being released right away,
over one pattern used twice back to back. A scratch trace put inside CoreTest named the two
shapes at the engine rather than at a host: the boundary of a 4-row clip of one channel
sits at absolute sample 24000, which is block 46 at offset 448 under that harness's 48 kHz,
120 bpm and 4 rows per beat - a row being 6000 samples of it and one 512-sample block
spanning 0.021333 ppq - and sent OFF 60, OFF 60, ON 60 all at that one sample when the clip
below entered the same pitch there, against one OFF then ON 62 when it entered another, the
first off being that clip's own end releasing its last row and the second the guard
re-articulating a pitch the synth may still be holding from a stop the host never delivered,
while in both shapes the row-0 note then sustained to that lower clip's own end. So the
engine's own ordering was already right and the only structural difference between the two
shapes was that duplicate note-off, which a host answering the sample rather than the order
inside one reads as the new note being opened at once rather than re-articulated. The fix
suppresses that duplicate rather than moving an event: ChannelState grew relNote / relMidiCh
/ relOffset beside activeNote / activeMidiCh / activeClip, naming the pitch, the MIDI channel
and the block-relative sample the last note-off this channel queued released, stopNote
recording those three with every release it queues - the clip's own end, a row's own note
change and the note an arpeggiator's release or a switched call takes away - process()
clearing them at the start of every playing block since a release only lives inside the block
being queued and a note entered at a same-numbered sample of the block below it still asks the
guard for its own opening, and the guard in startNote asking that record before it adds an
opening of its own, which is what leaves a note entered at the very sample one of that
channel's releases opened its own pitch at having that one release ahead of it and no second
one beside its note-on; reset() takes the three back with its own c = ChannelState ().
allNotesOff's own per-channel note-offs are deliberately not recorded, that block being the
one the transport hands over while stopped and may never deliver at all, which is why the
guard still re-articles there. What did not move is the ordinary one release plus one note-on
at one sample a note change inside a clip and a same-pitch re-trigger at a row already had,
neither of them reaching the record since the change releases a pitch other than the one
entered below it and a re-trigger sits at a sample no release of that pitch was queued at, or
the re-arm and loop-wrap allNotesOff ordering the fixtures above already pin. CoreTest went
from 314 checks to 319 with a section of its own, "A clip boundary re-entering the pitch the
clip's foot entered", placed ahead of the transport re-arm ones rather than beside the stuck-
note one: one 4-row pattern holding one pitch at its head row and at its foot row, a second
clip of that same pattern placed at the first's own end through patternLengthBeats, 100 blocks
played past that second clip's own end, the pitch entered at 4 places and released 4 times
rather than the 5 the guard's extra made before this fix, and its raw ordering naming the third
note-on of that pitch having exactly one note-off of it ahead of it rather than two - the shape
a note change inside one clip already sends - and nothing of that pitch right after it, so that
note still sounds over the rows below its row 0. The scratch trace that found the shape, one
pattern used twice back to back printed block by block with absolute sample positions and a
run per shape, was deleted once that section stood in its place. The plugin rebuilt with only
the warnings the tree has always shown - the PluginEditor.cpp -Wshadow pair, the hidden
processBlock (double&) overload, the -Wsign-conversion notes against the stuck-key guard's own
bitset test and set and the ui layer's deprecated and unhandled-switch ones - and CoreTest 319,
UiTest 676 and Smoke 13 all came back PASS with 0 failures, the serialised state still 2392
bytes since the three numbers are engine state rather than saved state, against the bundle
refreshed by rsync -a --checksum over its DIRECTORY rather than its single .so, the built and
installed binary both sha256 42f217c3d7f32a9f6cadb8284e2416c599ddf784d025bd91ce3138799e88ef94
with the 18450614c5e9862582472bd0a56e2f7261954d4ba86fe87877151ab4083c30a6 the previous round
carried gone, as expected since only the engine changed. The change is engine-only, so no new
shot was invented and the four pinned ones came back byte-identical rather than a fifth being
made: /tmp/opencode/slide-u.png still c19a018bf14bb9e1ae5b306c2ba8a88aadda0ff4ef3c301e674d242c2c6382fc
at --slide=6 --slide-arg=12, /tmp/opencode/slide-d.png still e1a76f3baf61481d2ee462be4c183d1d9551dbc00dd0588b8935fa217b9fe149
at --slide-down=10 --slide-arg=3, /tmp/opencode/slide-off.png still 5386fb80f9ccd0434cf122048522bb9e36e19eec98b8be9e50003b222afe5d26
at --slide=6 --slide-arg=128 and /tmp/opencode/slide-stp.png still c1fc0fa130b2d570389be930639820e04f1655f1c1126be872751fda952a19bd
at --slide-down=10 --slide-arg=129, the four pictures being the proof that nothing about the
drawing, the typing or the layout moved. The portability read still tops out at GLIBC_2.38 with
GLIBCXX_3.4.32 and names no sqrtf or atan2f relocation, the single-precision names the binary
keeps being JUCE's own cosf, expf, floorf, hypotf, logf, powf, roundf, sincosf, sinf and tanf,
and the release record is three ints of engine state so it asks nothing of either library. The user's
own check of that boundary, one pattern placed twice back to back, has since found no abrupt note cut
there in the host the report was seen in, which is the one thing the suites could not show at all, so
that shape is gone where it was reported rather than only in the fixture, and what remains unheard is
the rest of that boundary's own shapes rather than that one - a different pitch there, a note entered
at the row above it, a stop sitting exactly at it, a loop wrap of one pattern placed twice back to
back, which takes the releases away through allNotesOff rather than through that clip's own end, and
the moved-clip-from-time-0 stuck-note scenario - beside Bitwig's and Carla's hearing of one fewer
note-off of one pitch at one sample.
The vibrato macros and the wobble they run landed (commit 579e210): the model grew a `VibMacro` beside
`ArpMacro` and a grow-only `std::vector<VibMacro> vibs` beside `macros`, with `kVibCmd = 147`, the shape
top `kVibTypeTop = 5`, the amplitude top `kVibAmpTop = 100` (a hundredth of full pitchbend each), the
cycle top `kVibFreqTop = 127` subticks, the fresh macro's `kVibDefaultAmp = 50` and `kVibDefaultFreq = 8`
defaults and the `firstUndefVibId` / `lastDefVibId` pair the arp one already had; `VIBS`/`VIB` with
`id`/`type`/`amp`/`freq` props was added to the saved form next to `ARPS`, both sides of it emptied before
the read so an undo that took a macro away leaves it undefined rather than holding its knobs, and `reset()`
now defines vibrato id 1 the way it defines arp id 1, which is what the VIB MACRO tab has a card for. The
engine grew `flushVib`, `vibCommand` and the static `vibShape` over the six curves, and `ChannelState` its
`vibOn` / `vibId` / `vibClip` / `vibOrigin` / `vibCount` / `vibRowEnd` / `vibBase` / `vibCycle` / `vibPoints`
/ `vibHeight` fields plus `endVib` and `vibRunning`, the swing being measured on `max (cycle,
kVibCyclePoints = 8)` counted subticks about `c.lastBendPos`, its height `amplitude * kBendCentrePos /
kVibAmpTop` so an amplitude of 100 is the wheel's own half-height, its points clamped to `[kBendBottomPos,
kBendTopPos]` and only a point that changes the wheel's position saying anything, the `Up` kinds negating
the shape once `phase >= 0.5` and the `Down` kinds while `phase < 0.5`, and one `std::sin` or the triangle's
own ramp per point rather than a table of shapes. The clip-gone loop now names `c.arpClip` then `c.vibClip` when a column has
no active note, which is what a wobble armed with no note of its own takes back at a clip's end. The UI
grew `Source/ui/VibMacrosPanel.h/.cpp`, a card of one `juce::ComboBox` over the six spellings and two
`juce::Slider`s per macro with the tab's own `vibShapeLabel` and `vibAmplitudeLabel` readers, its scrollbar
10 px in from its right edge answering the wheel and a grab, and its `+ Add macro` / `- Remove selected`
pair; the editor's tab enum grew `vibs` and the tab buttons now read `ARP MACRO` and `VIB MACRO` 10 px
wider than the plain ones, with `wireUndoRedo`, `wireClipboard`, `wireSlice` and `onModelChanged` given
the panel so all seven walk one point per press and a tab that is not showing stays deaf, the tracker
claiming `V` as `kVibCmd` in the command number field only, drawing the `V` pair in `theme::cmdOther`
lilac, reading a `0` argument there as the `V OFF` its release spells, refusing an `s` beside the letter
in `commandArgumentReleases` and telling a command row's kind through `commandKindAt`, and the pool's
grid preview grew its fourth wheel tick. Rebuilt with no new warnings, the .so at sha256
046b3e93927f0fe8319bf95efce70a4dbecb56c63bf7b00e5a1b6abd89cb1cfc and the installed bundle refreshed
by rsync -a --checksum over its directory rather than its single .so, so the installed copy matches
that hash while moduleinfo.json stayed at b5db3e6f2540699356e34c5362666e2fe3340f554d5e211bd523290dca1e83ee
since nothing about the plug-in's identity changed. CoreTest went from 319 to 379 checks PASS with 0
failures, its six new sections pinning the model, the grid, the six shapes, the point a wobble swings
about, the release, the STP and a row retargeting one another and a macro taken away mid-swing; its
wobble armed with no note of its own takes all 16 bends of its two cycles out over the clip's two rows,
13985/16383/13985/8192/2399/0/2399/8192 twice, the whole run the gone-clip fallback was needed for,
since without it that loop named no clip for such a column at all, ended the swing and sent nothing
of the wheel, so the same fixture came back with no bends at all. The cycle-one fixture at a quarter
of full pitchbend repeats the same eight points six times inside one row and its last point leaves
the wheel at 6744 rather than back at the base, which is what makes the 48th message the note-off row
below taking that point back to the centre rather than one more point of the cycle; UiTest went from
676 to 734 checks PASS with 0 failures, its new VIB MACRO section driving the ComboBox through
setSelectedItemIndex with juce::sendNotificationSync and the two knobs through their text boxes,
reading the six spellings back through the panel's own vibShapeLabel rather than a second copy of
what the tab draws and the amplitude through vibAmplitudeLabel (25) == 0.25, and asserting the card's
three settings as three undo points against the depth the Settings tab's Undo steps row asks for,
which is why its depth checks read the line back as juce::jmin (before + n, song.settings.undoSteps)
rather than one more than it can hold; Smoke stayed at 13 checks PASS with the serialised state grown
from 2392 to 2463 bytes since the VIBS section is saved state rather than the engine's three numbers.
Five shots pinned it, each 1240x760: /tmp/opencode/arptab.png 991fffe6c52af501f45c55dd2e708b72a0b07bfaedf06737ac06fcbc0bbf6443
with the tab button now reading ARP MACRO rather than MACROS, /tmp/opencode/vibmac.png 21a9117c19c6e2c6741cba068b82bf3f109d1b75e319b28025c19b6973720534
showing one card whose dropdown is picked out at Triangle Up and whose knobs read 0.75 and 12,
/tmp/opencode/vibmany.png showing 25 macros of which 22 are laid out at 116x22 and 3 hidden at 0x0
rather than drawn past the window, the strip's thumb at the far end of its track with V 25's own card
clipped at the bottom edge, and /tmp/opencode/vibletter.png 82f2b95c2f7f539e2cbbb987e57a64fe38e221a64121401b8cbc036d67dc6de3
/ /tmp/opencode/viboff.png 7580fec80bf3b70b5fe3720a2762b51e4e6595367b3b5bbf1f560384d79de305 showing
the tracker's lilac V 1 at row 6 carried dim over the rows under it and the same row spelled V OFF
with neither letter nor argument drawn under it; the baseline /tmp/opencode/vibnone.png re-taken with
no vibrato flags holds zero pixels of that exact lilac 0xffd8b4e8 and none within twelve of it, while
vibletter holds one exactly it and three more within twelve - the letter's glyph being drawn over a dark
plate so most of it blends with the background rather than spelling the colour - and viboff holds eight,
being the release's three-glyph OFF in that same colour, so the lilac those pictures hold is drawn by the
vibrato's own letters rather than by anything the demo's other rows already had. Portability
was re-checked rather than assumed: max GLIBC_2.38 and GLIBCXX_3.4.32 still, the four names the
new curve leans on - sin, sincos, sinf and sincosf, all at GLIBC_2.2.5 - already sitting in the older
build-instrument artefact's own dynsym beside them, so the curve asked for no symbol of its own, and
sqrtf and atan2f are still not asked for by name.
Arpeggiator first tick and the Enter key's focus (commit c22eba1): the note entered on the row that makes
an arpeggiator call is that run's first step, sounded at the pitch the first step moved it to at the call's
own subtick rather than one subtick below as it was typed, since the engine checks the macro that call arms
before that note is triggered - processRow's note block reads the call that row arms through a new file-local
arpCallArmedAtRow (const Cell&, int lastCmd) and its first step through a new arpFirstStepOf (const Song&,
int id) beside a new arpSteppedNote (note) that the octave snap left the step loop as, an empty first step
leaving the note where it was typed and a first step of the note column's own note-off sounding none of it
at all, state.arpNote taking the stepped pitch as the run's own the way any other step's does while a resting
first step leaves it at -1, and arpCount still starting at 1 for a row that named a note so the steps below
that row stay their own. flushArp's note-off branch now opens whatever the run holds at its subtick under
the guard c.activeNote >= 0 rather than the older c.arpNote >= 0 && c.activeNote == c.arpNote - that older
guard could not say anything at a run's head, where arpNote had never been set - and sets c.arpNote = -1
rather than ending the run, so a step entered as the note column's own note-off works as a rest at a run's
first tick and at its second both with a note beside the call and without one, the note entered at the call
row being that first step when the call's own row named one and the note the column's rows above named when
it did not, the run's steps under the rest measuring from the note the column still names, and the release
following only a note that run entered rather than cutting whatever another column or another run was
holding. CoreTest grew two fixtures for the two cases the report named, "A note-off step at a run's second
step opens the note entered at the call row" - an empty first step sounding the note as typed at the call's
own subtick, the note-off step under it opening it at its own rather than letting the stepped step re-articulate
it, and that rest's release being the run's own rather than the clip end's, leaving the note the stepped
step sounded held to the clip end - and "Empty steps and a note-off step under a call that row has no note
for", the same at a call whose own row entered no note, and three existing fixtures were repaired for the
new first step: the two whose offsets were counted from the note entering under the call no longer hear it
as typed once the first step names a jump, and the looped cycle now steps back onto that first step, so its
cycle counts the note entered at the call row as a step it passes through rather than as a note held before
it. The Enter key fires no button and flips no checkbox any more: the editor grew a file-local
clearButtonKeyboardFocus (Component&) that sweeps its whole component tree for Buttons and is called in
setTab after resized () and repaint (), which is what catches the tab buttons, the top bar's snap checkbox
and zoom pair, the +/- pair of every knob a panel built under the default look before the constructor's body
put the plugin's own look in force, and the fresh buttons and knob pairs a card brings with it every time its
macros are added to and taken away, while the theme's new createSliderButton override takes that focus away
from the +/- pair a Slider builds later, while that look is in force; setWantsKeyboardFocus (false) sits at
both macro tabs' Add and Remove pair, every card's LOOPED tick and every Settings row's checkbox, and
setWantsKeyboardFocus (true) was put back at every knob creation site - the bottom bar's OCT / STEP /
CHANNELS sliders, the tools' transpose amount, the Settings rows' sliders and both macro tabs' card knobs -
since Slider::init forces a Slider not to want that focus of its own. The ARP MACRO, VIB MACRO and Settings
panels want the keyboard focus and the editor's setTab grabs it for the tab that is showing beside the
tracker's own grabKeyboardFocus, and those three panels spend the returnKey in their own keyPressed rather
than handing it up. The Model.h ArpMacro paragraph, the PlaybackEngine.h ChannelState's arp paragraph and
the two engine comments on the note-off step now spell one rule rather than three: the column keeps naming
the note a rest opened, so the steps under the rest measure from it again. UiTest's focus section was
rewritten for what can be read headlessly - the three checks that leaned on
getCurrentlyFocusedComponent were dropped, since Component::takeKeyboardFocus no-ops without a ComponentPeer
and that harness creates no desktop window so nothing is ever focused there - and replaced by a checkNoButtonFocus
lambda beside a new collectButtons helper asserting that no Button inside the editor's tree, inside a showing
Settings, ARP MACRO or VIB MACRO panel or inside a knob wants the focus, that every knob wants it, that a
knob's own +/- pair is there and holds neither of them, and that each of the three panels spends a repeat of
the Enter leaving no checkbox flipped, no macro added or cleared and the undo line alone. The live sections
were reconciled to match: the Tests' CoreTest bullet now spells the first step sitting at the call row
itself, the note as typed never reaching the host when that step names a jump, the note-off step's own
openings and the looped cycle stepping back onto that first step, its UiTest bullet the focus model, the
MIDI model's first arpeggiator bullet the macro being checked before the note entered at the call row is
sounded, its note-off bullet a head OFF being a rest over the note the rows above left held and its opening
being the run's own rather than the clip end's, and the Keyboard map growing a paragraph for the focus model,
the ComboBox's own Enter and an AlertWindow's buttons being left alone. CoreTest 388 checks, UiTest 749 and
Smoke 13 all PASS with 0 failures; the built .so is unchanged in bytes at sha256
beffd594d7993f0cfef2a1d09130a6888e93ae8d1efd0852f605976087b8bc60 since the round's last edits were comments
alone, the installed bundle at /home/corthax/.vst3/Corthax/Re.Trektor.vst3/ being byte-identical to the built
one under diff -r rather than needing another rsync, moduleinfo.json unchanged at b5db3e6f2540699356e34c5362666e2fe3340f554d5e211bd523290dca1e83ee,
and the four pinned 1240x760 shots being /tmp/opencode/shot_tracker.png 938d8f49f8efea8a4928b2a10e9c604199294fd4fb91e1148c22201e15f250af
taken with --dump --arp-call=8 --arp=0,4,off,8 and showing row 008 as C-4 0 96 -- A 1 with the call beside
the note the same row enters and its A 1 carried dim through the rows under it, /tmp/opencode/shot_macros.png
aa47556ccc5764ad10dfc7f82179826a42cfeff7f4d6b095e5b9f5f471528657 taken with --tab=macros --arp=0,4,off,8
--arp-loop and showing one card at LENGTH 4 with its LOOPED tick set and its strip spelled 0 +4 OFF +8 with
the OFF in the release's own colour, /tmp/opencode/shot_vibs.png eaae15bca7fc617339eb1a7cdfb5fbb539c9089a422a11ede388d2188a0b48bf
at --tab=vibs --vib-shape="Triangle Down" --vib-amp=25 --vib-cycle=6 --vib-call=8 and /tmp/opencode/shot_settings.png
4d1df0bc31c7f79ddfcd201512faacf801304fa96c67e88e5d1415ead43f4a36 at --tab=settings with its 11 rows, Undo steps
at 50, Volume column CC at 7 and Pan column CC at 10; their dumps read 32, 28, 26 and 14 OUTSIDE PARENT lines
all being the hidden ScrollBars and the zero-sized children of a hidden panel rather than anything clipped
out.
Undo steps row made draggable and a drag along a row made one undoable action (commit
4ecf72ec27fce8118919fc3e72242ff5354a01c9): setIncDecButtonsMode
(juce::Slider::incDecButtonsDraggable_Horizontal) sits at all six Slider creation sites - the Settings rows'
own addSlider, a card's LENGTH knob, a wobble card's amplitude and cycle pair, the transpose amount and
PluginEditor::setupBottomSlider, which builds the bottom bar's OCT / STEP / CHANNELS trio - and the undo line
hears a gesture's two ends from the knob's own onDragStart and onDragEnd at the five sites whose rows reach
the model, so a drag over the Undo steps row changes the number that row shows, feeds song.settings.undoSteps
and caps how far a following Ctrl+Z can reach, one drag being one point of the line however many mouseDrag
events walked it there: pushUndoPoint appends the point the drag's first change made, moves that same point
along with replaceUndoPoint for the events after it, and endKnobDrag calls popUndoPoint to drop it when the
drag ended on the number it began at. UiTest grew a fixture beside the depth the Settings tab asks for that
grabs that row's knob as one mouseDown, several mouseDrags and one mouseUp and pins that the drag moves the
number the row shows and feeds the depth behind it, that the depth rises by exactly one for the whole drag
rather than one per event, that one Ctrl+Z takes the whole drag back to the number the row had before it, that
the way back is a second action of its own leaving the number where the round trip began rather than the
events between the two numbers being spent one at a time, that two presses of Ctrl+Z walk both drags and the
depth back as they were, and that a click of the row's own + button still costs one step of its interval and
one point of the line - the drag being a way in rather than the only one - and puts the tab back on the
tracker so the sections below still read its grid; the fixture's own loop over the pointer's path is bounded
by x < toX and x > toX rather than by x != toX against a float, which hung a run at a 600 s timeout. CoreTest
388 checks, UiTest 760 and Smoke 13 all PASS with 0 failures; the built .so changed bytes this round at sha256
1207c0862ca3159cf5069bca5c62c7f80a1c4426d2bce75dfe9f5790cb20aa16 - the drag wiring being code rather than
comments - so the installed bundle at /home/corthax/.vst3/Corthax/Re.Trektor.vst3/ was refreshed by rsync -a
--checksum over its whole directory rather than its single .so and the diff -r between the built and installed
trees reads clean rather than needing a second rsync, moduleinfo.json unchanged at
b5db3e6f2540699356e34c5362666e2fe3340f554d5e211bd523290dca1e83ee; the four pinned 1240x760 shots came back
byte-identical to the hashes the previous round pinned - /tmp/opencode/shot_tracker.png
938d8f49f8efea8a4928b2a10e9c604199294fd4fb91e1148c22201e15f250af at --dump --arp-call=8 --arp=0,4,off,8,
/tmp/opencode/shot_macros.png aa47556ccc5764ad10dfc7f82179826a42cfeff7f4d6b095e5b9f5f471528657 at --tab=macros
--arp=0,4,off,8 --arp-loop, /tmp/opencode/shot_vibs.png
eaae15bca7fc617339eb1a7cdfb5fbb539c9089a422a11ede388d2188a0b48bf at --tab=vibs --vib-shape="Triangle Down"
--vib-amp=25 --vib-cycle=6 --vib-call=8 and /tmp/opencode/shot_settings.png
4d1df0bc31c7f79ddfcd201512faacf801304fa96c67e88e5d1415ead43f4a36 at --tab=settings - their dumps still reading
32, 28, 26 and 14 OUTSIDE PARENT lines, all of them the hidden ScrollBars and the zero-sized children of a
hidden panel, which is what says the drag mode changes no paint at all.
Active
Nothing is open in the code: a number widget's row is draggable and one drag along it is one action of the
undo line end to end, at the editor and at UiTest. What is left is what a host hears rather than what this
build has to write: whether the pointer path a host moves along a row is heard as the several mouseDrags the
harness hands over the +/- pair and the row's own width or as one for the whole path, the plug-in earning one
point of the undo line either way since the line hears the gesture's two ends rather than counting its events;
and whether a host routes a Return to a plug-in panel's own text editors at all, the keyboard focus being
unobservable headlessly since Component::takeKeyboardFocus no-ops when getPeer () is empty and no harness here
creates a desktop window.
Blocked
No Reaper installed; the only host on this machine is /usr/bin/carla, so this round's drag is verifiable at
the editor and at UiTest - one mouseDown, several mouseDrags and one mouseUp handed to the Settings tab's Undo
steps knob moving the number that row shows, feeding song.settings.undoSteps, raising the depth by exactly one
for the whole drag rather than one per event, one Ctrl+Z taking the whole drag back to the number the row had
before it, the way back being a second action of its own and the row's own + button still costing one step of
its interval - rather than by what a host's own pointer path along the row hears, and a host that sends one
mouseDrag for the whole path rather than one per movement of the pointer is what has to confirm that the
coalescing earns one point of the undo line rather than one per event of its own. The keyboard focus remains
unobservable in UiTest rather than failing: Component::takeKeyboardFocus no-ops when getPeer () is empty and
no harness here creates a desktop window, so whether a host that holds the focus on one of these panels routes
a Return to that panel's own text editors rather than to a Button a click left there is what a host has to
confirm. The .so changed bytes this round at sha256
1207c0862ca3159cf5069bca5c62c7f80a1c4426d2bce75dfe9f5790cb20aa16 rather than staying put, since the drag
wiring is code rather than comments: the installed bundle and the built one being byte-identical under diff -r
and moduleinfo.json unchanged at b5db3e6f2540699356e34c5362666e2fe3340f554d5e211bd523290dca1e83ee is what says
the hosts that scanned the previous bundle still see one plug-in rather than a renamed one. The host floor was
left as the previous round recorded it rather than re-read against this build: the round asked for no symbol,
no math and no include beyond what the tree already had, so GLIBC_2.38 and GLIBCXX_3.4.32 still stand as what
the pinned Bitwig 6.1.4 Flatpak root can load rather than as a floor confirmed for this .so again. The warning
set is the tree's own rather than a new one: the hidden processBlock (double&) overload, the two
-Wsign-conversion notes at PlaybackEngine.cpp:104 and :112 against the stuck-key guard's bitset and the unused
song parameter of arpCommand at PlaybackEngine.cpp:366, that file not having been recompiled this round so
those notes stand as the previous round recorded them rather than being re-emitted, and the -Wshadow pair now
at PluginEditor.cpp:371 and :397 rather than the 354 and :380 the previous round recorded - the bottom bar's
CHANNELS handler and the pattern name editor's own onReturnKey, those lines having moved down by the drag
wiring this round added to that file; this round added no warning of its own. A shot's dump reads the hidden
ScrollBars and the zero-sized children of a hidden panel as OUTSIDE PARENT lines rather than as cards clipped
out, which is why the four shots report 32, 28, 26 and 14 of them rather than a shortfall of cards - the same
counts the previous round pinned, which is what says the drag mode changes no paint at all. The tracked
Re.Trektor.vst3.tar.gz the tree carries from commit 432dd55 is stale against this build rather than being
something this round left open in the code - it was cut from an older .so - and re-cutting it is the user's to
ask for rather than something a round does on its own.
Next Move
Commit this round's source as one commit - 4ecf72ec27fce8118919fc3e72242ff5354a01c9, the eight Source/** and
tests/UiTest.cpp files the drag round touched - and its docs as a second commit naming both that hash and the
previous round's c22eba1978b1d51bc990b80c26af6e978bd5ba52, since the README carries both rounds' splices and
its live sections now describe both the arpeggiator and the drag, rather than one commit for both, and never
push: origin/master sits at 432dd55 Create Re.Trektor.vst3.tar.gz, so the local tree is ahead of it by the
previous round's source at c22eba1, this round's source at 4ecf72e and the docs commit this paragraph is
being written for - three for the user's own push to spend - and a round that pushes spends a decision that
was never asked for. The tracked tarball cut at 432dd55 is stale against
this build rather than being silently re-cut here. Then ask the user for the next feature rather than
inventing one: the Command column's five letters, the arpeggiator's own first step, the note-off step's
openings, the two macro tabs, the undo and redo pair with a drag now being one action of its line, the Alt
clipboard and slice steps and the keyboard-focus model are all landed and pinned, and what remains of this
round belongs to what a host hears rather than to code this build still has to write. Ask the hosts the one
question this round cannot answer at the editor layer, since the editor only says what it does with the events
it is handed: whether a host's own pointer path along a knob's row is heard as several mouseDrags along that
row or as one for the whole path, the plug-in earning one point of the undo line either way since the line
hears the gesture's two ends rather than counting its events. Ask the hosts the fourth the Enter work cannot
answer at that layer either, since no focus was ever observable in UiTest: whether a host that holds the
keyboard focus on a plug-in panel routes a Return to that panel's own text editors rather than to whatever a
click left holding it, the plug-in having taken that focus away from every one of its Buttons so the press
should land on the field it was typed into wherever the pointer had last been. Keep the pinned bundle's host
checks alive rather than dropping them because a round made a knob draggable: the installed .so and the built
one being the same bytes at sha256 1207c0862ca3159cf5069bca5c62c7f80a1c4426d2bce75dfe9f5790cb20aa16 and
moduleinfo.json unchanged at b5db3e6f2540699356e34c5362666e2fe3340f554d5e211bd523290dca1e83ee is what says the
hosts that scanned the previous round's bundle still see one plug-in rather than a renamed one. Keep the
round's ritual for whatever comes next: implement, build with cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
and cmake --build build -j"$(nproc)", run CoreTest, UiTest and Smoke against the built bundle path given
explicitly rather than the harness's own default - UiTest, Smoke and EditorShot needing DISPLAY=:0 - take the
EditorShot screens the new layer needs and read their dumps rather than their pixels, refresh the installed
bundle by rsync -a --checksum over its whole directory rather than its single .so - the diff -r between the
built and installed trees being what says it landed rather than a second rsync - reconcile the README's live
sections against what the files hold and commit source and docs separately without pushing.
Relevant Files
/media/data2/GameDev/Plugins/ReTrektor/CMakeLists.txt: JUCE plugin build (VST3, MIDI effect).
/media/data2/GameDev/Plugins/ReTrektor/Source/Model.h / Model.cpp: core data model, song/pattern/clip ops, snapping, slicing, paste, serialization, per-channel column visibility bytes (Pattern::vis), the volume/pan CC settings and the interpolation of one column between the values that already exist (InterpField, Pattern::interpolate taking a std::function<bool (int row, int ch)> mask so the selection can be any shape and a cell outside the mask ends the anchor run), and the cell clipboard (CellClipboard holding one {row, ch, Cell} entry per copied cell with its offset inside the copied rectangle, the kPasteNote/kPasteInst/kPasteVol/kPastePan/kPasteCmd/kPasteCmdVal bits and kPasteAll naming which fields reach plus kPasteAdditive = 64 as a mode bit added to a filter rather than one more field, Pattern::pasteCells writing them at an anchor and, when that bit is set, masking each copied cell's own non-empty fields under the working mask so an empty field of the buffer leaves the target's value in place and a cell carrying nothing at all writes nothing, still skipping a target past length or channels so a paste never grows the pattern), and the Undo steps setting (Settings::undoSteps, default 50 with kMaxUndoSteps = 200 as the cap, written and read back clamped with the rest of the SETTINGS block so the depth a host reloads belongs to the song, the same block having lost the pattern paste's default property on both the write and the read back along with its Settings member, which leaves a song saved before the drop reloading with nothing to read there, this block also losing the hex display's property on both the write and the read back with its Settings member and the Settings tab's row for it gone, which is what lets an older state that still spells that choice reload with nothing to read it into and leaves every value the model holds the one decimal spelling valueString and instString draw it in, a shorter number padded with spaces so its digits land in the same columns as a longer one's), and the arpeggiator macros (ArpMacro holding signed semitone steps off the base note, kArpCmd = 128 as the command value the letter A enters and kMaxArpMacros = 127 naming the id cap with id 0 the release call, kMaxArpSteps = 64, kArpEmptyStep = -128 as the step that sounds nothing and, this round, as the value every step of a newly defined macro holds and every step growing one adds, the steps being one std::int16_t each now that a step spells the note column's own note-off as the new kArpNoteOffStep = -129 below the new kArpTopStep = 127 a typed jump is clamped to so the two spellings below -128 are the only values a step can hold that are not jumps, kArpDefaultSteps = 8 naming only how many of them a macro starts with while kArpDefaultJump was deleted outright since nothing builds a shape any more, Song::arps one grow-only table indexed by id - 1 beside the pattern rows and channels the build already treats that way so clearing a macro keeps the room its steps took and defining it again restores them, findArp growing it while peekArp reads it without growing it so the UI, the engine and the writer never invent a macro, arpDefined / arpLooped / firstUndefArpId / lastDefArpId / defineArp / clearArp / setArpLength clamped 1..64 and padded with kArpEmptyStep rather than with the base note, so a macro's shape is only ever where it was entered / setArpStep, which this round keeps the two spellings below -128 as sentinels and clamps only a jump to ±kArpTopStep so the opening passes through it and the octave snap untouched on either side / setArpLooped naming it, the ARPS section writing a macro's id, loop flag, length and its steps as one base64 block rather than one property per step - this round two bytes a step little endian, since the opening sits below what one byte can name, and the read telling the two spellings apart by the length the block declares rather than by what it finds, since a one-byte block is exactly as long as half a wide one's, so an older state's steps widen sign extended with their jumps, their steps that sound nothing and their absence intact, an older state's MACROS section dropped outright on the read since those kinds are gone, an older row's command in the 128-143 range landing on kArpCmd while the argument it holds stays the argument
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
Beside them the two slide letters (kOlderCmdTop = 143 as the highest command value an older build
could write for its M0-M15 macro kinds, kSlideUpCmd = 145 and kSlideDownCmd = 146 as the values the
letters U and D enter, both chosen to sit above that top so no older row can already be a slide while
this build's own three wheel values read back as themselves and the read rule went from rawCmd >= kArpCmd
to rawCmd >= kArpCmd && rawCmd <= kOlderCmdTop on the pattern data's one reader, and kSlideDefaultSpan = 8
as the row count a letter leaves as the argument its entry has to write, since a slide's argument counts
rows toward an extreme rather than naming a point on that 14-bit wheel, the Cell::cmd comment now naming
the four kinds a column can hold - a plain CC, the call, the bend and a slide - beside a value-space comment
saying a slide's argument is a span of rows rather than a position on it).
Beside them the argument field's two letter spellings (kArgOff = 128 as the OFF the entry writes for an o and
kArgStp = 129 as the STP it writes for an s, both sitting above kCcTop = 127 rather than among the numbers that
field can spell so no run of typed digits can invent either, clampValue (Field::cmdVal) staying jlimit (0, 127,
...), maxDigitsFor and fieldGlyphs staying 3 for that field and the interpolation's own top for the argument
staying 127, so a typed number and a climbing command number can neither of them reach a spelling only a letter
can write; cmdVal was already saved as a juce::int16 and is read back with no clamp, so both spellings survive
the saved form unchanged while the read sends an older state's 128 or 129 beside a call to whatever they now
mean to the call letter that number lands on; the Cell::cmdVal comment naming them beside the macro id, the
point on the wheel, a slide's span of rows and a vibrato macro id, and the value field's own doc paragraph now saying that two of
its spellings name the wheel's state rather than a number).
The same model grew the vibrato macros the letter `V` calls: a `VibMacro { bool present; int type; int amplitude; int frequency }`
in a grow-only `std::vector<VibMacro> vibs` whose `present` flag is what says a macro exists rather than any of
its three settings being nothing of its own - a card exists whether or not a knob is turned, so a macro whose
amplitude is at off is still a macro rather than no macro at all, and clearing one says it has none rather than
erasing the shape, the amplitude and the cycle its knobs had, which is what re-defining it then starts over from
and what makes a wobble whose macro was taken away answer by being ended rather than by swinging at nothing;
`kVibCmd = 147` as the command value the letter enters - above `kOlderCmdTop = 143` and `kCcTop = 127`, so no
older row and no typed digit run can already be one - and `kMaxVibMacros = 127` as the ids a row can name while
the id 0 call stays the release rather than becoming an id, `kVibTypeTop = 5` over the six shapes spelled `kVibTypeSin`
/ `kVibTypeSinUp` / `kVibTypeSinDown` / `kVibTypeTri` / `kVibTypeTriUp` / `kVibTypeTriDown` so the model's own
names say which half of a cycle is folded rather than the tab's spellings having to be read back, `kVibAmpTop = 100`
as hundredths of full pitchbend, `kVibFreqTop = 127` as the widest cycle a macro can ask for and `kVibDefaultAmp
= 50` / `kVibDefaultFreq = 8` subticks as what a freshly defined macro starts at - the amplitude being a share
of the wheel's own half height rather than a number of its own, which is what makes a knob at full reach the
wheel's own extremes from a swing sitting at the centre and the tab spell it as that share rather than as the
hundredth the model keeps, and the cycle counted in whole subticks on the same grid an arpeggiator's steps and
a slide's ramp use rather than over the rows, so neither of the two knobs is a number a row's own argument can name;
`vibDefined` / `vibType` / `vibAmplitude` / `vibFrequency` answering nothing while a macro is absent or an id is
out of room so the three setters refuse an id nothing entered rather than answering with a shape, an amplitude
or a cycle of their own, `setVibType` / `setVibAmplitude` / `setVibFrequency` clamping at their own tops rather
than wrapping - the cycle clamping up to one subtick rather than down to nothing, which is what a wobble whose cycle
knob was dragged to off swings on and what keeps a cycle's width from being nothing at all it would have to divide
by - `defineVib` and `clearVib` being the tab's two actions, `firstUndefVibId` / `lastDefVibId` naming the gap a
fresh macro takes and the id `- Remove selected` falls back to while nothing is picked out, `findVib` growing the
table only to name an id and `peekVib` reading it without growing it, and `reset ()` defining vibrato id 1 the way
it defines arp id 1 so the VIB MACRO tab has a card the way the ARP MACRO tab has steps; the saved form growing a
`VIBS` section of one `VIB` child per macro that is present with `id` / `type` / `amp` / `freq` props beside the
`ARPS` one rather than one entry per id the table has room for, and both sections emptied ahead of the read - the
`VIBS` read emptying `present` over the whole table ahead of filling it the way the `ARPS` read already did, since
that section names every macro that exists, which is what hands an undo an earlier state's own absence rather than
the settings its knobs had behind the state's added - the read clamping a saved state's own three values through
the same limits the knobs use so a state cannot reach a shape, an amplitude or a cycle outside what the tab can ask
for, naming a macro rather than leaving it an entry and, when the state named no vibrato at all, defining
the one default macro the way a fresh song does - which is what an older state reads in with rather than
with an empty tab and what a state that still had another macro defined leaves at its own ids rather than
having the read put the default one back.
 and this round's wording of the arpeggiator step that is the note column's own note-off, the ArpMacro
paragraph now saying that such a step releases whatever the run is holding there - the note it last entered
when it has already sounded one and the note the column had entered above the call when it has not - and
that the column keeps naming that note so the steps under the rest measure from it again rather than naming
a note the step above happened to have entered, one rule the model, the engine's comments and the README
each spell in their own words rather than three rules that had started to disagree about a run's head.
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
boundaries stay exact rather than drifting, flushArp answering a step spelled as the note column's own
note-off, kArpNoteOffStep, by releasing only the note this very run entered - its stopNote on the state's
own arpNote and only while the state still holds that very note - then sounding nothing and still counting
the step, so the run stays where it was and its cycle boundaries stay exact beside a loop while nothing a
column, another run or the row itself was holding is cut by a step, which is also why a step of the opening
at a run's head releases nothing of its own since the column's own note is still owned lower down and takes
it away with the row's note-off, and why the note an opening cut is not sounded again until a later step
names one; arpCommand answering the command value kArpCmd with
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
And the wheel slide its letters U and D drive: kBendBottomPos = 0 named in the header beside kBendTopPos
as the wheel's own ends, a ChannelState holding slideOn / slideOrigin / slideSpan / slideTotal / slideCount /
slideFrom / slideTo / slideRowEnd / slideClip beside a slideRunning () guard asking whether the ramp still has
steps of its own left to run, slideCount <= slideTotal, and an inline endSlide () that clears all of them ahead
of endBend ()'s own centring; slideCommand answering 145 and 146 by arming a ramp from where the wheel already
is to that letter's own extreme over the row count its argument names - the empty argument taking kSlideDefaultSpan,
a 0 reaching the extreme on the naming row itself, a row under a carried letter naming only an argument re-arming
a ramp of its own from where the wheel had got to and a row naming a command that is not a slide calling endSlide
() in the carry block rather than leaving a ramp still climbing under it - and the ramp now walked by flushSlide
beside flushArp at all three of process()'s flush sites rather than stepped inside processRow: it counts the steps
from the ramp's own start in transport ppq, one step being a row over the Settings' subticksPerRow, asks for the
steps that sit in [ppqStart, ppqLimit) where that limit is the end of the row the engine last owned so a row's own
steps wait for the block that reaches them the way an arpeggiator run's do, ends a ramp whose next step sits more
than a row behind where the block starts since a scrub or a jump cannot place it any more, keeps the position
arithmetic integer through slideFrom + (slideTo - slideFrom) * slideCount / juce::jmax (1, slideTotal) with the
count scaled before the division so the step sitting over the span's own last row lands on the extreme exactly
while the steps before it hold the whole position that far along the line between where the ramp left the wheel
and that extreme, sends no message for a step whose whole position is the one the wheel already sits at, which is
what the naming row's own step at count 0 holds to even for a span of 0 - slideCommand having become arm-only at
(ch, rowPpq, argument, up, clipUid, song) and slideStep being gone with the trailing call in processRow - queues
each step at the pitch wheel's own order 0 so it precedes the note-on of the row it sits inside, re-counts the
whole ramp as slideSpan * subticks so an argument-only row that re-arms mid-climb still lands its own extreme that
many rows below where it armed, and clears slideOn only once the ramp's last step is reached, which is what left
the release points grown rather than left alone: allNotesOff and stopNote each now calling one new
PlaybackEngine::endWheel (events, ch, midiCh, offset) in place of their own pair of guards, which centres the wheel
where wheelBent says it really is out of place and only puts a still-climbing ramp away where it does not, so a slide
ending at a note-off partway down its steps still sends nothing of its own while a wheel left bent there still goes
back to the centre, and the clip-end take-back still keyed on (bendClip == clip.uid && wheelBent) || (slideClip ==
clip.uid && slideRunning ()) beside that same helper; a row naming the arpeggiator call in processRow calling endWheel
ahead of that call, which is what puts a wheel the same column had left bent back at the centre there, a run having
no wheel of its own to put right and that row being the only one able, the const int midiCh hoisted above the command
carry block being the channel every command answer takes, and a file-local answerBend lambda shared by the row's own
B and by an argument-only row under a carried B mapping an OFF argument to kBendCentreVal and returning for an STP
one, which is what leaves the wheel sitting on the point its sub-rows had got to for the rows under that row. slideCommand
grew from an arm-only (ch, rowPpq, argument, up, clipUid, song) into (events, ch, offset, rowPpq, argument, up, noteOnRow,
clipUid, song), since an OFF row has to send a message of its own: it computes the MIDI channel from the instrument its
own column carries, closes the arpeggiator run that column was in ahead of everything else - closeArp plus endArp - so
only a bare slide row releases the note the run's last step entered while a row that struck a note of its own had released
it as that strike, answers an OFF with endWheel plus clearing lastCmd and lastCmdVal and returning so nothing at all is
carried below the row the way an arpeggiator's release leaves nothing, answers an STP with endSlide plus returning so the
letter keeps being carried and the point it holds needs no message of its own, and otherwise arms the ramp with jlimit
(1, 127, argument) rather than jlimit (0, ...) since the span 0 now means the OFF above it; arpCommand gained a bool
noteOffRow and a const bool releaseCall computed from id == 0 || id == kArgOff before id is clamped down to kMaxArpMacros,
since clamping first would have landed that sentinel on the highest macro instead of on the release, and its release
branch reads the base it re-strikes from c.columnNote >= 0 ? c.columnNote : c.arpCallNote before endArp takes them away
and strikes it again with startNote when the row is neither one that struck a note of its own nor one naming the column's
note-off, so a release goes back to the note the run measured from rather than going silent, that re-struck note being
owned on by a note-off below or by the clip's end the way a call row's own note was and carrying the stuck-key guard's
own extra note-off of the pitch it re-articles. flushSlide beside flushArp at all three flush sites of process() and the
stepPpq of (1 / rowsPerBeat) / subticks are as they were, and so is the ramp's own arithmetic of slideFrom + (slideTo
- slideFrom) * slideCount / jmax (1, slideTotal), a row's own sub-row steps still being flushed only after that row's
command answered, which is why an OFF or an STP read at a row is answered from the point the ramp reached at the last
sub-row step before that row rather than from one of that row's own. The release record a channel now
keeps beside activeNote / activeMidiCh / activeClip: relNote / relMidiCh / relOffset naming the pitch,
the MIDI channel and the block-relative sample the last note-off this channel queued released, written
by stopNote with every release it queues - the clip's own end, a row's own note change, an arpeggiator
opening or a switched call - cleared at the start of every playing block by the flush loop over every
tracker channel in process(), since a release only lives inside the block being queued and a note entered
at a same-numbered sample of the block below it still asks the guard for its own opening, and asked by
the stuck-key guard in startNote before it adds an opening of its own, which is what leaves a note entered
at the very sample one of that channel's releases opened its own pitch at having one release ahead of it
rather than two - the boundary of one pattern placed twice back to back being that shape, and the second
release there being what a host answering the sample rather than the order inside one read as the new note
being opened at once. allNotesOff's own per-channel note-offs are deliberately not recorded, that block
being the one the transport hands over while it is stopped and so possibly never delivered at all, which
is why a re-arm still asks the guard for its own opening there, and reset() takes the three back with its
own c = ChannelState ().
The vibrato the letter `V` runs wobbles the pitch wheel rather than moving it to one new point: `kVibCyclePoints
= 8` as the number of points a cycle is never drawn with fewer of, so the grid one cycle takes is `vibPoints
= jmax (cycle, kVibCyclePoints)` counted subticks rather than the row's own `subticksPerRow` steps - a cycle 6
subticks wide being exactly one row wide at the default Settings, one 12 two rows wide and drawn with 12 points
of it rather than the 8 it is asked for at least so its curve is cut at the twelfths of a cycle rather than at
the eighths, and one a single subtick wide still drawn with 8 points so the wheel moves 8 times inside that grid
rather than once over it and 6 cycles of it land inside one row - with the phase counted from the arming row's
`vibOrigin` in transport position and `vibCount` incremented for every point considered rather than for only
the ones that send, the way a run's steps and a ramp's are counted rather than read off the rows or carried round
from the point before, which is what a transport jump that makes this block play a row again leaves on the same
point of the cycle instead of advancing it twice and what keeps a swing that cycled many times over many blocks
exact rather than drifted by its own fractions; `ChannelState`'s `vibOn` / `vibId` / `vibClip` / `vibOrigin` /
`vibCount` / `vibRowEnd` / `vibBase` / `vibCycle` / `vibPoints` / `vibHeight` fields beside the slide's and
the bend's own,
`endVib` and `vibRunning` beside the slide's `endRamp` and `rampRunning`, and `vibClip` naming the clip whose
rows armed the swing rather than the clip a column's note sits in, which is what lets a row that armed a wobble
without striking a note be put away at that clip's own end at all; `flushVib` sitting with `flushArp` and `flushSlide`
after the rows a block processed and flushing up to the end of the row the engine last owned rather than moving
the wheel inside either letter's branch, which is what leaves a row's own sub-row points waiting for the block
that reaches them the way a run's steps and a ramp's do and ends a swing whose next point sits more than a row
behind where this block starts - a scrub or a jump leaving the swing ended at where it had got to rather than one
swinging late at that point - and re-reading the macro's shape, amplitude and cycle at every point of its cycle
rather than once at the arming row, so a shape, an amplitude or a cycle turned on the tab while a wobble was swinging
changes that wobble from where it is, and a macro taken away ends the swing at its next point rather than at the
row that took it, says nothing of its own and leaves the wheel at whatever point of its own the swing had got to,
a release row below being still what puts that away; `vibCommand` as the letter's own answer, an `STP` row returning
before it touches anything since a wobble has no move of the wheel to stop and the swing above keeps holding over
the rows below it, a release row - spelled by the id `0` a row names beside the letter or by the `OFF` the entry
writes for an `o`, both reading the same way an arpeggiator call's release does - putting the wobble away and taking
the wheel it had left swinging back to the centre at that row rather than at the note-off below it, so the rows
under a release sit under a straight pitch rather than at whatever point of the swing they would have got to, closing
the arpeggiator run that column happened to be in rather than keeping it, and leaving the column calling nothing
rather than calling id `0` so a further row naming only an argument arms nothing while a row under a carried `V`
naming another macro's id re-arms the cycle from that row and measures the new stretch from wherever the previous
one's last point had got the wheel rather than jumping to the new macro's own base; the swing's base being `c.lastBendPos`
rather than `kBendCentrePos` so a bend row's own point, a slide's extreme and the centre can all be the base a
wobble swings from, its `vibHeight` being `amplitude * kBendCentrePos / kVibAmpTop` so an amplitude of 100 is the
wheel's own half of 8192 and its points clamped to `[kBendBottomPos, kBendTopPos]` where the base has no room above
or below it for the full swing - the peak above landing one position past the top 16383 and clamping to it while
a trough below lands exactly on the bottom 0 - its position counted in whole wheel positions rather than in the
argument's steps of `kBendStep = 128`, which is what leaves nothing rounded where the swing's own curve crosses
the centre, and only a point that changes the position saying anything, so the arming row's own point, sitting
on the base, sends nothing of its own and a row that armed a wobble under a bend row leaves that row's point ahead
of the swing's first; one static `vibShape` over the six curves rather than a table, the sinusoidal one being
`std::sin (phase * two pi)` and the triangle one ramping through `q = phase * 4` so its corners sit on the cycle's
quarters rather than bunched at its ends, the `Up` kinds negating the shape once `phase >= 0.5` and the `Down`
kinds while `phase < 0.5`, which at full amplitude has the sinusoidal `Up` landing on the wheel's top twice a
cycle with nothing below the base to go to and the sinusoidal `Down` landing on its bottom twice and never rising
above it, the fold being the half the plain shape swung to the far side in, the whole thing sending the standard
CC105/CC106 pitch bend pair to the MIDI channel that row's instrument names like every other command row rather
than a message of its own; and the loop that closes a column whose clip was deleted or is no longer under the
playhead learning to name a run's or a swing's own clip rather than only `c.activeClip`, which a command-only row
never sets, so when `song.findClip (c.activeClip)` names nothing the loop falls back to `song.findClip (c.arpClip)`
when that column is in a run and then to `song.findClip (c.vibClip)` while one is running - without that fallback
the gone clip named nothing for such a column at all, that loop's `else` branch ending the swing with `endVib ()`
and leaving the wheel it had left off the centre never taken back, so the 16 bends two cycles of eight points
hold over a two-row clip's two rows came back as an empty run rather than as the swing's own points - and the
`c.vibClip == clip->uid` test in that loop is what returns the wheel such a swing had left swinging back to the
centre at that gone clip's own sample rather than leaving it bent over the clips that are still under the playhead,
while a column naming neither clip is left alone. The per-clip end above needed nothing of its own: its
`clipWobbledIt = c.vibClip == clip.uid && c.vibRunning ()` already named a swing the clip had armed and put
it away at that clip's end, so only the loop for a clip that is gone had to learn to name one.
, and this round's arpeggiator first tick: three file-local helpers were put between closeArp and the run's
own flushArp so the note handling above arpCommand and the steps below it read one rule rather than two -
arpSteppedNote naming the pitch a step's jump lands on, the octave snap that used to live inline in the
step loop and that the first step above and the steps below cannot both spell without one of them drifting,
arpFirstStepOf reading a macro's own first step through peekArp rather than growing the table for it, and
arpCallArmedAtRow naming the id the call at this row arms out of the plain int lastCmd the row had already
left in force beside the cell rather than out of the channel itself, ChannelState being a private nested
type of the engine that no helper outside its own file can name, so the note entered at a call row and the
call answered below it cannot disagree about what is about to be done to it. processRow's note block now
reads that call and its first step before it triggers the note entered at the row: the note goes out as
arpSteppedNote (cell.note + first step) at that row's own subtick rather than one subtick below as it was
typed, the stepped pitch becoming the run's own arpNote the way any other step's does - state.arpNote
taking it rather than the note as typed - while a first step entered as the note column's own note-off
triggers nothing there at all, releasing instead whatever the channel was left holding at that row under
the same guard flushArp uses and leaving arpNote at -1 so the run's own answer for its steps below the
rest is nothing rather than a note that never sounded, arpCount still starting at 1 for a row that named
a note so the steps under that row keep their own counting. flushArp's note-off branch opens whatever the
channel holds at that step's subtick under the guard c.activeNote >= 0 rather than the older c.arpNote >=
0 && c.activeNote == c.arpNote, which could not say anything at a run's head where arpNote had never been
set, and sets c.arpNote = -1 rather than ending the run, so a step entered as that note-off works as a rest
at a run's first tick and at its second both with a note beside the call and without one - its own first
step when the call's row named one, the note the column entered above it when it did not - with the column
still naming that note so the steps under the rest measure from it again, and that release following only
a note that run entered.
/media/data2/GameDev/Plugins/ReTrektor/Source/PluginProcessor.h / .cpp: processor, transport snapshot, state, lock, the size of the last GUI shown, and the undo line (undoStates holding the saved song XML with undoCursor and undoBytes counted in UTF-8 bytes, pushUndoPoint appending what a model change reached and skipping one that changed nothing, undo() / redo() stepping and refreshing the snapshot, resetUndoHistory anchoring a fresh line for a state the host hands back, getUndoDepth / getUndoRedoCount naming what is left to walk, trimUndoLine dropping the oldest states against the Settings depth and a 32 MB budget while never dropping the state the model is in or one ahead of the cursor, and stepUndoHistory keeping the pattern clipboard, the edit playhead and the mirrored transport position a saved state does not carry).
Beside that line the plug-in owns one gesture along a knob's row as one action of it: the dragBeingHeard and
dragPointMade pair the beginKnobDrag / endKnobDrag pair sets and clears, and the two lock-free moves of the
gesture's own point replaceUndoPoint and popUndoPoint, which pushUndoPoint calls from inside its own
ScopedLock and endKnobDrag calls after it, since modelLock is a juce::CriticalSection and a second ScopedLock
on the same thread would deadlock that spin lock.
/media/data2/GameDev/Plugins/ReTrektor/Source/PluginEditor.h / .cpp: main window layout, tabs, shortcuts, bottom bar, starting a new editor at the remembered window size, wiring the tools panel's isCellSelected hook to the tracker's selection, and the shortcuts where Ctrl+C/V name the cell clipboard (copySelectionToClipboard / pasteClipboardAtCursor) while the pattern clipboard's Ctrl+Shift+C / Ctrl+Shift+V pair is refused rather than answered, its two doCopy() / doPaste (bool forceUnique) members and its Ctrl+X slice branch gone altogether with that pair, the undo funnel commitEdit() (record the point the model reached, then refresh, so every component's onModelChanged ends in it while the bottom bar's OCT / STEP / CHANNELS handlers and selectPattern() record their own), performUndo / performRedo stepping the plug-in's line and refreshing the Settings panel as well because its rows hold their own values, and the Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z shortcuts with the Alt+Z / Alt+Y spellings the hosts leave free answering the same two steps, one each since the whole Alt+Shift family is refused rather than spent on a redo alias the hosts swallow, the Alt branches matching the letter over the key code and the printed character, all of them reaching the editor while every panel answers the Alt pair as well through one wireUndoRedo lambda that injects performUndo / performRedo into the tracker, timeline, pool, settings, tools and macros panels, so whichever panel the host routed the press to walks one point of the same line rather than one the panel owns, and one wireFocusRescue lambda injecting the same shape's focusRescue member into the tracker, timeline and pool so the panel that closed a menu, an alert or a drag hands keyboard focus back to the tracker (and to itself where the tracker is hidden), which is what makes the whole keyboard heard again rather than only the editor's own shortcuts, and one wireClipboard lambda injecting the cell clipboard's Alt steps as copyStep / pasteStep / cutStep members into all six panels with the tracker's own copySelectionToClipboard, pasteClipboardAtCursor (kPasteAll) and cutSelectionToClipboard, and one wireSlice lambda injecting the editor's own doSlice as a sliceStep member into all six of them beside those clipboard steps, so an Alt+C / Alt+V / Alt+X reaches one cell clipboard and an Alt+S reaches one clip slice at one snapped beat whichever panel the host routed the press to, and one press of either leaves one point of the undo line rather than two, the editor's own keyPressed answering those steps through the shared matchClipboardKey and answerSliceKey next to a Ctrl+C / Ctrl+V pair that refuses Shift so the hosts keep their own Ctrl+Shift+C / Ctrl+Shift+V meaning and no Ctrl+X branch left to it at all, and its first lines including the renamed header so no panel and the editor can disagree about what the Alt letters name.
The same editor grew the vibrato tab beside the arp one: the `Tab` enum growing `vibs` beside the tracker, macros and
settings,
the four tab buttons now reading `TRACKER` / `ARP MACRO` / `VIB MACRO` / `SETTINGS` rather than the one `MACROS`
the vibrato card had to be reached through before it had a tab of its own, the two macro tabs' buttons 10 px
wider than the plain ones so their two-word names are spelled rather than shrunk below the other tabs' letters,
and `vibTab.onClick` landing on that tab the way the other three do; the `VibMacrosPanel` constructed beside the
other panels and given the plug-in's undo and redo, its cell clipboard and its clip slice, so all seven panels
walk one point per press against the plug-in's own line rather than only the editor, and its `onModelChanged`
being the editor's `commitEdit ()` like every other panel's, which is what makes a macro's setting turned at the
tab record its point of the undo line and repaint the whole view through the one funnel rather than through a
line of its own.
, and this round's keyboard focus: a file-local clearButtonKeyboardFocus (juce::Component&) above the
constructor walks the editor's whole component tree and takes the keyboard focus away from every Button in
it - a Button wants that focus by default and answers the Return key while it holds it, which is what made
an Enter pressed only to confirm a value typed into a field fire whichever control the pointer touched last,
for every press after it, switching a tab back, flipping a checkbox, adding or clearing a macro, stepping a
knob or zooming the timeline - and setTab calls it after resized () and repaint () rather than only once at
construction, which is what catches the fresh buttons and the knob pairs each card brings with it whenever
its macros are added to and taken away; setTab now also grabs the keyboard focus of the panel that belongs
to the tab it lands on - the macros, vibs and settings panels beside the tracker's own grabKeyboardFocus -
the four tab buttons, the top bar's snap checkbox and the zoom pair taking that focus away instead, while the
bottom bar's OCT / STEP / CHANNELS knobs put it back, Slider::init having forced a Slider not to want it of
its own, which is what keeps the focus on the control whose text box a value is typed into rather than on
the Buttons it builds.
That editor hands the bottom bar's three knobs their gesture's two ends to the same line inside
setupBottomSlider, the one site that builds all three of them, because those three reach the model through the
change recorded in their own onValueChange rather than through a panel's handler, and the keyboard focus the
three knobs want rather than their own +/- pair now sits beside the drag that row asks for rather than instead
of it.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/TimelineComponent.h / .cpp: timeline, drag/drop, context menus, zoom, ruler (bar numbers only, no transport readout), and the Alt pair of undo and redo answered in its keyPressed, which is where a press aimed at undo has to be heard while the timeline's own clip menu is open or while a pattern is being dropped onto it, and a focusRescue member it calls at the end of both context menus and ahead of the playback guard in its drop, since both the popup and the dragged item are parentless desktop windows that take the keyboard focus with them when they close, and the Alt clipboard steps answered after the undo one through copyStep / pasteStep / cutStep members the editor injects, chained through the 4-argument answerClipboardKey so an Alt+C / Alt+V / Alt+X pressed while this panel routes the press reaches the tracker's cell clipboard rather than the host's, and the clip slice answered last of all through answerSliceKey with a sliceStep member of the same shape, so an Alt+S heard while this panel holds the press splits the clip under the playhead that the timeline's own "Slice at playhead" menu item reaches rather than leaving the Ctrl+X spelling the host already spent on its own cut.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/TrackerComponent.h / .cpp: tracker grid, key entry, per-channel column visibility chips, field widths from fieldGlyphs, an entered command argument staying implicit over the command number carried in the column (carriedCommandNumber), cell selection (selBits one bit per (row, channel) in kSelWords = 2 uint64 words per row, applyCellRect unioning/subtracting/replacing word masks, gestureOp reading the held modifiers, channelAtX and channelBoundsFor naming a cell, per-cell painting and a hidden chip inverting its plate (theme::text plate, letter cut out in theme::panelLight) instead of dimming) and the cell clipboard (cellClipboard of CellClipboard type, copySelectionToClipboard putting the selected cells on it with their offsets inside the copied rectangle counted from the top-left selected cell and doing nothing while nothing is selected, pasteClipboardAtCursor writing at the cursor and only marking the model changed when it wrote something, cutSelectionToClipboard taking that copy and then emptying exactly the cells it took through the clearSelectedCells loop the cell menu's Clear / Selection calls as well so the two do not drift apart, marking the model changed for the wipe alone since the copy changes nothing in the saved song, hasCellClipboard greying out both of the menu's pastes, and the cell menu's four top-level items - a "Clear" submenu whose leaves keep the old ids so the callback still answers them by id (4 Selection / 1 Cell / 2 Channel / 3 Pattern), "Copy selection" (id 5), "Cut selection" (id 6 right after the copy, greyed while nothing is selected and answered by the same cutSelectionToClipboard the Alt+X key reaches), and two sibling paste submenus carrying the same seven leaves, Everything / Note / Instrument / Volume / Pan / Command / Command Value, the plain "Paste" one at ids 7..13 and the "Paste (Additive)" one at ids 17..23, both greyed by hasCellClipboard and both answered through one static pasteMasks the callback hoists above its if-chain so the field and the mode compose rather than a leaf owning the mode, pasteClipboardAtCursor (pasteMasks[result - 7]) and pasteClipboardAtCursor (pasteMasks[result - 17] | kPasteAdditive), 16 left unused between the two runs and the undo pair's 14/15 still last), and the undo pair (undoStep / redoStep std::function members the editor injects so the menu's Undo and Redo, ids 14 and 15 after the paste submenu's leaves and greyed out by the plug-in's own counts, walk the same line Ctrl+Z and Ctrl+Y walk and refresh every panel rather than only the grid, and cancelPendingEdit reporting whether a numeric buffer was half typed and closing it the way Esc does, which the Ctrl+Z and Alt+Z branches and the menu's Undo consult before stepping so one press is one effect, and its keyPressed now answering the Alt pair itself through answerUndoRedoKey, with cancelPendingEdit as the closing hook so a half-typed number still closes on that press rather than being walked over, while any other letter held with Alt is still handed up rather than typed as a note at the cursor), and a focusRescue member it calls at the end of the cell menu's callback, whether the press picked an item, a greyed one or dismissed the popup with Escape, and the Alt clipboard steps answered through the 4-argument answerClipboardKey with copyStep / pasteStep / cutStep members of the same shape the editor injects, placed ahead of the hand-up that keeps other Alt letters out of the tracker's own key entry, since the three letters they name are three of the note-entry letters and an Alt held c or x that fell through would type a C or a D sharp at the cursor next to copying, pasting or cutting, and the Alt+S slice answered the same way through answerSliceKey with a sliceStep member of the same shape the editor injects, this very letter being the note entry's C sharp, so the slice is claimed before the key entry can leave a C sharp at the cursor whether or not there was a clip to split, and the command letters: the A and the B branch sitting in the path that owns a value field's first character, so either letter claims the command number as its own command value while a number that merely has an a or a b in it keeps its own spelling and the CC 10 and CC 11 a column asks for are typed as their own 10 and 11, a letter typed after a run of digits claiming the field and dropping that number rather than reading it, the pair painted in theme::cmdMacro with every value drawn in the view's one decimal spelling, the A a row carries from above drawn dim through carriedCommandNumber beside the bright argument a row below writes, and clampValue's command number capped at kCcTop rather than at the call, both letters' claim writing the command value it names straight into the cell rather than as a number that gets clamped there, so a typed digit run and an interpolation ramp can only reach a plain CC and typing 144 into the field gives the highest CC rather than inventing a letter - the letters being the only route to a command kind - beside each letter leaving its own argument behind, a call at the release id 0 and a bend at the wheel's centre kBendCentreVal so a row naming only the letter bends nowhere rather than locking the wheel to its bottom, and the bend pair painted in the lilac theme::cmdOther that was going unused with that argument drawn in the view's one decimal spelling like every other value and that letter carried down a column drawn dim like the call's is beside the bright argument a row below writes, the command cell now splitting its two halves by their own glyph counts rather than halving the whole cell and each half taking its own pad, with fieldGlyphs giving the number's half three glyphs and the argument's half three more in the view's one decimal spelling so a macro id of 100, a bend's point above 99 and an arpeggiator release's OFF all keep their last glyph while the instrument's half keeps room for its dash and two digits, and maxDigitsFor taking the three decimal digits at the keyboard everywhere so no letter can be invented by a number's spelling, valueString and instString drawing every value decimal with no setting and no row offering another spelling, the display's carry table and the two reads walking the command column together through one file-local stepCommandCarry so they cannot disagree - a row that named a command of its own leaves that one and its own argument, the value cell answering such a row with that row's own argument so a letter that had none draws nothing beside it rather than picking an argument up from above, and a row that named only an argument keeps the letter carried above it and leaves the argument it named itself carried for the rows below rather than the one that letter called with, an argument of 0 under a carried call clearing both since that argument is the release the engine's arpCommand already answers there; drawRows fills a row's entry from what the column carries into that row before that row steps it, which is what an argument-only release still reads its letter on its own row and draws neither letter nor argument over the ones under it, and carriedCommandNumber walks the column down from its top over the rows above rather than up from the row, since what a row leaves in force also depends on the arguments the rows above it entered, beside a new public carriedCommandValue reading the argument the same walk leaves so a test reads the carry rather than a pixel, while the letter A with the argument 0 still releases and leaves the column under it carrying neither a letter nor an argument and the value cell spells that release OFF rather than a bare 0, and this round's argument field beside it:
a file-local commandArgumentReleases (cmd, cmdVal) naming an OFF of any kind and a 0 read under a call or under a
slide letter but never a 0 beside the bend letter, asked by both commandLeavesCarry and the implicit half of stepCommandCarry
so the column stops drawing a carried letter under a release however that release was reached and keeps drawing one
under an STP; a new public commandKindAt (row, ch) declared in the header beside carriedCommandValue naming the kind
a row's argument belongs to, the row's own command where the row named one and carriedCommandNumber (row, ch) where
it did not, applyValue (Field::cmdVal) rewriting a typed 0 to kArgOff when that kind is a slide letter so the narrowest
span a row can ask for is one row rather than a slide over none, and the value field's entry answering a value field's
first character with two letters ahead of its digit path - an o or an O landing kArgOff beside a wheel or an arpeggiator
kind, an s or an S landing kArgStp beside a wheel kind only, a claimed write doing markChanged (), advanceRow (jmax
(1, stepSkip)) and repaint () the way a committed number does, and a letter neither of those two claims owns falling
through to return false so the cell keeps the value it had and no note is typed in the note column beside it; drawRows
reading OFF for an arpeggiator release, for a kArgOff and for a 0 read under a slide letter, reading STP for a kArgStp,
and keeping the colours it had, the four wheel kinds still sharing the one lilac theme::cmdOther beside the orange
of the call pair, with maxDigitsFor and fieldGlyphs still naming three glyphs for that field so the release's OFF and
a hold's STP keep their last glyph and the comment naming them rather than only the release.
The tracker grew the vibrato letter: the command number field claiming `V` as `kVibCmd = 147` beside the `A`, `B`, `U`
and `D` it already claimed, the claim being only the command number field's so the same letter typed where the
note column is asked for still types an F natural - the way the slide letter `D` still types a D natural - and
only `cmdNum` claims the letter while `v` stays a note key rather than the letter being spent twice; a row naming
only the letter leaving the argument as the release id `0` rather than a default macro, so it draws `V OFF` rather
than running the macro a fresh song defines, the `o` spelling landing the same `OFF` the entry writes and the `0`
a row names beside a named or a carried `V` landing that vibrato's own release there rather than a call of macro
`0`, since id `0` calls nothing, both spellings drawing `OFF` and both leaving the column under it carrying neither
the letter nor an argument; `commandArgumentReleases` refusing the `s` spelling beside the vibrato letter rather
than writing it, since a wobble has no move of the wheel to stop and such a row leaves the swing above still swinging
over the rows below it, so an `STP` beside a `V` is a number a saved state or a shot's own flag leaves there rather
than something the editor offers, and `commandKindAt` naming the fifth kind the column holds beside `carriedCommandNumber`
/ `carriedCommandValue` so a cell's own argument is refused or taken per kind rather than per number, a bare-argument
row under a carried `V` keeping the swing above rather than ending it; the `V` pair painted in the lilac `theme::cmdOther`
the bend and the two slide letters already shared rather than a fourth colour being brought in, since a vibrato
is a wheel move, with the release's `OFF` and a hold's `STP` still keeping their last glyph in the three glyphs
the widest spelling already needed and every value still drawn in the one decimal spelling.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/PoolComponent.h / .cpp: pattern pool, drag source (the drag image is the item's cell), grid preview, rename, and the Alt pair answered in its keyPressed, which is what a press aimed at undo needs while the pool's own drag is still held by the DragAndDropContainer and has made that drag modal, and a focusRescue member it calls at the end of its own menu and of the rename alert, plus on the release of a drag, which ends whether or not the item ever landed on a target, and the Alt clipboard steps answered after the undo one through copyStep / pasteStep / cutStep members the editor injects, since the same reason that made the undo pair unreachable gives the clipboard's steps to the host's own clipboard and cut as well, and the clip slice answered last of all through answerSliceKey with a sliceStep member of the same shape the editor injects, so an Alt+S is heard even while the pool's drag is still modal and the editor is never offered the press at all, and its grid preview's command tick now painted lilac for all four wheel kinds - the pitch bend's letter, the two slide letters and the vibrato letter's pair - beside the teal a plain CC and the orange an arpeggiator call keeps.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/ToolsPanel.h / .cpp: transpose as one semitone amount field (IncDecButtons slider with an editable text box, whole semitones 1-48 opening at 12) plus arrow direction buttons whose labels arrowUpGlyph/arrowDownGlyph build as explicit UTF-8, remap (source dropdown All/-0..-15 -> target dropdown -0..-15, the labels being the decimal ones the tracker's instrument column writes them in), cell scope (cellInScope answers whether a cell is in reach and applyToCells walks the cells, so the wider scopes name whole patterns and only the "Selection only" scope consults the tracker through the isCellSelected hook), interpolation with its single field dropdown (Note/Instrument/Volume/Pan/CC number/CC value, ids following the InterpField enumerators, the mask being the selection so there is no channel dropdown any more), the getCursorChannel hook still wired but unread, the scope box defaulting to "Selection only", and the Alt pair of undo and redo answered in its keyPressed, as is the Alt clipboard steps through copyStep / pasteStep / cutStep members the editor injects, so an Alt+C / Alt+V / Alt+X aimed at the cells is heard while a menu belongs to this panel, and so is the Alt+S slice, answered last of all through answerSliceKey with a sliceStep member of the same shape the editor injects.
, and this round's keyboard focus: its styleButton lambda takes that focus away from every one of the panel's
buttons, so an Enter pressed to confirm a value cannot transpose, remap or interpolate a second time for
whichever control the pointer left last, its arrow pair taking the same way out, while the transpose knob
puts the focus back since Slider::init forces a Slider not to want it; the panel keeps no focus of its own,
it being on screen on every tab, and focus resting there would leave the tracker, the only note entry there
is, deaf the same way.
The transpose amount's row is asked for the horizontal drag but not for the line's two ends: its number
belongs to the arrows' next click rather than to the model, so a drag over it writes no state of the song and
earns no point of the line to coalesce.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/SettingsPanel.h / .cpp: settings controls, the row that used to switch the values between hex and decimal having been dropped outright along with its toggle member, its setting and its saved property, so every value the tab moves is decimal and its refresh guard now starts at the mid-row toggle, including the Volume column CC and Pan column CC sliders and the Undo steps row (undoStepsSlider, 0-200 defaulting to 50, sitting with the input prefs ahead of the CC pair and pushed back by refresh() so an undo that moved the depth shows in the row), and the Alt pair answered in its keyPressed so a press aimed at undo is heard while a menu belongs to this panel, as is the Alt clipboard steps through the same copyStep / pasteStep / cutStep members the editor injects, and the Alt+S slice through a sliceStep member of the same shape answered by answerSliceKey, since a popup opened over this panel is what routes the press here rather than the editor, and the "Paste as unique pattern by default" row dropped along with the Settings member it read and the property it saved, which leaves the panel counting one row fewer and its refresh re-reading the rest in their order.
, and this round's keyboard focus: the panel wants the keyboard focus and the editor's setTab grabs it while
that tab shows, so an Enter pressed to confirm a value typed into one of the rows' own text boxes walks
here rather than to whichever control the pointer touched last, addToggle taking that focus away from every
row's checkbox and addSlider putting it back on every row's knob - a Slider in the IncDecButtons style
builds its own +/- buttons through the look and feel, and a Button answers a Return while it holds that
focus, which is what made a confirming Enter flip the checkbox beside the pointer and step the knob the
pointer left last - and the panel spending the returnKey in its own keyPressed after the slice key rather
than handing it up, its own fields having answered the press already, so a repeat of it changes no checkbox,
adds no point of the undo line and reaches neither the editor's shortcuts nor a Button of its own.
The tab's addSlider is the one site that builds every one of its rows, which is why the drag mode and the two
ends of the undo line's gesture both belong there rather than at each row, and the Undo steps row - the one at
rows [rows.size () - 3] beside the input preferences, ahead of the volume and pan CC pair - is thereby
draggable: a drag along it changes the number that row shows, feeds Settings::undoSteps and caps how far a
following Ctrl+Z can reach.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/MacrosPanel.h / .cpp: the arpeggiator macros the Command column's letter A calls, rewritten from the old name/kind/two-parameter editor: one card per macro that exists, named by the call a row writes (A 1) in orange, the tab painting nothing above the cards but its title and the one MACRO / LENGTH / LOOPED / STEPS label row at y 48 rather than the four dim caption lines that used to sit between the title and the cards, kContentTop taking them up from 92 to 70, since what the rules are belongs to the Commands map and to the README rather than to something a scrolled list has to be read under; each card's control line holding a LENGTH field, the plug-in's usual IncDecButtons Slider over 1..kMaxArpSteps with an editable text box, and a LOOPED ToggleButton, and the macro's own steps drawn beneath it 16 to a line as their signed semitones off the base note, the panel's own public stepLabel (int step) drawing -- for kArpEmptyStep, OFF for the new kArpNoteOffStep - the note column's own spelling, since === is a release and not a step, and the reason the label had to leave the file-local lambda the panel used to keep, so the strip's paint and the tests read one spelling rather than two - +N for a positive one and 0 for the base note, -- being the step that sounds nothing and sustains the one before and OFF being the step that opens what the run was holding rather than sounding one of its own, and a newly defined macro arriving with eight of them that all sound nothing rather than with the ramp kArpDefaultJump used to name; the step cursor (stepId / stepIndex) the arrows and a click on a drawn step move is what enters a step at the keyboard, and a step's number now living only in stepText until commitStepText lands it rather than being written by every press, so a sign and up to three digits accumulate into one value, the cell the cursor sits on draws that number while it lasts rather than the step's own value, commitStepText fires from keyPressed only when the incoming press is neither a further character of that number nor Escape, which is what lets setStepCursor, the arrows and a click of another card close it as well, one leading sign being kept with a repeat of it changing nothing, the other sign moving the number rather than trailing it and one typed after the digits it belongs to leading them, a number that is nothing but a sign meaning the base note, the value landing clamped to ±127 as one point of the undo line rather than one per keystroke and Escape dropping it altogether rather than leaving half of it behind, commitStepText handing the raw value to the setter rather than clamping it a second time at the panel since the setter owns that clamp and one rule cannot be stated twice; the note column's own note-off key spelling a step the same way it spells a release there - the backtick and its shifted tilde, the pair the note entry answers alike, writing kArpNoteOffStep at the step the cursor sits on rather than writing a number into it, claiming the press so no letter of the note table is typed beside it, firing onModelChanged once for it and leaving the cursor on the step it opened rather than stepping past it the way Delete and Backspace leave the cursor they cleared, which is why the next press lands beside the opening rather than running the macro past the release the user meant to hear there; the bottom of the panel holding + Add macro (defineArp at firstUndefArpId) and - Remove selected rather than - Remove last, that one clearing the macro the step cursor sits on, falling back to lastDefArpId while there is no cursor yet and taking the cursor off the macro it cleared before rebuild parks it on another one, its bounds widened to 132 so the longer label fits, so the ids above and below a cleared macro keep naming the same macros; the vertical scrollbar the tab draws on its right edge having its geometry in one set of helpers, scrollbarX at getWidth() - 10, thumbHeight, thumbTop and scrollToPointer, so paint and the drag cannot drift apart and the thumb a drag centres on is the one that is visible, with mouseDown answering a pointer in that column ahead of the card hit-test loop, keeping the grab in draggingThumb through mouseDrag while mouseUp clears it - JUCE spelling that callback mouseDrag rather than the mouseDragged the panel first declared, an override of which does not compile - and mouseWheelMove still turning the list beside it, which is what the mouse alone needs where a host gives the plug-in no wheel, and a public getScrollY () so a test can read the scroll back rather than only see it painted, beside a public stepBand (which) naming a step's group as its index over the Settings row "Subticks per row" - six at the default - named the same way rather than by the 16 cells a drawn line wraps at, the strip's paint shading every second group at 0.58 alpha of whatever colour that step's own state asks for, theme::cmdMacro for a step the macro holds or the cursor pending, else theme::noteOff, which is what makes a macro longer than the row that called it read as row spans rather than as one run of numbers and is what the accessor is public for, so UiTest reads the boundary at steps 0, 5, 6 and 12 rather than a pixel; rebuild / syncValues / cardHeight / blockTop / contentHeight / clampScroll laying the cards out against the scrollY that wheel and the drawn thumb move, and every change the panel makes answering through onModelChanged so the undo line holds a point for a drag of a length field, a click of the looped tick, a typed step and a cleared one as it does for the two buttons, which is what lets Ctrl+Z and Alt+Z take each of them back rather than stepping over them in silence; both Alt pairs answered in its keyPressed, the undo one and the clipboard one through copyStep / pasteStep / cutStep members of the same shape answered by the 4-argument answerClipboardKey, since the macros panel's own context menu is what routes a press to it rather than to the editor, and the Alt+S slice answered after the clipboard one through answerSliceKey with a sliceStep member of the same shape.
, and this round's keyboard focus: the panel wants that focus and the editor's setTab grabs it while its tab
shows, so a press walks to the panel rather than to whichever control the pointer touched last; the two
buttons' pair and every card's LOOPED tick take it away, which is what stops an Enter meant only to confirm
a typed step number from adding a macro, clearing one or flipping a loop again, while each card's LENGTH
knob puts it back, the +/- pair that knob builds through the look and feel taking it away instead; the panel
spends the returnKey in its own keyPressed beside the commit its top of the handler already did, so a repeat
of that press adds or clears no macro, leaves the undo line alone and reaches neither the editor's shortcuts
nor one of the tab's own Buttons.
A card's own LENGTH knob now asks for the horizontal drag and hands its gesture's two ends over to the undo
line, so the length a drag left is one Ctrl+Z step of the line rather than one per mouseDrag along the row,
its +/- pair still costing one step of the knob's interval apiece and its typed text box still setting the
length outright.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/VibMacrosPanel.h / .cpp: the VIB MACRO tab the Command column's
letter `V` calls, added to and removed from exactly as the arpeggiator macros are. One card per macro that
is `present`, each holding the three settings a wobble runs on one 22 px line plus a 4 px gap: the shape as
a `juce::ComboBox` over the six spellings `Sin`, `Sin Up`, `Sin Down`, `Triangle`, `Triangle Up` and `Triangle
Down` — one file-local `kShapes[]` holding the spellings beside the model's own `kVibType..` values, so the
spelling lives in this panel's own table once, the dropdown's item ids being one above the model's values,
and `vibShapeLabel` reading a shape back for a test as the same text the tab draws rather than a second copy
being invented there — then the plug-in's usual number widgets for the amplitude, a `juce::Slider` over 0..1
at an interval of 0.01 spelled by `vibAmplitudeLabel` as the share of full pitchbend rather than as the hundredth
the model keeps, and for the cycle, a second `juce::Slider` over 1..`kVibFreqTop` at an interval of 1 whole
subticks one sin or triangle cycle takes, the two knobs told apart by the span they walk rather than by the
order they were added in, so a check about one of them is about that one however the card was built, and a
slider's own `getInterval()` naming its step since `juce::Range<double>` has none of its own; the three controls
sitting at `kShapeX = 104`, `kAmpX = 232` and `kCycleX = 360` under the tab's title and its one label row —
`MACRO`, `SHAPE`, `AMPLITUDE (of full bend)` and `CYCLE (subticks per swing)` at y 48, `kContentTop = 70`
taking the first card below it and `kBottom = 40` the strip holding the two buttons — and nothing painted above
the cards but the tab's title and that label row, since the rules belong to the Commands map and the README
rather than to a memo the cards would have to be read under; a card named by the call a row writes (`V 1`) in
the lilac the wheel's own commands are read in rather than in an arpeggiator call's orange, which is what makes
the two macro tabs tell the two letters apart at a glance, and marked by a cursor rule drawn left of that name
rather than over one of its settings, so a click on the card's own name is what picks a macro out for the remove
button and the arrows while a pointer over one of the three settings lands on the control sitting there instead;
`contentHeight()` / `cardTop()` / `clampScroll()` / `scrollY` carrying a list longer than the panel rather than
growing it without bound, and the drawn scrollbar asked for in one place — `scrollbarX()` 10 px in from that
tab's right edge, its thumb as tall as the share of the track the cards that fit take so it reads as the window
that is showing, feeding `thumbHeight()` / `thumbTop()` / `scrollToPointer()` — so paint and the mouse cannot
drift apart, `mouseWheelMove` turning it forty px to a full unit of the wheel's own deltaY as well as a `mouseDown`
/ `mouseDrag` / `mouseUp` grab putting the thumb's middle under the pointer and holding it there for as long as
the button is held, which is what the mouse alone does where a host gives the plug-in no wheel, and the tab
answering a pointer in that strip's column ahead of its card hit-tests, which is why a grab of the strip never
reads as a click on the card behind it; the add button defining `firstUndefVibId()` at the shape, amplitude and
cycle the model gives a fresh one — the plain sinusoidal wobble at half full pitchbend over a cycle of eight
subticks, so the card shows a wobble rather than an empty line — and disabled while the table has no id left
that has none, and the remove button clearing `selectedId` or `lastDefVibId()` while nothing is picked out and
leaving the ids below the macro it put away naming the same macros, its cursor dropped rather than left pointing
at the card that just went, and clearing saying a macro has none rather than erasing the settings its knobs had,
which is what re-defining it then starts over from; the arrows, `Tab`, `PageUp` and `PageDown` moving that macro
cursor from card to card and to the list's own ends — `PageUp` to the lowest macro that is defined and `PageDown`
to the highest, one press going to an end rather than one card — with no cursor yet picking one out starting at
the highest that is defined above and the lowest below, and `prevDefVibId` / `nextDefVibId` naming the nearest
defined macro on either side since the gaps a cleared macro leaves are what a cleared list is made of; each of the three settings, a define and a remove being one action of its own, so
a card whose three settings were all touched and that was then defined and removed leaves five points of the
undo line rather than one, and `refresh()` / `rebuild()` / `syncValues()` laying the cards out again against what
the song holds while `onModelChanged` closes a tab that is not the one showing and refreshes the one that is;
the undo, redo, clipboard and slice `std::function`s the editor injects so the Alt pairs and the Alt+S slice are
answered at this tab as well as at the editor and all seven panels walk one point per press against the plug-in's
own line rather than only the editor walking it.
, and this round's keyboard focus: the panel wants that focus and the editor's setTab grabs it while its tab
shows, its Add and Remove pair taking it away so an Enter pressed to confirm a value typed into a card's
own box can add or clear no macro, each card's two knobs putting it back and the +/- pairs they build taking
it away, and the tab spending the returnKey in its own keyPressed once the dropdown and the knobs have
answered their own, so a repeat of the press leaves the macro table and the undo line exactly as they were.
The two knobs of a card - its amplitude as a share of full pitchbend and its cycle counted in subticks - ask
for the horizontal drag and hand their gesture's two ends over to the undo line the same way, so the share and
the cycle a drag left are one Ctrl+Z step of their own rather than one step of the line for every mouseDrag
that walked a row.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/AltShortcutKeys.h: the shared body every panel answers the Alt pairs the hosts leave free with, renamed from UndoRedoKeys.h once it grew the second pair (the references to that name above are this header under its old name). matchUndoRedoKey / answerUndoRedoKey as before (reading Alt+Z / Alt+Y over both the key code and the printed character and refusing Command, Ctrl and Shift so the whole Alt+Shift family stays the host's, claiming the press only when that panel holds the step, and closePending naming the half-typed numeric buffer the tracker closes on the press before stepping); matchClipboardKey / answerClipboardKey the new steps added to it (a ClipboardKey of none, copy, paste or cut, the letters read the same two ways, Alt+C copying what the tracker holds selected, Alt+V pasting at the cursor with everything and Alt+X cutting, which is that copy followed by emptying exactly the cells it took so the paste after it moves them, and the answer claiming the press whenever that panel holds the step rather than only when the step did something, since the three letters they name are three of the note-entry letters and an Alt held c or x that fell through would type a C or a D sharp at the cursor, next to copying, and a paste answered twice would walk the undo line twice); matchSliceKey / answerSliceKey the clip slice at the playhead added beside them, the s read the same two ways over the key code and the printed character, an Alt+Shift or Ctrl spelling refused so the whole Alt+Shift family and the host's own save stay the host's, and the press claimed whenever the panel holds the slice rather than only when the slice did something, since s is one of the note-entry letters and an Alt held s that fell through would type a C sharp at the cursor beside splitting a clip, while there is no Ctrl spelling of the slice left for a host and the plug-in to disagree about; matchSliceKey / answerSliceKey the clip slice at the playhead added beside them, the s read the same two ways over the key code and the printed character, Alt+Shift and Ctrl refused so the whole Alt+Shift family and the host's own save stay its, and the press claimed whenever the panel holds the slice rather than only when the slice did something, since s is one of the note-entry letters and an Alt held s that fell through would type a C sharp at the cursor beside splitting a clip, and there is no Ctrl spelling of the slice left in any panel for a host and the plug-in to disagree about.
/media/data2/GameDev/Plugins/ReTrektor/Source/ui/Theme.h: colors, fonts and a LookAndFeel that grew this round one override beside the colours it already had: the
Command column's five kinds already had the three colours its entry gives them, the teal of a plain CC, the orange
of an arpeggiator call's pair and the one lilac theme::cmdOther the bend, the two slide letters and the vibrato
letter's pair all share rather than a fourth colour being brought in for a wheel move, which is what makes the
VIB MACRO tab's own lilac `V 1` card names and the tracker's lilac `V` pair tell a wheel move from a run at a
glance rather than by their letters, and the pool's grid preview painting that same lilac for all four wheel kinds.
That override is createSliderButton (juce::Slider&, bool isIncrement), which hands the +/- buttons a Slider
drawn in the IncDecButtons style makes through the look and feel no keyboard focus of their own: a Slider
builds them inside itself, so nothing outside one can reach them and this is the one place their focus can
be taken away, which is what stops an Enter typed to confirm a value written into a knob's own text box
from stepping the knob whichever card the pointer left last. It fires for the sliders built while this look
is in force - the bottom bar's three, the tools' transpose amount and every card a macro tab rebuilds as
its macros are added to and taken away, the editor's constructor having put this look in force as the first
line of its body - and not for the sliders the editor's panels carry as their own members, built before
that line ran, whose buttons and whose knobs' pairs the editor's own sweep of its component tree takes the
focus away from instead, an AlertWindow being left alone by both since it is a window of its own with no
parent component and its own buttons answering a Return is what closes one by keyboard.
/media/data2/GameDev/Plugins/ReTrektor/Source/Portability.cpp: the sqrtf and atan2f defined inside the plug-in
so its libm requirement stays portable, aliased hidden in assembly and built without LTO, re-checked this round
against the same floor rather than assumed: the built .so still needs at most GLIBC_2.38 and GLIBCXX_3.4.32, the
four names the new curve leans on - sin, sincos, sinf and sincosf, all at GLIBC_2.2.5 - already being in the
older build-instrument artefact's own dynsym rather than being new to this build, so the float calls raised
no floor above what the pinned Bitwig 6.1.4 Flatpak root ships and sqrtf and atan2f are still not asked for
by name.
/media/data2/GameDev/Plugins/ReTrektor/tests/CMakeLists.txt: smoke/core/editor-shot test targets.
/media/data2/GameDev/Plugins/ReTrektor/tests/CoreTest.cpp, UiTest.cpp, SmokeTest.cpp, EditorShot.cpp: automated verification harnesses (CoreTest drives the interpolation predicate directly with rectangle masks and a holed mask, names one clip boundary of one pattern used twice back to back as one release of that pitch ahead of the note the lower clip enters at its row 0 rather than two - that clip's own end being the only thing queued there ahead of that note-on, and nothing of that pitch closing it again over the rows below - and pastes a cell clipboard built the way the tracker builds it, UiTest drives the selection gestures and the cell clipboard through juce::ModifierKeys-carrying mouse events and the editor's keyPressed for Ctrl+C, Ctrl+V and the Shift pair, and drives every panel's own keyPressed for the Alt+C / Alt+V / Alt+X steps, so each of the six panels is shown copying, cutting and pasting the tracker's selection at a fresh anchor with the cut leaving exactly the cells it took empty, the selection still painted and the cursor unmoved so the paste after it moves the cut cells to the anchor, and each of them refusing an Alt+Shift spelling and the Ctrl spellings instead; its fixture now puts the data into the selected cells through the clipboard and a paste rather than a silent write, since a cut that returns the model to the state the undo cursor already sits on is deduped and would leave no point for the checks to count; CoreTest drives the additive spelling of the same mask over the same clipboard, so the entries that carry something land their own fields on top of what the target carried while the fields the cell left empty keep the target's value, a one-field additive paste counts only the entry that really carries that field, and an all-empty clipboard cell reports nothing written where the plain spelling still writes it, UiTest drives that submenu pair's whole-buffer and one-field additive leaves through the same paste calls their callback makes at a clicked cursor, kPasteAll | kPasteAdditive and kPasteVol | kPasteAdditive, the one-field one showing a copied cell that does not carry that field skipped whole so the target keeps its value there while the copied cell that does carry one lands at its offset below the anchor and writes nothing else of its own, gives every panel the editor's sliceStep and splits one fresh clip of a fresh pattern for every Alt+S press at its own middle row so one press turns one clip into two over two new patterns and leaves exactly one point of the line, reads the cell the cursor sits on back again to show the Alt held s typed no C sharp, takes the lowercase Alt+s X11 hands over, refuses an Alt+Shift s and a Ctrl s and the editor's Ctrl+X and dropped Ctrl+Shift pair while the pattern clipboard and the clip count stay put, and still claims the slice while the transport runs without splitting, its fixture putting the rows the additive paste filled back to empty once it is done with them, since hidden rows survive a shortening only while they hold data and the shortening checks below read that, and its additive one-field paste staying over the cursor the block above left rather than moving the cursor up the pattern, since moving it lets ensureCursorVisible scroll the view and the blocks below map their clicks through it; EditorShot can hold a selection with --select=r0,c0,r1,c1 and paste it at a cursor with --paste-at=row,ch, and can walk the undo line with --undo=N / --redo=N before it renders, so a shot shows what an earlier action left behind retracted; CoreTest names the arpeggiator at the layer below a host, the grow-only table's defaults arriving as eight steps that sound nothing and the firstUndefArpId / lastDefArpId pair, the steps growing adds sounding nothing rather than a ramp the model no longer has, a run whose row's own note is its first step with its eight seeded steps landing a sixth of a row apart in order and spilling past the row that called them — six of its playback fixtures now say for themselves which offsets they are listening for through a file-scope seedSteps (Song&, int id, int jump) helper rather than trusting a ramp the model no longer builds — a looped run cycling past that row with its cycle boundaries exact, a finished unlooped run leaving its last step's note held, a row naming only the argument under a carried call switching to the macro that id names and restarting at its first step while an undefined id sounds nothing but still stops the run above it, the switched macro stepping its own offsets off the note the column named rather than off the note the run was left sounding with its two named offsets moved to the two only that base can reach, the note the run above left holding reaching the host as a note-off ordered ahead of any of the new run's note-ons, a call under a note-off in a column that names no note having nothing to step from while the same call carried onto a note entering below it steps from that one, an A 0 call releasing as a plain note-off with only the stuck-key guard's own extras around it and no All Sound Off, a call written in the note-off's own row being answered there rather than
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
--hex flag its usage header and parse block used to carry. This round they moved with the step's
own note-off: CoreTest 279, +14, adds three arpeggiator playback sections on the macro's own offsets
rather than on absolute samples, since the notes h.notes and the raw events h.raw name are block-relative
and only their order inside the clip is what the engine promises - one opening mid-run putting its release
between the two note-ons either side of it with one release per opening, one at a run's head and the tail
of the same run, where the head's opening releases nothing and the next step's note-on is the very next
event after the previous row's release, so nothing of its own was there to cut, and one looped cycle
stepping through its openings and sounding itself again after the single release, its note-on of the pitch
the cycle opened counted twice with only one release of that pitch between them; plus two round-trip checks
putting an opening at a step and reading it back through the wider saved block, and a hand-built ARP record
of four one-byte steps, typed RETREKTOR rather than at the shape a state writes so it reads at all, widening
with its jumps, its steps that sound nothing and their absence intact - the trap one such fixture had
already shown elsewhere, a root typed otherwise leaving fromValueTree to answer early with no macros at
all and an o.findArp (id)->steps deref then faulting at once; UiTest 629, +10, adds one self-contained
section after every section that reads the undo line's depth, since one more edit made upstream of those
checks moves the depth the paste and clipboard deltas ride and had already shown that trap twice, driving
the panel's own + Add macro leaving the cursor parked on the fresh macro's first step, the backtick spelling
it there claiming the press, spending exactly one point of the line for that one press and leaving the cursor
where it was, the opened step read back through the model as the sentinel and back through the panel's own
stepLabel as OFF against an empty one reading --, the step taken back to empty by Ctrl+Z and put again by
Ctrl+Y, and Backspace clearing the opened step at a point of its own; and EditorShot's --arp= grew the token
off, accepted the same way its own backtick is, so one flag can name either spelling and the Macros-tab shot
can draw the OFF cells among the jumps rather than only dashes.
This round they moved with the two slide letters: CoreTest 296, +17, six new sections past the pitch
bend ones, the model's two values above kOlderCmdTop and kCcTop with both letters and their row counts
surviving the saved form and an older state's calls at either end of its range still landing on the call
letter while this build's three wheel values read back as themselves, the climb to the top over a span of 4
landing on kBendTopPos exactly and returning only at the note-off below, D 0 reaching kBendBottomPos on its
own row, an argument-only row re-arming a span from where the wheel had got to and a further one over a wheel
already at the extreme moving nothing, a note-off ending a slide before its first step with nothing sent at
all, and a slide the clip runs out of having its ramp put away at that clip's end - those checks reaching the
wheel through the harness's own RawEvent kind for a pitch wheel rather than a new one, one of them having to
keep its row inside the 94 blocks the fixture runs rather than naming one past them; UiTest 652, +23, one
self-contained section after every undo-depth check, since the U claim there also spends a point of a line the
checks above had already counted to their cap and would otherwise read as one of the line's last points, that
section giving itself a pattern and a clip of its own parked clear of every other clip since the pattern the
tracker holds by that point sits on a clip the shared play helper never reaches, its climb read back as one
continuous run of 64-sample blocks rather than through the jumped helper, a transport jump releasing whatever
the synth was holding and that release being exactly what ends a slide the host jumped out of, that run primed
with a clear past the end of every clip so it answers only with its own rows, and its D-natural check naming the
octave it enters at since the sections above leave recordOctave at 9; and EditorShot gained --slide=ROW, --slide-down=ROW
and --slide-arg=VAL, the argument defaulting to 12 rows rather than to the 8 the letter's own entry leaves so one
shot can show all three command letters and all of their decimal arguments, /tmp/opencode/slide-u.png drawing the
lilac U 12 carried down from its row and /tmp/opencode/slide-d.png the lilac D 3, their differing sha256 being the
cheap proof the letters really are drawn there.
This round the same fixtures moved with the wheel's own grid rather than growing: CoreTest stayed at 296
checks with the six slide sections' arrays recomputed, a U 4 over four rows now reading 25 whole wheel
values rather than 5 with the row boundaries 10239, 12287, 14335 and kBendTopPos intact beside the steps
filling between them - that array first printed what the ramp really sent to find one mis-copied expected
value beside the real one rather than an engine miss, the printing loop then deleted once the array was
fixed - its retarget fixture's argument-only rows moving from rows 2 and 5 to rows 1 and 3 so each sits
inside the sub-row ramp above it and re-arms from 11604 the sub-rows had reached rather than from a whole-
row point, its note-off fixture now ending a D 4 a row below where the wheel had already sent 7851, 7510,
7168, 6827 and 6486 rather than ending one before its first step with nothing sent at all, and its clip
run-out reading 24 values rather than 4 since the clip's own end at 1.0 ppq falls between the naming row's
sub-row steps; UiTest stayed at 652 with its slide section retyping the argument from 12 to 2 so the same
12 wheel values are read over the two rows over whose sub-rows the wheel climbs rather than over twelve
rows, the values themselves numerically unchanged, which is what keeps every undo-depth check the sections
around it ride where they were while showing a wheel position landing on every sub-row of the span rather
than one per row; and EditorShot moved not at all, its --slide / --slide-down / --slide-arg flags drawing
the same two lilac letters with their decimal spans, both slide shots coming back with the sha256 they had
the round before - c19a018bf14bb9e1ae5b306c2ba8a88aadda0ff4ef3c301e674d242c2c6382fc at --slide=6 --slide-arg=12
and e1a76f3baf61481d2ee462be4c183d1d9551dbc00dd0588b8935fa217b9fe149 at --slide-down=10 --slide-arg=3 - the
two pictures being unchanged rather than a cheap proof of a new one, since what this round changed lives in
the engine rather than in anything the tracker paints, and this round's argument field's two letter spellings with them:
CoreTest went from 296 checks to 314 with sections of their own for the two spellings read back out of the saved form
and sitting above kCcTop so the read cannot be the one that clamps them back, an STP read under a carried U 4 stopping
the ramp after eleven sub-row steps and holding the wheel at 11946 until the note-off below sends the centre, an OFF
spelled by a bare 0 read under that same carried letter answering the same eleven values and leaving the column carrying
nothing under it so a further argument-only row answers nothing, an A 1 call on a column whose B 100 row had left the
wheel at 12800 sending the centre as its own next message while the run's steps below that row keep being measured from
the note the column named, a bare U 2 row closing the A 1 run its column was in with 12 note-ons of that run above the
closing row and the note 72 its last step entered released three times over - the step above's transition, the stuck-key
guard's and the close at that row - and the release ahead of the note the column strikes below the ramp being what leaves
nothing of that run sounding under it, and a U 2 written in the second tracker channel of the same MIDI channel leaving
that run stepping on with more than 20 of its note-ons below that row and every note of both commands reaching the one
channel the harness reads back as 1; the two arpeggiator-release fixtures moved to the re-trigger's counts rather than
losing their shape, the A 0 kill sounding 19 note-ons rather than 18 with the last note heard being the base 60 it re-struck
rather than the run's last step 64, and the release row hearing 13 note-ons rather than 12 with that same 60 last; UiTest
went from 652 to 676 with the slide section's new sub-block typing the spellings rather than reading pixels - a 0 typed
beside a named D landing kArgOff with carriedCommandNumber (25, 0) coming back empty, commandKindAt (24, 0) naming that
row's own letter, an s landing kArgStp with that letter and that argument still carried over the rows below it, an o typed
under a carried U landing kArgOff at a row that stays implicit with the read under it still coming back empty, an unclaimed
q leaving both the cell and the note column alone, an s beside a call asked for by nothing and refused rather than written
while the argument the entry had left stays in the cell, and the o beside a call landing the release's OFF spelling with
no call carried below it, each of those writes spending exactly one point of the undo line; and EditorShot gained no flag
since --slide-arg writes the cell's command value directly, so --slide-arg=128 names the OFF an o writes and --slide-arg=129
the STP its s writes and the shot's doc now says so, the two shots the round before pinned coming back byte-identical at
c19a018bf14bb9e1ae5b306c2ba8a88aadda0ff4ef3c301e674d242c2c6382fc at --slide=6 --slide-arg=12 and e1a76f3baf61481d2ee462be4c183d1d9551dbc00dd0588b8935fa217b9fe149
at --slide-down=10 --slide-arg=3 while this round made two new 1240x760 ones of its own, 5386fb80f9ccd0434cf122048522bb9e36e19eec98b8be9e50003b222afe5d26
at --slide=6 --slide-arg=128 showing the lilac U OFF painted at that row with the command column under it carrying nothing
and c1fc0fa130b2d570389be930639820e04f1655f1c1126be872751fda952a19bd at --slide-down=10 --slide-arg=129 showing the bright
D STP named there with the letter and that spelling carried dim over the four rows under it, both read back off crops of
the tracker band rather than off the whole-window pictures.
That harness grew this round's vibrato: CoreTest went 319 -> 379 checks PASS with 0 failures, its six new
sections naming the model - the grow-only table's one default macro at the plain sinusoidal shape at half full
pitchbend over a cycle of eight subticks, so the VIB MACRO tab has a card the way the ARP MACRO tab has steps,
its `firstUndefVibId` / `lastDefVibId` pair naming the same gap, naming the top id growing the grow-only table
to the id limit while id 0 stays the release call and there is no entry above the limit, naming an id extending
it with absent entries rather than moving the ones already in it so a fresh macro still takes the lowest gap,
clearing one leaving its room and the shape, amplitude and cycle its knobs had while the entry answers for nothing
of its own - which is what makes the engine stop a wobble whose macro was taken away rather than swing it at
nothing - and defining it again starting over from the defaults rather than from what the cleared one had still
held; the setters refusing an absent macro, so an id nothing entered never answers with a shape, an amplitude
or a cycle of its own, and the three settings clamping at their own tops rather than wrapping, which is what leaves
the cycle never at zero subticks - it clamps up to one - and the amplitude never past full pitchbend; the vibrato
letter's command value sitting above both the highest value an older build could write and the highest plain CC
the number field can spell, so no older row and no typed digit run can already be a vibrato, the letter and its
`OFF` and `STP` spellings surviving the saved form as the argument's own numbers above the field's top and a macro's
three settings arriving as their own properties, which is what a saved state brings the knob positions back at,
the saved form writing one entry per macro that is present rather than one per id the table has room for so a round
trip with a gap between two defined macros brings the two back at the ids they were at and leaves the gap a gap,
a state that had the default macro taken away reads back without it - which is what an undo that removed a vibrato
leaves it removed rather than the reset putting one back - and an older state's arriving as no section at all
leaves the read with the one default macro a fresh song defines rather than an empty tab; a wobble's grid, amplitude
and cycle - a cycle 6 subticks wide being exactly one row wide at the default Settings, so each of the three rows
a note is held over draws one cycle of eight points and the note-off below the last of them takes the wheel back
to the centre, and the full amplitude reaching the wheel's own extremes rather than one of them leaned against
further than it is: the height from the centre is the centre's own value, so the peak above would land one position
past the top and is clamped to it while the trough below lands exactly on the bottom - the amplitude being a share
of the wheel's own half height rather than a number of its own, so a knob at half swings half as far each way and
keeps the wheel's own centre as the point its cycle passes through, and a knob turned down to off moves the wheel
nowhere at all, the arming row's first point already sitting on the base and every point of the cycle sitting on
it too, which is what makes such a macro still a macro rather than no macro at all; the default macro's cycle of
eight subticks being one point per sub-row step and four thirds of a row wide so its cycle crosses the rows it
sits in rather than starting on each of them, a cycle 12 subticks wide being two rows wide and drawn with 12 points
of it rather than the 8 it is asked for at least, so its curve is cut at the twelfths of a cycle, and a cycle one
subtick wide still drawn with the 8 a cycle is never drawn with fewer of, so the wheel moves 8 times inside the
sub-row grid rather than once over it, 6 cycles of it land inside one row, each of them repeating the same 8 points,
and its last point leaves the wheel at 6744 rather than back at the base, which is what makes the 48th message the
note-off row below taking that point back to the centre rather than one more point of the cycle; the six shapes
and their Up and Down spellings drawing their cycle as the points of it the amplitude names, the sinusoidal one
curving and the triangle one ramping, the Up spellings having the cycle's second half inverted so the wobble never
reaches below its base and stays on one side of the pitch it swings about and the Down spellings having its first
half inverted so it never rises above it, the other half being the one the plain shape swung to the far side in -
at full amplitude those lean on the wheel's ends, the sinusoidal Up climbing to the wheel's top twice a cycle with
nothing below the base to go to, the fold landing on the peak the plain shape's second half would have reached below,
and the sinusoidal Down landing on the wheel's bottom twice a cycle and never rising above the base, that spelling
being the plain shape's cycle read through the base instead; the point a wobble swings about being wherever the
wheel already is rather than the centre, so one armed a row under a bend row swings about the point that row left
the wheel at, its peak clamped at the wheel's own top where the base has no room above it for the full swing, that
row above sending the point it named ahead of the swing's own first point and the two being one pitch rather than
a jump since the row bent the wheel at the same sample as the note it struck there, and a bend row under a running
wobble moving the point the swing measures about to the pitch it asks for rather than ending the swing or starting
a new one, the row's own point and the wobble's being one pitch and the points below reaching over the new base,
that bend row neither striking a note nor releasing the one the row above it held so the swing carries on across
it; a release row taking the wobble and the wheel it had left swinging away on its own row - the last point of
the cycle having moved the wheel to 2399 just above the bottom and the row's own message naming the centre rather
than a point of the cycle, so no pitchbent wheel is left over the rows below the release - while the note the row
above struck is still held to the note-off below, the release being about the wheel rather than about the note,
that centre arriving at the row that released rather than being saved for the note-off below; the `OFF` the entry
writes for an `o` doing what a `0` beside the letter does, the argument field's own top being one below what the
spelling needs, and a row naming only an argument below that release arming nothing, the release having left the
column calling nothing rather than calling id 0 the way a run's and a slide's release leave nothing below them;
an `STP` argument row asking for nothing and leaving the swing swinging over the rows below it, the row under that
naming another macro id re-arming the swing over that macro, its cycle counting again from that row rather than from
where the first stretch had got in its cycle, and that re-armed swing measuring its quieter cycle from the point
the first stretch's last point had got the wheel to rather than from where that stretch was armed, which is what
keeps the retarget from jumping to the quieter macro's own base; a row naming the letter itself and an `STP` argument
ending that row's own swing rather than re-arming one, a wobble having no move of the wheel to hold, and leaving
the wheel sitting at the point its last point had got to until the note-off below takes it back, that letter's own
row leaving the column calling it so a bare argument one row below re-arms the swing from the point the ended one
had left the wheel at - its fuller amplitude reaching over the base by more than the base has room for, the points
of it that would land below the wheel's own bottom all sitting on it rather than going past it - and that re-armed
swing saying nothing at its own arming row, whose first point sits on that base, which is what makes the first message
it sends one part way up its cycle rather than a jump to one of its extremes; a macro taken away while its wobble
swings ending the swing at its next point rather than at the row that took it and saying nothing of its own, the wheel
keeping whatever point of its own the swing had got to, whose last point sits above the base so the wheel is left
bent rather than one already at its centre, and a release row below is what puts that wheel away, the note-off
taking the point the swing had left back to its centre; and the clip those rows live in running out under a wobble
that armed no note of its own, a swing that clip's single cell armed having no rows below the clip left to keep
swinging over so its cycle stops at the clip's end and the wheel it had left swinging comes back to its centre there
- 16 bends of the two cycles of eight points it holds over the clip's two rows going out, the wobble armed with
no note of its own having nothing to release so the wheel being all that is left to put away, and the only all-notes-offs
there the ones the transport's own start asks for, one per channel, which is what leaves the clip's end sending
only the wheel's take-back rather than releasing anything of its own; the fixtures naming the wheel's own values
rather than the shapes that made them, the sinusoidal cycle at full amplitude over the default 6 subticks swinging
13985/16383/13985/8192/2399/0/2399/8192 per cycle - the peak one position past the wheel's top and clamped to
16383 while the trough lands exactly on 0 - a quarter of full pitchbend over a cycle one subtick wide repeating
9640/10240/9640/8192/6744/6144/6744/8192 six times inside one row so its last point sits at 6744 rather than back
at the base and the 48th message is the note-off row below taking it back, and a swing armed a row under a bend row
reaching over the new base by more than that base has room for, its wheel values clamped at 16383 above and 0 below
and a re-armed swing's first point sitting on the base its own row says rather than jumping to one of its extremes.
UiTest went 676 -> 734 checks PASS with 0 failures, its tab lookups renamed from the one `MACROS` button to `ARP
MACRO`, the VIB MACRO panel added to the pair-on, copy-pair-on and slice-one-on walks so all seven panels walk one
point per press against the plug-in's line rather than six, and its own VIB MACRO section driving the `ComboBox`
through `setSelectedItemIndex (…, juce::sendNotificationSync)` rather than a keyboard nudge - there is no UiTest
coverage of the tracker's own claim of the `V` letter, so that typing is pinned at the model by CoreTest and at the
paint by EditorShot rather than by a press driven through the grid - reading the dropdown's items back through the
panel's own `vibShapeLabel` and the amplitude through `vibAmplitudeLabel (25)` == `0.25` rather than through a second
copy of what the tab draws, the two knobs told apart by the span they walk rather than by the order they were added
in, the amplitude turning off to full pitchbend in hundredths - the same hundredths the model keeps - and the cycle
counting whole subticks one cycle takes from one to the limit a row's argument can name, the card opening at the
amplitude and the cycle a newly defined macro starts at, that macro getting its own dropdown and its two knobs,
the dropdown's own ids being one above the model's own, the dropdown's own change writing the shape to the macro -
which is what a row already calling that id swings by from there - and spending one point of the undo line of its
own, a knob turned writing its own setting, the card's three settings being three separate actions so once the line
is full one of them drops out of it at the far end, which is why those depth checks read the line back as `juce::jmin
(before + n, song.settings.undoSteps)` rather than one more than it can hold, `+ Add macro` taking the lowest id
that has none yet and moving the gap below it down the way the ARP MACRO tab adds one, the fresh macro arriving at
the shape, amplitude and cycle the model gives a newly defined one rather than at what the card below it had, and
`- Remove selected` putting away the macro the last gesture picked out and leaving the ids naming the same macros
below it, the same pairing the ARP MACRO tab uses, the macro that was never picked out keeping the settings the knobs
had left it at - the card being about which macro there is rather than about what it holds - and a macro defined
and a macro taken away being one action each; Smoke stayed at 13 checks PASS with the serialised state grown from
2392 to 2463 bytes since the `VIBS` section is saved state rather than the engine's three numbers. EditorShot gained
`--vib-shape=`, `--vib-amp=` (the hundredths of full pitchbend the model keeps, so 25 is the knob spelled 0.25),
`--vib-cycle=` (the subticks one sin or triangle cycle takes on the same grid an arpeggiator's steps and a slide's
ramp are measured over), `--vib-macros=N` (having the song define N of them, ids 1..N all of them fresh, which is
what makes such a shot long enough that the cards do not all fit and the tab has to draw its scrollbar and thumb),
`--vib-call=ROW` (naming a vibrato call in tracker channel 0 at that row, which paints the command column's lilac
`V` and its decimal macro id and leaves that letter and that argument carried dim over the rows under it in that
channel only) and `--vib-call-id=VAL` (naming the id that call writes, so `0` makes it the release, whose argument
is drawn `OFF` rather than as a `0` and which leaves the rows below it carrying no letter and no argument at all),
kept its short tab aliases (`--tab=macros` naming the ARP MACRO tab the older shots asked for and `--tab=vibs` naming
the VIB MACRO one) since the tab buttons now spell those two-word names out, and its usage header says so; its dumps
read the panel back at `VibMacrosPanel` 176,38 908x688, and five 1240x760 shots pinned this round: /tmp/opencode/arptab.png
991fffe6c52af501f45c55dd2e708b72a0b07bfaedf06737ac06fcbc0bbf6443 with the tab button now reading ARP MACRO, /tmp/opencode/vibmac.png
21a9117c19c6e2c6741cba068b82bf3f109d1b75e319b28025c19b6973720534 at --tab=vibs --vib-macros=8 --vib-shape=Triangle-Up
--vib-amp=75 --vib-cycle=12 so a card shows a shape picked out and two knobs turned rather than a fresh macro's own,
/tmp/opencode/vibmany.png at --vib-macros=25 --vib-shape=Sin-Down --vib-amp=40 --vib-cycle=3 whose dump reads 25
ComboBoxes and 50 sliders of which 22 are laid out at 116x22 and 3 hidden at 0x0 rather than drawn past the window -
the OUTSIDE PARENT lines there being the hidden ScrollBars and the disabled +/- buttons beside a slider rather than
cards clipped out - with the strip's thumb at the far end of its track and `V 25`'s own card clipped at the bottom
edge, and /tmp/opencode/vibletter.png 82f2b95c2f7f539e2cbbb987e57a64fe38e221a64121401b8cbc036d67dc6de3 at --vib-call=6
against /tmp/opencode/viboff.png 7580fec80bf3b70b5fe3720a2762b51e4e6595367b3b5bbf1f560384d79de305 at --vib-call=10
--vib-call-id=0, the first reading the tracker's lilac `V 1` carried dim over the rows under it and the second the
same row spelled `V OFF` with neither letter nor argument drawn under it; the baseline /tmp/opencode/vibnone.png taken
with no vibrato flags holds zero pixels of that exact lilac 0xffd8b4e8 while vibletter holds one - the middle of
one glyph of the `V 1` at full opacity, the rest of the glyph sitting over the dark background and blending - and
viboff holds eight, being the release's three-glyph `OFF` drawn in the same lilac, so the lilac those pictures
hold is drawn by the vibrato's own letters rather than by anything the demo's other rows already had. Neither harness
touches `kVibCmd` through the tracker's own key entry, so that letter's typing is pinned at the model by CoreTest
and at the paint by EditorShot rather than by a press driven through the grid, and the shot's own flags rather than
; this round CoreTest grew two fixtures for the two note-off cases the report named, "A note-off step at a
run's second step opens the note entered at the call row" - the call sitting beside the note it steps, an
empty first step sounding that note as it was typed at the call's own subtick, the note-off step under it
opening it at its own subtick rather than letting the stepped step under that re-articulate it, that
release being the run's own rather than the clip end's and the note the stepped step sounded being left
held to that end - and "Empty steps and a note-off step under a call that row has no note for", the same
at a call whose own row entered no note, where the rest opens the note the column entered above the call
instead, so a note-off step works at a run's first tick and at its second both with a note beside the call
and without one; three of its older arpeggiator fixtures were repaired for the first step now sitting at
the call row rather than one subtick below, the two whose offsets were counted from the note entering under
the call no longer hearing that note as typed once the first step names a jump and the looped cycle counting
the note entered at the call row as a step its cycle passes back onto rather than as a note held before the
cycle begins. UiTest grew a collectButtons helper beside the collectSliders and findToggle ones it already
had and rewrote its focus section for what can be read headlessly: the three checks that leaned on
getCurrentlyFocusedComponent were dropped, since Component::takeKeyboardFocus no-ops when getPeer () is
empty and that harness creates no desktop window so nothing there is ever focused at all, and replaced by
a checkNoButtonFocus lambda asserting of the Settings tab, the ARP MACRO tab and the VIB MACRO tab, each
opened the way a host clicks its name, that the Buttons inside it - its checkboxes, its cards' Add and Remove
pair, its LOOPED ticks and the +/- pairs inside its knobs - hold none of that focus once the panel is showing
and rebuilding its cards, beside the checks that no Button in the editor's tree takes it, that every knob
wants it, that the +/- pair a knob builds inside itself is there and holds neither, that the four panels
that own typed fields want it, and that each of the three tab panels spends a repeat of the Enter leaving
no checkbox flipped, no macro added or cleared and the undo line alone; its header's list of what it covers
grew the keyboard focus among them.
Its header's list grew a knob dragged along its own row beside that keyboard focus, and a new fixture grabs the
Settings tab's Undo steps knob the way a host grabs it - one mouseDown, several mouseDrags 12 px apart and one
mouseUp - and pins there that the drag moves the number the row shows and feeds the depth behind it, that the
whole drag earns one point of the undo line rather than one per event however many walked it there, that one
Ctrl+Z takes that point back to the number the row had before the drag, that the way back along the row is a
second action of its own rather than the events between the two numbers being spent one at a time and leaves
the number where the round trip began, that a second Ctrl+Z walks that second action too, and that a click of
the row's own + button still costs one step of its interval and one point of the line - the drag being a way
in rather than the only one - and ends by putting the tab back on the tracker so the sections below still read
its grid; that fixture's own loop over the pointer's path is bounded by x < toX and x > toX rather than by x
!= toX against a float, which hung a run at a 600 s timeout.
/media/data2/GameDev/Plugins/ReTrektor/JUCE: vendored framework.
Its juce_gui_basics/widgets/juce_Slider.cpp and juce_Slider.h are where a knob's row is heard: Slider's own
incDecButtonMode defaults to incDecButtonsNotDraggable at that file's line 1336, which is what made a
horizontal drag over a row worth zero delta, its handleMouseDown / mouseDrag / mouseUp trio driving the drag
once the row asks for the horizontal one, and its ScopedDragNotification currentDrag being what sends
sendDragStart () and sendDragEnd () at a gesture's two ends once each at the mouse-down and the mouse-up
rather than once per mouseDrag - that pair being what the plug-in's own onDragStart and onDragEnd hang the
undo line's one point of a whole drag on.
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
Git (newest first): 40d9804 the U and D slide's ramp interpolated over the Settings' sub-row grid rather
than moved to one new point once on every row of its span: the wheel is a state the column carries down
like a volume, a pan or an arpeggiator run and the two letters only arm a ramp, so what moves it belongs
to the position between the rows rather than to each row of its own - flushSlide now sits beside flushArp
at all three of process()'s flush sites and walks the ramp over sub-rows a row over the Settings' subticksPerRow,
the same grid an arpeggiator's steps are counted on, its steps counted from the ramp's own start in transport
ppq rather than read off the row, its limit being the end of the row the engine last owned so a row's own
steps wait for the block that reaches them rather than being bunched onto one sample, and a ramp whose next
step sits more than a row behind where the block starts ended outright the way an unplaceable run's is, which
is what a scrub or a jump leaves behind; a step's wheel position is slideFrom + (slideTo - slideFrom) * slideCount
/ juce::jmax (1, slideTotal), the count scaled before the division so the step sitting over the span's own
last row lands on the extreme exactly and the ones before it hold the whole position that far along the
straight line rather than a staircase of whole positions per row - a downward wheel sitting a hair above
the exact line, since integer division truncates toward zero - and a step whose whole position is the one
the wheel already sits at left unspoken, which is why slideCommand became arm-only at (ch, rowPpq, argument,
up, clipUid, song) and slideStep was deleted with the trailing call in processRow: the naming row's own step
at count 0 is exactly that, so even a span of 0 has its extreme sent by flushSlide on the naming row's own
sample at count 0 rather than by the arm of its own. ChannelState's slideRow0 / slideLastRow became slideOrigin
/ slideRowEnd beside slideTotal / slideCount beside the existing slideOn / slideSpan / slideFrom / slideTo
/ slideClip, the whole ramp re-counted as slideSpan * subticks so an argument-only row that re-arms mid-climb
still lands its own extreme that many rows below where it armed rather than where the old row grid had put
it; slideRunning () went from comparing rows to slideCount <= slideTotal with slideOn cleared only after the
ramp's last step, which is what left the existing else if (c.slideRunning ()) c.endBend () release guards in
allNotesOff and stopNote and the (bendClip == clip.uid && wheelBent) || (slideClip == clip.uid && slideRunning ())
clip-end take-back unchanged, a ramp that reached its extreme still leaving the wheel at a point of the
column's own for those same points to take back. Each step is queued at the pitch wheel's own order 0 so it
precedes the note-on of the row it sits inside, and the position arithmetic stays integer throughout so the
wheel stays 14-bit while only the step's time is a double. What the fixtures now show is the row's own sub-row
steps landing inside a block boundary: a U 4 over four rows sends 25 wheel messages rather than 5 with its
row boundaries 10239, 12287, 14335 and kBendTopPos intact beside the steps filling between them; its retarget
fixture's argument-only rows moved from rows 2 and 5 to rows 1 and 3 so each sits inside the sub-row ramp
above it and re-arms from 11604 the sub-rows had reached mid-climb rather than from the whole-row point above
it; its note-off a row below a D 4 now ends a ramp that had already sent 7851, 7510, 7168, 6827 and 6486
rather than one that had sent nothing at all; and its slide the clip runs out of sends 23 of its 48 sub-row
steps rather than 3 of its 8 rows, the clip's own end at 1.0 ppq falling between the naming row's sub-row
steps and that still-climbing ramp being put away there exactly as it was before. CoreTest stayed at 296
checks and UiTest at 652 with the same values at the row boundaries and new ones between them, one CoreTest
array first printed against the ramp's real values to find a mis-copy rather than an engine miss, and UiTest's
slide section retyping its argument from 12 to 2 so its 12-value climb stays 12 values over two rows rather
than twelve - the counts staying put is what keeps every undo-depth check the sections around it ride where
they were; the tracker's drawing, the typing and the Settings rows were untouched, so both slide shots came
back with the sha256 they had the round before. Smoke stayed at PASS with 0 failures and 13 checks, its
serialised state still 2392 bytes since the ramp costs the saved form nothing of its own, against the bundle
refreshed by rsync -a --checksum over the bundle's DIRECTORY rather than its single .so, the built and installed
binary both sha256 6d9b4362fcf76ca3c859c7617a977fbaa7984bbaa774b195acc2c31c7d6d61f0 with the sha the previous
build carried gone, as it must since the engine changed rather than the saved form, the portability read still
topping out at GLIBC_2.38 with GLIBCXX_3.4.32 and naming no sqrtf or atan2f relocation - the new ramp's arithmetic
being integer throughout, so the only single-precision names the binary keeps are JUCE's own powf, sinf, cosf,
expf, logf, sincosf and atan2.
0c00ba7 the Command column gains the letters U and D as 145 and 146, a slide of the pitch wheel to its
top or its bottom over the rows their argument names: the naming row starts the ramp where the wheel already
is and sends nothing of its own so that row's note plays straight, the rows under it move the wheel one whole
position at a time and land on the extreme exactly, an argument of 0 reaches it there, a row under a carried
letter re-arms a span from where the wheel had got to, a note-off or a clip's end puts a still-climbing ramp
away even before its first step moved the wheel, and the two values sit above kOlderCmdTop so no older row
can already be a slide. A slide's argument counts rows rather than naming a point, which is the one thing that
made it a new kind rather than another spelling of the wheel the bend already owned: the bend had a point and
one move, a slide has a ramp, so the state grew slideOn / slideRow0 / slideSpan / slideLastRow / slideFrom /
slideTo / slideClip beside a slideRunning () guard and an inline endSlide (), and the ramp is walked by integer
truncation of slideFrom + (slideTo - slideFrom) * steps / juce::jmax (1, span) rather than by a float run, which
is what makes the endpoints exact and a row landing on the position the wheel already sits at send nothing at
all. The step reads the row rather than counting it, so a transport jump that makes this block play a row again
leaves it on the same point of the ramp rather than advancing it twice - the ramp advancing in processRow through
one unconditional slideStep placed after the command chain rather than inside either letter's branch, its wheel
queued at the pitch wheel's own order 0 so a note entering while the wheel climbs is bent by where the climb had
got to rather than by the point above it, the two span guards needing juce::jmax rather than a bare max since
nothing in this file pulls the C math names in, and a row naming a command that is not a slide calling endSlide
() in the carry block rather than leaving a ramp still climbing under it. A slide is carried state like a bend
rather than one its own row only, so every release point that ended a bend now ends a slide too: allNotesOff and
stopNote each gained an else if (c.slideRunning ()) c.endBend (), which is what ends a slide at a note-off even
before its first step moved the wheel and why nothing keeps climbing under that note-off, and the clip-end take-back
went from bendClip == clip.uid && wheelBent to (bendClip == clip.uid && wheelBent) || (slideClip == clip.uid && slideRunning
()) with the centring message only sent where wheelBent says the wheel really is out of place, since a ramp still
climbing at a clip's end has no rows below that clip left to finish it even when its own wheel never left the centre -
one ramp needs putting away and no message is what takes it back, which is also why kBendBottomPos = 0 came in beside
kBendTopPos as the wheel's own ends rather than as a literal 0 at two call sites. The model took kOlderCmdTop = 143,
kSlideUpCmd = 145, kSlideDownCmd = 146 and kSlideDefaultSpan = 8, and the older-command read rule went from rawCmd >=
kArpCmd to rawCmd >= kArpCmd && rawCmd <= kOlderCmdTop, so an older state's macro calls at either end of its range
still land on the call letter while 144, 145 and 146 read back as themselves; the two new values also sit above kCcTop
= 127, the number field's own top, so a run of typed digits cannot invent either letter - typing 146 gives the highest
CC - while the note column's own D keeps entering a D natural there and an interpolation ramp still climbs toward the
highest CC rather than inventing a command. Both letters draw in the lilac theme::cmdOther the bend pair already had,
with their arguments in decimal beside them and the entry claiming the field's first character and writing the default
span of 8 rows as the argument it has to leave, so a row naming only a letter takes its extreme over 8 rows rather than
over none at all, and the pool's grid-preview tick paints that lilac for all three wheel kinds. The tests moved with all
three layers: CoreTest 296, +17, six new sections - the model and the older-state read, the climb to the top over a span
of 4 landing on kBendTopPos exactly and returning only at a note-off below, D 0 reaching kBendBottomPos on its own row,
an argument-only row re-arming a span of 2 from where the wheel had got to while a further one over a wheel already at
the extreme moves nothing, a note-off one row below ending a slide before its first step with nothing sent at all, and a
slide of 8 rows over a four-row clip climbing 9215, 10239 and 11263 and being put away at that clip's end; UiTest 652,
+23, one self-contained section after every undo-depth check, since a claim there also spends a point of a line the checks
above had already counted to their cap - it gives itself a pattern and a clip of its own parked clear of every other clip,
reads the whole twelve-row climb back as one continuous run of 64-sample blocks under that clip rather than through the
shared jumped helper, a transport jump releasing whatever the synth was holding and that release being exactly what ends a
slide the host jumped out of, primes it past the end of every other clip so the run answers only with its own rows, and names
the octave its D-natural check enters at since the sections above leave recordOctave at 9; and EditorShot gained --slide=ROW
/ --slide-down=ROW / --slide-arg=VAL, 12 by default rather than the 8 the letter's entry leaves, so the two shots at
/tmp/opencode/slide-u.png and /tmp/opencode/slide-d.png show all three command letters and all of their decimal arguments.
Smoke stayed at PASS with 0 failures and 13 checks and its serialised state at 2392 bytes, a slide costing the saved form
nothing of its own since the two values and their arguments live in the cells a pattern already saves, against the bundle
refreshed by rsync -a --checksum over the bundle's DIRECTORY - the single .so being what the earlier attempt asked of it and
rsync answering that with Not a directory - so the built and installed binaries are both sha256 3d0755c45500d341907af8c0fca8d9e6304fcce08575c59f9f7894e77f759bc9
and the sha the previous build carried is gone, and the portability read still tops out at GLIBC_2.38 with GLIBCXX_3.4.32 and
names no sqrtf or atan2f, the slide's arithmetic being integer throughout so the only float names the binary keeps are JUCE's
own powf and sinf.
5a14699 an arpeggiator macro step spells the note column's note-off: the backtick
the note entry answers there opens a step, that step opens only the note its own run was holding rather
than sounding one of its own, and the saved steps block grew to two bytes a step so the opening reads back
as it was. One step's int widened rather than a flag beside it, since a step already used every value above
-128 as a jump and -128 as the step that sounds nothing, so there was no room left for a third spelling and
the ask could not be read at all until one more bit of width was given to it: ArpMacro::steps went to
std::vector<std::int16_t>, kArpEmptyStep stayed at -128, the new kArpNoteOffStep took -129 below it and the
new kArpTopStep capped a typed jump at ±127 so the two spellings below -128 are the only values a step can
hold that are not jumps, setArpStep and the octave snap letting both sentinels through untouched on either
side - which is what keeps the opening a shape rather than a landing the snap would have folded into a
note. The engine answers it inside flushArp rather than as another kind of row: at that subtick it stops
only the note this very run entered, through stopNote on the state's own arpNote and only while the state
still holds that very note, then sounds nothing and still counts the step, so the run stays where it was,
a looped cycle's boundaries stay exact beside the loop and nothing a column, another run or the row itself
was holding is cut by a step; a step of OFF at a run's head therefore releases nothing of its own, since
the column's own note is still owned lower down and the row's note-off takes it away there, and the note an
opening cut is not sounded again until a later step names one. closeArp and endArp, the row's whole `A 0`
release, the note-off taking the column's note away and the clip end's close all stayed as they were beside
it. The saved ARP block writes two bytes a step little endian because the opening sits below what one byte
can name, and the reader tells the two spellings apart by the length the block declares rather than by what
it finds - a one-byte block is exactly as long as half a wide one's - so an older state's steps widen
sign extended with their jumps, their steps that sound nothing and their absence intact, which is what
lets a step beyond a one-byte jump and an opening survive a state while a project saved before this round
keeps what it had. The Macros tab shares one spelling rather than keeping a second: the panel's file-local
stepText lambda became the public MacrosPanel::stepLabel (int step), which the strip's paint and the tests
read, so OFF, the note column's own word for a release rather than === which is not a step, is what an
opened cell draws in the jump's own orange and an empty one draws -- in the release's dim, the band's alpha
and the colour rule otherwise untouched; the backtick and its shifted tilde, the pair the note entry answers
alike, write the opening at the step cursor, claim the press so no letter of the note table is typed beside
it, fire onModelChanged once for it and leave the cursor on the step it opened rather than stepping past
it the way Delete and Backspace leave the step they cleared, since stepping past it would run the macro
past the release the user meant to hear there; and commitStepText now hands the raw value to the setter
rather than clamping it a second time at the panel, the setter owning that clamp so one rule is not stated
twice. The tests moved with all three layers: CoreTest 279, +14, names an opening mid-run ordering its
release between the note-ons either side of it and counting one release per opening, one at a run's head
releasing nothing while the tail of the same run closes what the next step entered, a looped cycle stepping
through its openings and sounding itself again after the one release, and that the only note an opening ever
stops is the one that run entered, so nothing a column or another run was holding is cut - those sections
asserting the order of the raw events inside the clip rather than at absolute samples, since h.notes and
h.raw are block-relative; the round trip putting an opening at a step and reading it back through the wider
block, and a hand-built older ARP record of four one-byte steps widening with its jumps, its empties and its
absence intact, that fixture's root typed RETREKTOR because fromValueTree answers early for anything else
and leaves no macros at all, which is what had already made an o.findArp (3)->steps deref fault elsewhere;
UiTest 629, +10, one self-contained section placed after every section that reads the undo line's depth -
a step written earlier in the run moves the depth the paste and clipboard deltas ride, the trap two earlier
sections had already shown - driving the panel's own + Add macro leaving the cursor on the fresh macro's
first step, the backtick spelling that step and claiming the press, exactly one point of the line for that
one press, the cursor left where it was, that step then read through the panel's own stepLabel as OFF against
an empty one reading --, Ctrl+Z taking the step back to empty and Ctrl+Y putting the opening again, and
Backspace clearing the opened step to the one that sounds nothing at a point of its own; EditorShot's
--arp= grew the token off beside the literal backtick so one flag can name either spelling, and the Macros-tab
shot at /tmp/opencode/arp-off.png - 1000x640, --tab=macros --arp=+0,off,+4,-8,off,+12 --macros=4 - draws the
OFF cells among the jumps in the jump's own orange, against /tmp/opencode/arp-empty.png with dots in the
same places, whose different sha256 is the cheap proof the word really is drawn rather than a dash; the
9 files changed being Source/Model.h Source/Model.cpp Source/PlaybackEngine.h Source/PlaybackEngine.cpp
Source/ui/MacrosPanel.h Source/ui/MacrosPanel.cpp tests/CoreTest.cpp tests/UiTest.cpp tests/EditorShot.cpp.
9cda2f5 the carried argument follows the newest argument the column entered
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
