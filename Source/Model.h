#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <vector>
#include <memory>
#include <cstdint>
#include <functional>

namespace rtr
{

constexpr int kEmpty = -1;
constexpr int kNoteOff = 253;
constexpr int kMaxRows = 1024;
constexpr int kMaxTrackerChannels = 127;
constexpr int kMidiChannelCount = 16;
/** The Command column's number field holds a plain CC number, 0..kCcTop, and
    above that the command letters: the letter A names kArpCmd, the letter B
    names kBendCmd, the letters U and D name a slide of the pitch wheel to
    its top and to its bottom, the letter V runs a vibrato and the letter C
    cuts a note's length, whether the note entered at the row it is spelled
    beside or has been held by that column from the rows above, each claimed as
    the field's first character rather than typed as a number, which is what stops
    a typed digit run from inventing a letter - a decimal field spells CC 10 as
    10, so nothing about a number's spelling reaches one of the letters. kBendCmd,
    the two slide values, the vibrato letter and this build's cut letter all sit
    above kOlderCmdTop, the highest value an older build could write for its M0-M15
    macro kinds, so the values this build owns for the pitch wheel and for a note's
    length are values that build could not have written, and an older
    state's macro calls can all land on the letter they now means without any
    of them becoming a bend, a slide, a wobble or a cut.

    A bend's argument names a point on the 14-bit pitch wheel in 128s: 0 is the
    wheel's bottom, kBendCentreVal its exact centre and 127 near its top, so the
    argument keeps the decimal spelling its centre is read in.

    A slide's argument says how many rows the slide takes rather than where on
    that wheel it goes: the row that named the letter leaves the wheel where it
    already is and the extreme sits that many rows below it, so the widest slide
    the field can name takes 127 rows and the narrowest takes 1. A row that names
    the letter and leaves the argument empty takes the default span of kSlideDefaultSpan
    rows, the number the entry writes so the cell spells the slide it asks for.

    Two of the argument field's spellings name no number at all but the wheel's
    own state, entered as letters rather than typed as a number since the field
    only ever spells 0..kCcTop: the o the entry answers lands the OFF value, which
    switches the pitchbending off and puts the wheel back at its centre, and the s
    it answers beside a wheel letter lands the STP value, which stops whatever
    move the wheel was making and leaves it sitting on the point it had got to.
    A 0 entered beside a slide letter is the same release rather than a span of
    no rows, so the span 0 no longer means anything and a slide always takes at
    least one row, while a 0 beside a bend letter keeps naming the wheel's own
    bottom. An argument that is neither a number nor one of those letters is
    ignored and leaves the cell's value as it was.

    A vibrato's argument names which vibrato macro the column runs, the same way
    an arpeggiator call names one of its macros: id 0 and the o spelling are the
    release rather than a macro, which switches the pitchbending off and puts the
    wheel back at its centre, and the s spelling asks for nothing at all, so the
    wobble above keeps holding the point it is at. A vibrato moves about whatever
    point the wheel is at where it was armed rather than about the wheel's centre,
    which is what lets the letter that bent the pitch, a slide's extreme or a
    plain bend argument all be the base a wobble swings from.

    A cut's argument says how long a note is held rather than what any other row is
    asked to do: the subticks are counted on the same sub-row grid an arpeggiator's
    steps, a slide's ramp and a wobble's points already use, so a cut of 3 subticks
    at the default of 6 to the row holds the note for half a row and the widest one
    the entry can name holds it for kCutTopTicks subticks, three digits on that grid.
    The letter cuts the note its own row entered and, beside a row that named no note
    of its own, the note that channel has been holding from the rows above it, released
    at the length this row names past it, which is what lets the letter cut across the
    rows rather than only the one that entered a note. Unlike the wheel's letters, this
    one keeps being carried down under it: a note entering a row that named no command
    of its own under a carried cut is held for the length the column carries, which is
    what lets the letter gate a whole staccato section rather than cutting only the row
    that named it, and an argument named under a carried letter retargets that length
    for the notes entering there and the rows below rather than re-timing a note the row
    above had already armed. A row that names the letter and leaves the argument empty
    takes the default length of kCutDefaultTicks subticks, the number the entry writes
    so the cell spells the cut it asks for, and both a 0 beside the letter and the OFF
    the entry writes for an o are the note column's own note-off rather than a cut of no
    subticks at all, drawn OFF the way its === is drawn: the note this channel is
    holding at that row is released at that row's own sample, and the gate is put away
    rather than carried on below, so the notes entering the rows under a release sustain
    again and the column carries no letter there. Where that row entered a note of its
    own as well, that note's arrival had already released the note above it, so the
    release adds nothing of its own and the note entered there is left to whatever ends
    it below - a note of no length at all cannot be spelled, its note-off being sorted
    ahead of its note-on at the same sample. Two lengths of the grid have no spelling of
    their own: the numbers 128 and 129 the argument field holds stay the OFF and STP
    spellings, so a cut of 128 or 129 subticks cannot be named. The s spelling asks for
    nothing beside a cut, since a cut has no move of the wheel to stop, and leaves the
    length above as it was. A gate whose note-off has not reached its subtick yet is
    discarded when a row under it names a command that is not the cut letter: the letter
    the column stopped having has no note-length left to hold the notes below it, and
    the release that gate had armed for a note entered above it has no gate of its own
    left to cut it, which is how a run's and a ramp's waiting steps go when the column
    supersedes them lower down - the note entered above is left to whatever ends it
    below, the rows under that command having nothing left to be cut. */
constexpr int kArgOff = 128;  // the argument that reads OFF: an arpeggiator going off or the wheel's release
constexpr int kArgStp = 129;  // the argument that reads STP: the wheel held at the point it is at
constexpr int kMaxArpMacros = 127; // arpeggiator macros, ids 1..127; the id 0 call releases
constexpr int kMaxArpSteps = 64;   // most steps one arpeggiator macro can hold
constexpr int kArpCmd = 128;       // command value 128 calls an arpeggiator, entered as A
constexpr int kCcTop = 127;        // highest plain CC number a column can hold
constexpr int kOlderCmdTop = 143;  // highest command value an older build could write, one of its M0-M15 macro kinds
constexpr int kBendCmd = 144;      // command value 144 bends the pitch, entered as B
constexpr int kBendCentreVal = 64; // the bend argument that sits at the wheel's centre
constexpr int kSlideUpCmd = 145;   // command value 145 slides the wheel to its top, entered as U
constexpr int kSlideDownCmd = 146; // command value 146 slides the wheel to its bottom, entered as D
constexpr int kSlideDefaultSpan = 8; // rows a slide takes when its row leaves the argument empty
constexpr int kArpTopStep = 127;      // a step's jump is clamped to this far off the base note
constexpr int kArpEmptyStep = -128; // a step that sounds nothing and sustains the one before
constexpr int kArpNoteOffStep = -129; // a step that opens the note it would have held
constexpr int kArpDefaultSteps = 8; // steps a newly defined macro starts with
constexpr int kVibCmd = 147;      // command value 147 runs a vibrato, entered as V
constexpr int kMaxVibMacros = 127; // vibrato macros, ids 1..127; the id 0 call releases
constexpr int kVibTypeTop = 5;    // the highest of the six vibrato shapes
constexpr int kVibTypeSin = 0;    // a normal sinusoidal wobble, both above and below the base
constexpr int kVibTypeSinUp = 1;  // the same cycle with its second half inverted, so only above
constexpr int kVibTypeSinDown = 2; // the same cycle with its first half inverted, so only below
constexpr int kVibTypeTri = 3;    // the triangle wobble, both above and below the base
constexpr int kVibTypeTriUp = 4;  // the same ramp with its far half inverted, so only above
constexpr int kVibTypeTriDown = 5; // the same ramp with its near half inverted, so only below
constexpr int kVibAmpTop = 100;   // a vibrato's amplitude at full pitchbend, in hundredths
constexpr int kVibFreqTop = 127;  // the widest cycle a vibrato can ask for, in subticks
constexpr int kVibDefaultAmp = 50; // the amplitude a newly defined macro starts at, in hundredths
constexpr int kVibDefaultFreq = 8; // the cycle a newly defined macro starts at, in subticks

constexpr int kCutCmd = 148; // command value 148 cuts a note's length, entered as C
constexpr int kCutTopTicks = 999; // the widest cut the entry can name, in subticks on the Settings' grid; 128 and 129 below it stay the OFF and STP spellings
constexpr int kCutDefaultTicks = 3; // subticks a note is held for when the row leaves the argument empty
constexpr int kMaxUndoSteps = 200; // most undo steps the undo line can be asked for

/** Column visibility bits: the chips above a channel toggle one field of one
    channel, so the state is one byte per channel (see Pattern::vis). */
constexpr std::uint8_t kVisInst = 1;
constexpr std::uint8_t kVisVol = 2;
constexpr std::uint8_t kVisPan = 4;
constexpr std::uint8_t kVisCmd = 8;
constexpr std::uint8_t kVisAll = 15;

//==============================================================================
/** A single tracker cell (one row of one tracker channel). */
struct Cell
{
    int note = kEmpty;    // kEmpty, kNoteOff (===), or 0..127
    int inst = kEmpty;    // kEmpty or 0..15 (MIDI output channel)
    int vol = kEmpty;     // kEmpty or 0..127
    int pan = kEmpty;     // kEmpty or 0..127
    int cmd = kEmpty;     // kEmpty, 0..127 = CC; 128 = arpeggiator call (A); 144 = pitch bend (B); 145/146 = wheel slide up/down (U/D); 147 = vibrato (V); 148 = cut (C)
    int cmdVal = kEmpty;  // kEmpty, 0..127 or a cut's own subticks to kCutTopTicks: a macro id, a point on the wheel, a slide's rows, a vibrato macro id or a cut's subticks; kArgOff = OFF and kArgStp = STP above the field's own top

