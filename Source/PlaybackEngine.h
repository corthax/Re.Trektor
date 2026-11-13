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
            and a further note-off below takes the answer away again. The notes a
            run entered are tracked in arpNote so closing a run releases only
            those, and a note-off step is what lets go of the one it sits at. */
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

        /** Takes the channel's idea of the wheel back to the centre and leaves
            no slide in force, which is what every release point does once it has
            sent the message that says so: a wheel left bent has no clip below it
            to put it right again, and a slide whose rows the column stopped
            having has no rows left to reach its extreme. */
        void endBend() noexcept
        {
            lastBendPos = kBendCentrePos;
            bendClip = juce::Uuid();
            endSlide();
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
