#pragma once

#include "Model.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <bitset>

namespace rtr
{

/** The pitch wheel's own positions. A row naming the bend letter holds an
    argument 0..127 that names a point on the wheel in steps of kBendStep, which
    is what puts the argument's centre kBendCentreVal on the wheel's exact
    centre kBendCentrePos rather than a hair off it. A row naming a slide letter
    instead heads the wheel for one of the two extremes below it, so both ends of
    the wheel are named here rather than written as bare numbers. */
constexpr int kBendStep = 128;       // one argument step, in wheel positions
constexpr int kBendCentrePos = 8192; // the wheel's centre
constexpr int kBendBottomPos = 0;    // the wheel's lowest position, where a D slide ends
constexpr int kBendTopPos = 0x3fff;  // the wheel's highest position, where a U slide ends

static_assert (kBendCentreVal * kBendStep == kBendCentrePos,
               "the argument's centre names the wheel's centre exactly");

/** A vibrato's cycle is drawn with at least this many points of its own, which is
    what lets a cycle the macro asked for shorter than one point per sub-row step
    still show its curve rather than landing on the same point of it every sub-row,
    and what leaves a cycle of eight subticks or wider having one point per sub-row
    step, the grid the arpeggiator's steps and a slide's ramp already use. */
constexpr int kVibCyclePoints = 8;

/** Converts the song timeline into MIDI events, following the host transport. */
class PlaybackEngine
{
public:
    struct Transport
    {
        bool playing = false;
        double ppq = 0.0;
        double bpm = 120.0;
        double ppqPerQuarter = 1.0;
    };

    PlaybackEngine() { reset(); }

    void reset();

    /** Drops transport tracking but keeps note state: the synth may still be
        holding notes when the host restarts the transport (Reaper does not
        call a MIDI effect while stopped, so stop-time note-offs may never
        reach the synth). */
    void forgetTransport ();

    /** Fills `midi` with generated events. `transport.ppq` is the position at the
        start of the block. */
    void process (const Song& song, juce::MidiBuffer& midi, const Transport& transport,
                  int numSamples, double sampleRate, bool passThroughInput = false);

    /** Called when the editor wants an "all notes off". */
    void panic (juce::MidiBuffer& midi);

    bool isPlaying() const noexcept { return wasPlaying; }
    double lastPpq() const noexcept { return previousPpq; }

    /** The row currently sounding in each tracker channel (for the UI). */
    std::array<std::atomic<int>, kMaxTrackerChannels> uiRow {};
    std::array<std::atomic<int>, kMaxTrackerChannels> uiMidiChannel {};

private:
    struct ChannelState
    {
        int lastInst = -1;
        int lastVol = -1;
        int lastPan = -1;
        int lastCmd = -1;
        int lastCmdVal = -1;
        int columnNote = -1; // the note this channel's column last named, empty until one does
        int activeNote = -1;
        int activeMidiCh = 0;
        juce::Uuid activeClip;

        /** What the last note-off this channel queued released, and at which
            sample of the block it was queued in: a note entered at the row a
            clip ends on, which is the same sample, finds its own pitch already
            opened there, so the stuck-key guard below does not add a second
            note-off of it beside that one. Two releases of one pitch around one
            note-on at one sample is what hosts like Reaper read as the new note
            being opened at once rather than re-articulated, while the one
            release a row's own note already puts ahead of it works everywhere. */
        int relNote = -1;
        int relMidiCh = -1;
        int relOffset = -1;
        int lastRow = -1;
        int lastBendPos = kBendCentrePos; // the wheel position this channel last left in force

