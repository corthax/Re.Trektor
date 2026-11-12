#include "PlaybackEngine.h"
#include <algorithm>
#include <set>

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
        c.lastVol = 100;
        uiRow[(size_t) i].store (-1);
        uiMidiChannel[(size_t) i].store (c.lastInst);
    }

    for (int i = 0; i < kMidiChannelCount; ++i)
        everPlayed[(size_t) i].reset();

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

        if (c.lastBendPos != kBendCentrePos)
        {
            events.push_back ({ offset, 0, juce::MidiMessage::pitchWheel (c.activeMidiCh + 1, kBendCentrePos) });
            c.endBend();
        }

        c.activeNote = -1;
        c.lastRow = -1;
        c.endArp();
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
    // stop-time note-offs may never be delivered). Guarantee re-articulation.
    if (everPlayed[(size_t) midiCh].test (note) && c.activeNote != note)
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

    if (c.lastBendPos != kBendCentrePos)
    {
        events.push_back ({ offset, 0, juce::MidiMessage::pitchWheel (midiCh + 1, kBendCentrePos) });
        c.endBend();
    }

    if (c.activeNote == note)
        c.activeNote = -1;
}

//==============================================================================
/** The close of an arpeggiator run, which releases the note its last step
    entered. Only a note the run itself entered may be released here: a note the
    column struck is the run's base rather than one of its steps, and it belongs
    to whatever ends it below, which is what a run whose steps all sound nothing
    leaves the note above it alone. */
void PlaybackEngine::closeArp (std::vector<PendingEvent>& events, int offset, int ch)
{
    auto& c = channels[(size_t) ch];

    if (c.activeNote >= 0 && c.arpNote >= 0 && c.activeNote == c.arpNote)
        stopNote (events, offset, ch, c.activeMidiCh, c.activeNote);
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
    const int vel = juce::jlimit (1, 127, c.lastVol > 0 ? c.lastVol : 100);

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
                // the note-off step: it opens whatever the channel was holding at
                // its subtick rather than stepping anywhere, and only a note the
                // run itself entered is its own to open: a note the column struck
                // is the run's base rather than one of its steps, and it belongs
                // to whatever ends it below, which is what a run whose head step
                // is a note-off leaves that note alone
                if (c.arpNote >= 0 && c.activeNote == c.arpNote)
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
                int note = base + (int) step;

                while (note < 0)
                    note += 12;

                while (note > 127)
                    note -= 12;

                startNote (events, offset, ch, midiCh, note, vel);
                c.activeClip = c.arpClip;
                c.arpNote = note;
            }

            // an empty step sounds nothing, which sustains the step before it,
            // and the next step releases what the step before it left holding, so
            // a looped run's cycle keeps the note its last step entered sounding
            // until the next cycle's first step reaches its own subtick; a note-off
            // step is the one that lets go of that note instead of holding it
        }

        ++c.arpCount;
    }
}

