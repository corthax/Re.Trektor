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
    centre kBendCentrePos rather than a hair off it. */
constexpr int kBendStep = 128;      // one argument step, in wheel positions
constexpr int kBendCentrePos = 8192; // the wheel's centre
constexpr int kBendTopPos = 0x3fff;  // the wheel's highest position

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
        int activeNote = -1;
        int activeMidiCh = 0;
        juce::Uuid activeClip;
        int lastRow = -1;
        int lastBendPos = kBendCentrePos; // the wheel position this channel last left in force

        /** An arpeggiator run that has steps still to sound: how many steps of
            it have already played and where the run started, so the next step's
            position is the start plus that many step widths - counted, not
            summed, which is what keeps a looped run's cycle boundaries exact.
            The steps measure from arpBase: the row's own note when the call
            row had one, the note the channel carries when it had none, re-based
            by any note that enters further down the column while a run is
            still going. */
        int arpId = 0;             // macro id in force (0 = no run)
        int arpCount = 0;          // steps of the run already played
        double arpOrigin = -1.0;   // where the run's first step sits, in transport beats
        int arpBase = -1;          // the note the run's steps measure from
        juce::Uuid arpClip;        // clip whose rows own the run, so its end closes it
        double arpRowEnd = -1.0;   // end of the row the engine last processed

        /** The clip whose rows last moved this channel's pitch wheel, so that
            clip's end has a wheel to take back even when the row that moved it
            had no note of its own. */
        juce::Uuid bendClip;

        /** Ends a run: an arpeggiator that no longer has steps or no longer has
            a clip to hold it leaves nothing to play. */
        void endArp() noexcept
        {
            arpId = 0;
            arpCount = 0;
            arpOrigin = -1.0;
            arpBase = -1;
            arpClip = juce::Uuid();
        }

        /** Takes the channel's idea of the wheel back to the centre, which is
            what every release point does once it has sent the message that says
            so: a wheel left bent has no clip below it to put it right again. */
        void endBend() noexcept
        {
            lastBendPos = kBendCentrePos;
            bendClip = juce::Uuid();
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
        holding and ends it, another id starts or restarts that macro's run. A
        row that also names a note has already played its first step, so its run
        starts at the next one. */
    void arpCommand (std::vector<PendingEvent>& events, int ch, int id,
                     int offset, double rowPpq, const juce::Uuid& clipUid,
                     bool noteOnRow, const Song& song);

    /** Moves the pitch wheel for one row that named the bend letter: the row's
        argument names a point on it, 0 at the bottom and 127 near the top, so an
        argument the row left empty is taken as the wheel's centre, which bends
        nowhere. The message is the standard pair, and it goes out ahead of the
        note its own row strikes, so the note that row hits is already bent. What
        the wheel is left at is remembered, together with the clip whose rows
        asked for it, so a release point below can take it back. */
    void bendTo (std::vector<PendingEvent>& events, int ch, int midiCh,
                 int argument, int offset, const juce::Uuid& clipUid);

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