        /** An arpeggiator run that has steps still to sound: how many steps of
            it have already played and where the run started, so the next step's
            position is the start plus that many step widths - counted, not
            summed, which is what keeps a looped run's cycle boundaries exact.
            The steps measure from columnNote, the note this channel's column
            last named, which is the call row's own note where it had one: a
            further call in a column steps from the note its rows named rather
            than from whatever note the run above happened to be sounding when
            the switch came. A call at a row whose note column holds a note-off
            is answered too, against the note that row is releasing, and keeps it
            in arpCallNote for as long as the column has none of its own - a note
            entering further down the column re-bases the run by being that note,
            and a further note-off below takes the answer away again. The first
            step of a run whose call row named a note runs at that row rather than
            one subtick below it, that note being sounded at the pitch the first
            step moved it to rather than as it was typed and tracked in arpNote as
            the run's own the way any other step's is, which is what closing a run
            releases the steps it sounded including that first one while leaving a
            note it only stepped away from to whatever ends it below; a note-off
            step lets go of whatever the channel holds at its own subtick, the run's
            last note when it has sounded one and the column's note when it has not,
            and leaves the note it rests over named for the steps below it. */
        int arpId = 0;             // macro id in force (0 = no run)
        int arpCount = 0;          // steps of the run already played
        double arpOrigin = -1.0;   // where the run's first step sits, in transport beats
        int arpNote = -1;          // the note the run last entered, -1 when it entered
                                    // none or its last step opened the note instead
        int arpCallNote = -1;      // the note the run's call answered against, which the
                                    // column's own note takes over as soon as it has one
        juce::Uuid arpClip;        // clip whose rows own the run, so its end closes it
        double arpRowEnd = -1.0;   // end of the row the engine last processed

        /** The clip whose rows last moved this channel's pitch wheel, so that
            clip's end has a wheel to take back even when the row that moved it
            had no note of its own. */
        juce::Uuid bendClip;

        /** A slide this channel's column has in force: the ramp from where the
            wheel already was to the extreme the letter names, run on the same
            sub-row grid an arpeggiator's steps use rather than one step per row,
            so the wheel is interpolated between the rows instead of jumping to a
            new point once on each of them. The row that named the letter is the
            ramp's start, so the wheel sits where it was there and the extreme sits
            slideSpan rows below it; the OFF argument switches the wheel off at
            this row and the STP one holds it at the point it is at, so a span is
            never fewer than one row. The ramp's next step is counted from its
            start rather than read off the row - slideOrigin plus slideCount step
            widths - which is what makes a row a transport jump makes this block
            play again land on the same point of the ramp instead of advancing it
            twice, and what leaves the steps a block could not reach waiting for
            the block that does. slideTotal is the whole ramp, the span over that
            same grid, and slideCount how much of it has already moved the wheel,
            which is what makes a release point able to tell whether the ramp
            still has steps left to run, the wheel having reached the extreme
            being told on its own. slideRowEnd is the end of the row the engine
            last processed, the bound a ramp's waiting steps are flushed up to. */
        bool slideOn = false;                    // a slide is in force in this column
        double slideOrigin = -1.0;               // where the ramp leaves the wheel, in transport beats
        int slideSpan = 1;                       // rows the ramp takes to reach slideTo
        int slideTotal = 0;                      // sub-row steps the whole ramp takes
        int slideCount = 0;                      // steps of the ramp already played
        int slideFrom = kBendCentrePos;          // where the ramp leaves the wheel
        int slideTo = kBendCentrePos;            // the extreme it heads for
        double slideRowEnd = -1.0;               // end of the row the engine last processed

        /** The clip whose rows armed the slide this channel is in, so that clip's
            end has a ramp to stop: a ramp a further clip armed belongs to that
            clip rather than to this one, and its rows are not the ones this clip
            ran out of. */
        juce::Uuid slideClip;

