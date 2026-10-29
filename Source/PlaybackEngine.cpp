#include "PlaybackEngine.h"
#include <algorithm>
#include <set>

namespace rtr
{

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

    wasPlaying = false;
    previousPpq = -1.0;
    justStarted = false;
}

void PlaybackEngine::allNotesOff (std::vector<PendingEvent>& events, int offset)
{
    for (int i = 0; i < kMaxTrackerChannels; ++i)
    {
        auto& c = channels[(size_t) i];

        if (c.activeNote >= 0)
            events.push_back ({ offset, 0, juce::MidiMessage::noteOff (c.activeMidiCh + 1, c.activeNote) });

        c.activeNote = -1;
        c.lastRow = -1;
        uiRow[(size_t) i].store (-1);
    }

    int order = 1000;

    for (int ch = 0; ch < kMidiChannelCount; ++ch)
    {
        events.push_back ({ offset, order++, juce::MidiMessage::allNotesOff (ch + 1) });
        events.push_back ({ offset, order++, juce::MidiMessage::allSoundOff (ch + 1) });
    }
}

void PlaybackEngine::panic (juce::MidiBuffer& midi)
{
    std::vector<PendingEvent> events;
    allNotesOff (events, 0);

    for (auto& e : events)
        midi.addEvent (e.message, 0);
}

void PlaybackEngine::startNote (std::vector<PendingEvent>& events, int offset, int ch,
                                int midiCh, int note, int velocity)
{
    auto& c = channels[(size_t) ch];

    if (c.activeNote >= 0)
        events.push_back ({ offset, 0, juce::MidiMessage::noteOff (c.activeMidiCh + 1, c.activeNote) });

    events.push_back ({ offset, 1, juce::MidiMessage::noteOn (midiCh + 1, note, (juce::uint8) juce::jlimit (1, 127, velocity)) });

    c.activeNote = note;
    c.activeMidiCh = midiCh;
    uiMidiChannel[(size_t) ch].store (midiCh);
}

void PlaybackEngine::stopNote (std::vector<PendingEvent>& events, int offset, int ch, int midiCh, int note)
{
    events.push_back ({ offset, 0, juce::MidiMessage::noteOff (midiCh + 1, note) });

    auto& c = channels[(size_t) ch];

    if (c.activeNote == note)
        c.activeNote = -1;
}

void PlaybackEngine::processRow (const Pattern& pattern, const Clip& clip, int row,
                                 std::vector<PendingEvent>& events, int offset)
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

        if (cell.note == kNoteOff)
        {
            if (state.activeNote >= 0)
                stopNote (events, offset, ch, state.activeMidiCh, state.activeNote);

            continue;
        }

        if (cell.note != kEmpty && cell.note >= 0 && cell.note <= 127)
        {
            const std::pair<int, int> key { midiCh, cell.note };

            if (! notesThisRow.insert (key).second)
                continue; // same note already triggered on this row - ignore duplicate

            const int velocity = cell.vol != kEmpty ? cell.vol : juce::jlimit (1, 127, state.lastVol > 0 ? state.lastVol : 100);
            startNote (events, offset, ch, midiCh, cell.note, velocity);
            state.activeClip = clip.uid;
        }

        // Volume without a note acts as channel volume; with a note it became velocity.
        if (cell.vol != kEmpty && cell.note == kEmpty)
            events.push_back ({ offset, 2, juce::MidiMessage::controllerEvent (midiCh + 1, 7, juce::jlimit (0, 127, cell.vol)) });

        if (cell.pan != kEmpty)
            events.push_back ({ offset, 2, juce::MidiMessage::controllerEvent (midiCh + 1, 10, juce::jlimit (0, 127, cell.pan)) });

        if (cell.cmd != kEmpty)
            events.push_back ({ offset, 3, juce::MidiMessage::controllerEvent (midiCh + 1,
                                                                              juce::jlimit (0, 127, cell.cmd),
                                                                              juce::jlimit (0, 127, cell.cmdVal >= 0 ? cell.cmdVal : 0)) });

        uiRow[(size_t) ch].store (row);
    }
}

void PlaybackEngine::process (const Song& song, juce::MidiBuffer& midi, const Transport& t,
                              int numSamples, double sampleRate, bool /*passThrough*/)
{
    std::vector<PendingEvent> events;

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

    if (justStarted || jumped)
        allNotesOff (events, 0);

    if (justStarted)
    {
        for (auto& c : channels)
            c.lastRow = -1;
    }

    const double rowsPerBeat = (double) juce::jmax (1, song.settings.rowsPerBeat);

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

            if (rowTime < ppqStart - 1.0e-9 || rowTime >= ppqEnd)
                continue;

            const int offset = juce::jlimit (0, juce::jmax (0, numSamples - 1),
                                             (int) std::llround ((rowTime - ppqStart) / juce::jmax (1.0e-12, ppqPerSample)));

            processRow (*pattern, clip, row, events, offset);

            for (int ch = 0; ch < pattern->channels && ch < kMaxTrackerChannels; ++ch)
                channels[(size_t) ch].lastRow = row;
        }

        // Starting in the middle of a row: optionally process that row.
        if ((justStarted || jumped) && ! song.settings.midRowIgnored)
        {
            const int rowAtStart = (int) std::floor ((ppqStart - clipStart) * rowsPerBeat);

            if (rowAtStart >= 0 && rowAtStart < pattern->length)
            {
                const double rowTime = clipStart + (double) rowAtStart / rowsPerBeat;

                if (rowTime < ppqStart - 1.0e-9)
                    processRow (*pattern, clip, rowAtStart, events, 0);
            }
        }

        // Notes that were started by this clip must stop when the clip ends.
        if (ppqStart < clipEnd && ppqEnd >= clipEnd)
        {
            const int offset = juce::jlimit (0, juce::jmax (0, numSamples - 1),
                                             (int) std::llround ((clipEnd - ppqStart) / juce::jmax (1.0e-12, ppqPerSample)));

            for (int ch = 0; ch < pattern->channels && ch < kMaxTrackerChannels; ++ch)
            {
                auto& c = channels[(size_t) ch];

                if (c.activeNote >= 0 && c.activeClip == clip.uid)
                    stopNote (events, offset, ch, c.activeMidiCh, c.activeNote);
            }
        }
    }

    // Notes whose owning clip was deleted or is no longer under the playhead.
    for (int ch = 0; ch < kMaxTrackerChannels; ++ch)
    {
        auto& c = channels[(size_t) ch];

        if (c.activeNote < 0)
            continue;

        const auto* clip = song.findClip (c.activeClip);
        bool stillUnder = false;

        if (clip != nullptr)
        {
            const auto* pattern = song.findPattern (clip->patternId);

            if (pattern != nullptr)
            {
                const double end = clip->startBeats + (double) pattern->length / rowsPerBeat;
                stillUnder = ppqStart >= clip->startBeats - 1.0e-9 && ppqStart < end;
            }
        }

        if (! stillUnder)
            stopNote (events, 0, ch, c.activeMidiCh, c.activeNote);
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
