#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <vector>
#include <memory>

namespace rtr
{

constexpr int kEmpty = -1;
constexpr int kNoteOff = 253;
constexpr int kMaxRows = 1024;
constexpr int kMaxTrackerChannels = 127;
constexpr int kMidiChannelCount = 16;

//==============================================================================
/** A single tracker cell (one row of one tracker channel). */
struct Cell
{
    int note = kEmpty;    // kEmpty, kNoteOff (===), or 0..127
    int inst = kEmpty;    // kEmpty or 0..15 (MIDI output channel)
    int vol = kEmpty;     // kEmpty or 0..127
    int pan = kEmpty;     // kEmpty or 0..127
    int cmd = kEmpty;     // kEmpty or 0..127 (CC number for now)
    int cmdVal = kEmpty;  // kEmpty or 0..127

    bool empty() const noexcept
    {
        return note == kEmpty && inst == kEmpty && vol == kEmpty && pan == kEmpty && cmd == kEmpty;
    }

    bool hasCommand() const noexcept { return cmd != kEmpty; }

    void clear() noexcept { *this = Cell(); }
};

//==============================================================================
struct Pattern
{
    int id = 0;
    juce::String name { "Pattern" };
    int length = 64;
    int channels = 1;

    bool showInst = true;
    bool showVol = true;
    bool showPan = true;
    bool showCmd = true;

    std::vector<std::vector<Cell>> rows; // [row][channel]

    void allocate()
    {
        length = juce::jlimit (1, kMaxRows, length);
        channels = juce::jlimit (1, kMaxTrackerChannels, channels);

        rows.resize ((size_t) length);

        for (auto& r : rows)
            r.resize ((size_t) channels);
    }

    Cell& cell (int row, int ch) noexcept
    {
        return rows[(size_t) juce::jlimit (0, length - 1, row)][(size_t) juce::jlimit (0, channels - 1, ch)];
    }

    const Cell& cell (int row, int ch) const noexcept
    {
        return rows[(size_t) juce::jlimit (0, length - 1, row)][(size_t) juce::jlimit (0, channels - 1, ch)];
    }

    void setLength (int newLength)
    {
        newLength = juce::jlimit (1, kMaxRows, newLength);
        rows.resize ((size_t) newLength);
        for (auto& r : rows)
            r.resize ((size_t) channels);
        length = newLength;
    }

    void setChannels (int newChannels)
    {
        newChannels = juce::jlimit (1, kMaxTrackerChannels, newChannels);
        for (auto& r : rows)
            r.resize ((size_t) newChannels);
        channels = newChannels;
    }

    void clearAll()
    {
        for (auto& r : rows)
            for (auto& c : r)
                c.clear();
    }
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
/** A user defined macro command (arpeggiator etc.) - stored for later use. */
struct Macro
{
    juce::String name { "Arpeggio" };
    int kind = 0; // 0 = arpeggiator, 1 = pitch glide, 2 = slide
    int param1 = 0;
    int param2 = 0;
};

//==============================================================================
struct Settings
{
    bool hexDisplay = false;
    bool midRowIgnored = true;
    int subticksPerRow = 6;
    int rowsPerBeat = 4;
    int defaultPatternRows = 64;
    int maxTrackerChannels = 1;
    bool pasteAsUnique = false;
    bool snapEnabled = true;
    bool adaptiveGrid = true;
    int fixedGridRows = 4;
    int recordOctave = 4;
    int stepSkip = 1;
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

    //== Serialization =========================================================
    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& tree);
    void reset();

    //== State =================================================================
    Settings settings;
    std::vector<Pattern> patterns;
    std::vector<Clip> clips;
    std::vector<Macro> macros;

    int activePatternId = 0;
    double editPlayheadBeats = 0.0;

    int nextPatternId = 1;
    double songEndBeats() const noexcept;

private:
    Clip* overlapFor (double startBeats, double lengthBeats, const juce::Uuid* clipToIgnore) const noexcept;
};

//==============================================================================
juce::String noteName (int note);
juce::String valueString (int value, bool hex, int digits = 2);
juce::String instString (int inst, bool hex, bool decimalInstruments = false);

} // namespace rtr