        /** A vibrato this channel's column has in force: a wobble about the point
            the wheel was at where it was armed, run on the same sub-row grid an
            arpeggiator's steps and a slide's ramp use rather than one step per row, so
            the pitch swings over the grid rather than taking a new point once on each
            row. Unlike a ramp it has no end to reach: the cycle repeats for as long as
            the column carries the letter, which is what makes the clip that armed it the
            thing that puts it away. The cycle's own phase is counted from the arming row
            rather than read off the rows - vibOrigin plus vibCount step widths - the way
            a run's steps and a ramp's are, which is what a row a transport jump makes
            this block play again leaves on the same point of the cycle instead of
            advancing it twice, and what keeps a wobble that cycled many times exact. The
            macro's two knobs are re-read at every flush, so a macro whose amplitude or
            cycle was turned while its wobble ran changes the wobble from where it is
            rather than leaving the settings it had at the arming row still in force.
            vibBase is the point the wobble swings about, the wheel's own point where the
            call armed it or the point a bend row below moved it to, and vibHeight is how
            far it swings from there in whole wheel positions, the macro's amplitude taken
            as a share of the wheel's own half-height, which is what makes an amplitude of
            full reach the wheel's own extremes. */
        bool vibOn = false;                    // a vibrato is in force in this column
        int vibId = 0;                         // macro id the wobble runs (0 = none)
        double vibOrigin = -1.0;               // where the wobble's first cycle starts, in transport beats
        int vibCount = 0;                      // points of the wobble already played
        int vibBase = kBendCentrePos;          // the wheel point the wobble swings about
        int vibHeight = 0;                     // how far it swings from that base, in wheel positions
        int vibCycle = 1;                      // sub-row steps one whole cycle of it takes
        int vibPoints = 1;                     // points one cycle of it is drawn with
        double vibRowEnd = -1.0;               // end of the row the engine last processed
        juce::Uuid vibClip;                    // clip whose rows armed the wobble

        /** A note-length gate this channel's column has in force, and the note-off
            a cut of that gate still has waiting: how many subticks the gate holds a
            note for, 0 leaving none, counted on the same sub-row grid an arpeggiator's
            steps, a slide's ramp and a wobble's points use, so a cut of 3 at the
            Settings' 6 to the row holds the note for half a row and one wider than the
            row waits for the block that reaches it. The gate keeps being carried down
            under the letter, which is what lets one row's letter gate the notes of the
            rows under it rather than cutting only the row that named it, and the letter
            also cuts across a row that named no note of its own, arming its release for
            the note this channel has been holding from above rather than for nothing at
            all. cutTime is where the release a cut armed sits in absolute transport
            position rather than read off the rows below, the way a run's steps, a ramp's
            steps and a wobble's points are counted, cutNote and cutMidiCh name the pitch
            that release belongs to and cutRowEnd is the end of the row the engine last
            processed, the bound the flush below walks up to. The waiting release only
            ever closes the pitch the row that armed it left this channel holding and
            this channel is still holding, which is what makes it safe to leave a waiting
            cut in place while the gate above it is retargeted or while the column's own
            note-off, a run's step or a clip's end has already taken that note away,
            there being nothing left for it to close. */
        int cutTicks = 0;                      // subticks a note entering this column is held for (0 = no gate)
        double cutTime = -1.0;                 // where the waiting release sits, in transport beats (-1 = none)
        int cutNote = -1;                      // the pitch that waiting release belongs to
        int cutMidiCh = -1;                    // the MIDI output channel it was entered on
        double cutRowEnd = -1.0;               // end of the row the engine last processed

        /** Leaves no gate in force and no cut waiting: the column stopped having the
            letter, so the length it held has nothing left to gate the notes below it,
            and the release that gate had armed for a note entered above it has no gate
            of its own left to cut it, however far that counter had got - a cut of a long
            length still waiting under a row that names another command is discarded rather
            than left to close whatever note the column happens to be holding there, which
            is how a run's and a ramp's waiting steps go when the column supersedes them
            lower down. The note that row entered is left to whatever ends it below, a
            further note entering the column having already opened it at that row's own
            sample. */
        void endCut() noexcept
        {
            cutTicks = 0;
            cutWaiting ();
        }

        /** Leaves no cut waiting without putting the gate away: the note the release
            belonged to has been taken away by something else, a note-off row, a run's
            own step, a transport re-arm or a clip's end, or the release could no longer
            be placed where it sits, so it has nothing to close and the letter the column
            carries keeps gating the notes under it. */
        void cutWaiting () noexcept
        {
            cutTime = -1.0;
            cutNote = -1;
            cutMidiCh = -1;
        }

        /** Whether the column still has a gate in force, which is what a note entering
            a row that named no command of its own asks before it is cut, and what makes
            the letter's carried spelling and the engine's own idea of the length agree. */
        bool cutGating () const noexcept
        {
            return cutTicks > 0;
        }

