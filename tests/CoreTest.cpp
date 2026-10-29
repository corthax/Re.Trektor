/*
    Unit tests for the Re.Trektor song model and playback engine.
    These do not need a display or a host.
*/

#include "Model.h"
#include "PlaybackEngine.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <iostream>
#include <vector>

using namespace rtr;

namespace
{
int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  ok   " : "  FAIL ") << what << "\n";
    if (! ok)
        ++failures;
}

struct NoteEvent
{
    int offset;
    bool on;
    int channel;
    int note;
    int velocity;
};

struct Harness
{
    Song song;
    PlaybackEngine engine;
    std::vector<NoteEvent> notes;
    double ppq = 0.0;

    static constexpr int blockSize = 512;
    static constexpr double sampleRate = 48000.0;
    static constexpr double bpm = 120.0;

    double beatsPerBlock() const { return (double) blockSize / sampleRate * bpm / 60.0; }

    void runBlocks (int count, bool playing = true, bool collect = true)
    {
        for (int i = 0; i < count; ++i)
        {
            PlaybackEngine::Transport t;
            t.playing = playing;
            t.bpm = bpm;
            t.ppq = ppq;

            juce::MidiBuffer midi;
            engine.process (song, midi, t, blockSize, sampleRate);

            if (collect)
            {
                for (const auto metadata : midi)
                {
                    const auto m = metadata.getMessage();

                    if (m.isNoteOn())
                        notes.push_back ({ metadata.samplePosition, true, m.getChannel(),
                                           m.getNoteNumber(), m.getVelocity() });
                    else if (m.isNoteOff())
                        notes.push_back ({ metadata.samplePosition, false, m.getChannel(),
                                           m.getNoteNumber(), 0 });
                }
            }

            ppq += beatsPerBlock();
        }
    }

    void resetEvents() { notes.clear(); }
};
} // namespace

