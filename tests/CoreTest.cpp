/*
    Unit tests for the Re.Trektor song model and playback engine.
    These do not need a display or a host.
*/

#include "Model.h"
#include "PlaybackEngine.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <iostream>
#include <vector>
#include <algorithm>

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

struct ControllerEvent
{
    int offset;
    int channel;
    int number;
    int value;
};

struct RawEvent
{
    int offset;
    int kind; // 0 note-off, 1 note-on, 2 all-notes-off, 3 all-sound-off, 4 anything else
    int pitch; // the note a note event carries, otherwise the controller number
};

struct Harness
{
    Song song;
    PlaybackEngine engine;
    std::vector<NoteEvent> notes;
    std::vector<ControllerEvent> controllers;
    std::vector<int> bends;
    std::vector<RawEvent> raw;
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
                    else if (m.isController())
                        controllers.push_back ({ metadata.samplePosition, m.getChannel(),
                                                 m.getControllerNumber(), m.getControllerValue() });
                    else if (m.isPitchWheel())
                        bends.push_back (m.getPitchWheelValue());

                    // the order the host would see the events in, which is what
                    // lets a check name a release and the note it must not cut
                    int kind = 4;
                    int pitch = 0;

                    if (m.isAllNotesOff ())
                        kind = 2;
                    else if (m.isAllSoundOff ())
                        kind = 3;
                    else if (m.isNoteOn ())
                        kind = 1;
                    else if (m.isNoteOff ())
                        kind = 0;

                    if (kind == 0 || kind == 1)
                        pitch = m.getNoteNumber ();
                    else if (m.isController ())
                        pitch = m.getControllerNumber ();