        /** Ends a run: an arpeggiator that no longer has steps or no longer has
            a clip to hold it leaves nothing to play. The note the column last
            named is not part of the run, so it stays: a call further down the
            column still steps from the note its rows named. */
        void endArp() noexcept
        {
            arpId = 0;
            arpCount = 0;
            arpOrigin = -1.0;
            arpNote = -1;
            arpCallNote = -1;
            arpClip = juce::Uuid();
        }

        /** Leaves no slide in force: the ramp's rows the column stopped having
            have nothing left to reach the extreme, and a wheel a row left at a
            point of its own is not a ramp any more, which is what naming another
            command kind ends. The wheel itself is left where it is, which is what
            makes a row that bent it stay bent until a release point below. */
        void endSlide() noexcept
        {
            slideOn = false;
            slideOrigin = -1.0;
            slideSpan = 1;
            slideTotal = 0;
            slideCount = 0;
            slideFrom = kBendCentrePos;
            slideTo = kBendCentrePos;
            slideClip = juce::Uuid();
        }

        /** Leaves no wobble in force: the cycles the column stopped having have
            nothing left to swing, and the wheel is left sitting at whatever point of
            its own the wobble's points had got to rather than being made to sit at a
            point here, which is what a row that named another command kind ends and a
            release point below puts right on its own. */
        void endVib() noexcept
        {
            vibOn = false;
            vibId = 0;
            vibOrigin = -1.0;
            vibCount = 0;
            vibBase = kBendCentrePos;
            vibHeight = 0;
            vibCycle = 1;
            vibPoints = 1;
            vibClip = juce::Uuid();
        }

        /** Takes the channel's idea of the wheel back to the centre and leaves
            no slide and no wobble in force, which is what every release point does once
            it has sent the message that says so: a wheel left bent has no clip below it
            to put it right again, a slide whose rows the column stopped
            having has no rows left to reach its extreme, and a wobble whose cycles the
            column stopped having has none left to swing. */
        void endBend() noexcept
        {
            lastBendPos = kBendCentrePos;
            bendClip = juce::Uuid();
            endSlide();
            endVib();
        }

        /** Whether the slide this channel is in still has steps of its ramp left
            to run, which is what a release point asks before it decides that a
            wheel sitting at the centre has nothing to take back. A ramp that
            reached its extreme has its wheel sitting at a point of the column's
            own rather than still climbing, which is told here too, and so has a
            ramp the OFF argument put away and an STP argument left holding the
            wheel's own point - neither of which is still running. */
        bool slideRunning() const noexcept
        {
            return slideOn && slideCount <= slideTotal;
        }

        /** Whether a wobble is in force in this channel, which is what a release
            point asks alongside a ramp: a wobble swings for as long as the column has
            it, so unlike a ramp it has no extreme to have reached and nothing that
            says it stopped by itself, and its wheel may sit at its base and still
            have cycles left to swing. */
        bool vibRunning() const noexcept
        {
            return vibOn;
        }
    };

    struct PendingEvent
    {
        int offset = 0;
        int order = 0;
        juce::MidiMessage message;
    };

    void allNotesOff (std::vector<PendingEvent>& events, int offset, bool soundOff = false);

    /** Sounds the arpeggiator steps of one channel that belong to the position
        range [ppqStart, ppqLimit), which is how a row whose steps spill past its
        own span gets them played: they wait in the channel state and the block
        that reaches them emits them at their real sample. `ppqLimit` never passes
        the end of the row the engine last owned, so a run the column supersedes
        lower down loses the steps that were still below it. */
    void flushArp (std::vector<PendingEvent>& events, int ch, double ppqStart,
                   double ppqLimit, double ppqPerSample, int numSamples,
                   const Song& song);