    bool empty() const noexcept
    {
        return note == kEmpty && inst == kEmpty && vol == kEmpty && pan == kEmpty && cmd == kEmpty;
    }

    bool hasCommand() const noexcept { return cmd != kEmpty; }

    void clear() noexcept { *this = Cell(); }
};

//==============================================================================
/** A tracker column the interpolation tool can ramp between the values that
    already exist inside a row range. Only linear ramps are supported: the blank
    cells between two neighbouring anchors take the linear position between them.
    In the note column a note-off (`===`) has no number to ramp to or from, so it
    closes a segment instead of anchoring one. */
enum class InterpField
{
    note = 0, // 0..127, a note-off ends the segment
    inst,     // 0..15
    vol,      // 0..127
    pan,      // 0..127
    cmdNum,   // 0..128, the CC number the column carries or the arpeggiator call
    cmdVal    // 0..127, the command argument
};

//==============================================================================
/** Paste filter bits (Pattern::pasteCells): which fields of the copied cells
    reach the pattern. The tracker's Ctrl+V is kPasteAll, while the right-click
    paste submenu offers one option per bit. The command pair splits because
    "Command" names the number the column carries and "Command Value" its
    argument. A clipboard field that was empty clears the target's field, so
    a pasted cell goes back to carrying whatever the column carried above it. */
constexpr int kPasteNote = 1;
constexpr int kPasteInst = 2;
constexpr int kPasteVol = 4;
constexpr int kPastePan = 8;
constexpr int kPasteCmd = 16;    // command number
constexpr int kPasteCmdVal = 32; // command argument
constexpr int kPasteAll = 63;

/** Not a field bit but a mode bit to add to the filter (kPasteAll | kPasteAdditive
    means everything, additively): only the buffer's non-empty fields land, so an
    empty clipboard field leaves the target's own value in place rather than
    clearing it. A cell whose every field is empty writes nothing at all, and a
    paste that writes nothing at all leaves no undo point behind. */
constexpr int kPasteAdditive = 64;

/** Cell clipboard content: every copied cell with its offset inside the copied
    rectangle, measured from its top-left cell. A Ctrl-drag hole stays a hole
    here, since only selected cells are stored, so a paste never writes through
    it. The clipboard is UI state (the tracker view holds one), not part of the
    saved song, but it survives a pattern switch so a copy can paste elsewhere. */
struct CellClipboard
{
    struct Entry
    {
        int row = 0; // offset within the copied rectangle
        int ch = 0;
        Cell cell;
    };