int main()
{
    //== Song defaults ========================================================
    {
        std::cout << "Defaults\n";
        Song song;
        check (song.patterns.size() == 1, "one pattern created on reset");
        check (song.clips.size() == 1, "pattern placed at bar 1");
        check (song.patterns[0].length == 64, "default pattern length is 64 rows");
        check (song.patterns[0].channels == 1, "default channel count is 1");
        check (song.settings.rowsPerBeat == 4, "default 4 rows per beat");
        check (song.settings.subticksPerRow == 6, "default 6 subticks per row");
        check (song.settings.maxTrackerChannels == 1, "default 1 tracker channel");
        check (song.settings.snapEnabled && song.settings.adaptiveGrid, "snap + adaptive grid on by default");

        song.settings.defaultPatternRows = 32;
        auto& np = song.addPattern();
        check (np.length == 32, "new patterns use the configured default length");
    }

    //== Basic sequencing =====================================================
    {
        std::cout << "Sequencing\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (16);
        p.cell (0, 0) = Cell(); // note 60 via helper below
        auto& c0 = p.cell (0, 0);
        c0.note = 60;
        c0.inst = 0;
        c0.vol = 100;

        auto& c2 = p.cell (4, 0);
        c2.note = 67;
        c2.inst = 0;

        h.runBlocks (60);

        int noteOns = 0;
        for (auto& n : h.notes)
            if (n.on)
                ++noteOns;

        check (noteOns == 2, "two notes played in the first beat (" + juce::String (noteOns) + ")");
        check (! h.notes.empty() && h.notes.front().on && h.notes.front().note == 60,
               "first event is note-on 60");
        check (! h.notes.empty() && h.notes.front().velocity == 100, "velocity comes from the volume column");
        check (! h.notes.empty() && h.notes.front().channel == 1, "instrument 0 maps to MIDI channel 1");

        bool sawOffBeforeSecond = false;
        int seen = 0;
        for (auto& n : h.notes)
        {
            if (n.on)
            {
                ++seen;

                if (seen == 2 && sawOffBeforeSecond)
                    break;
            }
            else if (n.note == 60)
            {
                sawOffBeforeSecond = true;
            }
        }

        check (sawOffBeforeSecond, "previous note is stopped when a new note starts on the channel");

        h.resetEvents();
        h.runBlocks (1, false);
        bool allNotesOff = false;

        // A single stopped block flushes note-offs.
        for (auto& n : h.notes)
            if (! n.on)
                allNotesOff = true;

        check (allNotesOff, "note-offs are flushed when the transport stops");
    }

    //== Instrument carry =====================================================
    {
        std::cout << "Instrument carry\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (16);
        p.cell (0, 0).note = 60;
        p.cell (0, 0).inst = 3;
        p.cell (4, 0).note = 62; // no instrument: should carry 3

        h.runBlocks (60);

        int ch = -1;
        for (auto& n : h.notes)
            if (n.on && n.note == 62)
                ch = n.channel;

        check (ch == 4, "instrument carries to later notes (MIDI channel 4), got " + juce::String (ch));
    }

    //== Duplicate note suppression ===========================================
    {
        std::cout << "Duplicate suppression\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setChannels (2);
        p.cell (0, 0).note = 60;
        p.cell (0, 0).inst = 0;
        p.cell (0, 1).note = 60; // same MIDI channel + note on the same row
        p.cell (0, 1).inst = 0;

        h.runBlocks (5);

        int on60 = 0;
        for (auto& n : h.notes)
            if (n.on && n.note == 60)
                ++on60;

        check (on60 == 1, "identical note on two tracker channels is played once, got " + juce::String (on60));
    }

    //== Grid snapping ========================================================
    {
        std::cout << "Grid snapping\n";
        Song song;
        song.settings.rowsPerBeat = 4;
        const double ppb = 48.0;
        check (juce::approximatelyEqual (song.beatsPerRow(), 0.25), "beats per row = 0.25");
        check (juce::approximatelyEqual (song.currentGridRows (ppb), 2.0), "adaptive grid at 48 px/beat is 2 rows");
        check (juce::approximatelyEqual (song.snapBeats (0.6, ppb), 0.5), "0.6 snaps to 0.5");
        check (juce::approximatelyEqual (song.snapBeats (0.9, ppb), 1.0), "0.9 snaps to 1.0");

        song.settings.adaptiveGrid = false;
        song.settings.fixedGridRows = 1;
        check (juce::approximatelyEqual (song.snapBeats (0.30, ppb), 0.25), "fixed 1-row grid snaps 0.30 to 0.25");
        song.settings.fixedGridRows = 4;
        check (juce::approximatelyEqual (song.snapBeats (0.60, ppb), 1.0), "fixed 4-row grid snaps 0.60 to 1.0");

        song.settings.snapEnabled = false;
        check (juce::approximatelyEqual (song.snapBeats (0.37, ppb), 0.37), "snapping disabled leaves the value alone");
    }

    //== Slicing ==============================================================
    {
        std::cout << "Slicing\n";
        Song song;
        auto& p = song.patterns[0];
        p.setLength (8);
        p.cell (1, 0).note = 61;
        p.cell (5, 0).note = 66;
        const juce::Uuid clipUid = song.clips[0].uid;

        check (! song.sliceClipAt (clipUid, 0.0), "cannot slice at the clip start");
        check (! song.sliceClipAt (clipUid, song.patternLengthBeats (p.id)), "cannot slice at the clip end");

        const int patternCountBefore = (int) song.patterns.size();
        check (song.sliceClipAt (clipUid, 0.5), "slice at beat 0.5 succeeds");

        check ((int) song.patterns.size() == patternCountBefore + 2, "slice creates two new patterns");
        check (song.clips.size() == 2, "slice replaces one clip with two");
        check (song.patterns[1].length == 2 && song.patterns[2].length == 6, "pattern halves are 2 and 6 rows");
        check (song.patterns[1].cell (1, 0).note == 61, "first half keeps row 1");
        check (song.patterns[2].cell (3, 0).note == 66, "second half keeps its row 5 data at row 3");
        check (song.patterns[1].id != song.patterns[0].id && song.patterns[2].id != song.patterns[1].id,
               "the halves are unique patterns");
        check (juce::approximatelyEqual (song.clips[1].startBeats, 0.5), "second clip starts at the slice point");

        auto& oneRow = song.addPattern ("tiny", 1, 1);
        song.removeClip (song.clips[0].uid);
        song.removeClip (song.clips[0].uid);

        if (auto* c = song.addClip (oneRow.id, 4.0))
            check (! song.sliceClipAt (c->uid, 4.5), "cannot slice a 1-row pattern");
    }

    //== Paste instance / unique ==============================================
    {
        std::cout << "Paste\n";
        Song song;
        auto& p = song.patterns[0];
        p.setLength (8);
        p.cell (0, 0).note = 60;

        song.copyPattern (p.id);
        const int countBefore = (int) song.patterns.size();

        auto* inst = song.pasteAt (4.0, false);
        check (inst != nullptr && inst->patternId == p.id, "paste as instance shares the pattern");
        check ((int) song.patterns.size() == countBefore, "instance paste adds no pattern");

        auto* unique = song.pasteAt (8.0, true);
        check (unique != nullptr && unique->patternId != p.id, "unique paste references a new pattern");
        check ((int) song.patterns.size() == countBefore + 1, "unique paste adds one pattern");

        if (unique != nullptr)
        {
            auto* u = song.findPattern (unique->patternId);
            check (u != nullptr && u->cell (0, 0).note == 60, "unique paste copies the data");
        }

        check (song.pasteAt (4.0, false) == nullptr, "overlapping paste is rejected");
    }

    //== State round trip =====================================================
    {
        std::cout << "Serialisation\n";
        Song a;
        a.settings.rowsPerBeat = 8;
        a.settings.defaultPatternRows = 32;
        a.settings.hexDisplay = true;
        a.settings.maxTrackerChannels = 2;
        auto& p = a.patterns[0];
        p.setChannels (2);
        p.setLength (12);
        p.cell (3, 1).note = 72;
        p.cell (3, 1).inst = 5;
        p.cell (3, 1).cmd = 17;
        p.cell (3, 1).cmdVal = 99;

        const auto tree = a.toValueTree();

        Song b;
        b.fromValueTree (tree);

        check (b.settings.rowsPerBeat == 8, "rows per beat restored");
        check (b.settings.defaultPatternRows == 32, "default pattern rows restored");
        check (b.settings.hexDisplay, "hex setting restored");
        check (b.patterns.size() == a.patterns.size(), "pattern count restored");
        check (b.clips.size() == a.clips.size(), "clip count restored");
        check (b.patterns[0].length == 12, "pattern length restored");
        check (b.patterns[0].channels == 2, "channel count restored");
        check (b.patterns[0].cell (3, 1).note == 72, "note restored");
        check (b.patterns[0].cell (3, 1).inst == 5, "instrument restored");
        check (b.patterns[0].cell (3, 1).cmd == 17 && b.patterns[0].cell (3, 1).cmdVal == 99,
               "command restored");
    }

    //== Overlap rules ========================================================
    {
        std::cout << "Overlap rules\n";
        Song song;
        auto& p = song.patterns[0];
        p.setLength (4);
        check (song.addClip (p.id, 0.5) == nullptr, "clip cannot overlap the existing clip");
        check (song.addClip (p.id, -1.0) == nullptr, "clip cannot be placed before 0");
        check (song.addClip (p.id, 0.0) == nullptr, "clip cannot start at 0 again");
        check (song.addClip (p.id, 1.0) != nullptr, "a clip may start exactly at the end of another");
    }

    std::cout << (failures == 0 ? "PASS" : "FAILED") << " (" << failures << " failures)\n";
    return failures == 0 ? 0 : 1;
}