    /** Moves the pitch wheel for the steps of the slide this channel has in force
        that belong to the position range [ppqStart, ppqLimit). A slide's steps are
        one sub-row wide, the sub-row being a row over the Settings row "Subticks
        per row" like an arpeggiator's, which is what interpolates the wheel between
        the rows the slide's argument names rather than leaving it at one new point
        for a whole row: the step's position is the ramp's start plus its count
        rather than a sum of step widths, so the step sitting span rows below the
        one that named the letter lands on the extreme exactly, and the ones before
        it hold the whole wheel position that far along the straight line between
        where the ramp left the wheel and that extreme. Only a step that changes
        the position sends a message, which is what the row that named the letter
        sends nothing of its own: its step is where the wheel already is. `ppqLimit`
        never passes the end of the row the engine last owned, which is what leaves
        the steps of a row that spills past this block waiting for the block that
        reaches them, the way an arpeggiator run's do. */
    void flushSlide (std::vector<PendingEvent>& events, int ch, double ppqStart,
                     double ppqLimit, double ppqPerSample, int numSamples,
                     const Song& song);

    /** Moves the pitch wheel for the points of the vibrato this channel has in force
        that belong to the position range [ppqStart, ppqLimit). A vibrato's points are
        a fraction of a sub-row apart rather than one sub-row wide, the fraction being
        the macro's own cycle length over the grid the Settings row "Subticks per row"
        names divided by the points that cycle is drawn with, which is what lets a
        cycle shorter than one point per sub-row step still show its curve: the point's
        position is the arming row plus its count times that fraction, never a sum of
        the fractions themselves, so a wobble that cycled many times stays exact and a
        row a transport jump makes this block play again lands on the same point of its
        cycle rather than advancing it twice. The point's wheel position is the base the
        wobble swings about plus the macro's shape at that point of its cycle times how
        far the macro's amplitude reaches, counted in whole wheel positions and kept
        inside the wheel's own extremes, so full amplitude reaches them and an amplitude
        of off reaches nothing. Only a point that changes the position sends a message,
        which is what the arming row's own point sends nothing of its own: it sits on the
        base the wheel is already at. `ppqLimit` never passes the end of the row the
        engine last owned, which is what leaves the points of a row that spills past
        this block waiting for the block that reaches them, the way a run's steps and a
        ramp's do. */
    void flushVib (std::vector<PendingEvent>& events, int ch, double ppqStart,
                   double ppqLimit, double ppqPerSample, int numSamples,
                   const Song& song);

    /** Closes the note a cut this channel had armed for, at the subtick that cut
        named: the release sits at an absolute transport position the row that entered
        the note counted out rather than being read off the rows below it, the way a
        run's steps, a ramp's steps and a wobble's points wait, so a cut wider than one
        row's own subticks waits for the block that reaches it and is emitted at its own
        sample rather than bunched onto the row that named it. `ppqLimit` never passes
        the end of the row the engine last owned, which is what leaves a waiting release
        the column superseded below that row not being placed at a sample of its own
        beyond it. The release is only ever sent for the pitch the row that armed it
        entered and this channel is still holding, which is what makes it safe for
        something else to have taken that note away already - the release is dropped
        rather than closing whatever note the column happens to be holding there - and
        a release that can no longer be placed where it sits, which is what a scrub or a
        jump leaves behind, is dropped the way a run's, a ramp's and a wobble's waiting
        steps are, the note being left to whatever ends it below. */
    void flushCut (std::vector<PendingEvent>& events, int ch, double ppqStart,
                   double ppqLimit, double ppqPerSample, int numSamples,
                   const Song& song);

    /** A row naming the arpeggiator command: the release call, id 0 or the OFF
        argument the entry also writes for an o, releases what the run is holding,
        ends it and strikes the note the run measured from again at this row, so
        the column goes back to sounding its own base note rather than only losing
        the run's note; the row that names a note-off of its own is what the note
        column already released, so its call answers the run without re-striking
        anything, and noteOffRow says which of the two a row is. The release also
        leaves the column not calling a macro below it, which is what stops a
        further row naming only an argument being answered there and stops the
        tracker drawing a carried call over those rows. Another id stops the run
        above it and starts or restarts that macro's run, its steps measuring from
        the note the column last named, which is the call row's own note where it
        had one, and the note a note-off row is releasing where its cell holds a
        note-off instead - such a row's call is answered, not ignored, and the
        note-off takes the note away for the rows below it. A row that also names
        a note has already played its first step, so its run starts at the next
        one. The STP argument means nothing beside an arpeggiator call, so such a
        row sounds nothing and leaves the run above it alone. */
    void arpCommand (std::vector<PendingEvent>& events, int ch, int id,
                     int offset, double rowPpq, const juce::Uuid& clipUid,
                     bool noteOnRow, bool noteOffRow, const Song& song);

