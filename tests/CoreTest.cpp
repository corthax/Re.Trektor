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

/** Steps a playback test listens for: a macro a row calls now arrives holding
    steps that sound nothing, so a fixture that wants a shape in one says for
    itself which offsets it is asking for rather than trusting a built ramp. */
void seedSteps (Song& s, int id, int jump)
{
    for (int i = 0; i < kArpDefaultSteps; ++i)
        s.setArpStep (id, i, (std::int8_t) (i * jump));
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
    int kind; // 0 note-off, 1 note-on, 2 all-notes-off, 3 all-sound-off, 4 anything else, 5 pitch wheel
    int pitch; // the note a note event carries, otherwise the controller number, or a wheel position
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
                    else if (m.isPitchWheel ())
                        kind = 5;
                    else if (m.isNoteOn ())
                        kind = 1;
                    else if (m.isNoteOff ())
                        kind = 0;

                    if (kind == 0 || kind == 1)
                        pitch = m.getNoteNumber ();
                    else if (m.isController ())
                        pitch = m.getControllerNumber ();
                    else if (kind == 5)
                        pitch = m.getPitchWheelValue ();

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

    //== Arpeggiator macros ===================================================
    {
        std::cout << "Arpeggiator model (id 1 default, grow-only table)\n";
        Song s;
        check (s.arpDefined (1) && ! s.arpLooped (1), "the default song defines arpeggiator id 1, not looped");
        const auto* m1 = s.peekArp (1);
        check (m1 != nullptr && m1->length() == kArpDefaultSteps && m1->steps[7] == kArpEmptyStep,
               "a newly defined macro holds eight steps that sound nothing, which is what a call of it has to have entered into it");
        check (s.firstUndefArpId() == 2 && s.lastDefArpId() == 1,
               "the lowest free id and the highest defined one name the same gap");
        check (s.findArp (127) != nullptr && s.arps.size() == 127,
               "naming id 127 grows the grow-only table to the id limit");
        check (s.peekArp (0) == nullptr && s.peekArp (128) == nullptr && s.findArp (0) == nullptr,
               "id 0 is the release call and there is no macro entry above the limit");
    }
    {
        std::cout << "Arpeggiator playback (a stepped macro spilling past its row)\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (16);

        auto& c0 = p.cell (0, 0);
        c0.note = 60;
        c0.inst = 0;
        c0.vol = 100;
        c0.cmd = kArpCmd; // A
        c0.cmdVal = 1;

        // a macro is defined with steps that sound nothing, so the test says what
        // shape it wants the run to step through
        seedSteps (h.song, 1, 4);

        // a row is 6000 samples at these sizes and a step 1000: eight steps reach
        // 7000, so the two past the row's end wait for the blocks below it
        h.runBlocks (14);

        std::vector<int> ons;
        for (auto& n : h.notes)
            if (n.on)
                ons.push_back (n.note);

        check (ons.size() == 8, "the row's own note is the first step and the run sounds its steps once, got " + juce::String ((int) ons.size()));
        static const int ramp[] = { 60, 64, 68, 72, 76, 80, 84, 88 };
        check (ons == std::vector<int> (ramp, ramp + 8),
               "the steps play +4 semitones per subtick in order, spilling past the row that called them");
        check (! ons.empty() && ons.back() == 88, "the run ends with the last step's note, left held");
    }
    {
        std::cout << "Arpeggiator looped run\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (32);

        auto& c0 = p.cell (0, 0);
        c0.note = 60;
        c0.inst = 0;
        c0.vol = 100;
        c0.cmd = kArpCmd;
        c0.cmdVal = 1;
        seedSteps (h.song, 1, 4);
        h.song.setArpLooped (1, true);

        // 20 blocks reach 10240 samples: eleven steps fit before the block edge
        h.runBlocks (20);

        std::vector<int> ons;
        for (auto& n : h.notes)
            if (n.on)
                ons.push_back (n.note);

        check (ons.size() == 11, "a looped run keeps cycling while the rows below it run, got " + juce::String ((int) ons.size()));
        check (std::count (ons.begin(), ons.end(), 60) == 2, "the cycle starts over at the first step after the last");
    }
    {
        std::cout << "Finished unlooped run sustains\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (32);

        auto& c0 = p.cell (0, 0);
        c0.note = 60;
        c0.inst = 0;
        c0.vol = 100;
        c0.cmd = kArpCmd;
        c0.cmdVal = 1;
        seedSteps (h.song, 1, 4);

        h.runBlocks (30);

        std::vector<int> ons;
        for (auto& n : h.notes)
            if (n.on)
                ons.push_back (n.note);

        check (ons.size() == 8 && std::count (ons.begin(), ons.end(), 88) == 1,
               "a finished run leaves its last step's note held rather than repeating it");
    }
    {
        std::cout << "Value-only row under a carried call switches the macro\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (16);

        auto& c0 = p.cell (0, 0);
        c0.note = 60;
        c0.inst = 0;
        c0.vol = 100;
        c0.cmd = kArpCmd;
        c0.cmdVal = 1;
        seedSteps (h.song, 1, 4);

        h.song.defineArp (2);
        h.song.setArpLength (2, 4);
        h.song.setArpStep (2, 0, 12);
        h.song.setArpStep (2, 1, 24);
        h.song.setArpStep (2, 2, 36);
        h.song.setArpStep (2, 3, 48);

        p.cell (2, 0).cmdVal = 2; // implicit: the A above carries, the argument names a new macro
        p.cell (3, 0).cmdVal = 5; // an undefined id under the carried call sounds nothing

        h.runBlocks (40); // rows 2 and 3 sit at 12000 and 18000 samples

        std::vector<int> ons;
        for (auto& n : h.notes)
            if (n.on)
                ons.push_back (n.note);

        check (ons.size() == 12, "the id-only row restarts at the new macro's first step, got " + juce::String ((int) ons.size()));
        check (std::find (ons.begin(), ons.end(), 96) != ons.end() && std::find (ons.begin(), ons.end(), 108) != ons.end(),
               "the switched macro steps its own offsets from the note the column named, not the note the run above was left sounding");

        // the switch also stops the run it replaces: the note that run left
        // holding is released at the row that switched, and the host sees it
        // ahead of the new run's first step
        int offLast = -1;
        int onsAfter = 0;

        for (size_t i = 0; i < h.raw.size(); ++i)
            if (h.raw[i].kind == 0 && h.raw[i].pitch == 88)
                offLast = (int) i;

        for (size_t i = 0; i < h.raw.size(); ++i)
            if ((int) i > offLast && h.raw[i].kind == 1)
                ++onsAfter;

        check (offLast >= 0 && onsAfter == 4, "the run above the switch is stopped there, its last note released ahead of the new run's steps, got " + juce::String (onsAfter));
    }
    {
        std::cout << "A call the column has no note for sounds nothing\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (16);

        p.cell (0, 0).note = 60; // the column's note
        p.cell (0, 0).inst = 0;
        p.cell (0, 0).vol = 100;
        seedSteps (h.song, 1, 4);

        p.cell (1, 0).note = kNoteOff; // the note the column named is gone below here

        p.cell (2, 0).cmd = kArpCmd;
        p.cell (2, 0).cmdVal = 1; // a call, and the column has no note to step from

        p.cell (3, 0).note = 62; // the note that comes back in below
        p.cell (3, 0).cmdVal = 1; // the call carried onto it, now based on that note

        h.runBlocks (40); // 20480 samples reach the row at 18000 and its first steps

        std::vector<int> ons;
        for (auto& n : h.notes)
            if (n.on)
                ons.push_back (n.note);

        check (std::count (ons.begin(), ons.end(), 60) == 1 && std::count (ons.begin(), ons.end(), 64) == 0,
               "a call the column has no note for sounds nothing, so no step leans on a note a note-off took away");
        check (std::count (ons.begin(), ons.end(), 62) == 1 && std::count (ons.begin(), ons.end(), 66) == 1,
               "the run carried onto the note that entered below it steps from that note");
    }
    {
        std::cout << "A call at a row holding a note-off answers the note that row is releasing\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (16);

        p.cell (0, 0).note = 60; // the column's note
        p.cell (0, 0).inst = 0;
        p.cell (0, 0).vol = 100;
        seedSteps (h.song, 1, 4);

        p.cell (1, 0).note = kNoteOff; // the note the column named is off at this row
        p.cell (1, 0).cmd = kArpCmd;
        p.cell (1, 0).cmdVal = 1; // the call written beside it rather than below it

        h.runBlocks (40); // rows 1 and below sit from 6000 samples on

        std::vector<int> ons;
        for (auto& n : h.notes)
            if (n.on)
                ons.push_back (n.note);

        check (std::count (ons.begin(), ons.end(), 64) == 1 && std::count (ons.begin(), ons.end(), 68) == 1,
               "the call beside a note-off is answered, its steps walking from the note that row was releasing");
        check (ons.size() == 9, "the note the column named and the run's own steps all sound, got " + juce::String ((int) ons.size()));

        // the release the row named reaches the host ahead of the step that
        // answers against the note it was releasing, at the same sample
        int firstOff60 = -1;
        int secondOn60 = -1;
        int seenOn60 = 0;

        for (size_t i = 0; i < h.raw.size(); ++i)
        {
            if (h.raw[i].kind == 1 && h.raw[i].pitch == 60 && ++seenOn60 == 2)
                secondOn60 = (int) i;

            if (firstOff60 < 0 && h.raw[i].kind == 0 && h.raw[i].pitch == 60)
                firstOff60 = (int) i;
        }

        check (firstOff60 >= 0 && secondOn60 > firstOff60,
               "the note-off arrives as a note-off, ordered ahead of the run's first note-on at that sample");
    }
    {
        std::cout << "A 0 releases the note the run holds\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (8);

        auto& c0 = p.cell (0, 0);
        c0.note = 60;
        c0.inst = 0;
        c0.vol = 100;
        c0.cmd = kArpCmd;
        c0.cmdVal = 1;
        seedSteps (h.song, 1, 4);
        h.song.setArpLooped (1, true);

        p.cell (3, 0).cmdVal = 0; // the carried call with the id 0 argument releases

        h.runBlocks (48); // the clip is eight rows, so the kill is well inside it

        int onCount = 0;
        int off64 = 0;
        int soundOff = 0;

        for (auto& n : h.notes)
        {
            if (n.on)
                ++onCount;
            else if (n.note == 64)
                ++off64;
        }

        for (auto& e : h.raw)
            if (e.kind == 3)
                ++soundOff;

        // the steps at 0..17000 samples play (18); the looped run would carry
        // on below the kill if the call did not end it; the guard's stuck-key
        // releases add a note-off of 64 at each re-struck step on top of the
        // release at the kill itself
        check (onCount == 18, "the kill stops the run at its row, got " + juce::String (onCount));
        check (off64 == 5, "the note the last step held is released at the kill along the re-articulations");
        check (soundOff == 0, "the release reaches the synth as a plain note-off, not as an All Sound Off");
    }
    {
        std::cout << "The release leaves the column carrying no call\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (16);

        auto& c0 = p.cell (0, 0);
        c0.note = 60;
        c0.inst = 0;
        c0.vol = 100;
        c0.cmd = kArpCmd;
        c0.cmdVal = 1;
        seedSteps (h.song, 1, 4);
        h.song.setArpLooped (1, true); // the run would keep cycling past its last step

        h.song.defineArp (2);
        h.song.setArpLength (2, 2);
        h.song.setArpStep (2, 0, 36);
        h.song.setArpStep (2, 1, 40);

        p.cell (2, 0).cmdVal = 0; // the carried call with the id 0 argument: the release
        p.cell (3, 0).cmdVal = 2; // only an argument, below a call that is no longer in force

        h.runBlocks (40); // the rows below the release are reached well inside it

        std::vector<int> ons;
        for (auto& n : h.notes)
            if (n.on)
                ons.push_back (n.note);

        check (std::find (ons.begin(), ons.end(), 96) == ons.end() && std::find (ons.begin(), ons.end(), 100) == ons.end(),
               "a row naming only an argument under a release answers nothing, so the macro it would have switched to stays unplayed");
        check (ons.size() == 12, "the release stops the run at its row and nothing restarts below it, got " + juce::String ((int) ons.size()));
    }
    {
        std::cout << "Empty steps sustain and out-of-range steps snap by octaves\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (16);

        h.song.defineArp (2);
        h.song.setArpLength (2, 4);
        h.song.setArpStep (2, 0, 0);
        h.song.setArpStep (2, 1, 4);
        h.song.setArpStep (2, 2, (std::int8_t) kArpEmptyStep);
        h.song.setArpStep (2, 3, 8);

        auto& c0 = p.cell (0, 0);
        c0.note = 60;
        c0.inst = 0;
        c0.vol = 100;
        c0.cmd = kArpCmd;
        c0.cmdVal = 2;

        h.runBlocks (8); // 4096 samples reach the +8 step at 3000

        std::vector<int> ons;
        for (auto& n : h.notes)
            if (n.on)
                ons.push_back (n.note);

        check (ons.size() == 3, "the empty step sounds nothing, got " + juce::String ((int) ons.size()));
        check (std::find (ons.begin(), ons.end(), 64) != ons.end() && std::find (ons.begin(), ons.end(), 68) != ons.end(),
               "the empty step holds the note before it and the next step sounds from its own subtick");

        Harness h2;
        auto& p2 = h2.song.patterns[0];
        p2.setLength (16);
        p2.cell (0, 0).note = 60;
        p2.cell (0, 0).inst = 0;
        p2.cell (0, 0).vol = 100;

        h2.song.defineArp (3);
        h2.song.setArpLength (3, 1);
        h2.song.setArpStep (3, 0, 100);
        p2.cell (1, 0).cmd = kArpCmd;
        p2.cell (1, 0).cmdVal = 3; // no note of its own: the run steps from the note above

        h2.runBlocks (16);

        std::vector<int> ons2;
        for (auto& n : h2.notes)
            if (n.on)
                ons2.push_back (n.note);

        check (std::find (ons2.begin(), ons2.end(), 124) != ons2.end(), "a step above the keyboard's top snaps back by whole octaves");

        Harness h3;
        auto& p3 = h3.song.patterns[0];
        p3.setLength (16);
        p3.cell (0, 0).note = 60;
        p3.cell (0, 0).inst = 0;
        p3.cell (0, 0).vol = 100;

        h3.song.defineArp (4);
        h3.song.setArpLength (4, 1);
        h3.song.setArpStep (4, 0, -100);
        p3.cell (1, 0).cmd = kArpCmd;
        p3.cell (1, 0).cmdVal = 4;

        h3.runBlocks (16);

        std::vector<int> ons3;
        for (auto& n : h3.notes)
            if (n.on)
                ons3.push_back (n.note);

        check (std::find (ons3.begin(), ons3.end(), 8) != ons3.end(), "a step below the bottom snaps up by whole octaves");
    }
    {
        std::cout << "Note-off steps open what the run was holding\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (16);

        auto& c0 = p.cell (0, 0);
        c0.note = 60;
        c0.inst = 0;
        c0.vol = 100;
        c0.cmd = kArpCmd;
        c0.cmdVal = 2;

        h.song.defineArp (2);
        h.song.setArpLength (2, 5);
        h.song.setArpStep (2, 0, 4);
        h.song.setArpStep (2, 1, 8);
        h.song.setArpStep (2, 2, kArpNoteOffStep); // a step that opens the note it would have held
        h.song.setArpStep (2, 3, 12);
        h.song.setArpStep (2, 4, kArpNoteOffStep);

        h.runBlocks (10); // the row sits at 0, so its steps reach 4000 samples

        std::vector<int> ons;
        int on68 = -1;
        int off68 = -1;
        int on72 = -1;
        int off72 = -1;
        int offs68 = 0;
        int offs72 = 0;

        for (auto& n : h.notes)
        {
            if (n.on)
                ons.push_back (n.note);
            else if (n.note == 68)
                ++offs68;
            else if (n.note == 72)
                ++offs72;
        }

        for (int i = 0; i < (int) h.raw.size(); ++i)
        {
            const auto& e = h.raw[i];

            if (e.pitch == 68 && e.kind == 1 && on68 < 0)
                on68 = i;
            else if (e.pitch == 68 && e.kind == 0 && off68 < 0)
                off68 = i;
            else if (e.pitch == 72 && e.kind == 1 && on72 < 0)
                on72 = i;
            else if (e.pitch == 72 && e.kind == 0 && off72 < 0)
                off72 = i;
        }

        check (ons.size() == 3, "a note-off step sounds nothing while the stepped steps do, got " + juce::String ((int) ons.size()));
        check (std::count (ons.begin(), ons.end(), 68) == 1 && std::count (ons.begin(), ons.end(), 72) == 1,
               "the steps either side of a note-off step land on their own pitches, so the run keeps its step count");
        check (on68 >= 0 && off68 > on68 && on72 > off68,
               "the note the run was holding opens at the note-off step's own subtick, ahead of the step that sounds the next one");
        check (on72 >= 0 && off72 > on72 && offs68 == 1 && offs72 == 1,
               "and the note the run stepped to opens at the next note-off step, once each, rather than at whatever ends the run below");
    }
    {
        std::cout << "A note-off step at a run's head and its tail\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (2); // the clip is two rows: 12000 samples

        p.cell (0, 0).note = 60; // the column's note
        p.cell (0, 0).inst = 0;
        p.cell (0, 0).vol = 100;
        p.cell (1, 0).cmd = kArpCmd; // a call with no note of its own, one row below
        p.cell (1, 0).cmdVal = 2;

        h.song.defineArp (2);
        h.song.setArpLength (2, 3);
        h.song.setArpStep (2, 0, kArpNoteOffStep); // a head step with nothing of its own to open
        h.song.setArpStep (2, 1, 4);
        h.song.setArpStep (2, 2, kArpNoteOffStep); // a tail step that opens its own note

        h.runBlocks (24); // 12288 samples: the clip end at 12000 sits inside this run

        std::vector<int> ons;
        int offs60 = 0;
        int offs64 = 0;
        int off60 = -1;
        int on64 = -1;
        int off64 = -1;

        for (auto& n : h.notes)
        {
            if (n.on)
                ons.push_back (n.note);
            else if (n.note == 60)
                ++offs60;
            else if (n.note == 64)
                ++offs64;
        }

        for (int i = 0; i < (int) h.raw.size(); ++i)
        {
            const auto& e = h.raw[i];

            if (e.pitch == 60 && e.kind == 0 && off60 < 0)
                off60 = i;
            else if (e.pitch == 64 && e.kind == 1 && on64 < 0)
                on64 = i;
            else if (e.pitch == 64 && e.kind == 0 && off64 < 0)
                off64 = i;
        }

        check (ons.size() == 2, "the head and tail note-off steps sound nothing, so only the note the column named and the stepped step sound, got " + juce::String ((int) ons.size()));
        check (off60 >= 0 && on64 == off60 + 1, "a head note-off step opens nothing of its own, so the note the column named opens right ahead of the note the stepped step sounds");
        check (on64 >= 0 && off64 > on64 && offs64 == 1 && offs60 == 1, "the tail note-off step opens the note its own step entered once, and the clip end has nothing left to release, got " + juce::String (offs64 + offs60));
    }
    {
        std::cout << "A looped cycle steps through its note-off steps\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (16);

        auto& c0 = p.cell (0, 0);
        c0.note = 60;
        c0.inst = 0;
        c0.vol = 100;
        c0.cmd = kArpCmd;
        c0.cmdVal = 2;

        h.song.defineArp (2);
        h.song.setArpLength (2, 3);
        h.song.setArpStep (2, 0, 4);
        h.song.setArpStep (2, 1, kArpNoteOffStep);
        h.song.setArpStep (2, 2, 8);
        h.song.setArpLooped (2, true);

        h.runBlocks (10); // 5120 samples: the cycle reaches its third pass before 5000

        std::vector<int> ons;
        int on64 = -1;
        int off64 = -1;
        int nextOn = -1;
        int offs64 = 0;

        for (auto& n : h.notes)
            if (n.on)
                ons.push_back (n.note);

        for (int i = 0; i < (int) h.raw.size(); ++i)
        {
            const auto& e = h.raw[i];

            if (e.pitch == 64 && e.kind == 1 && on64 < 0)
                on64 = i;
            else if (e.pitch == 64 && e.kind == 0)
            {
                ++offs64;

                if (off64 < 0)
                    off64 = i;
            }
            else if (e.kind == 1 && on64 >= 0 && off64 >= 0 && nextOn < 0)
                nextOn = i;
        }

        check (std::count (ons.begin(), ons.end(), 68) == 2 && std::count (ons.begin(), ons.end(), 64) == 1,
               "the cycle passes its last step and back to its first, and a note-off step in the cycle opens what the cycle was holding there rather than ending the run, got " + juce::String ((int) ons.size()));
        check (on64 >= 0 && off64 > on64 && offs64 == 1 && nextOn > off64,
               "the note the second pass stepped to is opened at the note-off step inside that pass, and the cycle steps on again past it");
    }
    {
        std::cout << "The clip end closes a looping run\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (2); // the clip is two rows: 12000 samples

        auto& c0 = p.cell (0, 0);
        c0.note = 60;
        c0.inst = 0;
        c0.vol = 100;
        c0.cmd = kArpCmd;
        c0.cmdVal = 1;
        seedSteps (h.song, 1, 4);
        h.song.setArpLooped (1, true);

        h.runBlocks (24); // 12288 samples: the clip end at 12000 sits inside this run

        std::vector<int> ons;
        int off72 = 0;
        for (auto& n : h.notes)
        {
            if (n.on)
                ons.push_back (n.note);
            else if (n.note == 72)
                ++off72;
        }

        check (ons.size() == 12, "the looping run stops where the clip ends, not at the row that called it");
        check (off72 == 3, "the note the last step left is released at the clip end");
    }
    {
        std::cout << "Arpeggiator state round trip and the old format\n";
        Song a;
        a.defineArp (2);
        a.setArpStep (2, 0, -7);
        a.setArpLength (2, 64); // keeps the step already entered
        a.setArpLooped (2, true);
        a.setArpStep (2, 63, -40);
        a.setArpStep (2, 40, kArpNoteOffStep); // a step that opens what the run holds there

        check (a.findArp (2) != nullptr && a.findArp (2)->length() == 64 && a.findArp (2)->steps[0] == -7,
               "growing a macro keeps the steps already entered");
        check (a.findArp (2)->steps[1] == kArpEmptyStep && a.findArp (2)->steps[62] == kArpEmptyStep,
               "and the steps growing adds sound nothing, so the macro's shape is only ever where it was entered");
        check (a.findArp (2)->steps[40] == kArpNoteOffStep,
               "a step below every jump a step can name is kept as it is, so a macro can hold a note-off");

        Song b;
        b.fromValueTree (a.toValueTree());
        check (b.arpDefined (2) && b.arpLooped (2) && b.findArp (2)->length() == 64 && b.findArp (2)->steps[63] == -40,
               "the id, the loop flag and all 64 signed steps reload from the state");
        check (b.findArp (2)->steps[40] == kArpNoteOffStep && b.findArp (2)->steps[41] == kArpEmptyStep,
               "a note-off step reloads from the saved form, which is why the block is two bytes a step");
        check (b.arpDefined (1), "the default macro survives a round trip as the default macro");

        // what an older build saved is one byte a step: its jumps and its step
        // that sounds nothing all fit inside that byte, so reading those blocks
        // as pairs would give half as many steps, and the loader has the step
        // count beside the block to tell the two spellings apart
        juce::ValueTree oldRoot ("RETREKTOR");
        juce::ValueTree oldArps ("ARPS");
        juce::ValueTree oldArp ("ARP");
        oldArp.setProperty ("id", 3, nullptr);
        oldArp.setProperty ("loop", true, nullptr);
        oldArp.setProperty ("len", 4, nullptr);

        juce::MemoryBlock older;
        older.setSize (4);
        auto* ob = static_cast<std::uint8_t*> (older.getData());
        ob[0] = 4;
        ob[1] = (std::uint8_t) kArpEmptyStep;
        ob[2] = (std::uint8_t) -6; // a jump an older build could write below the base note
        ob[3] = 12;
        oldArp.setProperty ("steps", older.toBase64Encoding(), nullptr);
        oldArps.appendChild (oldArp, nullptr);
        oldRoot.appendChild (oldArps, nullptr);

        Song o;
        o.fromValueTree (oldRoot);
        check (o.arpDefined (3) && o.arpLooped (3) && o.findArp (3) != nullptr
                   && o.findArp (3)->length() == 4,
               "an older state's one-byte block comes back with as many steps as it says it has");
        check (o.findArp (3) != nullptr && o.findArp (3)->steps[0] == 4
                   && o.findArp (3)->steps[2] == -6 && o.findArp (3)->steps[3] == 12,
               "widened by sign, so the jumps it could write keep their own values and their sides");
        check (o.findArp (3) != nullptr && o.findArp (3)->steps[1] == kArpEmptyStep,
               "and its step that sounds nothing keeps the place it had rather than leaning on a pair");

        // what an undo hands back is the song the changes are in rather than a
        // fresh one, so the read has to drop what that song holds ahead of the
        // state it is given: an undo of a step, of the loop flag or of defining
        // a macro at all has to be able to take each of them back
        Song kept;
        const auto keptXml = kept.toValueTree().toXmlString();

        kept.setArpStep (1, 0, 100);
        kept.setArpLooped (1, true);
        kept.defineArp (2);

        kept.fromValueTree (juce::ValueTree::fromXml (keptXml));
        check (kept.arpDefined (1) && kept.findArp (1) != nullptr
                   && kept.findArp (1)->length() == kArpDefaultSteps && kept.findArp (1)->steps[0] == kArpEmptyStep,
               "an earlier state's steps replace the ones the macro had kept rather than being added behind them");
        check (! kept.arpLooped (1), "and the loop flag reloads as the state has it, unset");
        check (! kept.arpDefined (2) && kept.lastDefArpId() == 1,
               "and a macro the state does not name is taken back, which is what makes undoing the definition of one work");

        Song c;
        auto vt = c.toValueTree();
        juce::ValueTree oldSec ("MACROS");
        oldSec.appendChild (juce::ValueTree ("MACRO"), nullptr);
        vt.appendChild (oldSec, nullptr);
        Song d;
        d.fromValueTree (vt);
        check (d.arpDefined (1) && d.lastDefArpId() == 1,
               "an older state's MACROS section is dropped outright and the default macro comes back");

        Song e;
        e.patterns[0].cell (5, 0).cmd = 140; // an older row calling M12
        e.patterns[0].cell (5, 0).cmdVal = 12;
        Song f;
        f.fromValueTree (e.toValueTree());
        check (f.patterns[0].cell (5, 0).cmd == kArpCmd && f.patterns[0].cell (5, 0).cmdVal == 12,
               "an older row's macro call lands on A with its old argument naming the id");

        Song g;
        g.clearArp (1);
        check (! g.arpDefined (1), "clearing a macro leaves it undefined, so no row can call it");
        g.defineArp (1);
        check (g.arpDefined (1) && ! g.arpLooped (1), "defining it again restores eight steps that sound nothing, not looped");
        g.setArpLength (1, 100);
        check (g.findArp (1)->length() == kMaxArpSteps, "asking past the step limit gives the limit");
        g.setArpLength (1, 0);
        check (g.findArp (1)->length() == 1, "asking for no steps still leaves one, so the macro stays defined");
    }

    //== Pitch bend ===========================================================
    {
        std::cout << "Pitch bend model (the wheel position an argument names)\n";
        check (kBendCmd > 143, "the bend letter's command value sits above the values an older build could write for its macro kinds, so no older row can already be a bend");
        check (kBendCentreVal * kBendStep == kBendCentrePos,
               "the argument's centre names the wheel's exact centre rather than a position a step off it");

        Song s;
        s.patterns[0].cell (3, 0).cmd = kBendCmd;
        s.patterns[0].cell (3, 0).cmdVal = 96;
        Song r;
        r.fromValueTree (s.toValueTree());
        check (r.patterns[0].cell (3, 0).cmd == kBendCmd && r.patterns[0].cell (3, 0).cmdVal == 96,
               "the bend letter and its argument survive the saved form unchanged");

        Song o;
        o.patterns[0].cell (0, 0).cmd = 128;
        o.patterns[0].cell (1, 0).cmd = 140;
        o.patterns[0].cell (2, 0).cmd = 143; // an older build's M0, M12 and M15 calls
        Song q;
        q.fromValueTree (o.toValueTree());
        check (q.patterns[0].cell (0, 0).cmd == kArpCmd && q.patterns[0].cell (1, 0).cmd == kArpCmd
               && q.patterns[0].cell (2, 0).cmd == kArpCmd,
               "an older state's macro calls at both ends of that range land on the call letter rather than the new bend letter");
    }
    {
        std::cout << "Pitch bend playback (the standard pair, ahead of its own row's note)\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (16);

        auto& c0 = p.cell (0, 0);
        c0.note = 60;
        c0.inst = 2;
        c0.vol = 100;
        c0.cmd = kBendCmd;
        c0.cmdVal = 96; // the wheel's upper quarter: 96 read in steps of 128

        h.runBlocks (4);

        check (h.bends.size() == 1 && h.bends[0] == 96 * kBendStep,
               "the row moves the wheel once, to 12288, and the pair the host gets is the standard pitch wheel message");
        check (h.notes.size() == 1 && h.notes[0].on && h.notes[0].channel == 3,
               "the note of that row is played on the channel its instrument names, like every other command row");
        // the host's own re-arm release sits ahead of everything the first block
        // plays, so the ordering check names the two events rather than their
        // places in the stream
        auto indexOfKind = [] (const Harness& hw, int kind)
        {
            for (size_t i = 0; i < hw.raw.size(); ++i)
                if (hw.raw[i].kind == kind)
                    return (int) i;

            return -1;
        };

        const int wheelAt = indexOfKind (h, 5);
        const int onAt = indexOfKind (h, 1);
        check (wheelAt >= 0 && onAt > wheelAt && h.raw[wheelAt].offset == h.raw[onAt].offset,
               "the wheel moves before the note-on of the same row, so the note that row strikes is bent from its start");
    }
    {
        std::cout << "Pitch bend carried down a column and released by a note-off\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (16);

        auto& c0 = p.cell (0, 0);
        c0.note = 60;
        c0.inst = 0;
        c0.vol = 100;
        c0.cmd = kBendCmd;
        c0.cmdVal = 96;

        p.cell (4, 0).note = 61; // a note further down the column, under no row of its own
        p.cell (8, 0).note = kNoteOff;

        // 94 blocks reach 2.005 ppq, past the row 8 that sits at 2.0
        h.runBlocks (94);

        check (h.bends.size() == 2 && h.bends[0] == 12288 && h.bends[1] == kBendCentrePos,
               "the wheel is moved once and returns only at the note-off, so nothing straightens the notes in between");
        check (h.notes.size() == 4 && h.notes[1].on == false && h.notes[3].on == false,
               "the channel releases the first note when the second enters and releases the second at the note-off row");
        check (h.notes[3].channel == 1 && h.notes[3].note == 61,
               "the note-off that takes the wheel back releases the note the channel was holding, on the channel its instrument named");
    }
    {
        std::cout << "Pitch bend argument rows under a carried letter\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (16);

        auto& c0 = p.cell (0, 0);
        c0.note = 60;
        c0.inst = 0;
        c0.vol = 100;
        c0.cmd = kBendCmd;
        c0.cmdVal = 96;

        p.cell (2, 0).cmdVal = 32; // the letter carries, so this is a point on the same wheel
        p.cell (4, 0).cmdVal = 0;  // its bottom
        p.cell (6, 0).note = kNoteOff;
        p.cell (8, 0).cmdVal = 96; // re-asserted, the letter still being in force

        h.runBlocks (94);

        static const int wheel[] = { 12288, 4096, 0, kBendCentrePos, 12288 };
        check (h.bends.size() == 5 && h.bends == std::vector<int> (wheel, wheel + 5),
               "each argument row moves the same wheel in turn, a note-off returns it to the centre, and a further argument puts the bend back");
    }
    {
        std::cout << "Pitch bend of a row without a note of its own\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (16);

        auto& c0 = p.cell (0, 0);
        c0.inst = 1;
        c0.vol = 100;
        c0.cmd = kBendCmd;
        c0.cmdVal = 96; // the row asks for a point on the wheel and strikes nothing

        // the clip is 16 rows long at four rows a beat: its end sits at 4.0 ppq,
        // which block 187 straddles
        h.runBlocks (180);
        check (h.bends.size() == 1 && h.bends[0] == 12288,
                "the wheel stays bent through the clip's own rows even though no note of that channel ever played");

        h.runBlocks (8);

        check (h.bends.size() == 2 && h.bends[1] == kBendCentrePos,
               "the clip's own end takes that wheel back, which is what a release point cannot do for a row that never struck a note");
        check (h.notes.empty (), "nothing of that channel was ever playing, so only the clip's end could put the wheel right");

        bool channelNamed = false;
        for (const auto& e : h.controllers)
            if (e.channel == 2 && e.number == 7 && e.value == 100)
                channelNamed = true;

        check (channelNamed, "the row's instrument names the MIDI channel its command reaches, which is also where the take-back goes");
    }

    //== Pitch slide ==========================================================
    {
        std::cout << "Pitch slide model (an argument naming rows rather than a point)\n";
        check (kSlideUpCmd > kOlderCmdTop && kSlideDownCmd > kOlderCmdTop,
               "the two slide letters' command values sit above the highest value an older build could write, so no older row can already be a slide");
        check (kSlideUpCmd > kCcTop && kSlideDownCmd > kCcTop,
               "they sit above the highest plain CC the number field can hold, so a typed digit run cannot reach either of them");

        Song s;
        s.patterns[0].cell (3, 0).cmd = kSlideUpCmd;
        s.patterns[0].cell (3, 0).cmdVal = 12;
        s.patterns[0].cell (4, 0).cmd = kSlideDownCmd;
        s.patterns[0].cell (4, 0).cmdVal = 3;
        Song r;
        r.fromValueTree (s.toValueTree());
        check (r.patterns[0].cell (3, 0).cmd == kSlideUpCmd && r.patterns[0].cell (3, 0).cmdVal == 12
               && r.patterns[0].cell (4, 0).cmd == kSlideDownCmd && r.patterns[0].cell (4, 0).cmdVal == 3,
               "both slide letters and their row counts survive the saved form unchanged");

        Song o;
        o.patterns[0].cell (0, 0).cmd = 140; // an older build's M12 call
        o.patterns[0].cell (1, 0).cmd = kOlderCmdTop; // its M15, the highest value that build could write
        o.patterns[0].cell (2, 0).cmd = kBendCmd;
        o.patterns[0].cell (3, 0).cmd = kSlideUpCmd;
        o.patterns[0].cell (4, 0).cmd = kSlideDownCmd;
        Song q;
        q.fromValueTree (o.toValueTree());
        check (q.patterns[0].cell (0, 0).cmd == kArpCmd && q.patterns[0].cell (1, 0).cmd == kArpCmd,
               "an older state's macro calls at either end of its range land on the arpeggiator letter rather than turning into a bend or a slide");
        check (q.patterns[0].cell (2, 0).cmd == kBendCmd && q.patterns[0].cell (3, 0).cmd == kSlideUpCmd
               && q.patterns[0].cell (4, 0).cmd == kSlideDownCmd,
               "this build's own three wheel values read back as themselves, the letters being the only way to name them");
    }
    {
        std::cout << "Pitch slide up (the wheel climbing to its top over the rows its argument names)\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (16);

        auto& c0 = p.cell (0, 0);
        c0.note = 60;
        c0.inst = 0;
        c0.vol = 100;
        c0.cmd = kSlideUpCmd;
        c0.cmdVal = 4; // four rows to reach the wheel's top

        p.cell (8, 0).note = kNoteOff;

        // 94 blocks reach 2.005 ppq, past the row 8 that sits at 2.0
        h.runBlocks (94);

        // from the centre 8192 toward the top 16383 over the four rows the
        // argument names, in six sub-row steps each rather than one per row: the
        // whole 8191 the two are apart is walked in steps of 341, and the row
        // boundaries keep the whole-row values they had, 10239, 12287, 14335 and
        // the top itself, with the steps between them filling in the straight line
        static const int wheel[] = { 8533, 8874, 9215, 9557, 9898, 10239, 10581, 10922, 11263, 11604,
                                     11946, 12287, 12628, 12970, 13311, 13652, 13993, 14335, 14676, 15017,
                                     15359, 15700, 16041, kBendTopPos, kBendCentrePos };
        check (h.bends.size() == 25 && h.bends == std::vector<int> (wheel, wheel + 25),
               "the wheel moves once on each sub-row over the four rows under the one that named the slide, lands on its top exactly, and returns only at the note-off below");
        check (h.notes.size() == 2 && h.notes[0].on && h.notes[1].on == false && h.notes[1].note == 60,
               "the note that row struck is held until the note-off releases it, the slide being a state the column carries rather than one that ends with the row");

        auto indexOfKind = [] (const Harness& hw, int kind)
        {
            for (size_t i = 0; i < hw.raw.size(); ++i)
                if (hw.raw[i].kind == kind)
                    return (int) i;

            return -1;
        };

        const int onAt = indexOfKind (h, 1);
        const int wheelAt = indexOfKind (h, 5);
        check (onAt >= 0 && wheelAt > onAt && h.raw[onAt].offset < h.raw[wheelAt].offset,
               "the row that named the slide sends no message of its own, so the note it struck is played straight and the wheel first moves on a sub-row of that same row rather than a row below it");
    }
    {
        std::cout << "Pitch slide down (an argument of 0 reaching the wheel's bottom on its own row)\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (16);

        auto& c0 = p.cell (2, 0);
        c0.note = 60;
        c0.inst = 0;
        c0.vol = 100;
        c0.cmd = kSlideDownCmd;
        c0.cmdVal = 0; // no rows to travel over: the wheel jumps to the bottom here

        p.cell (6, 0).note = 61; // a note further down, played at the bottom it jumped to
        p.cell (8, 0).note = kNoteOff;

        // 94 blocks reach 2.005 ppq, past the row 8 that sits at 2.0
        h.runBlocks (94);

        check (h.bends.size() == 2 && h.bends[0] == kBendBottomPos && h.bends[1] == kBendCentrePos,
               "an argument of 0 puts the wheel at its bottom on the naming row itself, and a note-off below takes it back");
        check (h.notes.size() == 4 && h.notes[1].on == false && h.notes[3].on == false,
               "the two notes of the column are released as usual, so a slide that reached its extreme leaves the wheel held like a bend does");
    }
    {
        std::cout << "Pitch slide retargeted by argument rows under a carried letter\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (16);

        auto& c0 = p.cell (0, 0);
        c0.note = 60;
        c0.inst = 0;
        c0.vol = 100;
        c0.cmd = kSlideUpCmd;
        c0.cmdVal = 2; // two rows toward the wheel's top

        p.cell (1, 0).cmdVal = 1; // the letter carries, so this is a new span of one from where the sub-rows got to
        p.cell (3, 0).cmdVal = 1; // and a span of one over a wheel already at the top, which names nothing

        p.cell (8, 0).note = kNoteOff;

        h.runBlocks (94);

        // one sub-row step of two rows over the centre is 8191/12, so the row that
        // named only an argument re-arms over the wheel sitting at 11604, the point
        // its sub-rows had reached rather than the point a whole row reaches for:
        // its own steps climb 4779 in six, 12400, 13197, 13993, 14790 and 15586,
        // and the top itself is the step sitting one row below the one that named it
        static const int wheel[] = { 8874, 9557, 10239, 10922, 11604, 12400, 13197,
                                     13993, 14790, 15586, kBendTopPos, kBendCentrePos };
        check (h.bends.size() == 12 && h.bends == std::vector<int> (wheel, wheel + 12),
               "the row that named only an argument retargets the span from where the wheel had got to mid-row, a further one over a wheel already at the extreme moves nothing, and the note-off still takes the wheel back");
        check (h.notes.size() == 2 && h.notes[0].on && h.notes[1].on == false,
               "the rows naming only an argument still belong to the letter above, the note the column struck being held across them");
    }
    {
        std::cout << "Pitch slide a note-off a row below ends partway down its ramp\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (16);

        auto& c0 = p.cell (0, 0);
        c0.note = 60;
        c0.inst = 0;
        c0.vol = 100;
        c0.cmd = kSlideDownCmd;
        c0.cmdVal = 4; // four rows toward the wheel's bottom

        p.cell (1, 0).note = kNoteOff; // the ramp's steps over this row had not all run
        p.cell (4, 0).note = 61; // and a note below it, played straight

        h.runBlocks (94);

        // the note-off sits a row below the one that named the slide, so the sub-row
        // steps inside that row had already moved the wheel before it: the wheel is
        // taken back from where its steps had got to rather than from a whole-row
        // point, which is what a climb of four rows in twenty-four steps lands on
        static const int wheel[] = { 7851, 7510, 7168, 6827, 6486, kBendCentrePos };
        check (h.bends == std::vector<int> (wheel, wheel + 6),
               "the note-off ends the slide partway down its ramp, taking the wheel back from the point its sub-rows had reached and leaving nothing climbing under it");
        check (h.notes.size() == 3 && h.notes[0].on && h.notes[1].on == false && h.notes[2].on,
               "the column's own note-offs still release what it entered, and the note below plays against a wheel left at the centre");
    }
    {
        std::cout << "A slide the clip runs out of, taken back at that clip's end\n";
        Harness h;
        auto& p = h.song.patterns[0];
        p.setLength (4); // the clip ends at 1.0 ppq, four rows below its first one

        auto& c0 = p.cell (0, 0);
        c0.inst = 0;
        c0.vol = 100;
        c0.cmd = kSlideUpCmd;
        c0.cmdVal = 8; // a slide wider than the clip that named it, striking no note of its own

        // block 46 straddles 1.0 ppq, the clip's own end
        h.runBlocks (47);

        // three of the rows the eight it asks for fit inside a four-row clip, and
        // the sub-row steps inside them are six times as many: every step below the
        // clip's own end sounds, 8362, 8533 and so on up to 12116 at the step a hair
        // above the clip's end, and the clip's end takes the wheel back
        static const int wheel[] = { 8362, 8533, 8703, 8874, 9045, 9215, 9386, 9557, 9727, 9898, 10069,
                                     10239, 10410, 10581, 10751, 10922, 11092, 11263, 11434, 11604, 11775,
                                     11946, 12116, kBendCentrePos };
        check (h.bends.size() == 24 && h.bends == std::vector<int> (wheel, wheel + 24),
               "the wheel climbs over every sub-row of the rows the clip has and its end takes it back, which is what a slide that never reached its extreme leaves no wheel bent for the clip below");
        check (h.notes.empty (), "no note of that channel was ever playing, so only the clip's end could put the wheel right");

        bool channelNamed = false;
        for (const auto& e : h.controllers)
            if (e.channel == 1 && e.number == 7 && e.value == 100)
                channelNamed = true;

        check (channelNamed, "the row's instrument names the MIDI channel its command reaches, which is also where the take-back goes");
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
        a.settings.midRowIgnored = false;
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
        check (b.settings.midRowIgnored == false, "the mid-row switch restored");
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
        // was dropped and before this build dropped the hex display row and its
        // setting outright, still reloads: the properties nothing reads any more
        // are simply ignored, so nothing beside them is lost and the reload does
        // not carry them on, which is what makes the older state a one-way
        // read
        auto older = tree;
        if (auto st = older.getChildWithName ("SETTINGS"); st.isValid())
        {
            st.setProperty ("pasteAsUnique", true, nullptr);
            st.setProperty ("hexDisplay", true, nullptr); // the spelling this build no longer has a row for
        }

        Song then;
        then.fromValueTree (older);
        check (then.settings.volumeCc == 5 && then.settings.panCc == 11,
               "an older saved state that still carries the dropped properties reloads its other settings");
        check (then.patterns.size() == back.patterns.size() && then.clips.size() == back.clips.size(),
               "and nothing beside them is lost, the pattern and clip counts unchanged");
        check (! then.toValueTree().toXmlString().contains ("pasteAsUnique"),
               "the property does not come back on a reload of it, which leaves the older state a one-way read");
        check (! then.toValueTree().toXmlString().contains ("hexDisplay"),
               "nor does the older state's hex spelling, which the current build has no setting to put it back into");
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

        // macros carry too, but the argument of an arpeggiator call is its macro
        // id: a value-only row below a carried call becomes a call of that id, so
        // an undefined one stays silent and no controller is invented either way
        Harness h3;
        auto& p3 = h3.song.patterns[0];
        p3.setLength (8);
        p3.cell (0, 0).cmd = kArpCmd;
        p3.cell (0, 0).cmdVal = 2; // A 2: an undefined macro, so nothing sounds
        p3.cell (3, 0).cmdVal = 44;

        h3.runBlocks (40);

        int fromMacro = 0;

        for (auto& cc : h3.controllers)
            if (cc.value == 44)
                ++fromMacro;

        check (fromMacro == 0, "a value-only row under a carried call plays that call, not a controller");
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
        const int cmdRangeId = a.addPattern ("Cmd range", 32, 1).id;
        const int holedId = a.addPattern ("Holed", 32, 2).id;

        auto* pn = a.findPattern (noteId);
        auto* pi = a.findPattern (instId);
        auto* pc = a.findPattern (cmdId);
        auto* po = a.findPattern (offId);
        auto* pr = a.findPattern (rampId);
        auto* pm = a.findPattern (cmdRangeId);
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

        pm->cell (0, 0).cmd = 120;
        pm->cell (10, 0).cmd = 140; // above the top of the number range: clamped to the highest CC
        check (pm->interpolate (InterpField::cmdNum, rect (0, 10, 0, 0)) == 9,
               "the command column ramps like any other number");
        check (pm->cell (1, 0).cmd == 121 && pm->cell (5, 0).cmd == 124
               && pm->cell (9, 0).cmd == 126,
               "the ramp climbs toward the highest CC rather than the value the row above was holding");
        bool cmdIntact = true;
        for (int row = 1; row <= 9; ++row)
            if (pm->cell (row, 0).cmd < 120 || pm->cell (row, 0).cmd > kCcTop)
                cmdIntact = false;

        check (cmdIntact, "a ramping command number never invents a letter command");

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