                    raw.push_back ({ metadata.samplePosition, kind, pitch });
                }
            }

            ppq += beatsPerBlock();
        }
    }

    void resetEvents()
    {
        notes.clear();
        controllers.clear();
        bends.clear();
        raw.clear();
    }
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
        check (kMaxTrackerChannels == 127, "channel limit is the constant 127, not a setting");
        check (song.settings.snapEnabled && song.settings.adaptiveGrid, "snap + adaptive grid on by default");
        check (song.settings.volumeCc == 7 && song.settings.panCc == 10, "volume + pan columns default to CC7 and CC10");

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

    //== Command macros =======================================================
    {
        std::cout << "Command macros (arpeggiator)\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (16);

        // default song includes one macro (M0); redefine as a small arpeggiator
        h.song.macros[0].name = "Arp 3x+2";
        h.song.macros[0].kind = 0;
        h.song.macros[0].param1 = 3;
        h.song.macros[0].param2 = 2;

        p.cell (0, 0).note = 60;
        p.cell (0, 0).inst = 0;
        p.cell (0, 0).vol = 100;
        p.cell (0, 0).cmd = 128; // call M0

        h.runBlocks (12);

        std::vector<int> ons;
        for (auto& n : h.notes)
            if (n.on)
                ons.push_back (n.note);

        check (ons.size() == 3, "arpeggiator re-triggers the note once per slot, got " + juce::String ((int) ons.size()));
        check (std::find (ons.begin(), ons.end(), 62) != ons.end(), "arp adds the +2 semitone note");
        check (std::find (ons.begin(), ons.end(), 64) != ons.end(), "arp adds the +4 semitone note");
    }
    {
        std::cout << "Command macros (pitch glide)\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (16);

        h.song.macros[0].kind = 1;
        h.song.macros[0].param1 = 5; // bend target in semitones
        h.song.macros[0].param2 = 4; // ramp steps

        p.cell (0, 0).note = 60;
        p.cell (0, 0).inst = 0;
        p.cell (0, 0).cmd = 128;

        h.runBlocks (12);

        check (h.bends.size() == 4, "glide sweeps the pitch wheel in the configured number of steps, got " + juce::String ((int) h.bends.size()));

        bool rising = ! h.bends.empty();
        for (int i = 0; i < (int) h.bends.size() && rising; ++i)
            if (h.bends[(size_t) i] <= 8192 || h.bends[(size_t) i] > 16383)
                rising = false;

        check (rising, "pitch wheel ramps upward from centre");
        check (! h.bends.empty() && h.bends.back() == 16383, "glide reaches the configured bend target");
    }
    {
        std::cout << "Command macros (slide)\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (16);

        h.song.macros[0].kind = 2;
        h.song.macros[0].param1 = 2; // semitone step
        h.song.macros[0].param2 = 1; // one up/down pair

        p.cell (0, 0).note = 60;
        p.cell (0, 0).inst = 0;
        p.cell (0, 0).vol = 100;
        p.cell (0, 0).cmd = 128;

        h.runBlocks (12);

        std::vector<int> ons;
        for (auto& n : h.notes)
            if (n.on)
                ons.push_back (n.note);

        check (ons.size() == 3, "slide adds an up and a down re-trigger, got " + juce::String ((int) ons.size()));
        check (std::find (ons.begin(), ons.end(), 62) != ons.end(), "slide oscillates up to +2 semitones");
        check (std::find (ons.begin(), ons.end(), 58) != ons.end(), "slide oscillates down to -2 semitones");
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

    //== Pattern resize keeps hidden rows =====================================
    {
        std::cout << "Pattern resize keeps hidden rows\n";

        Song s;
        auto& p = s.patterns[0];
        p.setLength (16);
        p.cell (0, 0).note = 60;
        p.cell (4, 0).note = 62;
        p.cell (12, 0).note = 64;
        p.cell (12, 0).inst = 3;

        p.setLength (8);
        check (p.length == 8, "pattern shortened to 8 rows");
        check (p.capacity() == 13, "capacity keeps the last row holding data");
        check (p.rows[12][0].note == 64 && p.rows[12][0].inst == 3,
               "the note past the new end is kept, not discarded");

        p.setLength (16);
        check (p.cell (12, 0).note == 64 && p.cell (12, 0).inst == 3,
               "enlarging again restores the hidden note");
        check (p.cell (11, 0).empty(), "rows in between stay blank");

        const auto tree = s.toValueTree();
        Song r;
        r.fromValueTree (tree);
        check (r.patterns[0].length == 16, "grown length restored from state");
        check (r.patterns[0].capacity() >= 13, "hidden capacity restored from state");
        check (r.patterns[0].cell (12, 0).note == 64, "hidden data survives a state round trip");

        Song s2;
        s2.patterns[0].setLength (16);
        s2.patterns[0].setLength (4);
        check (s2.patterns[0].capacity() == 4, "hidden rows that hold nothing are trimmed");

        Harness h;
        auto& q = h.song.patterns[0];
        q.setLength (16);
        q.cell (0, 0).note = 65;
        q.cell (12, 0).note = 77;
        q.setLength (8); // clip now spans 2 beats, the row 77 note is hidden

        h.runBlocks (120); // about 2.56 beats, past the shortened clip end

        int hiddenBefore = 0, visibleBefore = 0;

        for (auto& n : h.notes)
        {
            if (n.on && n.note == 77)
                ++hiddenBefore;

            if (n.on && n.note == 65)
                ++visibleBefore;
        }

        check (hiddenBefore == 0, "hidden rows do not play while the pattern is shortened");
        check (visibleBefore >= 1, "visible rows still play after the shrink");

        q.setLength (16); // the clip now reaches 4 beats, row 12 sits at 3 beats
        h.resetEvents();
        h.runBlocks (60); // to about 3.84 beats: reaches row 12 at 3 beats

        int hiddenAfter = 0;

        for (auto& n : h.notes)
            if (n.on && n.note == 77)
                ++hiddenAfter;

        check (hiddenAfter >= 1, "enlarging the pattern plays the restored row again");
    }

    //== Pattern channel count keeps hidden columns ==========================
    {
        std::cout << "Pattern channel count keeps hidden columns\n";

        Song s;
        auto& p = s.patterns[0];
        p.setChannels (4);
        p.cell (0, 3).note = 64;
        p.cell (0, 3).inst = 3;

        p.setChannels (2);
        check (p.channels == 2, "pattern narrowed to 2 channels");
        check (p.channelCapacity() == 4, "hidden columns kept down to the last one holding data");
        check (p.rows[0].size() == 4, "the widest row still spans 4 columns");
        check (p.rows[0][3].note == 64 && p.rows[0][3].inst == 3, "hidden column data is kept");

        p.setChannels (4);
        check (p.cell (0, 3).note == 64 && p.cell (0, 3).inst == 3, "widening restores the hidden column");
        p.setChannels (2); // narrow again so the state has to carry a hidden column

        Song s2;
        s2.patterns[0].setChannels (4);
        s2.patterns[0].setChannels (1);
        check (s2.patterns[0].channelCapacity() == 1, "hidden columns holding nothing are trimmed");

        const auto tree = s.toValueTree();
        Song r;
        r.fromValueTree (tree);
        check (r.patterns[0].channels == 2, "narrow channel count restored from state");
        check (r.patterns[0].channelCapacity() == 4, "hidden channel capacity restored from state");
        r.patterns[0].setChannels (4);
        check (r.patterns[0].cell (0, 3).note == 64, "hidden column data survives a state round trip");

        Harness h;
        auto& q = h.song.patterns[0];
        q.setLength (16);
        q.setChannels (4);
        q.cell (0, 0).note = 65;
        q.cell (8, 3).note = 77; // row 8 sits at 2 beats, in the hidden 4th channel
        q.setChannels (2);

        h.runBlocks (60); // about 1.28 beats

        int hiddenBefore = 0, visibleBefore = 0;

        for (auto& n : h.notes)
        {
            if (n.on && n.note == 77)
                ++hiddenBefore;

            if (n.on && n.note == 65)
                ++visibleBefore;
        }

        check (hiddenBefore == 0, "hidden channel columns do not play");
        check (visibleBefore >= 1, "visible channel columns still play");

        q.setChannels (4);
        h.runBlocks (60); // to about 2.56 beats: reaches row 8 at 2 beats

        int hiddenAfter = 0;

        for (auto& n : h.notes)
            if (n.on && n.note == 77)
                ++hiddenAfter;

        check (hiddenAfter >= 1, "widening the pattern plays the restored column again");
    }

    //== Stuck-note guard (Reaper-style host) =================================
    {
        std::cout << "Stuck-note guard (Reaper-style host)\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (64);

        for (auto& c : h.song.clips)
            c.startBeats = 0.0;

        p.cell (0, 0).note = 60;
        p.cell (0, 0).inst = 0;
        p.cell (0, 0).vol = 100;

        h.runBlocks (94, true, true); // transport stops mid-clip: row 0 note still held

        int on1 = 0, off1 = 0;
        for (auto& n : h.notes)
            if (n.note == 60)
                n.on ? ++on1 : ++off1;

        check (on1 == 1, "playback 1 sends one note-on 60");
        check (off1 == 0, "no note-off while the transport stops inside the clip");

        // Stopped block whose output the host discards (Reaper never calls a
        // MIDI effect while stopped at all; the engine clears its own state).
        h.runBlocks (1, false, false);

        // Edit while stopped: move the clip away from song start.
        h.song.clips[0].startBeats = 8.0;
        h.ppq = 0.0;
        h.resetEvents();

        h.runBlocks (1130, true, true); // play from ppq 0 past the moved clip's end

        int on2 = 0, off2 = 0;
        for (auto& n : h.notes)
            if (n.note == 60)
                n.on ? ++on2 : ++off2;

        check (on2 == 1, "playback 2 re-triggers the moved clip first row once");
        check (off2 == 2, "a matching note-off precedes the re-trigger (stuck-key guard), plus the clip-end close");
        check (! h.notes.empty() && ! h.notes.front().on && h.notes.front().note == 60,
               "the moved clip plays with a note-off before the re-triggered note-on");
    }

    //== Transport re-arm ordering ============================================
    {
        std::cout << "Transport re-arm ordering\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (8); // two beats of rows at the default four rows per beat

        auto& c0 = p.cell (0, 0);
        c0.note = 60; // a note sitting right at the transport start
        c0.inst = 0;
        c0.vol = 100;

        p.cell (4, 0).note = 61; // a row away from that start, to play as usual
        h.song.clips[0].startBeats = 0.0;

        // reading one block's events in the order the host is handed them
        auto firstOf = [] (const Harness& hw, int kind, int pitch)
        {
            for (size_t i = 0; i < hw.raw.size(); ++i)
                if (hw.raw[i].kind == kind && (pitch < 0 || hw.raw[i].pitch == pitch))
                    return (int) i;

            return -1;
        };
        auto countAfter = [] (const Harness& hw, int kind, int pitch, int fromIndex)
        {
            int n = 0;

            for (size_t i = 0; i < hw.raw.size(); ++i)
                if ((int) i > fromIndex && hw.raw[i].kind == kind && (pitch < 0 || hw.raw[i].pitch == pitch))
                    ++n;

            return n;
        };

        // A fresh start whose first block begins right on the note row: the
        // release the re-arm sends has to reach the host before the note-on it
        // queues for the same sample, or a host honouring all-notes-off cuts it.
        h.runBlocks (1);

        int on = firstOf (h, 1, 60);
        check (on >= 0 && h.raw[on].offset == 0, "a start on the row sends its note-on at the block start");
        check (on >= 0 && countAfter (h, 2, 123, on) == 0, "no all-notes-off follows that note-on at all");
        check (countAfter (h, 2, 123, -1) >= kMidiChannelCount, "the re-arm still releases every channel ahead of the note-on");
        check (countAfter (h, 3, 120, -1) == 0, "and no All Sound Off, which has no message turning it back on");

        // Run the clip through, then wrap back to a block boundary the host left
        // inside the row at the loop start, which is what a looping region does.
        h.resetEvents();
        h.runBlocks (95); // past the clip end at two beats

        h.resetEvents();
        h.ppq = 0.125;
        h.runBlocks (1);

        on = firstOf (h, 1, 60);
        check (on >= 0 && h.raw[on].offset == 0, "the wrapped pass plays the row the start landed inside at the block start");
        check (on >= 0 && firstOf (h, 0, 60) >= 0 && firstOf (h, 0, 60) < on,
               "the stuck-key guard still puts a note-off before that re-triggered note-on");
        check (on >= 0 && countAfter (h, 2, 123, on) == 0, "the release comes before it, never after it");
        check (on >= 0 && countAfter (h, 0, 60, on) == 0, "nothing closes that note again inside the block that re-armed it");
        check (h.song.settings.midRowIgnored, "the row sits at its default while the jump re-arms it anyway");

        h.runBlocks (50); // on through the pass, past the row at one beat

        int on60 = 0, off60 = 0, on61 = 0;
        for (const auto& n : h.notes)
        {
            if (n.note == 60)
                n.on ? ++on60 : ++off60;
            else if (n.note == 61 && n.on)
                ++on61;
        }

        check (on60 == 1, "the wrapped pass re-arms that row exactly once, not twice");
        check (off60 == 2, "the guard's release before it plus the one the next row's note makes for the channel");
        check (on61 == 1, "the rows after it play normally, one note-on for the row at one beat");

        // A reported start a hair past the row rather than a block inside it.
        h.resetEvents();
        h.ppq = 1.0e-13;
        h.runBlocks (1);

        on = firstOf (h, 1, 60);
        check (on >= 0 && h.raw[on].offset == 0, "a start a hair past the row still plays it once at the block start");
        check (on >= 0 && countAfter (h, 1, 60, on) == 0, "and once only, the re-arm branch not playing it again");
        check (countAfter (h, 3, 120, -1) == 0, "the hair case sends no All Sound Off either");

        // A fresh start inside a row, which the Settings row still shapes.
        h.resetEvents();
        h.ppq = 0.125;
        h.runBlocks (1, false); // stopped, so the next block starts fresh
        h.ppq = 0.125;
        h.runBlocks (1);
        check (firstOf (h, 1, 60) < 0, "a fresh start inside a row plays nothing while the setting says to ignore one");

        h.resetEvents();
        h.ppq = 0.125;
        h.runBlocks (1, false);
        h.song.settings.midRowIgnored = false;
        h.ppq = 0.125;
        h.runBlocks (1);
        on = firstOf (h, 1, 60);
        check (on >= 0 && h.raw[on].offset == 0, "with the setting off that same fresh start plays the row at the block start");

        // A seek forward into the same row keeps the documented answer.
        h.resetEvents();
        h.song.settings.midRowIgnored = true;
        h.ppq = 0.0;
        h.runBlocks (1, false); // stopped, so the engine starts knowing 0
        h.ppq = 0.0;
        h.runBlocks (1); // one block at the start, so the next press is a seek
        h.resetEvents();
        h.ppq = 0.125;
        h.runBlocks (1);
        check (firstOf (h, 1, 60) < 0, "a seek forward into that row still honours the setting");
    }

    //== Volume + pan controllers =============================================
    {
        std::cout << "Volume + pan controllers\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (8);

        auto& c0 = p.cell (0, 0);
        c0.note = 60;
        c0.inst = 0;
        c0.vol = 127; // three decimal digits: the largest volume

        p.cell (1, 0).pan = 120;
        p.cell (2, 0).vol = 64; // volume without a note

        h.runBlocks (40);

        int cc7 = 0, cc10 = 0, velocity127 = 0;

        for (auto& cc : h.controllers)
        {
            if (cc.number == 7 && (cc.value == 127 || cc.value == 64))
                ++cc7;
            if (cc.number == 10 && cc.value == 120)
                ++cc10;
        }

        for (auto& n : h.notes)
            if (n.on && n.note == 60 && n.velocity == 127)
                ++velocity127;

        check (p.cell (0, 0).vol == 127, "volume holds a three-digit decimal value");
        check (cc7 >= 2, "both volume cells drive CC7, with and without a note");
        check (cc10 >= 1, "the pan cell drives CC10");
        check (velocity127 >= 1, "the volume also sets the note-on velocity");

        // the controllers also apply on a row that only says "stop the note"
        Harness hOff;
        auto& pOff = hOff.song.patterns[0];
        pOff.setLength (8);
        pOff.cell (0, 0).note = 60;
        pOff.cell (0, 0).inst = 0;
        pOff.cell (1, 0).note = kNoteOff;
        pOff.cell (1, 0).vol = 33;
        pOff.cell (1, 0).pan = 110;

        hOff.runBlocks (40);

        int cc7Off = 0, cc10Off = 0;

        for (auto& cc : hOff.controllers)
        {
            if (cc.number == 7 && cc.value == 33)
                ++cc7Off;
            if (cc.number == 10 && cc.value == 110)
                ++cc10Off;
        }

        check (cc7Off >= 1 && cc10Off >= 1,
               "a note-off row still applies its explicit volume and pan controllers");

        // the columns drive whatever CC the settings name
        Harness h2;
        auto& p2 = h2.song.patterns[0];
        p2.setLength (8);
        p2.cell (0, 0).note = 60;
        p2.cell (0, 0).inst = 0;
        p2.cell (0, 0).vol = 100;
        p2.cell (1, 0).pan = 30;
        h2.song.settings.volumeCc = 5;
        h2.song.settings.panCc = 11;

        h2.runBlocks (40);

        int cc5 = 0, cc11 = 0, stray = 0;
        for (auto& cc : h2.controllers)
        {
            if (cc.number == 5)
                ++cc5;
            else if (cc.number == 11)
                ++cc11;
            else if (cc.number == 7 || cc.number == 10)
                ++stray;
        }

        check (cc5 >= 1, "volume drives the configured CC5");
        check (cc11 >= 1, "pan drives the configured CC11");
        check (stray == 0, "no CC7/CC10 once the settings name other controllers");

        const auto tree = h2.song.toValueTree();
        Song back;
        back.fromValueTree (tree);
        check (back.settings.volumeCc == 5 && back.settings.panCc == 11,
               "column controller numbers survive a state round trip");

        // an older saved state, one from before the pattern paste's default row
        // was dropped, still reloads: the property nothing reads any more is
        // simply ignored, so nothing beside it is lost and the reload does not
        // carry the property on, which is what makes the older state one-way
        auto older = tree;
        if (auto st = older.getChildWithName ("SETTINGS"); st.isValid())
            st.setProperty ("pasteAsUnique", true, nullptr);

        Song then;
        then.fromValueTree (older);
        check (then.settings.volumeCc == 5 && then.settings.panCc == 11,
               "an older saved state that still carries the dropped property reloads its other settings");
        check (then.patterns.size() == back.patterns.size() && then.clips.size() == back.clips.size(),
               "and nothing beside it is lost, the pattern and clip counts unchanged");
        check (! then.toValueTree().toXmlString().contains ("pasteAsUnique"),
               "the property does not come back on a reload of it, which leaves the older state a one-way read");
    }

    //== A command argument without a command number ===========================
    {
        std::cout << "A command argument without a command number\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (8);

        p.cell (0, 0).cmd = 37;    // the column names CC37 on the first row
        p.cell (0, 0).cmdVal = 8;
        p.cell (3, 0).cmdVal = 99; // the lower row only names an argument

        h.runBlocks (40);

        int named = 0, carried = 0, pinned = 0;

        for (auto& cc : h.controllers)
        {
            if (cc.number == 37 && cc.value == 8)
                ++named;
            else if (cc.number == 37 && cc.value == 99)
                ++carried;
            else if (cc.number == 0)
                ++pinned;
        }

        check (named == 1, "the row naming the command emits CC37 with its own argument");
        check (carried == 1, "the row naming only an argument emits the carried CC37 with 99");
        check (pinned == 0, "the argument is not pinned to CC0 while the column carries one");
        check (p.cell (3, 0).cmd == kEmpty, "a value-only cell stays implicit in the model");

        // with nothing named above, the row has no command to answer, so it is
        // silent; inventing CC0 would fire a controller nobody asked for
        Harness h2;
        auto& p2 = h2.song.patterns[0];
        p2.setLength (8);
        p2.cell (0, 0).cmdVal = 55;

        h2.runBlocks (40);

        int invented = 0;

        for (auto& cc : h2.controllers)
            if (cc.number == 0 || cc.number == 55)
                ++invented;

        check (invented == 0, "an argument with nothing carried above emits no controller");

        // macros carry too, but they take no controller argument, so a value-only
        // row below one still stays silent
        Harness h3;
        auto& p3 = h3.song.patterns[0];
        p3.setLength (8);
        p3.cell (0, 0).cmd = kMacroCmdBase + 2; // M2
        p3.cell (3, 0).cmdVal = 44;

        h3.runBlocks (40);

        int fromMacro = 0;

        for (auto& cc : h3.controllers)
            if (cc.value == 44)
                ++fromMacro;

        check (fromMacro == 0, "a value-only row under a macro command emits no controller");
    }

    //== Column visibility ====================================================
    {
        std::cout << "Column visibility\n";
        Song a;
        auto& p = a.patterns[0];
        p.setChannels (3);

        p.toggleColumn (1, 1); // hide the volume of channel 1
        p.toggleColumn (1, 0); // ... and its instrument
        p.toggleColumn (2, 3); // hide the command of channel 2

        check (! p.showColumn (1, 1), "volume hidden on channel 1");
        check (p.showColumn (0, 1) && p.showColumn (2, 1), "volume stays visible on the other channels");
        check (! p.showColumn (2, 3) && p.showColumn (1, 3), "command hidden on channel 2 only");

        p.cell (0, 0).note = 60;
        p.cell (0, 1).note = 61;
        p.cell (0, 2).note = 62;

        // hidden columns keep their state through a channel resize and a state
        p.setChannels (2);
        check (p.channelCapacity() == 3, "the hidden channel is kept");

        const auto tree = a.toValueTree();
        Song b;
        b.fromValueTree (tree);

        check (b.patterns[0].channels == 2, "channel count restored");
        check (! b.patterns[0].showColumn (1, 1) && ! b.patterns[0].showColumn (1, 0),
               "channel 1 visibility restored per field");
        check (b.patterns[0].showColumn (0, 1), "channel 0 volume unaffected");
        check (b.patterns[0].rows[0][2].note == 62, "the hidden channel's cells survive too");

        b.patterns[0].setChannels (3);
        check (! b.patterns[0].showColumn (2, 3), "the hidden channel keeps its command flag");

        // another pattern has its own visibility
        auto& np = a.addPattern ("Other", 8, 3);
        check (np.showColumn (1, 1) && np.showColumn (2, 3), "new patterns show every column");

        // visibility only affects the display: hidden columns still play
        Harness h;
        auto& hp = h.song.patterns[0];
        hp.setChannels (1);
        hp.toggleColumn (0, 1);
        hp.toggleColumn (0, 2);
        hp.cell (0, 0).note = 60;
        hp.cell (0, 0).inst = 0;
        hp.cell (0, 0).vol = 90;
        hp.cell (0, 0).pan = 40;
        h.runBlocks (40);

        int cc7 = 0, cc10 = 0;
        for (auto& cc : h.controllers)
        {
            if (cc.number == 7 && cc.value == 90)
                ++cc7;
            if (cc.number == 10 && cc.value == 40)
                ++cc10;
        }

        check (cc7 >= 1 && cc10 >= 1, "hidden columns still play: visibility is display only");
    }

    //== Interpolation ========================================================
    {
        std::cout << "Interpolation\n";
        Song a;
        const int noteId = a.addPattern ("Notes", 32, 3).id;
        const int instId = a.addPattern ("Insts", 32, 2).id;
        const int cmdId = a.addPattern ("Cmds", 32, 1).id;
        const int offId = a.addPattern ("Offs", 32, 1).id;
        const int rampId = a.addPattern ("Ramp", 32, 1).id;
        const int macroId = a.addPattern ("Macros", 32, 1).id;
        const int holedId = a.addPattern ("Holed", 32, 2).id;

        auto* pn = a.findPattern (noteId);
        auto* pi = a.findPattern (instId);
        auto* pc = a.findPattern (cmdId);
        auto* po = a.findPattern (offId);
        auto* pr = a.findPattern (rampId);
        auto* pm = a.findPattern (macroId);
        auto* ph = a.findPattern (holedId);

        // the selection is a set of cells rather than a pair of row and channel
        // ranges, so the tool takes a predicate over (row, channel). A rectangle
        // is what a selection drag paints, and the masks here are those
        // rectangles written out.
        auto rect = [] (int row0, int row1, int ch0, int ch1)
        {
            return [row0, row1, ch0, ch1] (int row, int ch) noexcept
            {
                return row >= row0 && row <= row1 && ch >= ch0 && ch <= ch1;
            };
        };

        // --- only the gaps between anchors inside the range are ramped ---
        pn->cell (4, 0).note = 40; // above the range: ignored
        pn->cell (8, 0).note = 60;
        pn->cell (20, 0).note = 72;
        pn->cell (22, 0).note = 82;
        pn->cell (24, 1).note = 30;
        pn->cell (28, 1).note = 40;
        pn->cell (31, 2).note = 20; // an anchor outside the range stays as it is

        const int noteWritten = pn->interpolate (InterpField::note, rect (8, 29, 0, pn->channels - 1));
        check (noteWritten == 15, "every gap between neighbouring anchors is ramped on its own");

        check (pn->cell (7, 0).note == kEmpty && pn->cell (6, 0).note == kEmpty,
               "the ramp never reaches outside the selected rows");
        check (pn->cell (5, 0).note == kEmpty && pn->cell (4, 0).note == 40,
               "an anchor above the range is ignored and left alone");
        check (pn->cell (31, 2).note == 20 && pn->cell (30, 2).note == kEmpty,
               "an anchor below the range is ignored too");
        check (pn->cell (8, 0).note == 60 && pn->cell (20, 0).note == 72
               && pn->cell (22, 0).note == 82,
               "the anchors themselves are never rewritten");
        check (pn->cell (9, 0).note == 61 && pn->cell (14, 0).note == 66
               && pn->cell (19, 0).note == 71,
               "the blanks between two anchors take the linear position between them");
        check (pn->cell (21, 0).note == 77, "the next segment starts at the row after the anchor");
        check (pn->cell (23, 0).note == kEmpty && pn->cell (29, 0).note == kEmpty,
               "nothing is extrapolated after the last anchor");
        check (pn->cell (14, 2).note == kEmpty, "a channel without anchors in the range stays blank");
        check (pn->cell (23, 1).note == kEmpty, "a channel is not extrapolated before its own first anchor");
        check (pn->cell (25, 1).note == 33 && pn->cell (26, 1).note == 35
               && pn->cell (27, 1).note == 38,
               "each channel ramps between its own anchors, half steps round up");

        // --- a channel range: only the requested channels are touched ---
        pi->cell (10, 0).inst = 0;
        pi->cell (20, 0).inst = 31; // past the last instrument
        pi->cell (10, 1).inst = 0;
        pi->cell (20, 1).inst = 15;

        const int oneChannel = pi->interpolate (InterpField::inst, rect (10, 20, 0, 0));
        check (oneChannel == 9, "narrowing the channels fills one column only");
        check (pi->cell (15, 1).inst == kEmpty && pi->cell (19, 1).inst == kEmpty,
               "the channel outside the range is untouched");
        check (pi->cell (15, 0).inst == 8, "the anchor is clamped into the instrument range before ramping");
        check (pi->cell (20, 0).inst == 31, "the out-of-range anchor itself is left as it was");

        bool instrumentsInRange = true;
        for (int row = 11; row <= 19; ++row)
            if (pi->cell (row, 0).inst < 0 || pi->cell (row, 0).inst > rtr::kMidiChannelCount - 1)
                instrumentsInRange = false;

        check (instrumentsInRange, "filled instruments stay in the instrument range");
        check (pi->cell (11, 0).inst == 2 && pi->cell (19, 0).inst == 14,
               "the ramp climbs one instrument per row when the anchors are ten rows apart");

        const int allChannels = pi->interpolate (InterpField::inst, rect (10, 20, 0, 1));
        check (allChannels == 9, "widening the channel range fills the other column as well");
        check (pi->cell (15, 1).inst == 8, "the second column ramps the same way");

        // --- a ramp restarts after a note-off ---
        po->cell (10, 0).note = 60;
        po->cell (14, 0).note = kNoteOff;
        po->cell (20, 0).note = 72;

        check (po->interpolate (InterpField::note, rect (8, 24, 0, 0)) == 0,
               "a note-off in the way means nothing can be ramped across it");
        check (po->cell (12, 0).note == kEmpty && po->cell (17, 0).note == kEmpty,
               "the blanks on either side of a note-off stay blank");
        check (po->cell (14, 0).note == kNoteOff, "the note-off is never used as a value to write");

        po->cell (24, 0).note = 90;
        check (po->interpolate (InterpField::note, rect (8, 24, 0, 0)) == 3,
               "a segment after a note-off ramps between its own anchors");
        check (po->cell (21, 0).note == 77 && po->cell (22, 0).note == 81
               && po->cell (23, 0).note == 86,
               "the ramp restarts below the note-off");

        // --- commands and their arguments ramp apart ---
        pc->cell (10, 0).cmd = 74;
        pc->cell (20, 0).cmd = 90;
        check (pc->interpolate (InterpField::cmdNum, rect (10, 20, 0, 0)) == 9,
               "the CC number column ramps on its own");
        check (pc->cell (11, 0).cmd == 76 && pc->cell (15, 0).cmd == 82,
               "the CC number between two commands is linear too");
        check (pc->cell (15, 0).cmdVal == kEmpty,
               "ramping a CC number leaves the argument blank so it still carries");

        pc->cell (24, 0).cmd = 74;
        pc->cell (24, 0).cmdVal = 100;
        pc->cell (30, 0).cmdVal = 40;
        check (pc->interpolate (InterpField::cmdVal, rect (24, 30, 0, 0)) == 5,
               "the command argument column ramps on its own");
        check (pc->cell (25, 0).cmdVal == 90 && pc->cell (27, 0).cmdVal == 70
               && pc->cell (29, 0).cmdVal == 50,
               "the argument between two commands is linear");
        check (pc->cell (27, 0).cmd == kEmpty,
               "ramping an argument leaves the CC number unset so it carries from above");

        // --- rounding and clamping in a plain value column ---
        pr->cell (0, 0).vol = 0;
        pr->cell (2, 0).vol = 7;
        pr->cell (4, 0).vol = 127;
        pr->cell (10, 0).vol = 0;
        check (pr->interpolate (InterpField::vol, rect (0, 10, 0, 0)) == 7,
               "every one of the three segments is filled");
        check (pr->cell (1, 0).vol == 4 && pr->cell (3, 0).vol == 67,
               "a half step rounds up");
        check (pr->cell (5, 0).vol == 106 && pr->cell (6, 0).vol == 85
               && pr->cell (7, 0).vol == 64 && pr->cell (8, 0).vol == 42
               && pr->cell (9, 0).vol == 21,
               "a descending ramp rounds the same way");
        check (pr->cell (11, 0).vol == kEmpty, "nothing is written past the selected rows");

        pm->cell (0, 0).cmd = 138;
        pm->cell (10, 0).cmd = kMacroCmdBase + kMaxMacros; // past the last macro
        check (pm->interpolate (InterpField::cmdNum, rect (0, 10, 0, 0)) == 9,
               "the macro range ramps like any other number");
        check (pm->cell (1, 0).cmd == 139 && pm->cell (5, 0).cmd == 141
               && pm->cell (9, 0).cmd == 143,
               "the ramp is clamped to the last macro");
        bool macrosIntact = true;
        for (int row = 1; row <= 9; ++row)
            if (pm->cell (row, 0).cmd < kMacroCmdBase || pm->cell (row, 0).cmd > kMacroCmdBase + kMaxMacros - 1)
                macrosIntact = false;

        check (macrosIntact, "a filled command number always names a macro");

        // --- the selection is a set of cells, so it can have a hole in it ---
        ph->cell (4, 0).note = 60;
        ph->cell (10, 0).note = 70;
        ph->cell (12, 0).note = 80;
        ph->cell (4, 1).note = 60;
        ph->cell (12, 1).note = 80;

        check (ph->interpolate (InterpField::note, [] (int, int) { return false; }) == 0,
               "a selection with nothing in it writes nothing");

        // rows 4..12 of both channels, except row 11 of channel 0
        auto holedSelection = [] (int row, int ch) noexcept
        {
            if (row < 4 || row > 12)
                return false;
            return ! (ch == 0 && row == 11);
        };

        const int holed = ph->interpolate (InterpField::note, holedSelection);
        check (holed == 12, "a selection with a hole in it ramps up to the hole and stops there");

        check (ph->cell (5, 0).note == 62 && ph->cell (9, 0).note == 68,
               "the column with the hole ramps into it from above");
        check (ph->cell (11, 0).note == kEmpty, "the row left out of the selection is never written");
        check (ph->cell (11, 1).note == 78, "the neighbouring channel has no hole, so it keeps ramping");
        check (ph->cell (10, 0).note == 70 && ph->cell (12, 0).note == 80,
               "the two anchors on either side of the hole never join into one ramp");
        check (ph->cell (3, 0).note == kEmpty && ph->cell (13, 1).note == kEmpty,
               "the selection still never reaches outside itself");
    }

    //== Cell clipboard paste ================================================
    {
        std::cout << "Cell clipboard paste\n";
        Song a;
        const int pdId = a.addPattern ("Paste", 24, 2).id;
        const int wdId = a.addPattern ("Wide", 24, 3).id;
        auto* p = a.findPattern (pdId);
        auto* w = a.findPattern (wdId);

        // the tracker copies the selection into the clipboard: each entry keeps
        // its offset inside the copied rectangle, measured from the top-left
        // selected cell, and the cells left out of the selection (a Ctrl-drag
        // hole) never enter it
        auto clipOf = [] (const Pattern& pat, auto pred)
        {
            CellClipboard clip;
            int minR = kMaxRows;
            int minC = kMaxTrackerChannels;

            for (int r = 0; r < pat.length; ++r)
                for (int c = 0; c < pat.channels; ++c)
                    if (pred (r, c))
                    {
                        minR = juce::jmin (minR, r);
                        minC = juce::jmin (minC, c);
                    }

            if (minR == kMaxRows)
                return clip;

            for (int r = 0; r < pat.length; ++r)
                for (int c = 0; c < pat.channels; ++c)
                    if (pred (r, c))
                        clip.entries.push_back ({ r - minR, c - minC, pat.cell (r, c) });

            return clip;
        };

        p->cell (10, 0).note = 60;
        p->cell (10, 0).vol = 11;
        p->cell (10, 0).inst = 2;
        p->cell (10, 0).cmd = 17;
        p->cell (10, 0).cmdVal = 8;
        p->cell (12, 0).note = 62;
        p->cell (12, 0).pan = 44;
        // row 11 of channel 0 stays out of the selection: it is the hole a
        // Ctrl-drag leaves and the clipboard must not write through it
        auto holed = [] (int row, int ch) noexcept
        {
            return ch == 0 && row >= 10 && row <= 12 && row != 11;
        };

        CellClipboard clip = clipOf (*p, holed);
        check (clip.entries.size() == 2, "the clipboard holds one entry per selected cell");

        p->cell (2, 1).note = 70;
        p->cell (2, 1).pan = 50;
        p->cell (3, 1).vol = 99;

        const int written = p->pasteCells (clip, 2, 1, kPasteAll);
        check (written == 2, "the paste writes one cell per clipboard entry");
        check (p->cell (2, 1).note == 60 && p->cell (2, 1).vol == 11
                       && p->cell (2, 1).inst == 2,
                   "Everything rewrites every field the clipboard cell named");
        check (p->cell (2, 1).cmd == 17 && p->cell (2, 1).cmdVal == 8,
                   "Everything pastes the command number and its argument too");
        check (p->cell (2, 1).pan == kEmpty,
                   "a field the clipboard cell left empty clears the target, which carries again");
        check (p->cell (3, 1).vol == 99 && p->cell (3, 1).note == kEmpty,
                   "the hole inside the clipboard pastes nothing at all");
        check (p->cell (4, 1).note == 62 && p->cell (4, 1).pan == 44,
                   "every other entry keeps its place relative to the anchor");

        CellClipboard empty;
        check (p->pasteCells (empty, 0, 0, kPasteAll) == 0,
               "an empty clipboard pastes nothing");

        p->cell (14, 1).vol = 55;
        const int notesOnly = p->pasteCells (clip, 14, 1, kPasteNote);
        check (notesOnly == 2, "a single-field paste visits every entry");
        check (p->cell (14, 1).note == 60 && p->cell (14, 1).vol == 55,
               "a note paste overwrites only notes and leaves the target's volume its own");
        check (p->cell (16, 1).note == 62 && p->cell (16, 1).pan == kEmpty,
               "the pan of the lower entry is not pasted, so the target keeps its blank");

        p->cell (18, 1).cmd = 3;
        p->cell (18, 1).cmdVal = 42;
        p->pasteCells (clip, 18, 1, kPasteCmd);
        check (p->cell (18, 1).cmd == 17 && p->cell (18, 1).cmdVal == 42,
               "the Command option pastes the number and leaves the target's argument alone");

        // the same paste also reached the lower offset cell, so that one gets a
        // command of its own again before the argument-only paste is checked
        p->cell (20, 1).cmd = 9;
        p->cell (20, 1).cmdVal = 90;
        p->pasteCells (clip, 20, 1, kPasteCmdVal);
        check (p->cell (20, 1).cmd == 9 && p->cell (20, 1).cmdVal == 8,
               "the Command Value option pastes the argument and keeps the target's number");

        // --- the same clipboard additively: the mode bit drops the buffer's
        // empty fields from the mask per entry, so the target keeps whatever it
        // carried under them and an all-empty cell writes nothing at all ---
        p->cell (8, 1).note = 71;
        p->cell (8, 1).vol = 66;
        p->cell (8, 1).inst = 5;
        p->cell (8, 1).pan = 77;
        p->cell (8, 1).cmd = 21;
        p->cell (8, 1).cmdVal = 61;

        const int addAll = p->pasteCells (clip, 8, 1, kPasteAll | kPasteAdditive);
        check (addAll == 2, "an additive paste visits the entries that carry something, both of them");
        check (p->cell (8, 1).note == 60 && p->cell (8, 1).vol == 11 && p->cell (8, 1).inst == 2
                       && p->cell (8, 1).cmd == 17 && p->cell (8, 1).cmdVal == 8,
                   "the buffer's own values land on top of what the target already carried");
        check (p->cell (8, 1).pan == 77,
               "the field the clipboard cell left empty keeps the target's value rather than clearing, the additive spelling's whole meaning");
        check (p->cell (10, 1).note == 62 && p->cell (10, 1).pan == 44,
               "the lower entry keeps its offset and lands its own pan where the target was blank");

        // additively through a one-field filter, an entry whose cell does not
        // carry that field at all is skipped rather than written blank
        p->cell (8, 1).pan = 105;
        check (p->pasteCells (clip, 8, 1, kPastePan | kPasteAdditive) == 1,
               "a one-field additive paste only counts the entry that really carries that field");
        check (p->cell (8, 1).pan == 105 && p->cell (10, 1).pan == 44,
               "the entry that carries it lands and the one that does not leaves the target's own value in place");

        // an all-empty clipboard cell additively writes nothing, which is what
        // leaves the model unchanged and so leaves no point of the undo line
        CellClipboard onlyBlank;
        onlyBlank.entries = { { 0, 0, Cell() } };
        p->cell (6, 1).note = 73;
        p->cell (6, 1).pan = 91;
        check (p->pasteCells (onlyBlank, 6, 1, kPasteAll | kPasteAdditive) == 0,
               "an all-empty clipboard cell additively reports nothing written");
        check (p->cell (6, 1).note == 73 && p->cell (6, 1).pan == 91,
               "and the target keeps every value it carried, so nothing about the song changed");
        check (p->pasteCells (onlyBlank, 6, 1, kPasteAll) == 1,
               "the plain spelling still writes that one all-empty cell, which is the clearing the additive mode does without");

        // --- the wider block: two columns wide, and the last channel is at the
        // pattern's edge, so half the block does not fit ---
        w->cell (4, 1).note = 61;
        w->cell (4, 1).vol = 9;
        w->cell (4, 2).note = 63;
        w->cell (7, 2).note = 80;
        w->cell (7, 2).vol = 33;

        auto twoWide = [] (int row, int ch) noexcept
        {
            return (row == 4 || row == 5) && ch >= 1 && ch <= 2;
        };

        CellClipboard block = clipOf (*w, twoWide);
        check (block.entries.size() == 4, "selected cells that are empty go into the clipboard too");

        const int dropped = w->pasteCells (block, 6, 2, kPasteAll);
        check (dropped == 2, "the column that would stick out past the last channel is discarded");
        check (w->cell (6, 2).note == 61 && w->cell (6, 2).vol == 9,
               "the block's first column lands on the pattern's last channel");
        check (w->cell (7, 2).note == kEmpty && w->cell (7, 2).vol == kEmpty,
               "pasting an all-empty clipboard cell clears the target it lands on");
        check (w->channelCapacity() == 3, "a paste never widens the pattern");

        // --- pasting across the new end of a shortened pattern ---
        w->setLength (8);
        const int shortened = w->pasteCells (block, 7, 2, kPasteAll);
        check (shortened == 1, "rows past the shortened pattern's end are discarded");
        check (w->length == 8, "a paste never grows the pattern back");
        check (w->cell (7, 2).note == 61 && w->cell (7, 2).vol == 9,
               "the part that still fits lands at the anchor");
    }

    std::cout << (failures == 0 ? "PASS" : "FAILED") << " (" << failures << " failures)\n";
    return failures == 0 ? 0 : 1;
}
