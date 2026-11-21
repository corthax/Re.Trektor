#include "PlaybackEngine.h"
#include <algorithm>
#include <set>
#include <cmath>

namespace rtr
{

/** How far behind a transport start (in quarter notes) a row still counts as
    sitting on it: about half a nanosecond of transport, so this only absorbs
    the rounding a host's per-block position reporting brings in, never a real
    row, which is a whole row span away. */
constexpr double kRowAlignEps = 1.0e-9;

void PlaybackEngine::reset()
{
    for (int i = 0; i < kMaxTrackerChannels; ++i)
    {
        auto& c = channels[(size_t) i];
        c = ChannelState();
        c.lastInst = juce::jmin (i, kMidiChannelCount - 1);
        c.lastVol = kVolDefaultVel;
        uiRow[(size_t) i].store (-1);
        uiMidiChannel[(size_t) i].store (c.lastInst);
    }

    for (int i = 0; i < kMidiChannelCount; ++i)
    {
        vols[(size_t) i] = VolState();
        everPlayed[(size_t) i].reset();
    }

    wasPlaying = false;
    previousPpq = -1.0;
    justStarted = false;
}

void PlaybackEngine::forgetTransport()
{
    wasPlaying = false;
    previousPpq = -1.0;
    justStarted = false;
}

void PlaybackEngine::allNotesOff (std::vector<PendingEvent>& events, int offset, bool soundOff)
{
    for (int i = 0; i < kMaxTrackerChannels; ++i)
    {
        auto& c = channels[(size_t) i];

        if (c.activeNote >= 0)
            events.push_back ({ offset, 0, juce::MidiMessage::noteOff (c.activeMidiCh + 1, c.activeNote) });

        // a wheel this channel had left bent, or a ramp still climbing under it,
        // is put back here: endWheel sends the message that says so only where the
        // wheel itself had moved, which is what a ramp whose sub-rows had not
        // reached one yet needs nothing but putting away
        endWheel (events, i, c.activeMidiCh + 1, offset);

        c.activeNote = -1;
        c.lastRow = -1;
        c.endArp();

        // the all-notes-off has just taken every pitch this channel was holding
        // away, so a cut that was still waiting for its subtick has nothing left
        // to close and a retrigger has nothing left to strike again; the letter the
        // column carries keeps gating or holding the period for the notes entered
        // after the re-arm the way a carried volume or command number does
        c.cutWaiting();
        c.retrigWaiting();
        uiRow[(size_t) i].store (-1);
    }

    // The channel-wide controllers sort ahead of a note-on at this same offset
    // (a note-on is order 1): they release what the synth may still be holding
    // from the position the transport has left, and a host that honours an
    // all-notes-off, which Bitwig does while Reaper's receivers ignore it, would
    // otherwise release the note whose row sits exactly at the new position,
    // leaving it cut short at a start or missing after a loop wraps.
    for (int ch = 0; ch < kMidiChannelCount; ++ch)
    {
        events.push_back ({ offset, -2, juce::MidiMessage::allNotesOff (ch + 1) });

        // All Sound Off has no companion message turning it back on, so it only
        // belongs where the whole channel is meant to go silent for good.
        if (soundOff)
            events.push_back ({ offset, -1, juce::MidiMessage::allSoundOff (ch + 1) });
    }
}

void PlaybackEngine::panic (juce::MidiBuffer& midi)
{
    std::vector<PendingEvent> events;
    allNotesOff (events, 0, true);

    for (auto& e : events)
        midi.addEvent (e.message, 0);
}

void PlaybackEngine::startNote (std::vector<PendingEvent>& events, int offset, int ch,
                                int midiCh, int note, int velocity)
{
    auto& c = channels[(size_t) ch];

    if (c.activeNote >= 0)
        events.push_back ({ offset, 0, juce::MidiMessage::noteOff (c.activeMidiCh + 1, c.activeNote) });

    // The state may have been cleared while the synth still holds this pitch
    // (Reaper does not call a MIDI effect when the transport is stopped, so
    // stop-time note-offs may never be delivered). Guarantee re-articulation -
    // unless that pitch was already opened at this very sample by a release this
    // channel queued there, which is the clip's own end ahead of a note entered
    // at the row below it: a second note-off of the same pitch beside the note-on
    // one sample later is what a host reads as the new note cut at once
    if (everPlayed[(size_t) midiCh].test (note) && c.activeNote != note
        && ! (c.relNote == note && c.relMidiCh == midiCh && c.relOffset == offset))
        events.push_back ({ offset, 0, juce::MidiMessage::noteOff (midiCh + 1, note) });

    // A wheel that is still bent from a row above is left where it is: a bend is
    // a state the column carries down like a volume or a pan, so the notes below
    // the row that asked for it are bent until a release point takes it back
    events.push_back ({ offset, 1, juce::MidiMessage::noteOn (midiCh + 1, note, (juce::uint8) juce::jlimit (1, 127, velocity)) });
    everPlayed[(size_t) midiCh].set (note);

    c.activeNote = note;
    c.activeMidiCh = midiCh;
    uiMidiChannel[(size_t) ch].store (midiCh);
}

void PlaybackEngine::stopNote (std::vector<PendingEvent>& events, int offset, int ch, int midiCh, int note)
{
    events.push_back ({ offset, 0, juce::MidiMessage::noteOff (midiCh + 1, note) });

    auto& c = channels[(size_t) ch];

    // the wheel this channel had left bent, or a ramp still climbing under it,
    // goes back to its centre with the note this row releases, at this row's own
    // sample and ahead of anything else the row asks for
    endWheel (events, ch, midiCh + 1, offset);

    if (c.activeNote == note)
        c.activeNote = -1;

    // the release is in force at this sample for as long as the block being
    // queued lasts, which is what lets a note entered at that same sample call
    // its own pitch already opened
    c.relNote = note;
    c.relMidiCh = midiCh;
    c.relOffset = offset;
}

//==============================================================================
/** Ends the pitch wheel this channel has in force: the message that puts it back
    at its centre only where the channel had actually left it bent, and no wheel,
    ramp or wobble in force below that. A ramp that was still climbing while its own
    wheel never left the centre, or a wobble that had only just started its cycle and
    so was still sitting on its base, has only itself to put away, which is why no
    message is what ends it. The channel the message goes to is the one the row
    that ends the wheel names, the same way a bend row's own instrument names it,
    rather than the one a note the channel was holding came from. */
void PlaybackEngine::endWheel (std::vector<PendingEvent>& events, int ch, int midiCh, int offset)
{
    auto& c = channels[(size_t) ch];

    if (c.lastBendPos != kBendCentrePos)
    {
        events.push_back ({ offset, 0, juce::MidiMessage::pitchWheel (midiCh, kBendCentrePos) });
        c.endBend();
    }
    else if (c.slideRunning() || c.vibRunning())
        c.endBend(); // a ramp or a wobble still in force with its wheel never having
                     // left the centre has only itself to put away: a wobble in
                     // particular sits on its base at the start of its cycle, so
                     // nothing about its wheel says it is still swinging
}

//==============================================================================
/** The close of an arpeggiator run, which releases the note its last step
    entered. The first step of a run whose call row named a note is that row's
    own note, sounded at the pitch that step moved it to, so it is the run's own
    and gets released here like any other step; a note the column entered at
    another row, or one the run has only stepped away from without sounding, is
    its base rather than one of its steps and belongs to whatever ends it below,
    which is what a run whose steps all sound nothing leaves the note above it
    alone. */
void PlaybackEngine::closeArp (std::vector<PendingEvent>& events, int offset, int ch)
{
    auto& c = channels[(size_t) ch];

    if (c.activeNote >= 0 && c.arpNote >= 0 && c.activeNote == c.arpNote)
        stopNote (events, offset, ch, c.activeMidiCh, c.activeNote);
}

//==============================================================================
/** The pitch a step's jump lands on, snapped back into the keyboard by whole
    octaves rather than being cut off at either end, which is what keeps the
    step's shape a run is stepping through. Shared by a run's own steps below and
    by the first of them above, the step a call row's note is sounded as. */
static int arpSteppedNote (int note) noexcept
{
    while (note < 0)
        note += 12;

    while (note > 127)
        note -= 12;

    return note;
}

//==============================================================================
/** The first step of the macro an arpeggiator call names, or the step that
    sounds nothing when the call names an id that has no macro or no steps
    behind it: the note handling above arpCommand reads it before the note
    entered at a call row is triggered, since that note is this run's first
    step rather than a note the run leaves alone - the step's jump moves the
    note, its empty spelling leaves it where it was typed and its note-off
    spelling sounds nothing here at all. */
static int arpFirstStepOf (const Song& song, int id) noexcept
{
    const auto* macro = song.peekArp (id);

    if (macro == nullptr || ! macro->defined())
        return kArpEmptyStep;

    return (int) macro->steps[0];
}

//==============================================================================
/** The macro id the arpeggiator call written at this row arms, or 0 when the
    row arms no run: the letter's own argument beside the letter, and the
    argument a row names under a call that is already in force above it, whose
    run is started over at that row. A release (the id 0 a row names or the OFF
    spelling) and an STP one arm nothing, and neither does a bare note row
    under a carried call, whose run is still stepping and only re-bases. The
    note handling above arpCommand reads this beside arpFirstStepOf so the note
    entered at a call row knows what the call about to be answered below is
    going to do to it, and arpCommand itself arms the very same call. The letter
    in force is passed rather than the whole channel, the row having already left
    it there above the note handling. */
static int arpCallArmedAtRow (const Cell& cell, int lastCmd) noexcept
{
    // a row naming a letter had already left it in force above the note handling,
    // so the call this row makes is told by the letter in force and an argument,
    // which is also how a row naming only an argument under a carried call reads
    if (lastCmd != kArpCmd || cell.cmdVal == kEmpty)
        return 0;

    const int id = cell.cmdVal;

    if (id <= 0 || id > kMaxArpMacros)
        return 0; // id 0 and the two spellings above the id field's own top name the release and the STP rather than a macro

    return id;
}

//==============================================================================
/** Arpeggiator playback. A row naming the command only queues the run: its steps
    are one subtick wide, the subtick being a row divided by "Subticks per row",
    so a macro longer than that, which is what 64 steps against a default of 6
    means, cannot be squeezed inside the block the row falls in - at these sizes a
    row is some 5900 samples against a block of 512, and an event queued past the
    end of a block is clamped to its last sample by process(), which would bunch
    every step of the run onto one sample. So the steps wait in the channel state,
    with where the next one sits in absolute transport position, and the block that
    reaches it emits it there. The limit is never past the end of the row the
    engine last owned, which is what leaves a run the column supersedes lower down
    with the steps that were still below it rather than replaying them. */
void PlaybackEngine::flushArp (std::vector<PendingEvent>& events, int ch, double ppqStart,
                               double ppqLimit, double ppqPerSample, int numSamples,
                               const Song& song)
{
    auto& c = channels[(size_t) ch];

    if (c.arpId <= 0)
        return;

    const auto* macro = song.peekArp (c.arpId);

    if (macro == nullptr || ! macro->defined())
    {
        c.endArp();
        return;
    }

    const int len = macro->length();
    const double rowsPerBeat = (double) juce::jmax (1, song.settings.rowsPerBeat);
    const double rowSpanPpq = 1.0 / rowsPerBeat;
    const int subticks = juce::jmax (1, song.settings.subticksPerRow);
    const double stepPpq = rowSpanPpq / (double) subticks;

    if (! macro->looped && c.arpCount >= len)
        return; // the run is over: its last step stays sustained

    // a run whose next step sits more than a row behind where this block starts
    // cannot be placed any more, which is what a scrub or a jump leaves behind,
    // so it ends rather than sounding late
    if (c.arpOrigin + (double) c.arpCount * stepPpq < ppqStart - rowSpanPpq)
    {
        c.endArp();
        return;
    }

    const int midiCh = juce::jlimit (0, kMidiChannelCount - 1, c.lastInst);
    const int vel = juce::jlimit (1, kCcTop, c.lastVol);

    while (macro->looped || c.arpCount < len)
    {
        // the step's position is the start of the run plus its count, never a
        // sum of step widths, so a loop that cycles many times stays exact
        const double stepTime = c.arpOrigin + (double) c.arpCount * stepPpq;

        if (stepTime >= ppqLimit)
            break;

        if (stepTime >= ppqStart - kRowAlignEps)
        {
            const int offset = juce::jlimit (0, juce::jmax (0, numSamples - 1),
                                             (int) std::llround ((stepTime - ppqStart) / juce::jmax (1.0e-12, ppqPerSample)));

            const std::int16_t step = macro->steps[(size_t) (macro->looped ? c.arpCount % len : c.arpCount)];

            // the steps measure from the note the column last named, which a note
            // entering further down the column re-bases by being that note; a run
            // whose call was answered at a note-off row steps from the note that
            // row was releasing, which the note-off took from the column for the
            // rows below it, until a note enters or another note-off ends it
            const int base = c.columnNote >= 0 ? c.columnNote : c.arpCallNote;

            if (step == kArpNoteOffStep)
            {
                // the note-off step: it opens whatever the channel is holding at
                // its subtick rather than stepping anywhere, which is the note the
                // run last entered when the run has already sounded one and the
                // note the column entered above its call row when it has not - a
                // run whose head step or whose leading empty steps end on a note-
                // off step is a rest over that note, so the note the rows above had
                // left held goes silent there rather than being held on to whatever
                // ends it below. The column keeps naming that note either way, which
                // is what lets the steps under the rest measure from it again
                if (c.activeNote >= 0)
                    stopNote (events, offset, ch, c.activeMidiCh, c.activeNote);

                // the run keeps stepping: what it was holding is gone, so the next
                // step is the one that has to sound a note of its own again
                c.arpNote = -1;
            }
            else if (step != kArpEmptyStep && base >= 0 && base <= 127)
            {
                // the steps measure from the note the column last named, which a
                // note entering further down the column re-bases while a run is
                // still going, and a step landing off the keyboard snaps back by
                // whole octaves so it keeps its shape
                const int note = arpSteppedNote (base + (int) step);

                startNote (events, offset, ch, midiCh, note, vel);
                c.activeClip = c.arpClip;
                c.arpNote = note;
            }

            // an empty step sounds nothing, which sustains the step before it,
            // and the next step releases what the step before it left holding, so
            // a looped run's cycle keeps the note its last step entered sounding
            // until the next cycle's first step reaches its own subtick; a note-off
            // step is the one that lets go of that note instead of holding it. What
            // is not stepped here is the run's first step when the row that called
            // it named a note of its own: that note was already sounded at the row,
            // as the pitch the first step moved it to, which is why the run picks up
            // at the step below it
        }

        ++c.arpCount;
    }
}

void PlaybackEngine::arpCommand (std::vector<PendingEvent>& events, int ch, int id,
                                 int offset, double rowPpq, const juce::Uuid& clipUid,
                                 bool noteOnRow, bool noteOffRow, const Song& song)
{
    auto& c = channels[(size_t) ch];

    // an STP argument beside an arpeggiator call means nothing: a run has no wheel
    // to stop, so such a row sounds nothing and leaves the run above it as it was
    if (id == kArgStp)
        return;

    // the release spells itself two ways: the id 0 a row named beside the call
    // letter, and the OFF argument the entry writes for an o, which sits above the
    // field's own top and has to be read as the release before the id is clamped
    // down, since clamping first would land it on the highest macro instead
    const bool releaseCall = id == 0 || id == kArgOff;

    id = juce::jlimit (0, kMaxArpMacros, id);

    if (id == 0 || releaseCall)
    {
        // the note the run measured from, read before the run is put away below:
        // the column's own note where it has one, the note the call answered
        // against where a note-off took that away
        const int base = c.columnNote >= 0 ? c.columnNote : c.arpCallNote;

        // the release call: a row that named a note had already released what it
        // was holding before striking its own, and a row that names a note-off
        // released it as the note-off itself, so only a bare row has to
        if (! noteOnRow && c.activeNote >= 0)
            stopNote (events, offset, ch, c.activeMidiCh, c.activeNote);

        c.endArp();

        // turning a run off goes back to the note it stepped from rather than only
        // cutting the sound: the base note the column named is struck again at this
        // row, so a row that releases the arpeggiator keeps the column sounding its
        // own note until something below ends it, its note-off and its clip end
        // owning that note the way they owned the one a call row struck. A row that
        // struck a note of its own had just struck that base, and a row that names
        // a note-off is what took it away on its own, so neither re-strikes
        if (! noteOnRow && ! noteOffRow && base >= 0 && base <= 127)
        {
            const int baseMidiCh = juce::jlimit (0, kMidiChannelCount - 1, c.lastInst);
            const int baseVel = juce::jlimit (1, kCcTop, c.lastVol);
            startNote (events, offset, ch, baseMidiCh, base, baseVel);
            c.activeClip = clipUid; // the re-struck base belongs to the rows this clip has
        }

        // a release leaves the column calling nothing rather than calling id 0,
        // which is what the tracker stops drawing a carried letter and argument
        // over the rows below it and what stops a further row naming only an
        // argument from starting a run there
        c.lastCmd = -1;
        c.lastCmdVal = -1;
        return;
    }

    c.arpId = id;
    c.arpClip = clipUid;
    c.activeClip = clipUid;

    // the run this row's call replaces is stopped here rather than carried into
    // the new one: the note its last step entered is released at this row's
    // first subtick, so the new run steps from the note the column named instead
    // of continuing from whatever note that run happened to be sounding. A row
    // that struck a note of its own had already released it above this call, and
    // a row that names a note-off released the note as the note-off before
    // getting here, so in both those cases this close has nothing left to do.
    if (! noteOnRow)
        closeArp (events, offset, ch);

    // the run's steps measure from the note the column named - the call row's own
    // note where it named one, the note its rows above left named where it did
    // not - and the run keeps that note as its own answer for as long as the
    // column has none, which is what a call written beside a note-off walks the
    // note that row released rather than sounding nothing at all. The note entered
    // at the call row is the pitch that note was sounded as, which processRow above
    // had that row's own note sound as - the first step moved it there, left it where
    // it was typed or made that step a rest and took the held note away - and that
    // pitch is what arpNote was left naming there too, the run's own rather than the
    // note as typed, which is what a note-off step at the row below it releases
    c.arpCallNote = c.columnNote;
    c.arpOrigin = rowPpq;

    if (noteOnRow)
        c.arpCount = 1; // the row's own note was this run's first step, sounded at the row as that step moved it, so the run picks up at the next one
    else
        c.arpCount = 0;
}

//==============================================================================
/** A row naming the bend letter: the argument names a point on the 14-bit wheel
    in steps of kBendStep, so the pair the host gets is the standard pitch bend
    one and the argument's centre bends nowhere. An argument the row left empty
    is the centre as well, which is what makes the letter on its own a row that
    asks for nothing rather than one that locks the wheel to its bottom - and it
    is what the tracker's own entry writes for that letter. The row's MIDI
    channel is the one its instrument column carries, like every other command
    row, and the clip whose rows own the move is remembered so that clip's end
    can take the wheel back even when the row had no note of its own. */
void PlaybackEngine::bendTo (std::vector<PendingEvent>& events, int ch, int midiCh,
                             int argument, int offset, const juce::Uuid& clipUid)
{
    auto& c = channels[(size_t) ch];

    const int arg = argument >= 0 ? juce::jlimit (0, 127, argument) : kBendCentreVal;
    const int position = juce::jlimit (0, kBendTopPos, arg * kBendStep);

    // the wheel moves before anything else on the row asks for it, which is what
    // bends the note that same row strikes rather than leaving it straight until
    // the row below it; a note-off that had just taken the wheel back sits ahead
    // of this one, so the host sees the centre and then the new point
    events.push_back ({ offset, 0, juce::MidiMessage::pitchWheel (midiCh + 1, position) });
    c.lastBendPos = position;
    c.bendClip = clipUid;

    if (c.vibOn)
        c.vibBase = position; // a wobble this column has swinging moves about the point
                              // a bend row below it moved the wheel to rather than about
                              // the point it was armed at, and keeps swinging: the bend
                              // asked for a new point, not for the wobble to end, so its
                              // cycle carries on from where it was in it and the points
                              // below reach over the new base
}

//==============================================================================
void PlaybackEngine::slideCommand (std::vector<PendingEvent>& events, int ch, int offset,
                                   double rowPpq, int argument, bool up, bool noteOnRow,
                                   const juce::Uuid& clipUid, const Song& song)
{
    auto& c = channels[(size_t) ch];

    // a slide row closes the arpeggiator run this column was in: the two cannot
    // both be in force in one column, so the note the run's last step entered is
    // released at this row and the run is over. A row that also struck a note of
    // its own had already released it as that note's own arrival above, so only
    // a bare slide row has to release it here. A run another column of the same
    // MIDI channel was in is left where it is, which is how a slide and a run
    // written on different columns of one instrument work together rather than
    // one switching the other off
    if (c.arpId > 0 && ! noteOnRow)
        closeArp (events, offset, ch);

    c.endArp();

    if (argument == kArgOff || argument == 0)
    {
        // the OFF argument: the wheel is put away rather than made to climb, so
        // it goes back to its centre, no ramp is armed and nothing stays bent
        // below this row. The column also carries nothing rather than the slide
        // letter, which is what leaves an argument-only row further down
        // unanswered and stops the tracker drawing a carried slide over the rows
        // under it - the same ending an arpeggiator's release leaves below it
        const int midiCh = juce::jlimit (0, kMidiChannelCount - 1, c.lastInst) + 1;
        endWheel (events, ch, midiCh, offset);
        c.lastCmd = -1;
        c.lastCmdVal = -1;
        return;
    }

    if (argument == kArgStp)
    {
        // the STP argument stops whatever move the wheel was making and leaves it
        // sitting on the point it had got to, which is what holds that pitch over
        // the rows below: the point is already in force, so no message is sent,
        // and only the ramp that was moving it is put away. The letter keeps being
        // carried, so a further row under it can re-arm a ramp of its own or stop
        // the wheel again
        c.endSlide();
        return;
    }

    // the argument says how many rows the slide takes rather than where on the
    // wheel it goes: a row that left it empty takes the default span, and the
    // narrowest slide the field can ask for takes one row rather than jumping
    // there on the naming row itself, since a 0 now means the OFF argument above
    const int span = argument >= 0 ? juce::jlimit (1, 127, argument) : kSlideDefaultSpan;

    const int subticks = juce::jmax (1, song.settings.subticksPerRow);

    c.slideOn = true;
    c.slideClip = clipUid;
    c.slideOrigin = rowPpq;
    c.slideSpan = span;
    c.slideTotal = span * subticks; // the span over the Settings grid, the ramp's own
    c.slideCount = 0;
    c.slideFrom = c.lastBendPos;
    c.slideTo = up ? kBendTopPos : kBendBottomPos;

    // arming says nothing about the wheel: the row that named the letter is the
    // ramp's own start, so its first step holds the point the wheel is already at
    // and sends no message of its own, which is what keeps that row's note straight
    // and leaves the extreme to the step sitting span rows below
}

//==============================================================================
/** A row naming the vibrato letter, or one naming only an argument under a letter
    carried above it: the argument names which vibrato macro the column runs, so
    the row itself says nothing about the wheel and arms a wobble whose points the
    flush below moves over the grid the macro's own cycle length names. The wobble
    swings about the point the wheel was at where it was armed, which is what lets
    a bend row above it or a slide's extreme be the pitch it swings from, and about
    the point a bend row below moved it to while it runs, which is what keeps that
    row's own pitch and the wobble's swing in step with one another rather than
    making the bend end the swing. The release spells itself the two ways a call's
    does: the id 0 beside the letter and the OFF the entry writes for an o both put
    the pitchbending away, closing any arpeggiator run this column was in the same
    way a slide's OFF closes one and taking the wheel back to its centre so nothing
    stays bent under the rows below; the STP spelling asks for nothing beside a
    vibrato since a wobble has no move of the wheel to stop. An id naming a macro
    that has none of its own still arms the wobble here, and its flush puts it away
    at the first point of it, the way a run whose macro was taken away stops at its
    next step rather than at the row that took it. */
void PlaybackEngine::vibCommand (std::vector<PendingEvent>& events, int ch, int id, int offset,
                                 double rowPpq, const juce::Uuid& clipUid, bool noteOnRow,
                                 const Song& song)
{
    auto& c = channels[(size_t) ch];

    if (id == kArgStp)
        return; // a wobble has no move of the wheel to stop, so an STP row says
                // nothing at all and leaves the wobble above still swinging over
                // the rows below it, the same thing a row beside a call's STP asks

    if (id == 0 || id == kArgOff)
    {
        // the release: the wobble is put away and the wheel it had left swinging
        // taken back, both at this row, so the rows below it have neither the
        // letter nor a wobble and sit under a straight pitch
        if (c.arpId > 0 && ! noteOnRow)
            closeArp (events, offset, ch);

        c.endArp();
        endWheel (events, ch, juce::jlimit (0, kMidiChannelCount - 1, c.lastInst) + 1, offset);

        // a release leaves the column calling nothing rather than calling id 0, the
        // way a run's release and a slide's OFF leave nothing below them, which is
        // what stops an argument-only row further down arming a wobble again
        c.lastCmd = -1;
        c.lastCmdVal = -1;
        return;
    }

    // a vibrato and a run cannot both be in force in one column, so this row closes
    // the run the column was in exactly as a slide row does: the note the run's last
    // step entered is released here and the run is over, while a run another column
    // of the same MIDI channel was in stays where it is
    if (c.arpId > 0 && ! noteOnRow)
        closeArp (events, offset, ch);

    c.endArp();

    c.vibOn = true;
    c.vibId = juce::jlimit (1, kMaxVibMacros, id);
    c.vibClip = clipUid;
    c.vibOrigin = rowPpq;
    c.vibCount = 0;

    // the wobble's cycle starts at the row that armed it, counted from there rather
    // than read off the rows, the way a run's steps and a ramp's are counted
    c.vibBase = c.lastBendPos;

    // arming says nothing about the wheel, the way arming a ramp does: the cycle's
    // first point sits on the base the wheel is already at, which is what keeps the
    // row that named the letter and the row a bare wobble's note sits on at the
    // pitch they were already at rather than jumping to one side of the swing there
    // the macro's three settings are read at every point rather than here, so
    // arming needs nothing of the song but keeps the same shape as a slide's
    // answer for the row that wrote it
    (void) song;
}

//==============================================================================
/** One point of a vibrato's cycle: how far the macro's shape says the pitch sits
    from the base a wobble swings about, as a share of the macro's own amplitude,
    at the phase of its cycle given, 0 at the cycle's start and just short of a
    whole cycle round. The sinusoidal shape curves, the triangle one ramps, and the
    Up and Down spellings of each fold half of that cycle back over the base rather
    than swinging to the far side of it: the Up kinds have the cycle's second half
    of it inverted, so the pitch only goes above the base, and the Down kinds have
    its first half inverted, so it only goes below - the two halves being the ones
    the plain shape swings to the far side and to the near side in, which is what
    folding one of them back leaves the whole wobble on one side of its base. */
static double vibShape (int type, double phase)
{
    double shape;

    if (type >= kVibTypeTri)
    {
        // four straight sides over the cycle: the ramp climbs to the far top a
        // quarter of the way in, comes back through the base by halfway and to its
        // own bottom by three quarters, then closes on the base, which is what
        // leaves a triangle's corners on the cycle's quarters rather than having
        // them bunched at its ends
        const double q = phase * 4.0;
        shape = q < 1.0 ? q : ( q < 3.0 ? 2.0 - q : q - 4.0 );
    }
    else
        shape = std::sin (phase * juce::MathConstants<double>::twoPi);

    if (phase >= 0.5 && ( type == kVibTypeSinUp || type == kVibTypeTriUp ))
        shape = -shape;
    else if (phase < 0.5 && ( type == kVibTypeSinDown || type == kVibTypeTriDown ))
        shape = -shape;

    return shape;
}

//==============================================================================
/** The points of a vibrato this channel's column has in force, moved over the grid
    the macro's own cycle length names rather than the plain sub-row one: a cycle
    eight subticks wide at the eight points it is drawn with with the Settings' six
    subticks to the row is one point every 4/3 of a subtick, which is what lets a
    cycle the macro asked for shorter than one point per sub-row step still show its
    curve rather than landing on one point of it every sub-row, and what leaves a
    cycle eight subticks or wider having one point per sub-row step. The points are
    counted from the arming row rather than read off the rows, the way a run's steps
    and a ramp's are, which is what a row a transport jump makes this block play
    again leaves on the same point of the cycle instead of advancing it twice, and
    what keeps a wobble that cycled many times exact rather than drifted by its own
    fractions. Only a point that changes the wheel's position says anything, which is
    what the arming row's own point sends nothing of its own, and what leaves a
    macro whose amplitude knob is at off holding the pitch it is at over its cycles
    rather than moving the wheel every point of them. A wobble never finishes by
    itself: a cycle repeats for as long as the column carries the letter, so the
    clip that armed it and the release rows are what put it away. */
void PlaybackEngine::flushVib (std::vector<PendingEvent>& events, int ch, double ppqStart,
                               double ppqLimit, double ppqPerSample, int numSamples,
                               const Song& song)
{
    auto& c = channels[(size_t) ch];

    if (! c.vibRunning())
        return;

    const auto* macro = song.peekVib (c.vibId);

    if (macro == nullptr || ! macro->defined())
    {
        c.endVib(); // the macro the wobble was running has none of its own any more,
                    // which is what a run whose macro was taken away answers at its
                    // next step: the wheel keeps whatever point of its own the
                    // wobble's last point left it at, and a release row below is
                    // what puts that away
        return;
    }

    const double rowsPerBeat = (double) juce::jmax (1, song.settings.rowsPerBeat);
    const double rowSpanPpq = 1.0 / rowsPerBeat;
    const int subticks = juce::jmax (1, song.settings.subticksPerRow);

    // the macro's three settings are re-read here, so a macro whose shape,
    // amplitude or cycle was turned while its wobble ran changes the wobble from
    // where it is rather than leaving what the arming row had still in force
    const int cycle = juce::jlimit (1, kVibFreqTop, macro->frequency);
    const int amplitude = juce::jlimit (0, kVibAmpTop, macro->amplitude);

    c.vibCycle = cycle;
    c.vibPoints = juce::jmax (cycle, kVibCyclePoints);

    // the amplitude is a share of the wheel's own half-height, so a knob at full
    // reaches the wheel's own extremes from a wobble sitting at its centre, and
    // counted in whole wheel positions rather than in the argument's steps, which
    // is what leaves no rounding where the swing's own curve crosses the centre
    c.vibHeight = amplitude * kBendCentrePos / kVibAmpTop;

    const double cycleSpanPpq = rowSpanPpq * (double) cycle / (double) subticks;
    const double pointPpq = cycleSpanPpq / (double) c.vibPoints;

    // a wobble whose next point sits more than a row behind where this block starts
    // cannot be placed any more, which is what a scrub or a jump leaves behind, so
    // it ends rather than swinging late at where it had got to
    if (c.vibOrigin + (double) c.vibCount * pointPpq < ppqStart - rowSpanPpq)
    {
        c.endVib();
        return;
    }

    const int midiCh = juce::jlimit (0, kMidiChannelCount - 1, c.lastInst) + 1;

    while (true)
    {
        // the point's position is the arming row plus its count over the cycle's
        // own point width, never a sum of that width into itself, so a wobble that
        // cycled many times over many blocks stays exact
        const double pointTime = c.vibOrigin + (double) c.vibCount * pointPpq;

        if (pointTime >= ppqLimit)
            break; // it waits for the rows below or for the next block

        if (pointTime >= ppqStart - kRowAlignEps)
        {
            const int offset = juce::jlimit (0, juce::jmax (0, numSamples - 1),
                                             (int) std::llround ((pointTime - ppqStart) / juce::jmax (1.0e-12, ppqPerSample)));

            // the cycle's phase is the point inside it over the points that cycle
            // is drawn with, counted from the arming row rather than carried round
            // from the point before, which is what a cycle that wrapped many times
            // keeps its shape rather than its drift
            const double phase = (double) (c.vibCount % c.vibPoints) / (double) c.vibPoints;
            const double shape = vibShape (macro->type, phase);

            // the swing is measured from the base the wobble was armed at or a bend
            // row below moved it to, and kept inside the wheel's own extremes, which
            // is what an amplitude at full reaches without one of the wheel's ends
            // being leaned against further than it is
            const int position = juce::jlimit (kBendBottomPos, kBendTopPos,
                                               c.vibBase + (int) std::llround (shape * (double) c.vibHeight));

            if (position != c.lastBendPos)
            {
                // the same standard pair a bend row sends, at this point's sample,
                // and ahead of the note the row this point sits inside strikes, so
                // a note entering while the wheel swings is played at the point
                // the swing had got to rather than the one before it
                events.push_back ({ offset, 0, juce::MidiMessage::pitchWheel (midiCh, position) });
                c.lastBendPos = position;
                c.bendClip = c.vibClip; // the clip whose rows armed the wobble is
                                        // what has a swing to put away at its end
            }
        }

        ++c.vibCount;
    }
}

//==============================================================================
/** The ramp of a slide this channel's column has in force, moved along the same
    sub-row grid an arpeggiator's steps use rather than one step per row: the
    wheel is a state the column carries down like a volume or a pan, and the
    letters only arm a ramp, so the rows under the one that named a letter hold
    the wheel climbing toward the extreme it names rather than sitting on one new
    point for a whole row. The steps are counted from the ramp's start rather than
    read off the row, the way a run's are, which is what a row a transport jump
    makes this block play again leaves on the same point of the ramp rather than
    advancing it twice, and what leaves a row's own steps waiting for the block
    that reaches them. A step's position is the whole wheel position that far
    along the straight line between where the ramp left the wheel and the extreme,
    counted in whole positions, so the step sitting span rows below the one that
    named the letter lands on the extreme exactly and the ones before it keep the
    line straight rather than staircasing between the rows. */
void PlaybackEngine::flushSlide (std::vector<PendingEvent>& events, int ch, double ppqStart,
                                 double ppqLimit, double ppqPerSample, int numSamples,
                                 const Song& song)
{
    auto& c = channels[(size_t) ch];

    if (! c.slideRunning())
        return;

    const double rowsPerBeat = (double) juce::jmax (1, song.settings.rowsPerBeat);
    const double rowSpanPpq = 1.0 / rowsPerBeat;
    const int subticks = juce::jmax (1, song.settings.subticksPerRow);

    // the whole ramp is re-counted over the grid the Settings names, which is what
    // still puts its extreme span rows below the row that named the letter when
    // that row is changed while a slide is climbing
    c.slideTotal = c.slideSpan * subticks;

    if (c.slideCount > c.slideTotal)
        return; // the extreme is reached: the wheel sits at a point of its own

    const double stepPpq = rowSpanPpq / (double) subticks;

    // a ramp whose next step sits more than a row behind where this block starts
    // cannot be placed any more, which is what a scrub or a jump leaves behind,
    // so the ramp ends rather than the wheel arriving late at where it had got to
    if (c.slideOrigin + (double) c.slideCount * stepPpq < ppqStart - rowSpanPpq)
    {
        c.endSlide();
        return;
    }

    const int midiCh = juce::jlimit (0, kMidiChannelCount - 1, c.lastInst) + 1;

    while (c.slideCount <= c.slideTotal)
    {
        // the step's position is the ramp's start plus its count, never a sum of
        // step widths, so a ramp spread over many blocks stays exact
        const double stepTime = c.slideOrigin + (double) c.slideCount * stepPpq;

        if (stepTime >= ppqLimit)
            break; // it waits for the rows below or for the next block

        if (stepTime >= ppqStart - kRowAlignEps)
        {
            const int offset = juce::jlimit (0, juce::jmax (0, numSamples - 1),
                                             (int) std::llround ((stepTime - ppqStart) / juce::jmax (1.0e-12, ppqPerSample)));

            // the count is scaled before it is divided, so the step that reaches
            // the extreme lands on it exactly rather than a hair short of it, and
            // a step whose whole position is the one the wheel is already at is
            // left unspoken, which is what the naming row's own step holds to
            const int position = c.slideCount >= c.slideTotal
                                     ? c.slideTo
                                     : c.slideFrom + (c.slideTo - c.slideFrom) * c.slideCount / juce::jmax (1, c.slideTotal);

            if (position != c.lastBendPos)
            {
                // the same standard pair a bend row sends, at this step's sample,
                // and ahead of the note the row this step sits inside strikes,
                // which is what a note entering while the wheel climbs is played
                // at the point the climb had got to rather than the one before it
                events.push_back ({ offset, 0, juce::MidiMessage::pitchWheel (midiCh, position) });
                c.lastBendPos = position;
                c.bendClip = c.slideClip;
            }
        }

        ++c.slideCount;
    }

    if (c.slideCount > c.slideTotal)
        c.slideOn = false; // the extreme is reached: the wheel then sits at a point
                            // of the column's own, like a bend row left it, so a
                            // release point below is what puts it back
}

//==============================================================================
/** The note-off a cut this channel's column had armed for, at the subtick that
    cut named: a wider cut than the row's own subticks cannot be squeezed inside
    the block the row that entered the note falls in, so the release waits in the
    channel state with where it sits in absolute transport position, counted from
    the row that armed it over the same grid an arpeggiator's steps, a
    slide's ramp and a wobble's points use, and the block that reaches it emits it
    at its own sample. `ppqLimit` never passes the end of the row the engine last
    owned, which is what leaves a release the column superseded below that row
    unplaced rather than fired at a sample of its own beyond it. The release is
    only ever sent for the pitch the row that armed it left this channel holding,
    whether it entered one of its own there or has been holding it from the rows
    above, and this channel is still holding, which is what makes it safe for something
    to have taken that note away already, a note-off row, one of a run's steps, a
    transport re-arm or a clip's end all having said so: the waiting release is
    dropped rather than closing whatever note the column happens to be holding at
    that subtick, which is what keeps a cut from cutting a note it was never asked
    to cut. */
void PlaybackEngine::flushCut (std::vector<PendingEvent>& events, int ch, double ppqStart,
                               double ppqLimit, double ppqPerSample, int numSamples,
                               const Song& song)
{
    auto& c = channels[(size_t) ch];

    if (c.cutTime < 0.0)
        return;

    const double rowsPerBeat = (double) juce::jmax (1, song.settings.rowsPerBeat);
    const double rowSpanPpq = 1.0 / rowsPerBeat;

    if (c.cutTime < ppqStart - rowSpanPpq)
    {
        c.cutWaiting(); // the release sits more than a row behind where this block
                        // starts, so it cannot be placed any more, which is what a
                        // scrub or a jump leaves behind: the note it belonged to is
                        // left to whatever ends it below, the way a run's, a ramp's
                        // and a wobble's waiting steps are left behind there
        return;
    }

    if (c.cutTime >= ppqLimit)
        return; // it waits for the rows below it or for the block that reaches it

    if (c.cutTime >= ppqStart - kRowAlignEps)
    {
        const int offset = juce::jlimit (0, juce::jmax (0, numSamples - 1),
                                         (int) std::llround ((c.cutTime - ppqStart) / juce::jmax (1.0e-12, ppqPerSample)));

        if (c.activeNote == c.cutNote && c.activeMidiCh == c.cutMidiCh)
            // the cut's own release, which closes the note the row that armed it was
            // holding there, whether it entered one of its own at that row or has
            // been holding it from the rows above: the note-off that row asked for is
            // what the release point below would have sent there, so it does the same
            // thing a row's own note-off does about the wheel this channel had left
            // bent under that note, the row not having to name one of its own
            stopNote (events, offset, ch, c.cutMidiCh, c.cutNote);

        // the release has reached its own subtick, so it is spent whatever came
        // of it: a second one of it would close a note entered below the row that
        // armed it, which is what the next note entering this column arms instead
        c.cutWaiting();
    }
}

//==============================================================================
/** Arms the gate this channel's column has in force for the note it is left holding
    at the row that armed it: the pitch that row entered when it entered one of its own,
    otherwise the pitch the channel has still been holding from the rows above, which is
    what lets the cut letter cut across the rows rather than only the row that entered a
    note. The release the cut asks for is counted from that row rather than from the rows
    below it, which is what makes the subtick it sits on exact for whatever length was
    named, keeps a row a transport jump makes this block play again arming its own cut at
    that row rather than advancing one twice, and leaves a length retargeted by a further
    row from reaching back into a note above it that had already been armed at its own row.
    A row that left the channel holding no note at all, or a column with no gate in force,
    has nothing to hold open. */
void PlaybackEngine::armCut (int ch, int heldNote, int midiCh, double rowPpq, const Song& song)
{
    auto& c = channels[(size_t) ch];

    if (heldNote < 0 || heldNote > 127 || c.cutTicks <= 0)
        return; // the row left this channel holding no note or the column has no gate
                // in force: the cut has no length to hold anything open over, a row
                // that took its note away being released by the note column itself

    const double rowsPerBeat = (double) juce::jmax (1, song.settings.rowsPerBeat);
    const int subticks = juce::jmax (1, song.settings.subticksPerRow);

    // the row's own position plus the named count over the Settings' grid, never a
    // sum of step widths, so a cut reaching past this row and past the block that
    // holds it lands on the subtick the argument asked for rather than a hair off it
    c.cutTime = rowPpq + (double) c.cutTicks / (rowsPerBeat * (double) subticks);
    c.cutNote = heldNote;
    c.cutMidiCh = midiCh;
}

//==============================================================================
/** A row naming the cut letter, one naming only an argument under a letter carried
    above it or one naming neither of the two and entering a note under a letter the
    column carries: the argument counts subticks a note is held for, on the same sub-row
    grid an arpeggiator's steps, a slide's ramp and a wobble's points use, so the row
    itself sends nothing of its own and leaves the note it is holding sounding until the
    release below reaches the subtick the length names past it - a length wider than the
    row's own subticks reaching past that row and waiting for the block that gets there
    the way a run's steps, a ramp's steps and a wobble's points do, and reaching as far
    past it as the entry's own top of kCutTopTicks subticks. The note the release belongs
    to is the one this channel is left holding at that row: the letter beside a row that
    named no note of its own cuts across the rows, taking the note the column has been
    holding from above down to the length named there rather than leaving it to whatever
    ends it below, which is what the letter entered beside a row that did strike a note
    and a bare note row under a carried letter already do. An empty argument beside a
    letter the row itself named takes the default length rather than no length at all,
    the number the entry writes so the cell spells the cut it asks for, and an empty one
    under a carried letter keeps the length that letter is carrying, so a bare note row
    is cut to whatever the column says. An OFF argument or a 0 is the note column's own
    note-off rather than a cut of no subticks at all, drawn OFF the way its === is drawn:
    the note this channel has been holding at that row is released at that row's own
    sample, the gate is put away rather than carried on below and the column carries no
    letter there, so the notes entering the rows under a release sustain again. Where that
    row entered a note of its own as well, that note's arrival had already released the
    note above it at this very sample and a note-off at a sample sorts ahead of the note-on
    entered there, so a note of no length at all cannot be spelled and the note entered
    there is left to whatever ends it below. An argument under a carried letter keeps its
    reach where it was, that being the notes entering there and the rows below rather than
    a note the rows above had already armed. The STP spelling asks for nothing, a cut having
    no move of the wheel to stop, and leaves the length above as it was; either way the note
    this channel is left holding is held for whatever length is in force there, which is
    what lets the letter gate the notes of the rows under it rather than cutting only the
    row that named it. Like a slide's and a vibrato's rows, this row closes an arpeggiator
    run this column was in, a run and a gate not being two things one column can hold at
    once. */
void PlaybackEngine::cutCommand (std::vector<PendingEvent>& events, int ch, int offset,
                                 double rowPpq, int argument, bool rowNamedLetter,
                                 int enteredNote, bool noteOnRow, int midiCh,
                                 const Song& song)
{
    auto& c = channels[(size_t) ch];

    // the run above is closed here rather than carried into the gate below: the note
    // its last step entered is released at this row, so the gate holds the note the
    // column named instead of continuing from whatever note that run happened to be
    // sounding. A row that struck a note of its own had already released it as that
    // note's own arrival above, so only a bare row has to release it here, and a run
    // another column of the same MIDI channel was in is left where it is
    if (c.arpId > 0 && ! noteOnRow)
        closeArp (events, offset, ch);

    c.endArp();

    if (argument == kArgStp)
    {
        // the STP spelling beside a cut letter: a cut has no move of the wheel to
        // stop, so it asks for nothing at all and leaves the length above still being
        // carried, exactly as a row beside a call's or a vibrato's STP leaves the run
        // or the wobble above it alone; the note that row entered, where it entered one,
        // is held for that length all the same
    }
    else if (argument == kArgOff || argument == 0)
    {
        // the release, spelled two ways the way a call's, a slide's and a vibrato's are:
        // the 0 a row names beside or under the letter and the OFF the entry writes for an
        // o. It reads and answers as the note column's own note-off, drawn OFF the way its
        // === is drawn, and does what a note-off row does at that row: the note this channel
        // has been holding from above is released at this row's own sample, which is what
        // lets the letter cut a note off across the rows rather than only at the rows that
        // entered one, and the gate is put away rather than holding a note for no subticks
        // at all - the release that gate had already armed for that note goes with it, the
        // note having just been taken away here. The column also carries nothing rather than
        // the cut letter below a release, which is what stops a further row naming only an
        // argument gating a note there and stops the tracker drawing a carried letter and
        // length over the rows under it
        if (c.activeNote >= 0 && ! noteOnRow)
            // the note this channel has been holding is closed here, at this row's own
            // sample, rather than left to whatever ends it below, and where the column
            // had left the wheel bent under that note the release puts it back, the way
            // a row's own note-off does. A row that entered a note of its own as well had
            // already released the note above it as that note's arrival at this very
            // sample, so the release asks for nothing of its own there: a note-off at a
            // sample is sorted ahead of the note-on entered at it, which is what leaves a
            // note of no length at all without a spelling and leaves the note entered
            // here sounding until something below ends it
            stopNote (events, offset, ch, c.activeMidiCh, c.activeNote);

        c.endCut();
        c.lastCmd = -1;
        c.lastCmdVal = -1;
        return; // a release has no length to arm the note that row entered for
    }
    else
    {
        const int ticks = argument == kEmpty
                              ? ( rowNamedLetter ? kCutDefaultTicks : c.cutTicks )
                              : juce::jlimit (1, kCutTopTicks, argument);

        // a row that named only a number, or none at all under a carried letter, makes
        // that the length in force for the notes entered from here on: the narrowest
        // cut the field can name holds a note for one subtick rather than closing it at
        // the row that entered it, since a 0 is the release above, and the widest holds
        // it for as far as the entry reaches, kCutTopTicks subticks three digits deep,
        // its release waiting past this row and past the clip that has it, the clip's own
        // end being what closes the note there. The two numbers just below that top, 128
        // and 129, are the OFF and the STP spellings rather than lengths, so those two
        // lengths of the grid have no spelling of their own
        c.cutTicks = ticks;
    }

    if (enteredNote >= 0)
        armCut (ch, enteredNote, midiCh, rowPpq, song);
    else if (rowNamedLetter && ! noteOnRow)
        // the letter beside a row that named no note of its own cuts across the rows: the
        // note this channel has still been holding from the rows above is the one the
        // length named there belongs to, released at that many subticks past this row
        // rather than left to whatever ends it below, the way the letter entered beside
        // a row that did strike a note already has that note to hold open. A row naming
        // only an argument under a carried letter keeps its reach where it was, the notes
        // entered above having been armed at their own rows, which is what leaves a
        // retarget to the notes entering there and the ones below, and a row whose note
        // duplicated another channel's on the same MIDI output channel in this row sent
        // nothing of its own, so it leaves nothing armed here either
        armCut (ch, c.activeNote, c.activeMidiCh, rowPpq, song);
}

//==============================================================================
/** Plays the strikes a retrigger gate has waiting up to the end of the row the
    engine last owned, each one being the note column's own note-off and note-on at
    the one sample its period names rather than a hair apart, so the two sort off-
    before-on there and the note that is still being held is re-articulated rather
    than restarted or spelled at no length at all. The strikes sit at absolute
    transport positions: the first is counted from the row that armed the gate and
    each one after it from the strike the engine last played, at whatever period is
    in force where it was played rather than at the period the arming row called
    with, the way a run's steps, a ramp's steps, a wobble's points and a cut's
    release wait rather than being read off the rows below. A period narrower than
    the Settings' grid at a large number of them can drop below a block's own
    samples, which is why the strikes this block reaches all sound from it rather
    than one to the block, and a period wider than one row waits for the block that
    gets there the way a cut of that length does. A strike is only ever sent for the
    pitch the row that armed the gate left this channel holding and this channel is
    still holding, which is what makes it safe for the note column's own note-off,
    a run's step, a transport re-arm or a clip's end to have taken that note away
    already, the strike and the ones counted on from it being dropped rather than
    closing and re-opening whatever note the column happens to be holding at that
    sub-tick, the letter still holding the period so the notes entering under it
    below re-arm. The bound is the end of the row the engine last processed, the
    same one the cut's own waits on, which is what leaves a strike the column
    superseded below that row not being placed at a sample of its own beyond it, and
    a strike that can no longer be placed where it sits, which is what a scrub or a
    jump leaves behind, is dropped the way a run's, a ramp's, a wobble's and a cut's
    waiting points are left behind there. */
void PlaybackEngine::flushRetrig (std::vector<PendingEvent>& events, int ch, double ppqStart,
                                  double ppqLimit, double ppqPerSample, int numSamples,
                                  const Song& song)
{
    auto& c = channels[(size_t) ch];

    if (c.retrigTime < 0.0)
        return;

    const double rowsPerBeat = (double) juce::jmax (1, song.settings.rowsPerBeat);
    const int subticks = juce::jmax (1, song.settings.subticksPerRow);
    const double rowSpanPpq = 1.0 / rowsPerBeat;

    // the period is counted from one strike to the next rather than being a fixed
    // schedule off the arming row, so the strikes that follow a period the column
    // re-tells between them are counted at the new one rather than leaving the row
    // that armed the gate to have spelled them all
    while (c.retrigTime >= 0.0)
    {
        if (c.retrigTime < ppqStart - rowSpanPpq)
        {
            c.retrigWaiting(); // the strike sits more than a row behind where this block
                                // starts, so it cannot be placed any more, which is what a
                                // scrub or a jump leaves behind: the note it belonged to is
                                // left to whatever ends it below, the way a run's, a ramp's,
                                // a wobble's and a cut's waiting points are left behind there,
                                // and a strike that already sounded stays sounding. The letter
                                // above keeps holding the period, so a note entering this
                                // column under it below re-arms its own strikes
            return;
        }

        if (c.retrigTime >= ppqLimit)
            return; // it waits for the rows below it or for the block that reaches it

        if (c.retrigTime >= ppqStart - kRowAlignEps)
        {
            const int offset = juce::jlimit (0, juce::jmax (0, numSamples - 1),
                                             (int) std::llround ((c.retrigTime - ppqStart) / juce::jmax (1.0e-12, ppqPerSample)));

            if (c.activeNote == c.retrigNote && c.activeMidiCh == c.retrigMidiCh)
            {
                // the note is still being held, so the strike closes it and opens it again
                // at this very sample: the note-off the strike asks for sorts ahead of the
                // note-on entered there, which is what spells a re-articulation of the pitch
                // rather than a note of no length at all, and the wheel this channel had left
                // bent under that note is left where it is, the pitch not having moved. The
                // strike opens the note at the velocity the column carries, the way a run's
                // steps do, the letter having no velocity of its own to name
                const int vel = juce::jlimit (1, kCcTop, c.lastVol);
                startNote (events, offset, ch, c.retrigMidiCh, c.retrigNote, vel);

                // the next strike is counted from this one rather than from the row that
                // armed the gate, at whatever period is in force here: that is what lets a
                // period re-told by a row between two strikes move the strikes below it
                // rather than leave them to the period the arming row called with, what keeps
                // a row a transport jump makes this block play again arming its own strikes
                // at that row rather than advancing one twice and what leaves a note struck
                // many times over a long clip exact rather than drifting off the row it sits
                // in, the strike above having just been placed at its own sample
                c.retrigTime += (double) juce::jmax (1, c.retrigTicks) * rowSpanPpq / (double) subticks;
            }
            else
            {
                c.retrigWaiting(); // the note this strike belonged to has been taken away by
                                    // the note column's own note-off, a run's step, a transport
                                    // re-arm or a clip's end, so there is nothing left to strike
                                    // again at its sub-tick and the strikes counted on from it go
                                    // with it; the letter the column carries keeps holding the
                                    // period, which is what lets the notes entering the rows
                                    // below it re-arm their own, and a strike that had already
                                    // sounded left the note it belongs to sounding wherever that
                                    // note is held until
                return;
            }
        }
        else
        {
            c.retrigWaiting(); // the strike sits behind this block's start and within a row
                                // of it, where it can no longer be placed any more, and the
                                // strikes counted on from it would land behind it as well: the
                                // gate's own strikes are left behind there the way a run's, a
                                // ramp's, a wobble's and a cut's waiting points are, the period
                                // above it still being carried
            return;
        }
    }
}

//==============================================================================
/** Arms the gate this channel's column has in force for the note it is left holding
    at the row that armed it: the pitch that row entered when it entered one of its own,
    otherwise the pitch the channel has still been holding from the rows above, or the base
    of an arpeggiator run that column had in force rather than a note of its own being held
    - that base being the note the run measured its steps from, read as it was typed in the
    note column rather than as the run's first step moved it there, which is the same note
    an id 0 release call of that run goes back to. Either way this is what lets the retrigger
    letter work across the rows rather than only the row that entered a note, and what leaves
    a row that left the channel holding no note at all, or a column whose run had already had
    its base taken away by the note column's own note-off above, ignored rather than armed for
    nothing at all. The first strike sits subTicksToFirstStrike
    sub-ticks of the Settings' grid past that row: one whole period when the row entered a
    note of its own and when the row went back to the base of a run it had just taken away,
    that row's own arrival or strike of that base being the first trigger of what the gate
    counts on from, and no sub-ticks at all when the letter was spelled across a row whose
    note column named nothing of its own and the column had a note still being held rather
    than a run in force - the row itself re-opening the note it has been holding, since a
    row that named nothing would otherwise leave that note sustaining until a strike one
    whole period below the row reached it. The strikes the gate makes after that one are
    counted on from the strike the flush below last played rather than from that row, which
    is what makes them exact for whatever period was named, keeps a row a transport jump
    makes this block play again arming its own strikes at that row rather than advancing
    one twice and leaves a period retargeted by a further row from reaching back into a
    note above it that had already been armed at its own row. Where that row's column had a
    wobble swinging above it, the row had already taken that swing away and put the wheel
    back at its exact centre at its own sample ahead of the strike spelled there, so the
    strikes counted on from this gate play at the pitch the column named rather than at one
    the cycle left the wheel part way down its swing; a point a bend row left on the wheel
    or the point a slide's ramp had climbed to in this column is kept rather than straightened,
    and a ramp another column was climbing is left climbing on because the wheel is that
    column's own state. Where the row armed this gate for a run's base rather than for a note
    the column has been holding, that run's steps are gone below this row and its jumps belong
    to the column that called them, so a letter spelled on another column leaves such a run
    stepping on rather than trading it for its base, that column's own notes being all a
    retrigger there ever strikes. A row that left the channel holding no note at all, a
    column whose run had already had its base taken away above this row, or a column with
    no gate in force has nothing to strike again. */
void PlaybackEngine::armRetrig (int ch, int heldNote, int midiCh, double rowPpq,
                                int subTicksToFirstStrike, const Song& song)
{
    auto& c = channels[(size_t) ch];

    if (heldNote < 0 || heldNote > 127 || c.retrigTicks <= 0)
        return; // the row left this channel holding no note or the column has no gate
                // in force: the retrigger has no period to strike a note at, a row
                // that took its note away being answered by the note column itself

    const double rowsPerBeat = (double) juce::jmax (1, song.settings.rowsPerBeat);
    const int subticks = juce::jmax (1, song.settings.subticksPerRow);

    // the row's own position plus subTicksToFirstStrike sub-ticks of the Settings' grid,
    // never a sum of step widths, so a strike reaching past this row and past the block
    // that holds it lands on the sub-tick the number asked for rather than a hair off it;
    // that number is the period the row named where the row entered a note of its own and
    // where the row went back to the base of a run it had just taken away, that row's own
    // arrival or strike of that base being the first trigger of what the gate counts on
    // from, and nothing at all where the letter was spelled across a bare row whose column
    // had a note still being held rather than a run in force, that row striking that note
    // at its own sample.
    // The strikes the flush below counts on from either one are counted the same way off
    // the one the engine last played, which is what keeps them exact over a long clip
    c.retrigTime = rowPpq + (double) juce::jmax (0, subTicksToFirstStrike)
                                        / (rowsPerBeat * (double) subticks);
    c.retrigNote = heldNote;
    c.retrigMidiCh = midiCh;
}

//==============================================================================
/** A row naming the retrigger letter, one naming only an argument under a letter
    carried above it or one naming neither of the two and entering a note under a
    letter the column carries: the argument counts the sub-ticks apart a note that is
    already sounding in this column is struck again at, on the same sub-row grid an
    arpeggiator's steps, a slide's ramp, a wobble's points and a cut's length use, so
    a row that entered a note of its own sends nothing of its own there and leaves that
    note sounding until the strike below reaches the sub-tick the period names past it,
    that note's own arrival at the row being its first trigger, while a row that named
    no note of its own instead re-opens the note it has still been holding at its own
    sample as well as striking it again at that many sub-ticks below it - a period wider
    than the row's own sub-ticks reaching past that row and past the block that
    holds it the way a run's steps, a ramp's steps, a wobble's points and a cut's
    release do, and reaching as far past it as the entry's own top of kRetrigTopTicks
    sub-ticks. The note the strikes belong to is the one this channel is left holding
    at that row, or the base of an arpeggiator run that column had in force rather than
    a note of its own being held: the letter beside a row that named no note of its own
    works across the rows, taking the note the column has been holding from above and
    re-opening it at this row's own sample as well as striking it again at that many
    sub-ticks below it, and where the column instead had a run in force closes the pitch
    that run was holding there and goes back to the note that run measured its steps
    from, read as it was typed in the note column rather than as the run's first step
    moved it, opening that base at this row's own sample and counting the strikes on below
    at the period named there rather than leaving the column silent at whatever pitch a
    step happened to have got to - the run's own displacement being what goes beside the
    letter, that base being the same note an id 0 release call of the run goes back to.
    Either way the note is re-opened at the letter's row rather than left to whatever ends
    it below, which is what the letter entered beside a row that did strike a note and
    a bare note row under a carried letter already have. An empty argument beside a letter
    the row itself named takes the default period rather than no period at all, the
    number the entry writes so the cell spells the retrigger it asks for, and an empty
    one under a carried letter keeps the period that letter is carrying, so a bare note
    row is re-struck to whatever the column says. An OFF argument or a 0 stops the
    retriggering rather than striking a note every no sub-ticks at all: it reads and
    answers as the note column's own note-off, drawn OFF the way its === is drawn and
    the way a cut's 0 is, and does what a note-off row does at that row: the note this
    channel has been holding from above is released at this row's own sample, which is
    what lets the letter take a note away across the rows rather than only at the rows
    that entered one, and the gate is put away rather than holding a note for no
    sub-ticks at all - the strike that gate had already armed for that note goes with
    it, the note having just been taken away here. The column also carries nothing
    rather than the retrigger letter below a release, which is what stops a further
    row naming only an argument gating a note there and stops the tracker drawing a
    carried letter and period over the rows under it. Where that row entered a note
    of its own as well, that note's arrival had already released the note above it at
    this very sample, so the release asks for nothing of its own there: a note-off at a
    sample is sorted ahead of the note-on entered at it, which is what leaves a note of
    no length at all without a spelling and leaves the note entered here sounding until
    something below ends it. An argument under a carried letter retargets the period for
    the notes entering there and the rows below rather than re-timing a note the rows
    above had already armed, the strikes of that note carrying on from the strike the
    engine last played at whatever period is in force there. The STP spelling asks for
    nothing, a retrigger having no ramp of its own to hold at a point - the wheel such a
    row puts back at its centre answers for a wobble above it rather than for that row's
    argument, and goes back whatever the argument spells - and leaves the period above
    still being carried, exactly as a row beside a call's or a vibrato's STP leaves the
    run or the wobble above it alone; the note that row entered, where it entered one, is
    struck again at that period all the same. Either way the note this channel is left
    holding is struck at whatever period is in force there, which is what lets the letter
    gate the notes of the rows under it rather than striking only the row that named it.
    Where this column has a wobble swinging, processRow took that swing away and sent the
    wheel back to its exact centre at this row's own sample before this call is made, ahead
    of the strike spelled here: a strike is a fresh articulation of the note the column is
    holding and the pitch a host gives that articulation is the one the wheel is sitting at,
    so a cycle's last point leaving the wheel part way down its swing would have the letter's
    own strike and the ones counted on from it below play the note at wherever in its swing
    the cycle happened to have got to rather than at the pitch the column named. A swing's
    displacement is the only displacement this row takes away: a point a bend row left on the
    wheel and the point a slide's ramp had climbed to in this column keep their pitch below
    the letter, which is what lets the letter re-strike a note at the pitch the column asked
    to have bent rather than straightening a pitch it was asked to bend, and a ramp another
    column was climbing is left climbing on because the wheel is that column's own state
    rather than one this row has anything to do with. Like a slide's, a vibrato's and a
    cut's rows, this row closes an arpeggiator run this column was in, a run and a gate not
    being two things one column can hold at once, and where its own row named no note of
    its own and that run's steps were what the column was sounding, that run is traded for
    its own base rather than only taken away. A run another column armed is left alone: the letter
    spelled on a parallel column reaches nothing across to it, that run's steps belonging to
    the column that called them, which is what leaves such a run stepping on below a letter
    spelled elsewhere and keeps a retrigger on striking nothing but the notes of its own
    column. */
void PlaybackEngine::retrigCommand (std::vector<PendingEvent>& events, int ch, int offset,
                                    double rowPpq, int argument, bool rowNamedLetter,
                                    int enteredNote, bool noteOnRow, int midiCh,
                                    const Song& song)
{
    auto& c = channels[(size_t) ch];

    // the run above is closed here rather than carried into the gate below: the note
    // its last step entered is released at this row, so the gate strikes the note the
    // column named instead of continuing from whatever note that run happened to be
    // sounding. Where the letter's row named no note of its own, the note the column
    // named is the run's own base, read here while the run is still in force: the
    // note its steps measured from, as it was typed in the note column rather than as
    // the run's first step moved it, which is the same reading an id 0 release call of
    // that run goes back to. That base is what this row goes back to as well: the note
    // the run was holding closes at this row's own sample and the base opens again at
    // it, so the column is left sounding its own note below the letter rather than
    // silent, and the gate counts its strikes on from that base rather than from
    // whatever pitch the run happened to be sounding at the row - the run's own
    // displacement being what goes beside the letter, the way a wobble's swing is
    // the only displacement a row's take-back goes. A row that struck a note of its
    // own had already released it as that note's own arrival above, so only a bare row
    // has to release it here, and a run another column of the same MIDI channel was in
    // is left where it is: the letter on a parallel column reaches nothing across to
    // it, that column's own notes being all a retrigger ever strikes
    const int runBase = c.columnNote >= 0 ? c.columnNote : c.arpCallNote;
    const bool runWasInForce = c.arpId > 0;

    if (runWasInForce && ! noteOnRow)
        closeArp (events, offset, ch);

    c.endArp();

    if (argument == kArgStp)
    {
        // the STP spelling beside a retrigger letter: a retrigger has no ramp of its own to
        // hold at a point, so it asks for nothing at all and leaves the period above still
        // being carried - the wheel a row named with the letter puts back at its centre
        // answers for a wobble above it rather than for this argument, and goes back
        // whatever the argument spells - exactly as a row beside a call's or a vibrato's
        // STP leaves the run or the wobble above it alone; the note that row entered, where
        // it entered one, is struck again at that period all the same
    }
    else if (argument == kArgOff || argument == 0)
    {
        // the release, spelled two ways the way a call's, a slide's and a vibrato's are:
        // the 0 a row names beside or under the letter and the OFF the entry writes for an
        // o. It reads and answers as the note column's own note-off, drawn OFF the way its
        // === is drawn and the way a cut's 0 is, and does what a note-off row does at that
        // row: the note this channel has been holding from above is released at this row's
        // own sample, which is what lets the letter take a note away across the rows rather
        // than only at the rows that entered one, and the gate is put away rather than
        // holding a note for no sub-ticks at all - the strike that gate had already armed
        // for that note goes with it, the note having just been taken away here. The column
        // also carries nothing rather than the retrigger letter below a release, which is
        // what stops a further row naming only an argument gating a note there and stops
        // the tracker drawing a carried letter and period over the rows under it
        if (c.activeNote >= 0 && ! noteOnRow)
            // the note this channel has been holding is closed here, at this row's own
            // sample, rather than left to whatever ends it below, and where the column had
            // left the wheel bent under that note the release puts it back, the way a row's
            // own note-off does. A row that entered a note of its own as well had already
            // released the note above it as that note's arrival at this very sample, so the
            // release asks for nothing of its own there: a note-off at a sample is sorted
            // ahead of the note-on entered at it, which is what leaves a note of no length
            // at all without a spelling and leaves the note entered here sounding until
            // something below ends it
            stopNote (events, offset, ch, c.activeMidiCh, c.activeNote);

        c.endRetrig();
        c.lastCmd = -1;
        c.lastCmdVal = -1;
        return; // a release has no period to arm the note that row entered for
    }
    else
    {
        const int ticks = argument == kEmpty
                              ? ( rowNamedLetter ? kRetrigDefaultTicks : c.retrigTicks )
                              : juce::jlimit (1, kRetrigTopTicks, argument);

        // a row that named only a number, or none at all under a carried letter, makes
        // that the period in force for the notes entered from here on: the narrowest
        // period the field can name strikes a note again one sub-tick after it was entered
        // rather than closing it at the row that entered it, since a 0 is the release above,
        // and the widest strikes it again as far apart as the entry reaches, kRetrigTopTicks
        // sub-ticks three digits deep, its strike waiting past this row and past the block
        // that has it and the note being left to whatever ends it there. The two numbers just
        // below that top, 128 and 129, are the OFF and the STP spellings rather than periods,
        // so those two periods of the grid have no spelling of their own
        c.retrigTicks = ticks;
    }

    if (enteredNote >= 0)
        armRetrig (ch, enteredNote, midiCh, rowPpq, c.retrigTicks, song);
    else if (rowNamedLetter && ! noteOnRow && runWasInForce && runBase >= 0 && runBase <= 127)
    {
        // the letter beside a row that named no note of its own, on a column whose run was
        // still in force: the row takes the run away and goes back to the note that run
        // measured its steps from, as it was typed in the note column rather than as the
        // run's steps had moved it. That note is opened at this row's own sample, just
        // behind the release of whatever pitch the run was holding there, and is armed for
        // the period named at the row a whole period past that row rather than at it: this
        // row's own strike of the base is that note's first trigger, the way the note a
        // row entered at its own row is, and the strikes below it are counted on from one
        // to the next at that period. The run's own displacement is what goes beside the
        // letter, which is what makes the letter re-strike the note the column asked for
        // rather than keep stepping through the macro's jumps below it or leave the
        // column silent at whatever pitch the last step happened to have got to
        startNote (events, offset, ch, midiCh, runBase,
                   juce::jlimit (1, kCcTop, c.lastVol));

        armRetrig (ch, runBase, midiCh, rowPpq, c.retrigTicks, song);
    }
    else if (rowNamedLetter && ! noteOnRow)
        // the letter beside a row that named no note of its own works across the rows: the
        // note this channel has still been holding from the rows above is the one the
        // period named there belongs to, and this row re-opening it at its own sample
        // rather than leaving it sustaining until a strike one whole period below the row
        // reached it is what makes the retriggering of a note that has been sounding since
        // the rows above start on the row that named the letter. The strikes that follow
        // are then counted from one to the next at that period. Only the row that named the
        // letter gets that own-sample strike: a row naming only an argument under a carried
        // letter keeps its strikes where they were, the notes entered above having been
        // armed at their own rows, which is what leaves a retarget to the notes entering
        // there and the ones below, and a row whose note duplicated another channel's on
        // the same MIDI output channel in this row sent nothing of its own, so it leaves
        // nothing armed here either. A row that left this channel holding no note at all
        // has nothing to strike again, which is what makes the letter ignored rather than
        // armed for nothing where nothing is sounding
        armRetrig (ch, c.activeNote, c.activeMidiCh, rowPpq, 0, song);
}

//==============================================================================
/** A row naming one of the three volume letters or an argument under a carried
    level letter: what it asks for belongs to the MIDI output channel this row's
    instrument names rather than to the column the row sits in, which is what lets
    a letter spelled on any other column naming that instrument reach the same
    level and the same ramp, and lets the three work beside a run, a wheel's ramp,
    a wobble's swing and the two gates rather than trading any of them away. The
    level letter sends its level at this row's own sample and puts any ramp away
    rather than leaving one stepping toward the level above had called with; the
    two fade letters arm a ramp of their own at this row and put the other letter's
    away, that ramp's first value sitting at this row's own sample - nothing for a
    fade in and the whole level for a fade out, the way a run's first step sits at
    a row that named a note - and the rest of its values waiting for flushVolume
    below at one sub-tick of the Settings' grid apart. A step of 0 arms no ramp and
    puts any ramp away, which leaves the volume the last value had got to in force
    rather than sending one more, and a bare letter takes the default step the entry
    writes so the cell spells a fade rather than a hold. Where no level has been
    named for this channel yet, the level its ramps fade to and from is the default. */
void PlaybackEngine::volCommand (std::vector<PendingEvent>& events, int midiCh, int cmd,
                                 int argument, int offset, double rowPpq,
                                 const juce::Uuid& clipUid)
{
    const int idx = juce::jlimit (0, kMidiChannelCount - 1, midiCh);
    auto& v = vols[(size_t) idx];

    if (cmd == kLevelCmd)
    {
        // the level letter says what that controller is set to, ahead of anything the
        // ramp above had still got to say: the level is what a further I or O row fades
        // to or from, whether or not this row put a ramp away on the way
        // an argument the row left empty is the default level rather than silence,
        // the way the entry writes one beside a bare letter: only a spelled 0 asks
        // for the channel to be set to nothing
        const int level = argument == kEmpty ? kVolDefaultLevel
                                              : juce::jlimit (0, kCcTop, argument);
        v.level = level;
        v.endFade();

        events.push_back ({ offset, 3,
                            juce::MidiMessage::controllerEvent (idx + 1, volumeCc, level) });
        return;
    }

    // a step named by the row, a bare letter taking the default step the entry writes
    // so the cell spells a fade rather than a hold, and the argument of a letter the
    // entry could not clamp, such as one an older state left, staying inside the field's
    // own top rather than reading as the OFF or STP spellings those letters refuse
    const int step = argument == kEmpty ? kFadeDefaultStep
                                       : juce::jlimit (0, kCcTop, argument);

    if (step == 0)
    {
        // a step of no volume at all says no ramp: it stops the ramping, whichever
        // letter is spelled beside it, and leaves the volume the last value reached
        // still in force rather than sending one more of it
        v.endFade();
        return;
    }

    const bool fadingIn = cmd == kFadeInCmd;

    // the ramp of the other letter is over before this one starts: I stops an O
    // that was counting down and O stops an I that was counting up, so one channel
    // only ever has one ramp of its own counting at a time
    v.endFade();

    v.fadingIn = fadingIn;
    v.step = step;
    v.origin = rowPpq;
    v.rampClip = clipUid;

    // the values that ramp has to send: its first at the row that armed it and one
    // more every sub-tick until it reaches its target, that target being landed on
    // exactly, so a fade over the whole level at a step of 10 sends 13 values at the
    // default of 6 sub-ticks to the row rather than one a hair short of it and a
    // further one past it
    v.steps = 1 + (v.level + step - 1) / step;
    v.sent = 1;

    // the letter's own row carries the ramp's first value: nothing for a fade in and
    // the whole level for a fade out, the way a run's first step sits at a row that
    // named a note rather than a sub-row below it
    events.push_back ({ offset, 3,
                        juce::MidiMessage::controllerEvent (idx + 1, volumeCc,
                                                            fadingIn ? 0 : v.level) });
}

//==============================================================================
/** The values of the volume ramp one MIDI output channel has in force that belong
    to the position range [ppqStart, ppqLimit): they wait in that channel's state
    with where each sits counted from the row that armed the ramp over the Settings'
    grid, the way an arpeggiator's steps, a slide's ramp, a wobble's points, a cut's
    release and a retrigger's strikes do, which is what leaves the values of a row
    that spills past this block waiting for the block that reaches them rather than
    bunched onto the row that named the letter or dropped, and what keeps a fade
    reaching its target exactly however many blocks it spread over. A value is sent
    whether or not it is the one the channel's volume is already at, since unlike a
    wheel's position a controller has nothing that says a value is already in force.
    `ppqLimit` never passes the end of the row the engine last owned, and a ramp
    whose next value sits more than a row behind where this block starts is put away
    rather than arriving late, which is what a scrub or a jump leaves behind. */
void PlaybackEngine::flushVolume (std::vector<PendingEvent>& events, int midiCh,
                                  double ppqStart, double ppqLimit, double ppqPerSample,
                                  int numSamples, const Song& song)
{
    auto& v = vols[(size_t) midiCh];

    if (! v.fadeRunning())
        return;

    const double rowsPerBeat = (double) juce::jmax (1, song.settings.rowsPerBeat);
    const double rowSpanPpq = 1.0 / rowsPerBeat;
    const int subticks = juce::jmax (1, song.settings.subticksPerRow);
    const double stepPpq = rowSpanPpq / (double) subticks;

    if (v.origin + (double) v.sent * stepPpq < ppqStart - rowSpanPpq)
    {
        v.endFade(); // the next value sits more than a row behind where this block
                     // starts, so it cannot be placed any more, which is what a scrub
                     // or a jump leaves behind: the volume is left at whatever value
                     // had been sent, there being no message that puts a controller
                     // back, the way a run's, a ramp's, a wobble's and a cut's waiting
                     // steps are left behind there
        return;
    }

    while (v.sent < v.steps)
    {
        // the value's position is the arming row plus its count, never a sum of
        // sub-tick widths, so a fade spread over many blocks stays exact
        const double valueTime = v.origin + (double) v.sent * stepPpq;

        if (valueTime >= ppqLimit)
            break; // it waits for the rows below or for the block that reaches it

        if (valueTime >= ppqStart - kRowAlignEps)
        {
            const int offset = juce::jlimit (0, juce::jmax (0, numSamples - 1),
                                             (int) std::llround ((valueTime - ppqStart) / juce::jmax (1.0e-12, ppqPerSample)));

            // the value counts from the level rather than from the one the row above
            // reached, which is what lands the target exactly on the sub-tick the step
            // names rather than a hair off it and leaves nothing of this ramp to send
            // below that
            events.push_back ({ offset, 3,
                                juce::MidiMessage::controllerEvent (midiCh + 1, volumeCc,
                                                                    v.fadingIn ? juce::jmin (v.level, v.sent * v.step)
                                                                             : juce::jmax (0, v.level - v.sent * v.step)) });
        }

        ++v.sent;
    }
}

//==============================================================================
void PlaybackEngine::processRow (const Pattern& pattern, const Clip& clip, int row,
                                 std::vector<PendingEvent>& events, int offset,
                                 const Song& song, double rowPpq)
{
    std::set<std::pair<int, int>> notesThisRow;

    for (int ch = 0; ch < pattern.channels && ch < kMaxTrackerChannels; ++ch)
    {
        const auto& cell = pattern.rows[(size_t) row][(size_t) ch];
        auto& state = channels[(size_t) ch];

        // carry: explicit values become the new "last used" for this tracker channel
        if (cell.inst != kEmpty)
            state.lastInst = juce::jlimit (0, kMidiChannelCount - 1, cell.inst);

        if (cell.vol != kEmpty)
            state.lastVol = cell.vol;

        if (cell.pan != kEmpty)
            state.lastPan = cell.pan;

        const int midiCh = juce::jlimit (0, kMidiChannelCount - 1, state.lastInst);

        // the note this channel entered at this row, once the note handling below
        // has decided it: a row that entered one of its own sends that pitch and is
        // the one a cut letter, a length named under a carried one or a bare note row
        // under a carried one holds open, so the cut's own release below has a pitch to
        // belong to rather than whatever the channel happened to be holding from above.
        // A cut letter beside a row that named no note of its own arms for whatever the
        // channel has still been holding instead, which is what lets the letter cut
        // across the rows. A row whose note duplicates another tracker channel's on the
        // same MIDI output channel in this row was not sent at all, so it leaves nothing
        // here armed
        int enteredNote = kEmpty;

        // answers a row that named the bend letter or an argument under a carried
        // one: its argument names a point on the wheel, so the OFF spelling names the
        // wheel's centre rather than a number above the field's own top, and an STP
        // one asks for nothing at all, which is what leaves the wheel sitting at the
        // point it is at for the rows below
        auto answerBend = [&] (int argument)
        {
            if (argument == kArgStp)
                return;

            bendTo (events, ch, midiCh, argument == kArgOff ? kBendCentreVal
                                                            : argument,
                    offset, clip.uid);
        };

        if (cell.cmd != kEmpty)
        {
            state.lastCmd = cell.cmd;
            state.lastCmdVal = cell.cmdVal;

            // the volume letters only ever say something about the volume controller
            // of the MIDI output channel this row's instrument names, so a row naming
            // one of them leaves this column's run, the wheel's ramp, a wobble's swing,
            // a cut's gate and a retrigger's gate where they are: the three work in
            // parallel with the other seven rather than trading anything for anything,
            // and a letter spelled on another column naming the same instrument reaches
            // that instrument's level without saying anything about this column either
            const bool volumeLetter = cell.cmd == kLevelCmd || cell.cmd == kFadeInCmd
                                         || cell.cmd == kFadeOutCmd;

            if (cell.cmd == kArpCmd)
            {
                // an arpeggiator call on the column a pitch wheel was bent in ends
                // that wheel rather than leaving it bent under the run: a wheel is a
                // state the whole channel carries, and the row that calls a macro has
                // nothing else to put it right; a ramp still climbing there has the
                // same ending, its own rows not being the ones a run steps over
                endWheel (events, ch, midiCh + 1, offset);
            }
            else if (! volumeLetter && cell.cmd != kSlideUpCmd && cell.cmd != kSlideDownCmd)
            {
                // a row naming a command that is not a slide leaves no ramp in
                // force: the wheel such a row asks for is a point rather than a
                // climb, so the climb the column had is over and the wheel stays
                // where that row left it; a slide row arms its own ramp below, at
                // the point this left the wheel at
                state.endSlide();
            }

            // a row naming a command that is not the bend letter leaves no wobble in
            // force either: a bend row asked for a new point on the wheel rather than
            // for the swing to end, so it re-bases the swing below instead, while
            // every other command's wheel is a point or nothing at all and the swing
            // above it has no rows of its own left to keep swinging under this one;
            // the vibrato row that arms its own wobble does so below, at the point
            // this left the wheel at, the way a slide row arms its own ramp. Where the
            // letter taking the swing away is the retrigger letter, that row also puts
            // the wheel back at where it took it away from, sent at this row's own
            // sample ahead of the strike spelled there: a strike is a fresh
            // articulation of the note the column is holding and the pitch a host gives
            // that articulation is the one the wheel is sitting at, so a cycle's last
            // point leaving the wheel part way down its swing would have the letter's
            // own strike and the ones counted on from it below play the note at
            // wherever the cycle happened to have got to rather than at the pitch the
            // column named. A swing's displacement is the only displacement that goes:
            // a point a bend row left on the wheel and the point a slide's ramp had
            // climbed to on this column keep their pitch below the letter, which is
            // what makes the letter re-strike a note at the pitch the column asked to
            // have bent rather than straightening a pitch it was asked to bend, and a
            // ramp another column was climbing is left climbing on because the wheel
            // is that column's own state rather than one this row has anything to do
            // with
            if (! volumeLetter && cell.cmd != kBendCmd)
            {
                if (cell.cmd == kRetrigCmd && state.vibRunning())
                    endWheel (events, ch, midiCh + 1, offset);

                state.endVib();
            }

            // a row naming a command that is not the cut letter leaves no gate in force
            // under it either: the letter the column stopped having has no note-length
            // left to hold the notes below it, and the release that gate had armed for a
            // note entered above it has no gate of its own left to cut it however far its
            // counter had got, the way a run's and a ramp's waiting steps go when the
            // column supersedes them lower down - a cut of a long length still waiting at
            // this row is discarded rather than left to close whatever note the column
            // happens to be holding at the subtick it was waiting for, the note that entered
            // above being left to whatever ends it below, a further note entering this
            // column having already opened it at this row. The cut row that arms a gate
            // of its own does so below, at the note it entered or at the note it has been
            // holding, the way a slide row arms its own ramp at the point this left the
            // wheel at
            if (! volumeLetter && cell.cmd != kCutCmd)
                state.endCut();

            // a row naming a command that is not the retrigger letter leaves no retrigger
            // in force under it either, the way the cut's gate is put away above: the letter
            // the column stopped having has no strikes left to make and the one that gate
            // had armed for a note entered above it has no gate of its own left to strike
            // again however far its counter had got, so a strike still waiting at this row
            // is discarded rather than left to close and re-open whatever note the column
            // happens to be holding at the sub-tick it was waiting for, the note that entered
            // above being left to whatever ends it below, a further note entering this
            // column having already opened it at this row. A strike the gate had already
            // played stays sounding, its note-on having opened the note again there. The
            // retrigger row that arms a gate of its own does so below, at the note it entered
            // or at the note it has been holding, and a retrigger row leaves the cut's gate
            // away beside it, a cut and a retrigger not being two gates one column can hold
            // at once
            if (! volumeLetter && cell.cmd != kRetrigCmd)
                state.endRetrig();

            // a row naming one of the two fade letters leaves nothing in force below
            // it: the ramp it armed belongs to the MIDI output channel rather than to
            // this column, so it has no letter of this column left to answer the rows
            // under it, which is what makes a row naming only an argument there name
            // nothing at all, and what stops the tracker drawing a carried letter and
            // a step over the rows below. The level letter is not one-shot: it sits with
            // the plain CC numbers the column answers, so an argument named under a
            // carried one sets and sends a new level further down
            if (cell.cmd == kFadeInCmd || cell.cmd == kFadeOutCmd)
            {
                state.lastCmd = -1;
                state.lastCmdVal = -1;
            }
        }

        // An explicit volume cell no longer drives a controller at all: it names the
        // velocity a note entered in this row is struck at, which the note handling
        // below reads, so the row having only one says nothing on its own. A pan
        // cell still sends its configured controller (CC10 by default, named in the
        // Settings tab), and this happens before the note handling, so a row holding
        // only a note-off still applies it. The volume a MIDI output channel sounds
        // at is what the L, I and O letters of the Command column send instead, to
        // the controller the Settings name for them (CC7 channel volume by default)
        if (cell.pan != kEmpty)
            events.push_back ({ offset, 2,
                                juce::MidiMessage::controllerEvent (midiCh + 1, panCc,
                                                                    juce::jlimit (0, 127, cell.pan)) });

        const bool noteOff = cell.note == kNoteOff;
        const bool noteHere = ! noteOff && cell.note != kEmpty && cell.note >= 0 && cell.note <= 127;

        if (noteOff)
        {
            if (state.activeNote >= 0)
                stopNote (events, offset, ch, state.activeMidiCh, state.activeNote);

            // a run the rows above left in force has no note to sound again below
            // a note-off: its steps go silent until a note enters the column, which
            // is what ends a looped run there rather than only at the clip's end
            if (state.arpId > 0)
                state.arpCallNote = -1;

            // the note-off took the held note away, so a strike the retrigger gate had
            // armed for it has nothing left to close and re-open and the strikes counted
            // on from it go with it: the letter keeps holding the period rather than the
            // column having to name it again, which is what lets a note entering a row
            // under it below sound and restart its striking and leaves the letter beside
            // a row that entered no note of its own ignored where nothing is sounding
            state.retrigWaiting();
        }
        else if (noteHere)
        {
            // this is the note the column last named: a call at this row steps
            // from it, a call lower down steps from it too while nothing else
            // entered the column, and a run that is still going measures from it
            // from here on
            state.columnNote = cell.note;

            // the call this row arms runs its first step here rather than one
            // subtick below, so this row's note is that first step: the step's
            // jump moves the note that gets triggered, the step that sounds
            // nothing leaves it where it was typed, and the note-off step has
            // nothing to trigger at all. Which call the row arms is read the way
            // arpCommand below resolves it, so the two cannot disagree about what
            // the note entered here is about to be measured against - the pitch
            // as typed stays the note the column named and the run's own answer
            // for its steps, whatever the first step did to the note it sounded
            const int armedId = arpCallArmedAtRow (cell, state.lastCmd);
            const int firstStep = armedId > 0 ? arpFirstStepOf (song, armedId) : kArpEmptyStep;
            const bool firstStepRests = firstStep == kArpNoteOffStep;

            const int enterNote = firstStepRests || firstStep == kArpEmptyStep
                                      ? cell.note
                                      : arpSteppedNote (cell.note + firstStep);

            const std::pair<int, int> key { midiCh, enterNote };

            if (firstStepRests)
            {
                // the note entered here opens nothing of its own, so the note the
                // channel was holding at this row goes silent here rather than
                // being held on to whatever ends it below: the step that is a
                // rest belongs to the run, the same way a note-off row belongs to
                // the column, and the column still names the note entered above
                // it, which is what lets the run's steps below the rest measure from
                // it again
                if (state.activeNote >= 0)
                    stopNote (events, offset, ch, state.activeMidiCh, state.activeNote);

                state.arpNote = -1;
            }
            else if (notesThisRow.insert (key).second)
            {
                const int velocity = cell.vol != kEmpty ? cell.vol : juce::jlimit (1, kCcTop, state.lastVol);
                startNote (events, offset, ch, midiCh, enterNote, velocity);
                state.activeClip = clip.uid;
                enteredNote = enterNote; // the pitch this channel sent, which is what a
                                         // cut armed at this row or carried down to it
                                         // below holds open over its own subticks
            }

            // the run's first step left that pitch held rather than the note as
            // it was typed, so it is the run's own to release the way any other
            // step's is, and a run whose first step was a rest has nothing held
            if (armedId > 0)
                state.arpNote = firstStepRests ? -1 : enterNote;
        }

        if (cell.cmd != kEmpty)
        {
            // a row naming a note-off gets its command answered too: the release
            // that row asked for went out above as the note-off itself, so the
            // call written beside it is answered against the note the column had
            // named rather than being skipped along with the note
            if (cell.cmd == kArpCmd)
                arpCommand (events, ch, cell.cmdVal, offset, rowPpq, clip.uid,
                            noteHere, noteOff, song);
            else if (cell.cmd == kBendCmd)
                answerBend (cell.cmdVal);
            else if (cell.cmd == kSlideUpCmd || cell.cmd == kSlideDownCmd)
                // a slide names an extreme rather than a point on the wheel, so its
                // argument arms a ramp the sub-row steps below it walk toward that
                // extreme, answers an OFF one by putting the wheel away and holds an
                // STP one by leaving the wheel at the point it had got to
                slideCommand (events, ch, offset, rowPpq, cell.cmdVal,
                              cell.cmd == kSlideUpCmd, noteHere, clip.uid, song);
            else if (cell.cmd == kVibCmd)
                // the letter's argument names which vibrato macro the column runs
                // rather than anything about the wheel itself, so the row arms a
                // wobble whose points the flush below moves over the macro's own
                // cycle grid, answers an OFF one by taking the wheel back and holds
                // an STP one by asking for nothing at all
                vibCommand (events, ch, cell.cmdVal, offset, rowPpq, clip.uid,
                            noteHere, song);
            else if (cell.cmd == kCutCmd)
                // the letter's own argument counts subticks the note that row entered
                // is held for, so the row sends nothing of its own: the note-off it asks
                // for waits for the flush below, which reaches the subtick the length names
                // past this row, a length wider than the row's own subticks reaching past
                // the row and the block that holds it the way a run's steps do
                cutCommand (events, ch, offset, rowPpq, cell.cmdVal, true, enteredNote,
                            noteHere, midiCh, song);
            else if (cell.cmd == kRetrigCmd)
                // the letter's own argument counts the sub-ticks apart a note that row
                // entered or has been held in this column is struck again at, so the row
                // sends nothing there of its own where the row entered a note, that note's
                // own arrival at the row being its first trigger and the first strike waiting
                // for the flush below at the sub-tick the period names past this row, and
                // closes and re-opens the note it has still been holding at its own sample
                // where the row named no note of its own. Where the column instead had an
                // arpeggiator run in force rather than a note of its own being held, this
                // row closes the pitch that run was holding at its own sample and goes back
                // to the note that run measured its steps from, read as it was typed in the
                // note column rather than as the run's first step moved it there, opening
                // that base again at this row and waiting the whole period named here for
                // the strike below rather than leaving the column silent at whatever pitch
                // a step had got to - the run and a gate not being two things one column
                // can hold at once and the run's own displacement being what goes beside
                // the letter, a run another column armed being left alone rather than traded
                // for its base. The strikes the flush below counts on from either one go on
                // at that period, a period wider than the row's own sub-ticks reaching past
                // the row and the block that holds it the way the cut's release and a run's
                // steps do. Where this column
                // had a wobble swinging, the wheel's take-back above this call had already
                // put the strike spelled here against a straight wheel, so the letter strikes
                // a note again at the pitch the column named rather than at one the cycle
                // had left the wheel part way down its swing
                retrigCommand (events, ch, offset, rowPpq, cell.cmdVal, true, enteredNote,
                               noteHere, midiCh, song);
            else if (cell.cmd == kLevelCmd || cell.cmd == kFadeInCmd || cell.cmd == kFadeOutCmd)
                // one of the three volume letters: what it says is about the volume
                // controller of the MIDI output channel this row's instrument names,
                // which is the same level and the same ramp a further column naming
                // that instrument reaches, and one that is nothing at all to do with
                // this column's run, wheel, wobble or gates
                volCommand (events, midiCh, cell.cmd, cell.cmdVal, offset, rowPpq, clip.uid);
            else
                events.push_back ({ offset, 3, juce::MidiMessage::controllerEvent (midiCh + 1,
                                                                                  juce::jlimit (0, kCcTop, cell.cmd),
                                                                                  juce::jlimit (0, 127, cell.cmdVal >= 0 ? cell.cmdVal : 0)) });
        }

        // a row that only names an argument plays it on the command that is in
        // force above it in the column, which is what an implicit cell under a
        // carried command means; a column that never named one has nothing to
        // carry, so such a row emits nothing
        else if (cell.cmdVal != kEmpty && state.lastCmd >= 0)
        {
            if (state.lastCmd <= kCcTop)
            {
                events.push_back ({ offset, 3,
                                    juce::MidiMessage::controllerEvent (midiCh + 1, state.lastCmd,
                                                                        juce::jlimit (0, 127, cell.cmdVal)) });
            }
            else if (state.lastCmd == kBendCmd)
            {
                // under a carried bend the argument is a new point on the same
                // wheel, which is also how a row re-asserts the bend a note-off
                // below the row that asked for it had just taken back
                answerBend (cell.cmdVal);
            }
            else if (state.lastCmd == kSlideUpCmd || state.lastCmd == kSlideDownCmd)
            {
                // under a carried slide the argument is a new span rather than a
                // new point, so the row arms a ramp of its own from where the
                // wheel had got to: a slide can be retargeted as it climbs, and an
                // OFF or STP one answers the same way the letter's own row does
                slideCommand (events, ch, offset, rowPpq, cell.cmdVal,
                              state.lastCmd == kSlideUpCmd, noteHere, clip.uid, song);
            }
            else if (state.lastCmd == kVibCmd)
            {
                // under a carried vibrato letter the argument is a new macro id, so
                // the row starts that wobble rather than saying anything about the
                // wheel: an OFF or 0 one takes the pitchbending away and a STP one
                // asks for nothing, both the way the letter's own row answers them,
                // which is what lets a wobble be retargeted as it swings
                vibCommand (events, ch, cell.cmdVal, offset, rowPpq, clip.uid,
                            noteHere, song);
            }
            else if (state.lastCmd == kCutCmd)
            {
                // under a carried cut the argument is a new length rather than anything
                // the row itself names: the row retargets the gate for the notes entered
                // from here on, an OFF or a 0 one putting the gate away and a bare letter
                // row's own note being left sustained, and an STP one asking for nothing
                // at all since a cut has no move of the wheel to stop. Where this row
                // entered a note of its own as well, that note is held for the length in
                // force here rather than for whatever the letter above had called with,
                // which is what lets a staccato section be retargeted row by row
                cutCommand (events, ch, offset, rowPpq, cell.cmdVal, false, enteredNote,
                            noteHere, midiCh, song);
            }
            else if (state.lastCmd == kRetrigCmd)
            {
                // under a carried retrigger the argument is a new period rather than anything
                // the row itself names: the row retargets the gate for the notes entered
                // from here on, an OFF or a 0 one putting the gate away and a bare letter
                // row's own note being left struck again at the period above it, and an STP
                // one asking for nothing at all since a retrigger has no move of the wheel
                // to stop. Where this row entered a note of its own as well, that note is
                // re-struck at the period in force here rather than for whatever the letter
                // above had called with, which is what lets a repeated-hit passage be
                // retargeted row by row, and where the column had a note still being held
                // from above it rather than one entered here, the period named here moves
                // the strikes below it rather than the ones the engine had already played,
                // and where the column instead had an arpeggiator run in force rather than
                // a note of its own being held, this row takes that run away and goes back
                // to the note that run measured its steps from, as it was typed in the note
                // column rather than as the run's first step moved it, the period named here
                // striking that base again at the sub-tick below rather than leaving the
                // column silent at whatever pitch a step had got to
                retrigCommand (events, ch, offset, rowPpq, cell.cmdVal, false, enteredNote,
                               noteHere, midiCh, song);
            }
            else if (state.lastCmd == kLevelCmd)
            {
                // under a carried level letter the argument is a new level rather than
                // anything the row itself names: the row sets that level on the
                // instrument's own volume controller and sends it there, an argument
                // being as much a level as the row that named the L spelled one, and
                // puts any ramp that was still counting on away rather than leaving
                // one stepping toward the level above had called with
                volCommand (events, midiCh, kLevelCmd, cell.cmdVal, offset, rowPpq, clip.uid);
            }
            else
            {
                // the argument of an arpeggiator call is its macro id, so a row
                // under a carried call names a new one: id 0 releases what the
                // run was holding and ends it, another starts that macro
                arpCommand (events, ch, cell.cmdVal, offset, rowPpq, clip.uid,
                            noteHere, noteOff, song);
            }
        }

        // a row that named neither a command nor an argument still plays the gate a
        // carried letter leaves in force, where it entered a note of its own: the cut
        // and the retrigger are the two things in this column that keep being carried
        // down under it rather than answering the rows below it, which is what the tracker
        // draws as a dim letter and a dim period over those rows and what lets one row's
        // letter gate a whole staccato or repeated-hit passage rather than answering only
        // its own row
        else if (noteHere && state.lastCmd == kCutCmd)
            cutCommand (events, ch, offset, rowPpq, kEmpty, false, enteredNote,
                        noteHere, midiCh, song);
        else if (noteHere && state.lastCmd == kRetrigCmd)
            // a note entering a row that named no command of its own under a carried
            // retrigger letter plays that note and restarts the striking from that row:
            // the first strike sits the period the column carries past this row rather
            // than past the last one above it, which is what the letter's own row and a
            // period named under it already do and what keeps a row a transport jump
            // makes this block play again arming its own strikes at that row
            retrigCommand (events, ch, offset, rowPpq, kEmpty, false, enteredNote,
                           noteHere, midiCh, song);

        // the note-off takes the note the column named away for the rows below it,
        // which happens after the row's own command had the note it was releasing
        // to answer against
        if (noteOff)
            state.columnNote = -1;

        // the ramp this channel's column has in force is not moved here, and neither
        // is the swing of a wobble it has in force, the release of a cut it has in
        // force, the strikes a retrigger has waiting or the values of a volume fade
        // this row armed on the MIDI output channel: their steps, points, subticks,
        // strikes and values are nearer than a row, so process() flushes them after
        // the row it has processed, up to that row's end, which is what leaves a row's
        // own steps, points, waiting note-offs and fade values waiting for the
        // block that reaches them rather than being bunched onto this row's samples

        uiRow[(size_t) ch].store (row);
    }
}

void PlaybackEngine::process (const Song& song, juce::MidiBuffer& midi, const Transport& t,
                              int numSamples, double sampleRate, bool /*passThrough*/)
{
    std::vector<PendingEvent> events;

    volumeCc = juce::jlimit (0, 127, song.settings.volumeCc);
    panCc = juce::jlimit (0, 127, song.settings.panCc);

    const double ppqPerSample = (t.bpm / 60.0) / juce::jmax (1.0, sampleRate);
    const double ppqStart = t.ppq;
    const double ppqEnd = ppqStart + (double) numSamples * ppqPerSample;

    if (! t.playing)
    {
        if (wasPlaying)
        {
            allNotesOff (events, 0);

            for (auto& e : events)
                midi.addEvent (e.message, 0);
        }

        wasPlaying = false;
        justStarted = false;
        previousPpq = ppqStart;
        return;
    }

    justStarted = ! wasPlaying;
    const bool jumped = previousPpq >= 0.0 && std::abs (ppqStart - previousPpq) > (double) numSamples * ppqPerSample * 4.0 + 0.01;

    // A transport that came back to an earlier position, which is a loop wrap or
    // a scrub: the reported start is the point the host meant, so the row it
    // landed inside belongs to the new pass. Seeking forward into a row stays
    // what the Settings row about a mid-row start describes.
    const bool jumpedBack = jumped && ppqStart < previousPpq;

    // A transport starting or jumping releases what the synth may still be
    // holding from the position it left, before this block's rows play their own
    // notes, so these releases are ordered ahead of the note-ons this block
    // queues at the same sample.
    if (justStarted || jumped)
        allNotesOff (events, 0);

    if (justStarted)
    {
        for (auto& c : channels)
            c.lastRow = -1;
    }

    const double rowsPerBeat = (double) juce::jmax (1, song.settings.rowsPerBeat);

    // Hosts report per-block positions with their own rounding; a transport
    // stopping exactly at a clip end can end a hair before the computed bound.
    const double span = (double) numSamples * ppqPerSample;
    const double spanEps = std::max (1.0e-6, span * 1.0e-3);

    // Rows the previous block already covered must not be processed twice
    // (hosts may report a block start a few ulps below the previous computed
    // end); a deliberate backwards seek is excluded from this dedup.
    const double drift = std::max (1.0e-9, span * 1.0e-4);
    const bool nearContinuous = previousPpq >= 0.0 && ppqStart >= previousPpq - drift;

    // Steps still waiting from the block before this one: their run's rows have
    // already been processed, so the part this block may still play stops at the
    // end of the last row the engine owned, which is what keeps a run the column
    // supersedes lower down from replaying steps that belong below that row. The
    // steps of a slide's ramp, the points of a wobble's cycle and the note-off a
    // cut of a longer length than the row's own subticks waits for all wait the same
    // way, which is what leaves the block boundary landing inside a row's own sub-row
    // steps, inside a cycle's own points or inside one held note's own subticks to be
    // picked up rather than dropped or bunched onto one sample. The strikes a retrigger
    // has waiting wait the same way again, its periods being counted over that same grid,
    // which is what leaves a strike inside one held note's own sub-ticks being placed at
    // its own sample in the block that gets there rather than bunched onto the row that
    // armed it or dropped for sitting past this block's end
    for (int ch = 0; ch < kMaxTrackerChannels; ++ch)
    {
        auto& c = channels[(size_t) ch];

        // a release only lives inside the block being queued: the sample it was
        // queued at belongs to this block, so the record does not reach into the
        // block below it and a note entered at a same-numbered sample there still
        // asks the stuck-key guard for its own opening
        c.relNote = -1;
        c.relMidiCh = -1;
        c.relOffset = -1;

        if (c.arpId > 0)
            flushArp (events, ch, ppqStart, juce::jmin (ppqEnd, c.arpRowEnd), ppqPerSample, numSamples, song);

        if (c.slideRunning())
            flushSlide (events, ch, ppqStart, juce::jmin (ppqEnd, c.slideRowEnd), ppqPerSample, numSamples, song);

        if (c.vibRunning())
            flushVib (events, ch, ppqStart, juce::jmin (ppqEnd, c.vibRowEnd), ppqPerSample, numSamples, song);

        if (c.cutTime >= 0.0)
            flushCut (events, ch, ppqStart, juce::jmin (ppqEnd, c.cutRowEnd), ppqPerSample, numSamples, song);

        // the bound is the cut's own row end, which every row and channel sets below
        // whatever the column carries, so a gate the column has no letter for has its
        // strikes waited on over the same rows a gate it does have does
        if (c.retrigTime >= 0.0)
            flushRetrig (events, ch, ppqStart, juce::jmin (ppqEnd, c.cutRowEnd), ppqPerSample, numSamples, song);
    }

    // a volume ramp counts on the same grid rather than on one per column, so its
    // values wait between blocks the way a run's steps, a ramp's, a wobble's points
    // and a cut's release do, and they belong to the MIDI output channel rather than
    // to the column that armed them, which is why this loop goes over the channels
    // the tracker's does
    for (int m = 0; m < kMidiChannelCount; ++m)
    {
        auto& v = vols[(size_t) m];

        if (v.fadeRunning())
            flushVolume (events, m, ppqStart, juce::jmin (ppqEnd, v.rowEnd), ppqPerSample, numSamples, song);
    }

    for (auto& clip : song.clips)
    {
        const auto* pattern = song.findPattern (clip.patternId);

        if (pattern == nullptr)
            continue;

        const double clipStart = clip.startBeats;
        const double clipEnd = clipStart + (double) pattern->length / rowsPerBeat;

        if (ppqEnd <= clipStart || ppqStart >= clipEnd)
            continue;

        const int firstRow = juce::jmax (0, (int) std::floor ((ppqStart - clipStart) * rowsPerBeat) - 1);
        const int lastRow = juce::jmin (pattern->length - 1, (int) std::ceil ((ppqEnd - clipStart) * rowsPerBeat) + 1);

        for (int row = firstRow; row <= lastRow; ++row)
        {
            const double rowTime = clipStart + (double) row / rowsPerBeat;

            if (rowTime >= ppqEnd)
                continue;

            if (rowTime < ppqStart)
            {
                // A continuous run: the block before this one already owned the
                // rows just behind this start, so only a hair of reporting error
                // reaches them, and that hair is what keeps a row from being
                // played twice by two blocks.
                if (nearContinuous)
                {
                    if (! (rowTime >= previousPpq && rowTime >= ppqStart - drift))
                        continue;
                }
                // A transport that jumped back (a seek, a loop wrap): nothing
                // before this start has been covered, so a row a hair behind it
                // is the row the host meant and belongs to this block at offset
                // 0 rather than being dropped. Further behind it the re-arm
                // branch below owns the row the start landed inside.
                else if (rowTime < ppqStart - kRowAlignEps)
                {
                    continue;
                }
            }

            if (nearContinuous && rowTime >= ppqStart && rowTime < previousPpq)
                continue; // covered by the previous block already

            const int offset = juce::jlimit (0, juce::jmax (0, numSamples - 1),
                                             (int) std::llround ((rowTime - ppqStart) / juce::jmax (1.0e-12, ppqPerSample)));

            processRow (*pattern, clip, row, events, offset, song, rowTime);

            const double rowEnd = rowTime + 1.0 / rowsPerBeat;

            for (int ch = 0; ch < pattern->channels && ch < kMaxTrackerChannels; ++ch)
            {
                auto& c = channels[(size_t) ch];
                c.lastRow = row;
                c.arpRowEnd = rowEnd;
                c.slideRowEnd = rowEnd;
                c.vibRowEnd = rowEnd;
                c.cutRowEnd = rowEnd;

                // the steps of a run that reached this row sound up to its end;
                // the ones past it wait for the next rows or the next block,
                // which is what keeps a macro longer than one row from being
                // squeezed onto this row's samples. The steps of a ramp the column
                // is in sound the same way up to this row's end, which is what
                // keeps the wheel climbing over the sub-row grid rather than being
                // moved to the point a whole row reaches for, and the points of a
                // wobble's cycle swing the same way up to it, which is what keeps
                // the wheel moving over that cycle's own points rather than over
                // the rows it sits inside. The note-off a cut waits for sounds up
                // to it too, which is what holds a note over the subticks its
                // length names rather than over the rows the note sits inside, and
                // is what lets a cut narrower than one row close the note partway
                // through that row rather than at its end. The strikes a retrigger
                // has waiting sound up to it as well, which is what re-articulates
                // a note over the sub-ticks its period names rather than over the
                // rows the note sits inside, what lets a period narrower than a row
                // strike more than once inside one row and one wider than a row keep
                // striking over the rows below it rather than ending at this one
                flushArp (events, ch, ppqStart, juce::jmin (ppqEnd, rowEnd), ppqPerSample, numSamples, song);

                flushSlide (events, ch, ppqStart, juce::jmin (ppqEnd, rowEnd), ppqPerSample, numSamples, song);

                flushVib (events, ch, ppqStart, juce::jmin (ppqEnd, rowEnd), ppqPerSample, numSamples, song);

                flushCut (events, ch, ppqStart, juce::jmin (ppqEnd, rowEnd), ppqPerSample, numSamples, song);

                flushRetrig (events, ch, ppqStart, juce::jmin (ppqEnd, rowEnd), ppqPerSample, numSamples, song);
            }

            // the ramp a volume letter armed at this row counts its values from this
            // row too, so its values sit at their own sub-ticks of the Settings' grid
            // up to this row's end rather than being bunched onto it, a ramp wider than
            // the row's own sub-ticks waiting for the rows below or for the next block
            // the way a run's steps, a slide's ramp, a wobble's points, a cut's release
            // and a retrigger's strikes do, and they are owned by the MIDI output
            // channel rather than by the column that armed them, which is why this loop
            // goes over the channels rather than sitting inside the tracker's one
            for (int m = 0; m < kMidiChannelCount; ++m)
                vols[(size_t) m].rowEnd = rowEnd;

            for (int m = 0; m < kMidiChannelCount; ++m)
            {
                auto& v = vols[(size_t) m];

                if (v.fadeRunning())
                    flushVolume (events, m, ppqStart, juce::jmin (ppqEnd, rowEnd), ppqPerSample, numSamples, song);
            }
        }

        // A row the transport starts inside. A fresh start and a forward seek
        // honour the Settings row that says to ignore such a row; a transport
        // that came back to an earlier position does not, because the host meant
        // that position and the row its block boundary fell inside belongs to the
        // new pass - which is what lets a loop whose block boundary landed past
        // its start still play the note sitting on that start. A row sitting at
        // the start itself was already played by the loop above, so this only
        // ever reaches a row strictly behind it and never doubles one.
        if (jumpedBack || (justStarted && ! song.settings.midRowIgnored))
        {
            const int rowAtStart = (int) std::floor ((ppqStart - clipStart) * rowsPerBeat);

            if (rowAtStart >= 0 && rowAtStart < pattern->length)
            {
                const double rowTime = clipStart + (double) rowAtStart / rowsPerBeat;

                if (rowTime < ppqStart - kRowAlignEps)
                {
                    processRow (*pattern, clip, rowAtStart, events, 0, song, rowTime);

                    const double rowEnd = rowTime + 1.0 / rowsPerBeat;

                    for (int ch = 0; ch < pattern->channels && ch < kMaxTrackerChannels; ++ch)
                    {
                        auto& c = channels[(size_t) ch];
                        c.lastRow = rowAtStart;
                        c.arpRowEnd = rowEnd;
                        c.slideRowEnd = rowEnd;
                        c.vibRowEnd = rowEnd;
                        c.cutRowEnd = rowEnd;

                        // the run this row starts counts from the row itself, so
                        // its first step is played at this block's first sample
                        // rather than being dropped for sitting behind the start.
                        // A ramp this row armed counts from this row too, which is
                        // what a slide written at the row a seek landed inside
                        // starts climbing at that row rather than a row below it,
                        // and a wobble this row armed counts its cycle from it as
                        // well, which is what a vibrato written at the row a seek
                        // landed inside swings from that row rather than a row below
                        // and what a cut written there holds the note it entered
                        // from that row's own subticks rather than from the ones
                        // below it. A retrigger written there strikes the note it
                        // entered or the one it has been holding from that row's own
                        // sub-ticks rather than from the ones below it as well, which
                        // is what a row a loop wrapped back inside strikes again at
                        // the sub-ticks its period names past that row rather than
                        // at the ones the pass above had already counted out
                        flushArp (events, ch, rowTime, juce::jmin (ppqEnd, rowEnd), ppqPerSample, numSamples, song);

                        flushSlide (events, ch, rowTime, juce::jmin (ppqEnd, rowEnd), ppqPerSample, numSamples, song);

                        flushVib (events, ch, rowTime, juce::jmin (ppqEnd, rowEnd), ppqPerSample, numSamples, song);

                        flushCut (events, ch, rowTime, juce::jmin (ppqEnd, rowEnd), ppqPerSample, numSamples, song);

                        flushRetrig (events, ch, rowTime, juce::jmin (ppqEnd, rowEnd), ppqPerSample, numSamples, song);
                    }

                    // a ramp a volume letter armed at this row counts from this row as
                    // well, which is what a fade written at the row a seek landed inside
                    // starts sending its values from that row rather than a row below it
                    for (int m = 0; m < kMidiChannelCount; ++m)
                        vols[(size_t) m].rowEnd = rowEnd;

                    for (int m = 0; m < kMidiChannelCount; ++m)
                    {
                        auto& v = vols[(size_t) m];

                        if (v.fadeRunning())
                            flushVolume (events, m, rowTime, juce::jmin (ppqEnd, rowEnd), ppqPerSample, numSamples, song);
                    }
                }
            }
        }

        // Notes that were started by this clip must stop when the clip ends.
        if (ppqStart < clipEnd && ppqEnd + spanEps >= clipEnd)
        {
            const int offset = juce::jlimit (0, juce::jmax (0, numSamples - 1),
                                             (int) std::llround ((clipEnd - ppqStart) / juce::jmax (1.0e-12, ppqPerSample)));

            for (int ch = 0; ch < pattern->channels && ch < kMaxTrackerChannels; ++ch)
            {
                auto& c = channels[(size_t) ch];

                if (c.activeNote >= 0 && c.activeClip == clip.uid)
                    stopNote (events, offset, ch, c.activeMidiCh, c.activeNote);

                // an arpeggiator run this clip owns ends with the clip: whatever
                // of it was still waiting has no rows below it to play it, and
                // the note the last step left is released here
                if (c.arpClip == clip.uid)
                    c.endArp();

                // a wheel this clip's own row left bent returns to the centre at
                // that clip's end as well, so the notes of the clip below it are
                // not bent by a row that ended this one. A row that struck a note
                // already had its wheel taken back by the note-off above, so this
                // is what only a row that bent without striking needs, and the
                // message goes to the channel the bend row's own instrument named.
                // A ramp this clip armed that had not reached its extreme yet has
                // no rows below the clip left to finish it either: its own wheel
                // may still sit at the centre, in which case only the ramp needs
                // putting away and no message is what takes it back, and a swing
                // this clip armed has no rows below it left to keep swinging either,
                // which is what leaves a wobble at the pitch its last point had got
                // to rather than one that carries on over the clip below
                const bool wheelBent = c.lastBendPos != kBendCentrePos;
                const bool clipBentIt = c.bendClip == clip.uid && wheelBent;
                const bool clipRampedIt = c.slideClip == clip.uid && c.slideRunning();
                const bool clipWobbledIt = c.vibClip == clip.uid && c.vibRunning();

                if (clipBentIt || clipRampedIt || clipWobbledIt)
                {
                    if (wheelBent)
                    {
                        const int bendMidiCh = juce::jlimit (0, kMidiChannelCount - 1, c.lastInst) + 1;
                        events.push_back ({ offset, 0, juce::MidiMessage::pitchWheel (bendMidiCh, kBendCentrePos) });
                    }

                    c.endBend();
                }
            }

            // a volume ramp this clip's own row armed has no rows below the clip left
            // to keep counting its sub-ticks either, so it is put away at that clip's
            // end rather than finishing its fade over the rows of the clip below. Unlike
            // a wheel's ramp there is no message that says a channel's volume is back at
            // some neutral value, so putting the ramp away sends nothing of its own: the
            // volume stays at whatever value had been sent, the way a wobble this clip
            // armed is left at the pitch its last point had got to
            for (int m = 0; m < kMidiChannelCount; ++m)
            {
                auto& v = vols[(size_t) m];

                if (v.rampClip == clip.uid)
                    v.endFade();
            }
        }
    }

    // Notes whose owning clip was deleted or is no longer under the playhead,
    // the arpeggiator runs belonging to such a clip, and the wobble a such a clip
    // had armed: nothing below the playhead can play a run whose clip is gone, so
    // its waiting steps end as well, and nothing below it can swing a wobble whose
    // clip is gone either, which is what leaves the wheel that wobble had left off
    // the centre returned rather than bent over the clips that are still under it.
    // A volume ramp is deliberately left alone here: a level belongs to the output
    // channel rather than to a column's rows, and unlike the wheel no controller
    // has a state such a block could put that channel back to, so a fade whose
    // arming clip was taken away keeps counting toward its level rather than leaving
    // the channel stuck at wherever that fade had got to
    for (int ch = 0; ch < kMaxTrackerChannels; ++ch)
    {
        auto& c = channels[(size_t) ch];

        if (c.activeNote < 0 && c.arpId <= 0 && ! c.vibRunning())
            continue;

        const auto* clip = song.findClip (c.activeClip);

        // a column whose rows armed a run or a wobble without striking a note of
        // their own holds no note whose clip says which clip the column sits in, so
        // the clip those rows themselves live in is what their steps and points
        // belong to: their being gone is what ends them, the way the clip a wobble
        // was armed by is what puts its swing away at its end, and a column that
        // names neither clip has nothing of its own for a gone clip to be about
        if (clip == nullptr)
        {
            if (c.arpId > 0)
                clip = song.findClip (c.arpClip);

            if (clip == nullptr && c.vibRunning())
                clip = song.findClip (c.vibClip);
        }

        bool stillUnder = false;

        if (clip != nullptr)
        {
            const auto* pattern = song.findPattern (clip->patternId);

            if (pattern != nullptr)
            {
                const double end = clip->startBeats + (double) pattern->length / rowsPerBeat;

                // A block straddling the clip start still "covers" the clip:
                // a note may legitimately be started by this block's rows.
                stillUnder = ppqStart < end && ppqEnd > clip->startBeats - 1.0e-9;
            }
        }

        if (! stillUnder)
        {
            if (c.activeNote >= 0)
                stopNote (events, 0, ch, c.activeMidiCh, c.activeNote);

            c.endArp();

            // a wobble the same clip had armed belongs to that clip too, so the
            // wheel it had left swinging comes back to the centre here rather than
            // over the clips that are still under the playhead, the way a ramp the
            // gone clip armed comes back; a wheel this clip had nothing to do with
            // is left where its own rows left it, and only the wobble is put away
            if (clip != nullptr && c.vibClip == clip->uid)
            {
                if (c.lastBendPos != kBendCentrePos)
                {
                    const int bendMidiCh = juce::jlimit (0, kMidiChannelCount - 1, c.lastInst) + 1;
                    events.push_back ({ 0, 0, juce::MidiMessage::pitchWheel (bendMidiCh, kBendCentrePos) });
                }

                c.endBend();
            }
            else
                c.endVib();
        }
    }

    std::stable_sort (events.begin(), events.end(), [] (const PendingEvent& a, const PendingEvent& b)
    {
        if (a.offset != b.offset)
            return a.offset < b.offset;

        return a.order < b.order;
    });

    for (auto& e : events)
    {
        const int offset = juce::jlimit (0, juce::jmax (0, numSamples - 1), e.offset);
        midi.addEvent (e.message, offset);
    }

    wasPlaying = true;
    previousPpq = ppqStart + (double) numSamples * ppqPerSample;
}

} // namespace rtr