    /** Closes the arpeggiator run this channel is in: the note its last step
        entered is released, which is what a switch to another command and a
        looped run's cycle end leave no note sounding from the step before. A
        run that entered nothing has nothing to release, which is what a note
        the column itself struck is left alone, and a finished run's last step
        is not closed where the run simply ran out of steps. */
    void closeArp (std::vector<PendingEvent>& events, int offset, int ch);

    /** Ends the pitch wheel this channel has in force: the message that puts the
        wheel back at its centre when the channel had left it bent, and no wheel
        and no ramp in force below that, which is what an arpeggiator call beside
        a wheel that was bent does - the run's own note-offs belong to the column,
        but the wheel is a state the whole channel carries, so the row that calls
        the arpeggiator is what has to put it right. A ramp that was still climbing
        with its wheel never having left the centre has only itself to put away,
        which is why no message is what ends it. */
    void endWheel (std::vector<PendingEvent>& events, int ch, int midiCh, int offset);

    /** Moves the pitch wheel for one row that named the bend letter: the row's
        argument names a point on it, 0 at the bottom and 127 near the top, so an
        argument the row left empty is taken as the wheel's centre, which bends
        nowhere. The message is the standard pair, and it goes out ahead of the
        note its own row strikes, so the note that row hits is already bent. What
        the wheel is left at is remembered, together with the clip whose rows
        asked for it, so a release point below can take it back. */
    void bendTo (std::vector<PendingEvent>& events, int ch, int midiCh,
                 int argument, int offset, const juce::Uuid& clipUid);

    /** Answers one row that named a slide letter, or one that named only an
        argument under a letter carried above it: the argument says how many
        rows the slide takes rather than where on the wheel it goes, the empty
        one taking the default span and one below 0 being clamped, and the ramp
        runs from where the wheel already is to the extreme the letter names.
        The row that named the letter is the ramp's own start, so arming sends
        nothing and leaves the wheel where it was, which is what keeps that
        row's own note straight and puts the extreme span rows below; the wheel
        is moved by flushSlide over the sub-row grid below this. A row beside an
        OFF argument puts that wheel away instead, sending the message that leaves
        it at its centre and leaving no wheel and no ramp bent below it, and a row
        beside an STP argument ends the ramp while leaving the wheel sitting at the
        point it had got to, which is what holds that pitch over the rows below.
        Either row closes an arpeggiator run this column was in, since a slide and
        a run cannot both be in force in one column: the note the run's last step
        entered is released at this row and the run ends, while a run another
        column of the same MIDI channel was in stays where it is. A further row
        under a carried letter re-arms the ramp with its own span from where the
        wheel had got to, so a slide can be retargeted as it runs, and the ramp
        belongs to this row's position rather than to the row, which is what an
        argument-only row reaching its extreme the number of rows below it it asked
        for means. */
    void slideCommand (std::vector<PendingEvent>& events, int ch, int offset,
                       double rowPpq, int argument, bool up, bool noteOnRow,
                       const juce::Uuid& clipUid, const Song& song);