    std::vector<Entry> entries;

    bool empty() const noexcept { return entries.empty(); }
    void clear() { entries.clear(); }
};

//==============================================================================
struct Pattern
{
    int id = 0;
    juce::String name { "Pattern" };
    int length = 64;
    int channels = 1;

    std::vector<std::vector<Cell>> rows; // [row][channel]

    /** Column visibility is per pattern and per tracker channel: one byte per
        channel column, with a bit for each field (instrument, volume, pan,
        command). The chips above a channel toggle one field of one channel, so
        hiding the volume of channel 2 leaves the other channels alone. Like the
        channel capacity, the flags grow only: columns past the visible channel
        count keep their state so raising the count again restores it. */
    std::vector<std::uint8_t> vis;

    /** Field index used by the chips (0 instrument, 1 volume, 2 pan,
        3 command) mapped to its visibility bit. */
    static constexpr std::uint8_t fieldBit (int field) noexcept
    {
        const int bit = field < 0 || field > 3 ? 3 : field;
        return (std::uint8_t) (1u << bit);
    }

    bool showColumn (int ch, int field) const noexcept
    {
        return ch < 0 || ch >= (int) vis.size()
                   ? true
                   : ((vis[(size_t) ch] & fieldBit (field)) != 0);
    }

    void setColumn (int ch, int field, bool show) noexcept
    {
        if (ch < 0 || ch >= channelCapacity())
            return;

        if ((int) vis.size() < channelCapacity())
            vis.resize ((size_t) channelCapacity(), kVisAll);

        if (show)
            vis[(size_t) ch] = (std::uint8_t) (vis[(size_t) ch] | fieldBit (field));
        else
            vis[(size_t) ch] = (std::uint8_t) (vis[(size_t) ch] & ~fieldBit (field));
    }