void PlaybackEngine::arpCommand (std::vector<PendingEvent>& events, int ch, int id,
                                 int offset, double rowPpq, const juce::Uuid& clipUid,
                                 bool noteOnRow, const Song& song)
{
    auto& c = channels[(size_t) ch];

    id = juce::jlimit (0, kMaxArpMacros, id);

    if (id == 0)
    {
        // the release call: a row that named a note had already released what it
        // was holding before striking its own, and a row that names a note-off
        // released it as the note-off itself, so only a bare row has to
        if (! noteOnRow && c.activeNote >= 0)
            stopNote (events, offset, ch, c.activeMidiCh, c.activeNote);

        c.endArp();

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
    // note that row released rather than sounding nothing at all
    c.arpCallNote = c.columnNote;
    c.arpOrigin = rowPpq;

    if (noteOnRow)
        c.arpCount = 1; // the row's own note was the first step, so the run picks up at the next one
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

        if (cell.cmd != kEmpty)
        {
            state.lastCmd = cell.cmd;
            state.lastCmdVal = cell.cmdVal;
        }

        const int midiCh = juce::jlimit (0, kMidiChannelCount - 1, state.lastInst);

        // An explicit volume or pan cell always drives its configured controller
        // (CC7 and CC10 by default, named in the Settings tab). This happens
        // before the note handling, so a note-off row still applies them.
        if (cell.vol != kEmpty)
            events.push_back ({ offset, 2,
                                juce::MidiMessage::controllerEvent (midiCh + 1, volumeCc,
                                                                    juce::jlimit (0, 127, cell.vol)) });

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
        }
        else if (noteHere)
        {
            // this is the note the column last named: a call at this row steps
            // from it, a call lower down steps from it too while nothing else
            // entered the column, and a run that is still going measures from it
            // from here on
            state.columnNote = cell.note;

            const std::pair<int, int> key { midiCh, cell.note };

            if (notesThisRow.insert (key).second)
            {
                const int velocity = cell.vol != kEmpty ? cell.vol : juce::jlimit (1, 127, state.lastVol > 0 ? state.lastVol : 100);
                startNote (events, offset, ch, midiCh, cell.note, velocity);
                state.activeClip = clip.uid;
            }
        }

        if (cell.cmd != kEmpty)
        {
            // a row naming a note-off gets its command answered too: the release
            // that row asked for went out above as the note-off itself, so the
            // call written beside it is answered against the note the column had
            // named rather than being skipped along with the note
            if (cell.cmd == kArpCmd)
                arpCommand (events, ch, cell.cmdVal, offset, rowPpq, clip.uid,
                            noteHere, song);
            else if (cell.cmd == kBendCmd)
                bendTo (events, ch, midiCh, cell.cmdVal, offset, clip.uid);
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
                bendTo (events, ch, midiCh, cell.cmdVal, offset, clip.uid);
            }
            else
            {
                // the argument of an arpeggiator call is its macro id, so a row
                // under a carried call names a new one: id 0 releases what the
                // run was holding and ends it, another starts that macro
                arpCommand (events, ch, cell.cmdVal, offset, rowPpq, clip.uid,
                            noteHere, song);
            }
        }

        // the note-off takes the note the column named away for the rows below it,
        // which happens after the row's own command had the note it was releasing
        // to answer against
        if (noteOff)
            state.columnNote = -1;

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
    // supersedes lower down from replaying steps that belong below that row.
    for (int ch = 0; ch < kMaxTrackerChannels; ++ch)
    {
        auto& c = channels[(size_t) ch];

        if (c.arpId > 0)
            flushArp (events, ch, ppqStart, juce::jmin (ppqEnd, c.arpRowEnd), ppqPerSample, numSamples, song);
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

                // the steps of a run that reached this row sound up to its end;
                // the ones past it wait for the next rows or the next block,
                // which is what keeps a macro longer than one row from being
                // squeezed onto this row's samples
                flushArp (events, ch, ppqStart, juce::jmin (ppqEnd, rowEnd), ppqPerSample, numSamples, song);
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

                        // the run this row starts counts from the row itself, so
                        // its first step is played at this block's first sample
                        // rather than being dropped for sitting behind the start
                        flushArp (events, ch, rowTime, juce::jmin (ppqEnd, rowEnd), ppqPerSample, numSamples, song);
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
                // message goes to the channel the bend row's own instrument named
                if (c.bendClip == clip.uid && c.lastBendPos != kBendCentrePos)
                {
                    const int bendMidiCh = juce::jlimit (0, kMidiChannelCount - 1, c.lastInst) + 1;
                    events.push_back ({ offset, 0, juce::MidiMessage::pitchWheel (bendMidiCh, kBendCentrePos) });
                    c.endBend();
                }
            }
        }
    }

    // Notes whose owning clip was deleted or is no longer under the playhead,
    // and the arpeggiator runs belonging to such a clip: nothing below the playhead
    // can play a run whose clip is gone, so its waiting steps end as well.
    for (int ch = 0; ch < kMaxTrackerChannels; ++ch)
    {
        auto& c = channels[(size_t) ch];

        if (c.activeNote < 0 && c.arpId <= 0)
            continue;

        const auto* clip = song.findClip (c.activeClip);
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
