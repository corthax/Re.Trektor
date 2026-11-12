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
            wheel already was to the extreme the letter names, read off the row
            rather than counted from it, so a row a transport jump makes this
            block play again lands on the same point of the ramp instead of
            advancing it twice. The row that named the letter is the ramp's start,
            so the wheel sits where it was there and the extreme sits slideSpan
            rows below it; a span of 0 jumps there on the naming row itself.
            slideLastRow is the row the ramp last moved for, which is what makes
            a release point able to tell whether the ramp still has rows left to
            run, the wheel having reached the extreme being told on its own. */
        bool slideOn = false;                    // a slide is in force in this column
        int slideRow0 = -1;                      // the row the ramp starts from
        int slideSpan = 1;                       // rows the ramp takes to reach slideTo
        int slideLastRow = -1;                   // the row the ramp last moved for
        int slideFrom = kBendCentrePos;          // where the ramp leaves the wheel
        int slideTo = kBendCentrePos;            // the extreme it heads for

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
            slideRow0 = -1;
            slideSpan = 1;
            slideLastRow = -1;
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

        /** Whether the slide this channel is in still has rows of its ramp left
            to run, which is what a release point asks before it decides that a
            wheel sitting at the centre has nothing to take back. */
        bool slideRunning() const noexcept
        {
            return slideOn && slideLastRow < slideRow0 + slideSpan;
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

    /** A row naming the arpeggiator command: id 0 releases what the run is
        holding and ends it, and leaves the column not calling a macro below it,
        which is what stops a further row naming only an argument being answered
        there and stops the tracker drawing a carried call over those rows;
        another id stops the run above it and starts or restarts that macro's
        run. Its steps measure from the note the column last named, which is the
        call row's own note where it had one, and the note a note-off row is
        releasing where its cell holds a note-off instead - such a row's call is
        answered, not ignored, and the note-off takes the note away for the rows
        below it. A row that also names a note has already played its first step,
        so its run starts at the next one. */
    void arpCommand (std::vector<PendingEvent>& events, int ch, int id,
                     int offset, double rowPpq, const juce::Uuid& clipUid,
                     bool noteOnRow, const Song& song);

    /** Closes the arpeggiator run this channel is in: the note its last step
        entered is released, which is what a switch to another command and a
        looped run's cycle end leave no note sounding from the step before. A
        run that entered nothing has nothing to release, which is what a note
        the column itself struck is left alone, and a finished run's last step
        is not closed where the run simply ran out of steps. */
    void closeArp (std::vector<PendingEvent>& events, int offset, int ch);

    /** Moves the pitch wheel for one row that named the bend letter: the row's
        argument names a point on it, 0 at the bottom and 127 near the top, so an
        argument the row left empty is taken as the wheel's centre, which bends
        nowhere. The message is the standard pair, and it goes out ahead of the
        note its own row strikes, so the note that row hits is already bent. What
        the wheel is left at is remembered, together with the clip whose rows
        asked for it, so a release point below can take it back. */
    void bendTo (std::vector<PendingEvent>& events, int ch, int midiCh,
                 int argument, int offset, const juce::Uuid& clipUid);

    /** Arms the ramp of one row that named a slide letter: the argument says how
        many rows the slide takes, the empty one taking the default span and one
        below 0 being clamped, and the ramp runs from where the wheel already is
        to the extreme the letter names. The row that named the letter is the
        ramp's own start, so arming leaves the wheel where it was rather than
        moving it, which is what keeps that row's own note straight and puts the
        extreme span rows below. A further row under a carried letter re-arms the
        ramp with its own span, so a slide can be retargeted as it runs. */
    void slideCommand (std::vector<PendingEvent>& events, int ch, int midiCh,
                       int argument, bool up, int row, int offset,
                       const juce::Uuid& clipUid);

    /** Moves the wheel for one step of the slide this channel has in force, which
        every row the engine processes for that channel gets whether it named
        anything or not: the step is read off the row, so a row a transport jump
        makes this block play again does not advance the ramp twice, and a row
        that lands the ramp on the position the wheel is already at emits nothing.
        The message goes out ahead of the note the same row strikes, so a note
        entering while the wheel climbs is bent by where the climb had got to. */
    void slideStep (std::vector<PendingEvent>& events, int ch, int midiCh,
                    int row, int offset, const juce::Uuid& clipUid);

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