    void toggleColumn (int ch, int field) noexcept
    {
        setColumn (ch, field, ! showColumn (ch, field));
    }

    /** Number of visibility entries to store: at least one per channel. */
    int visCount() const noexcept
    {
        return juce::jmax ((int) vis.size(), channelCapacity());
    }

    void allocate()
    {
        length = juce::jlimit (1, kMaxRows, length);
        channels = juce::jlimit (1, kMaxTrackerChannels, channels);

        rows.resize ((size_t) length);

        for (auto& r : rows)
            r.resize ((size_t) channels);

        vis.assign ((size_t) channels, kVisAll);
    }

    Cell& cell (int row, int ch) noexcept
    {
        return rows[(size_t) juce::jlimit (0, length - 1, row)][(size_t) juce::jlimit (0, channels - 1, ch)];
    }

    const Cell& cell (int row, int ch) const noexcept
    {
        return rows[(size_t) juce::jlimit (0, length - 1, row)][(size_t) juce::jlimit (0, channels - 1, ch)];
    }

    /** Row capacity (rows.size()) may exceed the visible length: shortening a
        pattern keeps the rows beyond the new end, and enlarging it again
        restores them, so resizing never discards data. Trailing hidden rows
        that hold nothing are dropped to keep memory and state compact. */
    int capacity() const noexcept { return juce::jmax (length, (int) rows.size()); }

    void trimHiddenRows()
    {
        while ((int) rows.size() > length)
        {
            bool any = false;

            for (auto& c : rows.back())
                if (! c.empty())
                {
                    any = true;
                    break;
                }

            if (! any)
                rows.pop_back();
            else
                break;
        }
    }

    void setLength (int newLength)
    {
        newLength = juce::jlimit (1, kMaxRows, newLength);

        if (newLength > (int) rows.size())
            rows.resize ((size_t) newLength); // grows with fresh empty cells

        for (auto& r : rows)
            if ((int) r.size() < channels)
                r.resize ((size_t) channels); // grows only: keeps channel capacity

        length = newLength;
        trimHiddenRows();
    }

    /** Channel capacity works like row capacity: lowering the visible channel
        count keeps the cells of the removed columns, so raising it again restores
        them. Columns past the visible count that hold nothing are dropped. */
    int channelCapacity() const noexcept
    {
        int cap = channels;

        for (auto& r : rows)
            cap = juce::jmax (cap, (int) r.size());

        return cap;
    }

    void trimHiddenChannels()
    {
        for (;;)
        {
            int widest = channels;

            for (auto& r : rows)
                widest = juce::jmax (widest, (int) r.size());

            if (widest <= channels)
                break;

            bool any = false;

            for (auto& r : rows)
                if ((int) r.size() == widest && ! r[(size_t) widest - 1].empty())
                {
                    any = true;
                    break;
                }

            if (any)
                break;

            for (auto& r : rows)
                if ((int) r.size() > widest - 1)
                    r.resize ((size_t) widest - 1);
        }
    }

