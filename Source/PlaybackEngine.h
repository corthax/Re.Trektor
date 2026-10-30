#pragma once

#include "Model.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

namespace rtr
{

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
        int lastBendPos = 8192; // 14-bit pitch wheel centre
    };

    struct PendingEvent
    {
        int offset = 0;
        int order = 0;
        juce::MidiMessage message;
    };

    void allNotesOff (std::vector<PendingEvent>& events, int offset);
    void processRow (const Pattern& pattern, const Clip& clip, int row,
                     std::vector<PendingEvent>& events, int offset,
                     double rowSpanSamples, const std::vector<Macro>& macros);

    void startNote (std::vector<PendingEvent>& events, int offset, int ch, int midiCh, int note, int velocity);
    void stopNote (std::vector<PendingEvent>& events, int offset, int ch, int midiCh, int note);

    std::array<ChannelState, kMaxTrackerChannels> channels;
    bool wasPlaying = false;
    double previousPpq = -1.0;
    bool justStarted = false;
};

} // namespace rtr