    /** Answers one row that named the vibrato letter, or one that named only an
        argument under a letter carried above it: the argument names which vibrato
        macro the column runs rather than saying anything about the wheel itself, so
        the row that armed the wobble sends nothing and leaves the wheel sitting where
        it was, with the cycle's own swing reaching the sub-row grid at the point the
        macro's shape says for the cycle's start, the arming row's own point being on
        the base. The base is the point the wheel was at where the call armed it, which
        is what lets a bend row above the call or a slide's extreme be the pitch a
        wobble swings about, and a further row that bends the wheel while the wobble
        runs moves that base rather than ending the wobble. A row beside the release
        answers twice: the id 0 beside the letter and the OFF the entry writes for an
        o both put the pitchbending away, ending the wobble and taking the wheel back
        to its centre, which is what the rows below having no letter and no wobble to
        carry means; the STP spelling asks for nothing beside a vibrato, since a wobble
        has no move of the wheel to stop, and the wobble above keeps swinging. Like a
        slide and a call, this row closes an arpeggiator run this column was in, and an
        id naming a macro that has none of its own still arms the wobble, whose flush
        then puts it away at the first point of it, the way a run whose macro was taken
        away stops at its next step. A row that also struck a note of its own had
        already released whatever the run was holding as that note's own arrival, so
        only a bare row has to release it here, and the wobble's clip is this row's, so
        the clip that armed it is what puts it away. */
    void vibCommand (std::vector<PendingEvent>& events, int ch, int id, int offset,
                     double rowPpq, const juce::Uuid& clipUid, bool noteOnRow,
                     const Song& song);

    /** Answers one row that named the cut letter, one that named only an argument
        under a letter carried above it or a row that named neither and entered a note
        under a letter carried above it: the argument says how many subticks the note
        that row entered is held for rather than what any other row is asked to do, so
        the row itself sends nothing and leaves the pitch sounding, the note-off it asks
        for waiting for the block that reaches the subtick kCutDefaultTicks or the named
        number of them past the row - a wider cut reaching past the row that named it and
        past the clip that has it the way a run's, a ramp's and a wobble's steps do, the
        clip's own end being what closes the note there. An empty argument under a letter
        the row itself named takes the default length rather than no length at all, an
        empty one under a carried letter keeps the length that letter is carrying, an OFF
        or a 0 argument is the release rather than a cut of no subticks, so the note that
        row entered is left sustained and the column carries no letter below it, and an
        STP one asks for nothing, a cut having no move of the wheel to stop and leaving
        the length above as it was; either way the note that row entered is held for
        whatever length is in force there, which is what makes the letter gate the notes
        of the rows under it rather than only the row that named it. Like a slide's and a
        vibrato's rows, this row closes an arpeggiator run this column was in, since a run
        and a gate cannot both be in force in one column. */
    void cutCommand (std::vector<PendingEvent>& events, int ch, int offset,
                     double rowPpq, int argument, bool rowNamedLetter, int enteredNote,
                     bool noteOnRow, int midiCh, const Song& song);

    /** Leaves the note this channel entered gated for as long as the gate in force
        says: the waiting release is counted from the row that entered the note rather
        than from the rows below it, which is what makes a row a transport jump makes
        this block play again arm its own cut at that row rather than advancing one twice,
        and what retargeting a length under a carried letter leaves a note that row entered
        above it armed for at its own row's length instead of re-timing it there. */
    void armCut (int ch, int heldNote, int midiCh, double rowPpq, const Song& song);

    void processRow (const Pattern& pattern, const Clip& clip, int row,
                     std::vector<PendingEvent>& events, int offset,
                     const Song& song, double rowPpq);

    void startNote (std::vector<PendingEvent>& events, int offset, int ch, int midiCh, int note, int velocity);
    void stopNote (std::vector<PendingEvent>& events, int offset, int ch, int midiCh, int note);

    std::array<ChannelState, kMaxTrackerChannels> channels;

    /** Controller numbers the volume and pan columns drive. They are refreshed
        from the song settings at the start of every block, so the Settings
        tab can point the columns at any CC a synth listens to. */
    int volumeCc = 7;
    int panCc = 10;

    bool wasPlaying = false;
    double previousPpq = -1.0;
    bool justStarted = false;

    /** Notes this engine has ever sent a note-on for, per MIDI channel.
        Survives transport resets: hosts like Reaper never call a MIDI effect
        while stopped, so the synth may still be holding any of these notes
        when playback restarts. A new note-on of such a pitch is always
        preceded by a matching note-off so a stuck key can never stack. */
    std::array<std::bitset<128>, kMidiChannelCount> everPlayed {};
};

} // namespace rtr
