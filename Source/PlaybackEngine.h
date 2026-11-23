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

/** A tremolo's cycle is drawn with at least this many points of its own, the same
    rule the vibrato's cycle is drawn by: a cycle the macro asked for shorter than
    one point per sub-row step still shows its curve rather than landing on the same
    point of it every sub-row, and a cycle eight sub-ticks or wider has one point per
    sub-row step, the grid the arpeggiator's steps, a slide's ramp and a wobble's
    points already use, only read in volume points of the controller rather than in
    wheel positions. */
constexpr int kTremCyclePoints = 8;

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
        int lastVol = kVolDefaultVel; // the velocity a note this column enters is struck at, none yet being the default
        int lastPan = -1;
        int lastCmd = -1;
        int lastCmdVal = -1;

        /** What percent of a row this channel's column last left delaying its rows,
            the delay column's own value carried down under the row that named it the
            way the instrument, volume and pan columns carry theirs. A 0 is what stops
            the delaying rather than a delay of no part of a row, and nothing named yet
            is -1, which delays nothing; only that row's own messages move, an arpeggiator's
            steps, a wheel's ramp, a wobble's points, a cut's sub-ticks and a retrigger's
            periods still counting from the row itself. */
        int lastDelay = -1;
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
                                    // column's own note takes over as soon as it has one,
                                    // and which together with that note is what a retrigger
                                    // letter spelled across a bare row goes back to as the
                                    // run's base, read as it was typed rather than as a
                                    // step moved it
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

        /** A retrigger gate this channel's column has in force, and the next
            re-articulation that gate still has waiting: how many sub-ticks apart
            the strikes are, counted on the same sub-row grid an arpeggiator's
            steps, a slide's ramp, a wobble's points, a cut's length and this
            retrigger's own period use, so a period of 3 at the Settings' 6 to
            the row strikes the note a third of a row after it was armed and
            again a third of a row after that, and one narrower than a block's
            own samples striking more than once inside a block rather than once
            to it. The gate keeps being carried down under the letter, which is
            what lets one row's letter gate the notes of the rows under it rather
            than striking only the row that named it, and the letter also strikes
            across a row that named no note of its own, arming its strike for the
            note this channel has still been holding from above rather than for
            nothing at all - a row with no note to strike again leaving the letter
            ignored. retrigTime is where the next waiting strike sits in absolute
            transport position rather than read off the rows below, the way a run's
            steps, a ramp's steps, a wobble's points and a cut's release are
            counted, and each strike after the first is counted from the one the
            engine last played at whatever period is in force there, so a period
            retargeted between the strikes moves the ones below it rather than
            leaving them to the arming row's own. retrigNote and retrigMidiCh name
            the pitch that strike belongs to, and the bound the flush below walks
            up to is the end of the row the engine last processed, the same one the
            cut's own waits on since every row and channel sets it whatever the
            column carries. A waiting strike only ever closes and re-opens the
            pitch the row that armed it left this channel holding and this channel
            is still holding, its note-off and note-on being sorted off-before-on
            at one sample rather than a hair apart, which is what makes it safe to
            leave a strike in place while the period above it is retargeted or while
            the column's own note-off, a run's step or a clip's end has already
            taken that note away, there being nothing left for it to strike again. */
        int retrigTicks = 0;                   // sub-ticks apart a note sounding in this column is struck again at (0 = no gate)
        double retrigTime = -1.0;              // where the next waiting strike sits, in transport beats (-1 = none)
        int retrigNote = -1;                   // the pitch that waiting strike belongs to
        int retrigMidiCh = -1;                 // the MIDI output channel it was entered on

        /** Leaves no gate in force and no strike waiting: the column stopped having
            the letter, so the period it held has nothing left to gate the notes
            below it, and the strike that gate had armed for a note entered above
            it has no gate of its own left to strike again, however far its counter
            had got - a strike still waiting under a row that names another command
            is discarded rather than left to close and re-open whatever note the
            column happens to be holding at the sub-tick it was waiting for, which
            is how a run's, a ramp's, a wobble's and a cut's waiting points go when
            the column supersedes them lower down. A strike the gate had already
            made stays sounding, the note entered above being left to whatever ends
            it below, a further note entering this column having already opened it
            at this row. */
        void endRetrig() noexcept
        {
            retrigTicks = 0;
            retrigWaiting();
        }

        /** Leaves no strike waiting without putting the gate away: the note the
            strike belonged to has been taken away by something else, a note-off
            row, a run's own step, a transport re-arm or a clip's end, or the
            strike could no longer be placed where it sits, so there is nothing
            left to strike again and the strikes counted on from it go with it;
            the letter the column carries keeps holding the period, which is what
            lets the notes entering under it re-arm their own. */
        void retrigWaiting() noexcept
        {
            retrigTime = -1.0;
            retrigNote = -1;
            retrigMidiCh = -1;
        }

        /** Whether the column still has a gate in force, which is what a note
            entering a row that named no command of its own asks before it is
            re-struck, and what makes the letter's carried spelling and the
            engine's own idea of the period agree. */
        bool retrigGating() const noexcept
        {
            return retrigTicks > 0;
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

    /** What one MIDI output channel sounds its notes at: the volume level its
        L, I and O rows named and the ramp one of the two last letters left in
        force. This belongs to the channel rather than to a tracker column, which
        is what lets a letter spelled on any column naming that instrument reach
        the same level and lets a fade armed from one column and a letter
        answered from another count the same values. */
    struct VolState
    {
        int level = kVolDefaultLevel; // the volume an L last set (the default until one does)
        bool fadingIn = false;        // which way the running ramp moves: up toward level or down toward nothing
        int step = 0;                 // volume a ramp moves every sub-tick, 0 being no ramp in force
        int steps = 0;                // values that ramp has to send, its first at the row that armed it
        int sent = 0;                 // values of it already sent, so the next one's position is counted not summed
        double origin = -1.0;         // where the ramp's first value sits, in transport beats
        double rowEnd = -1.0;         // end of the last row the engine owned that ramp over
        juce::Uuid rampClip;          // the clip whose row armed that ramp

        /** Leaves no ramp in force and leaves the channel's volume at whatever
            value its last sub-tick had sent: unlike the wheel, a controller has
            no neutral state to put it back to, so putting a fade away sends
            nothing of its own. The level an L set is kept, which is what lets
            a further I or O row fade to or from it again. */
        void endFade() noexcept
        {
            step = 0;
            steps = 0;
            sent = 0;
            fadingIn = false;
            origin = -1.0;
            rampClip = juce::Uuid();
        }

        /** Whether the ramp this channel has in force still has values of its own
            left to send: a fade that reached its target sent it once and has
            nothing more to say, which is what makes a stopped ramp one the
            flush below still has to reach. */
        bool fadeRunning() const noexcept
        {
            return step > 0 && sent < steps;
        }
    };

    /** What one MIDI output channel's volume controller is swung about its level by
        a tremolo: the macro its T rows name and where that cycle stands. This belongs
        to the channel rather than to a tracker column exactly as the level an L named
        does, which is what lets a T spelled on any column naming that instrument reach
        one swing, retargeted as it goes the way a wobble is, and what keeps the swing
        and the fade of the three volume letters as parallel states of that one controller
        as the level, the ramp and the wheel are of a column. The level it swings about is
        read at every point rather than kept here, which is what lets a further L row move
        the level a running swing is swung about, the way a bend row moves the base a wobble
        swings about. */
    struct TremState
    {
        bool on = false;                  // a tremolo swing is in force on this channel
        int id = 0;                       // macro id the swing runs (0 = none)
        double origin = -1.0;             // where the swing's first cycle starts, in transport beats
        int count = 0;                    // points of the swing already played
        int height = 0;                   // how far it swings from its base, in volume points
        int cycle = 1;                    // sub-ticks one whole cycle of it takes
        int points = 1;                   // points one cycle of it is drawn with
        double rowEnd = -1.0;             // end of the last row the engine owned that swing over
        juce::Uuid swingClip;             // the clip whose rows armed the swing

        /** Leaves no swing in force and leaves the channel's volume at whatever value
            its last point had sent: like the fade above, a controller has no neutral
            state a row can put it back to, so putting a swing away sends nothing of
            its own, and the level an L set is kept rather than taken back. */
        void endSwing() noexcept
        {
            on = false;
            id = 0;
            origin = -1.0;
            count = 0;
            height = 0;
            cycle = 1;
            points = 1;
            rowEnd = -1.0;
            swingClip = juce::Uuid();
        }

        /** Whether a swing is in force on this channel, which is what the flush below
            reaches and what a row that took the letter away answers the way a wobble
            does: a swing has no end of its own to have reached, so the release rows and
            the clip that armed it are what put it away. */
        bool swingRunning() const noexcept
        {
            return on;
        }
    };

    struct PendingEvent
    {
        int offset = 0;
        int order = 0;
        juce::MidiMessage message;
    };

    /** One row's own messages that its column left delaying past the block that
        held that row: the whole pair is kept and placed at one later sample, counted
        from the row as a percent of that row rather than of the Settings' sub-row grid,
        so a delayed row's note-on, note-off, pan, controller and command answer wait
        for the block that reaches them the way a run's steps, a ramp's, a wobble's
        points, a cut's release and a retrigger's strikes do. */
    struct DelayedRow
    {
        double time = -1.0;                  // where the row's own sample moved to, in transport position
        std::vector<PendingEvent> messages;  // what the row sent of its own, in the order it sent it
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

    /** Plays the strikes a retrigger gate has waiting up to the end of the row the
        engine last owned, each one being the note column's own note-off and note-on
        at the one sample its period names: they are sorted off-before-on there, which
        is what re-articulates the note that is still being held rather than restarting
        it or spelling a note of no length at all. The strikes are placed at absolute
        transport positions counted from the row that armed the gate - that row's own
        sample where the letter was spelled across a row whose note column named nothing
        of its own, the first sub-tick the period names past it where that row entered a
        note of its own - and then from one
        strike to the next at whatever period is in force where the engine played it,
        the way a run's steps, a ramp's steps, a wobble's points and a cut's release
        wait rather than being read off the rows below, so a period narrower than a
        block's own samples strikes more than once inside that block rather than once
        to it and a period wider than a row waits for the block that gets there. `ppqLimit`
        never passes the end of the row the engine last owned, the same bound the cut's
        own waits on, which is what leaves a strike the column superseded below that row
        not being placed at a sample of its own beyond it. A strike is only ever sent for
        the pitch the row that armed it left this channel holding and this channel is
        still holding, which is what makes it safe for something else to have taken that
        note away already - the strike is dropped, and the ones counted on from it with
        it, rather than closing and re-opening whatever note the column happens to be
        holding there - and a strike that can no longer be placed where it sits, which is
        what a scrub or a jump leaves behind, is dropped the way a run's, a ramp's, a
        wobble's and a cut's waiting points are, the letter keeping the period in force
        so a note entering under it below re-arms its own. Below the row that named the
        letter the wheel needs nothing keeping up with the strikes: where that row's column
        had a wobble swinging, processRow had already taken that swing away and sent the
        wheel back to its exact centre at the row's own sample ahead of the strike spelled
        there, so the strikes counted on from it play at the pitch the column named rather
        than at one the cycle had left the wheel part way down its swing. */
    void flushRetrig (std::vector<PendingEvent>& events, int ch, double ppqStart,
                      double ppqLimit, double ppqPerSample, int numSamples,
                      const Song& song);

    /** Sends the volume controller values of the ramp one MIDI output channel has
        in force that belong to the position range [ppqStart, ppqLimit). A volume
        fade's values are one sub-row wide, the sub-row being a row over the Settings
        row "Subticks per row" like an arpeggiator's steps, a slide's ramp, a wobble's
        points and a retrigger's strikes, which is what fades the controller over that
        grid rather than naming a new value once per row: the value's position is the
        arming row plus its count rather than a sum of sub-tick widths, so a fade stays
        exact however many blocks it spread over. Unlike a wheel's ramp, every value is
        spoken even the one the channel's volume is already at, so a host hears the
        controller rather than only its own change of it, and a fade stops at its target
        exactly and sends it once, which is what leaves no value of a stopped fade to
        place below it. `ppqLimit` never passes the end of the row the engine last owned,
        which is what leaves the values of a row that spills past this block waiting for
        the block that reaches them, the way a run's steps, a ramp's, a wobble's points
        and a cut's release do, and a ramp whose next value sits more than a row behind
        where this block starts is put away rather than arriving late. */
    void flushVolume (std::vector<PendingEvent>& events, int midiCh, double ppqStart,
                      double ppqLimit, double ppqPerSample, int numSamples,
                      const Song& song);

    /** Sends the volume controller values of the swing one MIDI output channel has in
        force that belong to the position range [ppqStart, ppqLimit): the points of a
        tremolo's cycle wait with where each sits counted from the row that armed the
        swing over the macro's own cycle length, the way a fade's values, a run's steps,
        a slide's ramp, a wobble's points and a retrigger's strikes do, which is what
        leaves the points of a row that spills past this block waiting for the block that
        reaches them rather than bunched onto the row that named the letter or dropped, and
        what keeps a cycle that wrapped many times exact rather than drifted by its own
        fractions. The points are moved over the grid the macro's cycle names rather than
        the plain sub-row one, exactly as a wobble's are, so a cycle shorter than one point
        per sub-row step still shows its curve. Like a fade's values rather than a wobble's,
        every value is spoken even the one the channel's volume is already at, since unlike
        a wheel's position a controller has nothing that says a value is already in force,
        and a value outside the controller's 0..127 is clipped to the end it crossed rather
        than wrapped, which is what keeps a wider swing reaching past full and silence
        a host could not hear the difference in. A swing never finishes by itself: a cycle
        repeats for as long as the column carries the letter, so the clip that armed it and
        the release rows are what put it away, and a swing whose next point sits more than
        a row behind where this block starts is put away rather than arriving late, which
        is what a scrub or a jump leaves behind. */
    void flushTrem (std::vector<PendingEvent>& events, int midiCh, double ppqStart,
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

    /** A row naming the retrigger letter, one naming only an argument under a letter
        carried above it or one naming neither of the two and entering a note under a
        letter the column carries: the argument counts the sub-ticks apart a note that
        is already sounding in this column is struck again at, on the same sub-row grid
        an arpeggiator's steps, a slide's ramp, a wobble's points and a cut's length use,
        so a row that entered a note of its own sends nothing of its own there and leaves
        that note sounding, its arrival at the row being the note's own first trigger and
        the first strike waiting for the flush below at the sub-tick the period names past
        it - a period wider than the row's own sub-ticks reaching past that row and past
        the block that holds it the way a run's steps, a ramp's steps, a wobble's points
        and a cut's release do, and reaching as far past it as the entry's own top of
        kRetrigTopTicks sub-ticks. A letter spelled across a row that named no note of its
        own instead strikes the note this channel has still been holding from the rows
        above at that row's own sample as well, the row itself re-opening that note rather
        than leaving it sounding until the strike one whole period below the row reached
        it, and every strike after that one - at either row - is counted from the strike
        the engine last played at whatever period is in force there. The strikes belong to
        the note this channel is left holding at that row: the letter beside a row that
        named no note of its own works across the rows, taking the note the column has been
        holding from above and re-opening it there rather than leaving it to whatever
        ends it below, which is what the letter entered beside a row that did strike a note
        and a bare note row under a carried letter already have, and a row whose
        note column held nothing and entered nothing is ignored, there being no note to
        strike again. An empty argument beside a letter the row itself named takes the
        default period rather than no period at all, the number the entry writes so the cell
        spells the retrigger it asks for, and an empty one under a carried letter keeps the
        period that letter is carrying, so a bare note row is re-struck to whatever the
        column says; the notes entering there being armed at their own rows and the strikes
        of a note the rows above had armed carrying on from the strike the engine last
        played, which is what lets the letter gate a repeated-hit passage row by row rather
        than striking only the row that named it. An OFF argument or a 0 stops the
        retriggering rather than striking a note every no sub-ticks at all: it reads and
        answers as the note column's own note-off, drawn OFF the way its === is drawn and
        the way a cut's 0 is, closing the note this channel has been holding at that row at
        that row's own sample and putting the gate away rather than carrying it on below, so
        the notes entering the rows under a release sustain again and the column carries no
        letter there. Where that row entered a note of its own as well, that note's arrival
        had already closed the note above it at this very sample and sorted ahead of its own
        note-on there, so the release asks for nothing of its own and the note entered there
        is left to whatever ends it below - a note of no length at all cannot be spelled. An
        argument under a carried letter retargets the period for the notes entering there and
        the rows below rather than re-timing one the rows above had already armed at their
        own rows. The STP spelling asks for nothing, a retrigger having no ramp of its own to
        hold at a point - the wheel such a row puts back at its centre answers for a wobble
        above it rather than for that row's argument, and goes back whatever the argument
        spells - and leaves the period above still being carried; either way the note that
        row entered is left sounding under the period in force there, which is what makes the
        letter gate the notes of the rows under it rather than only the row that named it.
        Where this column has a wobble swinging, processRow has taken that swing away and
        sent the wheel back to its exact centre at this row's own sample before this call is
        made, ahead of the strike spelled here: a strike is a fresh articulation of the note
        the column is holding and the pitch a host gives that articulation is the one the
        wheel is sitting at, so a cycle's last point leaving the wheel part way down its
        swing would have the letter's own strike and the ones counted on from it below play
        the note at wherever in its swing the cycle happened to have got to rather than at the
        pitch the column named. A swing's displacement is the only displacement this row takes
        away: a point a bend row left on the wheel and the point a slide's ramp had climbed
        to in this column keep their pitch below the letter, which is what lets the letter
        re-strike a note at the pitch the column asked to have bent rather than straightening
        a pitch it was asked to bend, and a ramp another column was climbing is left climbing
        on because the wheel is that column's own state rather than one this row has anything
        to do with. Like a slide's, a vibrato's and a cut's rows, this row closes an arpeggiator
        run this column was in, a run and a gate not being two things one column can hold at
        once; where the letter's row named no note of its own and that run's steps were what the
        column was sounding, the row goes back to the note that run measured them from as well,
        read as it was typed in the note column rather than as the run's first step moved it there
        - the same note an id 0 release call of that run goes back to. The pitch that run was
        holding closes at this row's own sample and that base opens again at it, so the strike
        spelled here and the ones counted on below it land on the note the column named rather
        than on silent rows or on whatever pitch a step happened to have got to; that base is
        what the gate below is armed for as well, the strikes counted on from it rather than from
        a step's jump, the run's own displacement being what goes beside the letter. A run another
        column armed is left alone rather than traded for its base: a letter spelled on a parallel
        column reaches nothing across to it, a column's own notes being all its retrigger ever
        strikes. */
    void retrigCommand (std::vector<PendingEvent>& events, int ch, int offset,
                        double rowPpq, int argument, bool rowNamedLetter, int enteredNote,
                        bool noteOnRow, int midiCh, const Song& song);

    /** Arms the gate this channel's column has in force for the note it is left holding
        at the row that armed it: the pitch that row entered when it entered one of its own,
        otherwise the pitch the channel has still been holding from the rows above, or the base
        of an arpeggiator run that column had in force rather than a note of its own being held
        - that base being the note the run measured its steps from, read as it was typed in the
        note column rather than as the run's first step moved it there, which is the same note
        an id 0 release call of that run goes back to. Either way this is what lets the
        retrigger letter work across the rows rather than only the row that entered a note,
        and what leaves a row that left the channel holding no note at all, and a column whose
        run had already taken its base away at a row's own note-off above, ignored rather than
        armed for nothing at all. The first strike sits subTicksToFirstStrike
        sub-ticks of the Settings' grid past that row: one whole period when the row entered a
        note of its own and when the row instead went back to the base of a run it had just taken
        away, that row's own arrival or strike of the base being the first trigger of what the
        gate counts on from, and no sub-ticks at all when the letter was spelled across a row
        whose note column named nothing of its own and the column had a note still being held
        rather than a run in force - the row itself re-opening the note it has been holding,
        since a row that named nothing would otherwise leave that note sustaining until a strike
        one whole period below the row reached it. The strikes the gate makes after that one are
        counted on from the strike the flush below last played rather than from that row, which
        is what makes them exact for whatever period was named, keeps a row a transport jump
        makes this block play again arming its own strikes at that row rather than advancing
        one twice and leaves a period retargeted by a further row from reaching back into a
        note above it that had already been armed at its own row. Where the column had a wobble
        swinging above it, the row that armed this gate had already taken that swing away and
        put the wheel back at its exact centre at its own sample ahead of the strike spelled
        there, so the strikes counted on from that one play at the pitch the column named rather
        than at one the cycle left the wheel part way down its swing; a point a bend row left
        on the wheel or a same column's ramp had climbed to is kept rather than straightened, and
        a ramp another column was climbing is left climbing on. Where the row armed the gate for
        a run's base rather than for a note the column has been holding, that run's steps are
        gone below the row and its jumps belong to the column that called them: a letter spelled
        on another column leaves such a run stepping on rather than trading it for its base,
        that column's own notes being all a retrigger there ever strikes. */
    void armRetrig (int ch, int heldNote, int midiCh, double rowPpq,
                    int subTicksToFirstStrike, const Song& song);

    /** A row naming the letter L, an argument-only row under a carried L, or a row
        naming the letter I or O: what it says is about the volume controller of the
        MIDI output channel this row's instrument names, not about the column the row
        sits in, which is what lets a letter spelled on another column naming that
        same instrument reach the same level and the same ramp. The L letter sets a
        level outright, sends it at this row's own sample and puts any ramp away
        rather than leaving one counting on below it; a bare argument under a carried
        L does the same, which is what makes L a command the column carries down
        further. The I and O letters arm a ramp of their own and put the other letter's
        away: I sends nothing at this row's own sample and climbs toward the level by
        the step named, O sends that level there and falls toward nothing, both naming
        one value every sub-tick of the Settings' grid below it, the values waiting in
        that channel's state for flushVolume the way a run's steps, a ramp's, a wobble's
        points and a cut's release do. A step of 0 arms nothing and puts any ramp away,
        which leaves the volume the last value sent had got to in force rather than
        sending one more, and both letters leave nothing in force below their own row,
        so a bare argument under one answers nothing. None of the three says anything
        about a run, a wheel's ramp, a wobble's swing, a cut's gate or a retrigger's,
        which is what lets them work in parallel with the other seven letters. */
    void volCommand (std::vector<PendingEvent>& events, int midiCh, int cmd, int argument,
                     int offset, double rowPpq, const juce::Uuid& clipUid);

    /** A row naming the tremolo letter: what it says is about the volume controller of
        the MIDI output channel this row's instrument names rather than about this column,
        and its argument names which tremolo macro that channel swings that controller about
        the level it has, exactly as a vibrato call names a macro and a wobble swings about
        the point the wheel is at: an id of 0 or the OFF argument the entry writes for an o
        puts the swing away and leaves the channel sounding whatever value its last point
        sent, an STP one asks for nothing at all since a swing has no move of a wheel to
        stop, and another id arms that macro's swing at this row, its cycle's first point
        sitting here rather than one sub-row below, which is what keeps the row that named
        the letter at the level the channel was already at rather than at one side of the
        swing. The row's own column is named as well, since a release is what leaves the
        column carrying no letter below it, the way a wobble's release does, and the macro's
        three settings are read at every point of the flush below rather than here. */
    void tremCommand (std::vector<PendingEvent>& events, int ch, int midiCh, int id,
                      int offset, double rowPpq, const juce::Uuid& clipUid,
                      const Song& song);

    /** One tracker row's own messages: `offset` is where the row sits in the block,
        and what the row sends of its own goes out `offset` plus the delay its column
        leaves in force, so a column naming 50 strikes that row's note-on, note-off,
        pan, controller or command answer half a row later. The delay is a percent of
        a whole row, which is what makes it independent of the Settings' sub-ticks;
        the row's `ppqPerSample` is what reads that percent in samples. */
    /** Where the block this flush runs over reaches, in transport position: a delayed
        row's messages land there at their own sample and wait for the block that gets
        there when they do not, which is what keeps a delay wider than one block's own
        samples from being bunched onto the row that armed it or dropped for sitting
        past this block's end. A transport that jumped away drops what is still waiting
        below it, those rows having left the position the transport meant. */
    void flushDelay (std::vector<PendingEvent>& events, int ch, double ppqStart,
                     double ppqLimit, double ppqPerSample, int numSamples);

    void processRow (const Pattern& pattern, const Clip& clip, int row,
                     std::vector<PendingEvent>& events, int offset,
                     const Song& song, double rowPpq, double ppqPerSample);

    void startNote (std::vector<PendingEvent>& events, int offset, int ch, int midiCh, int note, int velocity);
    void stopNote (std::vector<PendingEvent>& events, int offset, int ch, int midiCh, int note);

    std::array<ChannelState, kMaxTrackerChannels> channels;

    /** What each tracker column has armed at one of its rows and left delaying past
        the block that held that row, oldest first: the delay column's state belongs to
        the column that named the percent, so the pair waits there rather than on the
        output channel a volume letter's ramp waits on. */
    std::array<std::vector<DelayedRow>, kMaxTrackerChannels> delayed {};

    /** What the MIDI output channels sound their notes at: the level an L, I or O
        row last named and the ramp such a row left counting on. These belong to the
        output channels rather than to the tracker columns, which is what lets a letter
        spelled on any column naming an instrument reach that instrument's level. */
    std::array<VolState, kMidiChannelCount> vols {};

    /** What the MIDI output channels' volume controllers are swung about their levels by
        a tremolo. These belong to the output channels rather than to the tracker columns
        exactly as the levels above do, which is what lets a T spelled on any column naming
        an instrument reach that instrument's swing and what keeps the swing and a fade of
        the volume letters states of one controller rather than one of each column. */
    std::array<TremState, kMidiChannelCount> tremors {};

    /** Controller numbers the volume letters and the pan column drive. They are refreshed
        from the song settings at the start of every block, so the Settings tab can point
        the L, I and O commands at any controller a synth listens to (CC7 channel volume
        by default) and the pan column at any other. */
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