    void setChannels (int newChannels)
    {
        newChannels = juce::jlimit (1, kMaxTrackerChannels, newChannels);

        for (auto& r : rows)
            if (newChannels > (int) r.size())
                r.resize ((size_t) newChannels); // grows only: keeps hidden columns

        channels = newChannels;

        if ((int) vis.size() < channels)
            vis.resize ((size_t) channels, kVisAll); // fresh columns start visible

        trimHiddenChannels();
    }

    void clearAll()
    {
        for (auto& r : rows)
            for (auto& c : r)
                c.clear();
    }

    /** Linear interpolation of one column, confined to the cells a selection
        names. `inCell (row, ch)` says whether a cell of the pattern is in reach,
        so the selection can be any shape: anchors come only from cells inside it,
        only cells inside it are ever written, and a cell left out of it ends the
        run, which means a selection with a gap in it ramps in pieces instead of
        across the gap. Every blank cell between two neighbouring anchors in the
        same channel gets the value linearly between them, rounded and clamped to
        the field's range; only blanks are written, so the anchors and any value
        outside the selection stay as they were. Nothing is extrapolated before or
        after the outermost anchors, and in the note column a note-off (`===`)
        closes a segment. A row whose cells are shorter than the channel asked for
        is skipped without ending the run. Returns the number of cells written. */
    int interpolate (InterpField field,
                     const std::function<bool (int row, int ch)>& inCell) noexcept;

    /** Pastes a cell clipboard anchored at (anchorRow, anchorChannel): the
        copied cell at rectangle offset (0, 0) lands on the anchor, and every
        other keeps its place relative to it. Targets outside the pattern's
        visible length or channel count are simply discarded, so a paste never
        grows the pattern. `fieldMask` is the paste filter (kPaste* bits):
        kPasteAll rewrites every field of the cell, a single-field choice
        touches only its own column, and a clipboard field that was empty
        clears the target's field. Adding the kPasteAdditive mode bit instead
        keeps the target's own value under every empty field of the buffer, so
        only what the clipboard really carries lands on top. Returns the
        number of cells written, which is zero when the additive paste found
        nothing to write. */
    int pasteCells (const CellClipboard& clip, int anchorRow, int anchorChannel,
                    int fieldMask) noexcept;
};

//==============================================================================
/** A pattern instance placed on the song timeline. */
struct Clip
{
    juce::Uuid uid;
    int patternId = 0;
    double startBeats = 0.0;
};

//==============================================================================
/** One arpeggiator macro: the signed semitone offsets it steps through, one step
    per subtick, measured from the note the tracker channel is holding. A row
    naming `A <id>` in the Command column starts it, where the letter A is the
    command value kArpCmd and the argument names the macro id, 1..kMaxArpMacros;
    the id 0 call releases the note the run is holding instead of starting one.
    A step holding kArpEmptyStep sounds nothing and sustains the step before it,
    and a step whose note falls outside the keyboard snaps back by whole octaves,
    so the pitch keeps the step's shape. A step holding kArpNoteOffStep is the
    note column's own note-off: at its subtick it releases whatever the run is
    holding there, the note it last entered when it has already sounded one and
    the note the column had entered above the call when it has not, and sounds
    nothing, so a leading note-off step is a rest over the note the rows above had
    left held while the column keeps naming it and the steps under the rest measure
    from it again. The note entered at the row that calls a macro is that run's first
    step, sounded at the pitch the first step moved it to rather than as it was
    typed, which is what makes the first of the steps affect a note the call row
    named. A jump is clamped to kArpTopStep away from the base note, which is what
    keeps both sentinels free of the jumps a step can name and makes the note-off
    the value below the empty one rather than another byte. The steps are held wide, two
    bytes saved per step, so a note-off sits below the range a single byte could
    name and a state older than this still reads: its one-byte block widens step
    by step unchanged. A macro that is looped starts over at its first step
    after the last and runs until the clip closes it or the column
    names another command; one that is not runs through its steps once and leaves
    the last one sustained. A macro with no steps is not defined. */
struct ArpMacro
{
    bool looped = false;
    std::vector<std::int16_t> steps; // signed semitone jumps, or one of the two sentinels

    int length() const noexcept { return (int) steps.size(); }
    bool defined() const noexcept { return ! steps.empty(); }

