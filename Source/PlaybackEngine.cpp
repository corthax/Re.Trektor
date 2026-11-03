#include "PlaybackEngine.h"
#include <algorithm>
#include <set>

namespace rtr
{

constexpr int kBendCentre = 8192;   // 14-bit pitch wheel centre
constexpr int kBendFull   = 16383;  // full positive bend

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

void PlaybackEngine::allNotesOff (std::vector<PendingEvent>& events, int offset)
{
    for (int i = 0; i < kMaxTrackerChannels; ++i)
    {
        auto& c = channels[(size_t) i];

        if (c.activeNote >= 0)
            events.push_back ({ offset, 0, juce::MidiMessage::noteOff (c.activeMidiCh + 1, c.activeNote) });

        if (c.lastBendPos != kBendCentre)
        {
            events.push_back ({ offset, 0, juce::MidiMessage::pitchWheel (c.activeMidiCh + 1, kBendCentre) });
            c.lastBendPos = kBendCentre;
        }

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

    // The state may have been cleared while the synth still holds this pitch
    // (Reaper does not call a MIDI effect when the transport is stopped, so
    // stop-time note-offs may never be delivered). Guarantee re-articulation.
    if (everPlayed[(size_t) midiCh].test (note) && c.activeNote != note)
        events.push_back ({ offset, 0, juce::MidiMessage::noteOff (midiCh + 1, note) });

    if (c.lastBendPos != kBendCentre)
    {
        events.push_back ({ offset, 0, juce::MidiMessage::pitchWheel (c.activeMidiCh + 1, kBendCentre) });
        c.lastBendPos = kBendCentre;
    }

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

    if (c.lastBendPos != kBendCentre)
    {
        events.push_back ({ offset, 0, juce::MidiMessage::pitchWheel (midiCh + 1, kBendCentre) });
        c.lastBendPos = kBendCentre;
    }

    if (c.activeNote == note)
        c.activeNote = -1;
}

void PlaybackEngine::processRow (const Pattern& pattern, const Clip& clip, int row,
                                 std::vector<PendingEvent>& events, int offset,
                                 double rowSpanSamples, const std::vector<Macro>& macros)
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

        if (cell.cmd != kEmpty)
        {
            if (cell.cmd >= kMacroCmdBase && cell.cmd < kMacroCmdBase + kMaxMacros)
            {
                const int mi = cell.cmd - kMacroCmdBase;

                if (mi < (int) macros.size())
                {
                    const auto& macro = macros[(size_t) mi];
                    const int baseNote = cell.note != kEmpty ? cell.note : state.activeNote;
                    const int vel = cell.vol != kEmpty ? cell.vol
                                                       : juce::jlimit (1, 127, state.lastVol > 0 ? state.lastVol : 100);

                    if (baseNote >= 0 && baseNote <= 127)
                    {
                        if (macro.kind == 0) // arpeggiator
                        {
                            const int slots = juce::jlimit (1, 16, macro.param1);
                            const int step = juce::jlimit (1, 24, macro.param2);

                            // an explicit note in this row already started slot 0
                            for (int k = (cell.note != kEmpty ? 1 : 0); k < slots; ++k)
                            {
                                const int arpNote = juce::jlimit (0, 127, baseNote + (k * step) % 12);
                                const int sub = offset + (int) std::llround ((double) k * rowSpanSamples / (double) slots);
                                startNote (events, sub, ch, midiCh, arpNote, vel);
                            }
                        }
                        else if (macro.kind == 1 && state.activeNote >= 0) // pitch glide
                        {
                            const int semis = juce::jlimit (1, 24, macro.param1);
                            const int steps = juce::jlimit (2, 16, macro.param2);

                            events.push_back ({ offset, 2, juce::MidiMessage::controllerEvent (state.activeMidiCh + 1, 105, semis) });

                            for (int k = 1; k <= steps; ++k)
                            {
                                const int pos = kBendCentre + (int) std::llround (8191.0 * (double) k / (double) steps);
                                const int sub = offset + (int) std::llround ((double) k * rowSpanSamples / (double) steps);
                                events.push_back ({ sub, 1, juce::MidiMessage::pitchWheel (state.activeMidiCh + 1, pos) });
                                state.lastBendPos = pos;
                            }
                        }
                        else if (macro.kind == 2) // slide up/down
                        {
                            const int step = juce::jlimit (1, 24, macro.param1);
                            const int pairs = juce::jlimit (1, 16, macro.param2);
                            const int total = 2 * pairs;

                            for (int j = 1; j <= total; ++j)
                            {
                                const int n = (j + 1) / 2;
                                const int delta = (j % 2 == 1 ? n : -n) * step;
                                const int slideNote = baseNote + delta;

                                if (slideNote < 0 || slideNote > 127)
                                    continue;

                                const int sub = offset + (int) std::llround ((double) j * rowSpanSamples / (double) total);
                                startNote (events, sub, ch, midiCh, slideNote, vel);
                            }
                        }
                    }
                }
            }
            else
                events.push_back ({ offset, 3, juce::MidiMessage::controllerEvent (midiCh + 1,
                                                                                  juce::jlimit (0, 127, cell.cmd),
                                                                                  juce::jlimit (0, 127, cell.cmdVal >= 0 ? cell.cmdVal : 0)) });
        }

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

    if (justStarted || jumped)
        allNotesOff (events, 0);

    if (justStarted)
    {
        for (auto& c : channels)
            c.lastRow = -1;
    }

    const double rowsPerBeat = (double) juce::jmax (1, song.settings.rowsPerBeat);
    const double rowSpanSamples = (60.0 / juce::jmax (1.0, t.bpm)) * sampleRate / rowsPerBeat;

    // Hosts report per-block positions with their own rounding; a transport
    // stopping exactly at a clip end can end a hair before the computed bound.
    const double span = (double) numSamples * ppqPerSample;
    const double spanEps = std::max (1.0e-6, span * 1.0e-3);

    // Rows the previous block already covered must not be processed twice
    // (hosts may report a block start a few ulps below the previous computed
    // end); a deliberate backwards seek is excluded from this dedup.
    const double drift = std::max (1.0e-9, span * 1.0e-4);
    const bool nearContinuous = previousPpq >= 0.0 && ppqStart >= previousPpq - drift;

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

            if (rowTime < ppqStart && ! (nearContinuous && rowTime >= previousPpq && rowTime >= ppqStart - drift))
                continue;

            if (nearContinuous && rowTime >= ppqStart && rowTime < previousPpq)
                continue; // covered by the previous block already

            const int offset = juce::jlimit (0, juce::jmax (0, numSamples - 1),
                                             (int) std::llround ((rowTime - ppqStart) / juce::jmax (1.0e-12, ppqPerSample)));

            processRow (*pattern, clip, row, events, offset, rowSpanSamples, song.macros);

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
                    processRow (*pattern, clip, rowAtStart, events, 0, rowSpanSamples, song.macros);
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

                // A block straddling the clip start still "covers" the clip:
                // a note may legitimately be started by this block's rows.
                stillUnder = ppqStart < end && ppqEnd > clip->startBeats - 1.0e-9;
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