    /** A newly defined macro starts with steps that sound nothing, so a call of
        it plays the note its channel is holding until steps are entered into it,
        which is what lets the tab be shown as it arrives without a shape in it. */
    void setDefault (int len = kArpDefaultSteps)
    {
        steps.resize ((size_t) juce::jlimit (1, kMaxArpSteps, len));

        for (int i = 0; i < (int) steps.size(); ++i)
            steps[(size_t) i] = (std::int16_t) kArpEmptyStep;
    }
};

//==============================================================================
/** A vibrato macro the Command column's letter V calls: one shape, one amplitude
    and one cycle length, all three entered on the VIB MACRO tab. The shape names
    how the wobble moves about the point the wheel is already at, the amplitude
    says how far it moves in hundredths of full pitchbend and the cycle length
    says how many subticks one whole cycle of it takes, on the same sub-row grid
    an arpeggiator's steps and a slide's ramp use. A macro holds a value for each
    of the three at all times, so being present is what makes it one: clearing it
    says so rather than erasing a setting, which is what keeps a macro that was
    only asked to sound nothing at amplitude 0 a macro rather than no macro. */
struct VibMacro
{
    bool present = false;
    int type = kVibTypeSin;                    // one of the six shapes
    int amplitude = kVibDefaultAmp;            // hundredths of full pitchbend
    int frequency = kVibDefaultFreq;           // subticks one cycle takes

    bool defined() const noexcept { return present; }

    /** A newly defined macro starts at the plain sinusoidal shape at half full
        pitchbend over a cycle of eight subticks, a wobble that reads at once on
        the tab without anything being entered into it. */
    void setDefault()
    {
        present = true;
        type = kVibTypeSin;
        amplitude = kVibDefaultAmp;
        frequency = kVibDefaultFreq;
    }
};

//==============================================================================
struct Settings
{
    bool midRowIgnored = true;
    int subticksPerRow = 6;
    int rowsPerBeat = 4;
    int defaultPatternRows = 64;
    bool snapEnabled = true;
    bool adaptiveGrid = true;
    int fixedGridRows = 4;
    int recordOctave = 4;
    int stepSkip = 1;

    /** Controller numbers the volume and pan columns drive. They are emitted
        whenever such a cell is written explicitly, so the tracker can feed
        anything a synth listens to (CC7 channel volume by default, CC10 pan). */
    int volumeCc = 7;
    int panCc = 10;

    /** How far Ctrl+Z can walk back: the number of model states the undo line
        keeps. It belongs to the song rather than to the view, so a host that
        saves the state or hands the plug-in a brand new editor gets the same
        depth back, and changing it is itself an undoable edit. Zero turns
        undoing off, which also stops the plug-in from storing states at all. */
    int undoSteps = 50;
};

//==============================================================================
enum class ToolScope
{
    selection = 0,
    pattern,
    songPatterns,
    allPatterns
};

//==============================================================================
/** Clipboard content: reference pattern id plus a data copy. */
struct Clipboard
{
    bool valid = false;
    int patternId = 0;
    Pattern data;

    void clear()
    {
        valid = false;
        patternId = 0;
        data = Pattern();
    }
};

//==============================================================================
class Song
{
public:
    Song();

    //== Patterns ==============================================================
    Pattern& addPattern (const juce::String& name = {}, int length = 0, int channels = 0);
    Pattern* findPattern (int id) noexcept;
    const Pattern* findPattern (int id) const noexcept;
    int patternIndex (int id) const noexcept;
    Pattern* activePattern() noexcept { return findPattern (activePatternId); }
    const Pattern* activePattern() const noexcept { return findPattern (activePatternId); }
    bool patternInUse (int id) const noexcept;
    void removePattern (int id);

    //== Clips =================================================================
    Clip* findClip (const juce::Uuid& uid) noexcept;
    const Clip* findClip (const juce::Uuid& uid) const noexcept;
    Clip* clipAtBeat (double beats) noexcept;

    double patternLengthBeats (int patternId) const noexcept;
    double clipLengthBeats (const Clip& clip) const noexcept { return patternLengthBeats (clip.patternId); }

    bool canPlace (double startBeats, double lengthBeats, const juce::Uuid* clipToIgnore = nullptr) const noexcept;

    Clip* addClip (int patternId, double startBeats);
    bool moveClip (const juce::Uuid& uid, double newStartBeats);
    bool resizeClip (const juce::Uuid& uid, int newLengthRows);
    void removeClip (const juce::Uuid& uid);
    bool sliceClipAt (const juce::Uuid& uid, double atBeats);

    //== Grid ==================================================================
    double beatsPerRow() const noexcept { return 1.0 / (double) juce::jmax (1, settings.rowsPerBeat); }
    double adaptiveGridRows (double pixelsPerBeat) const noexcept;
    double currentGridRows (double pixelsPerBeat) const noexcept;
    double snapBeats (double beats, double pixelsPerBeat) const noexcept;
    double snapRow (double beats, double pixelsPerBeat) const noexcept;

    //== Clipboard =============================================================
    Clipboard clipboard;

    void copyPattern (int patternId);
    Clip* pasteAt (double startBeats, bool asUnique, const juce::Uuid* clipToIgnore = nullptr);

    //== Tools =================================================================
    std::vector<Pattern*> patternsForScope (ToolScope scope, int patternId, const std::vector<std::pair<int, int>>& selection);

    //== Arpeggiator macros ====================================================
    /** Arpeggiator macros live in one grow-only table indexed by id - 1, so
        clearing one keeps the room its steps took and defining it again restores
        them, the same rule the pattern row and channel capacities use. A macro
        with no steps is not defined, which is what the Macros tab lists and what
        playback refuses to run; id 0 is the release call and has no entry. */
    ArpMacro* findArp (int id); // grows the table to name the id, null out of range
    const ArpMacro* peekArp (int id) const noexcept; // reads it without growing it
    bool arpDefined (int id) const noexcept;
    bool arpLooped (int id) const noexcept;
    int firstUndefArpId() const noexcept; // 0 once every id is taken
    int lastDefArpId() const noexcept;    // 0 when nothing is defined
    void defineArp (int id); // fresh macro with empty steps, not looped
    void clearArp (int id); // leaves it undefined, keeping its room
    void setArpLength (int id, int length); // keeps the steps already entered
    void setArpStep (int id, int index, int value) noexcept; // a jump, clamped, or a sentinel
    void setArpLooped (int id, bool looped) noexcept;

    //== Vibrato macros ========================================================
    /** Vibrato macros live in one grow-only table of their own, added to and
        removed from exactly as the arpeggiator ones are: naming an id extends the
        table with absent entries, removing one leaves its room and the values it
        had, and defining it again starts from the defaults. The id 0 call is the
        release and has no entry. */
    VibMacro* findVib (int id); // grows the table to name the id, null out of range
    const VibMacro* peekVib (int id) const noexcept; // reads it without growing it
    bool vibDefined (int id) const noexcept;
    int vibType (int id) const noexcept;     // the shape it holds, 0 when it has none
    int vibAmplitude (int id) const noexcept; // its amplitude, 0 when it has none
    int vibFrequency (int id) const noexcept; // its cycle length, 0 when it has none
    int firstUndefVibId() const noexcept; // 0 once every id is taken
    int lastDefVibId() const noexcept;    // 0 when nothing is defined
    void defineVib (int id); // fresh macro at the default shape, amplitude and cycle
    void clearVib (int id); // leaves it undefined, keeping its room
    void setVibType (int id, int type) noexcept; // one of the six shapes
    void setVibAmplitude (int id, int amplitude) noexcept; // hundredths of full pitchbend
    void setVibFrequency (int id, int frequency) noexcept; // subticks one cycle takes

    //== Serialization =========================================================
    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& tree);
    void reset();

    //== State =================================================================
    Settings settings;
    std::vector<Pattern> patterns;
    std::vector<Clip> clips;
    std::vector<ArpMacro> arps;
    std::vector<VibMacro> vibs;

    int activePatternId = 0;
    double editPlayheadBeats = 0.0;

    /** The host transport position the edit playhead last mirrored, in PPQ
        beats (negative before the first mirror). The editor follows the host
        only while its position changes, so a playhead parked by a timeline
        click stays put while the host is stopped. */
    double transportMirrorBeats = -1.0;

    int nextPatternId = 1;
    double songEndBeats() const noexcept;

private:
    Clip* overlapFor (double startBeats, double lengthBeats, const juce::Uuid* clipToIgnore) const noexcept;
};

//==============================================================================
juce::String noteName (int note);
juce::String valueString (int value, int digits = 3);
juce::String instString (int inst);

} // namespace rtr
