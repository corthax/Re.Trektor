#include "Model.h"

namespace rtr
{

//==============================================================================
juce::String noteName (int note)
{
    if (note == kEmpty)
        return "---";

    if (note == kNoteOff)
        return "===";

    static const char* names[] = { "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-" };
    const int n = juce::jlimit (0, 127, note);
    // MIDI note 60 is C-4 (Renoise convention), so note 0 is C--1.
    const int octave = n / 12 - 1;
    return juce::String (names[n % 12]) + juce::String (octave);
}

juce::String valueString (int value, int digits)
{
    if (value == kEmpty)
        return juce::String::repeatedString ("-", juce::jmax (1, digits));

    // the one spelling the tracker shows values in: decimal, padded with spaces so
    // a shorter number still lands on the same digit column as a longer one
    return juce::String (value).paddedLeft (' ', digits);
}

juce::String instString (int inst)
{
    if (inst == kEmpty)
        return "--";

    return juce::String (inst);
}

//==============================================================================
namespace
{

/** Field accessors so the interpolation tool can treat every column the same
    way. A field that is not set is kEmpty, which also means the cell only carries
    a value from an earlier row. */
int fieldValue (const Cell& c, InterpField field) noexcept
{
    switch (field)
    {
        case InterpField::note:   return c.note;
        case InterpField::inst:   return c.inst;
        case InterpField::vol:    return c.vol;
        case InterpField::pan:    return c.pan;
        case InterpField::cmdNum: return c.cmd;
        case InterpField::cmdVal: return c.cmdVal;
        case InterpField::delay:  return c.delay;
    }

    return kEmpty;
}

void setFieldValue (Cell& c, InterpField field, int value) noexcept
{
    switch (field)
    {
        case InterpField::note:   c.note = value; break;
        case InterpField::inst:   c.inst = value; break;
        case InterpField::vol:    c.vol = value; break;
        case InterpField::pan:    c.pan = value; break;
        case InterpField::cmdNum: c.cmd = value; break;
        case InterpField::cmdVal: c.cmdVal = value; break;
        case InterpField::delay:  c.delay = value; break;
    }
}

/** Highest value the field can hold. */
int fieldTop (InterpField field) noexcept
{
    switch (field)
    {
        case InterpField::inst:   return kMidiChannelCount - 1;
        case InterpField::cmdNum: return kCcTop; // a ramp stays a plain CC number
        case InterpField::delay:  return kDelayTop; // percent of a row, two digits of it

        case InterpField::note:
        case InterpField::vol:
        case InterpField::pan:
        case InterpField::cmdVal: return 127;
    }

    return 127;
}

/** True when the cell already names a value the ramp can anchor on. A note-off
    has no number, so it anchors nothing. */
bool isRampAnchor (const Cell& c, InterpField field) noexcept
{
    const int raw = fieldValue (c, field);
    if (raw == kEmpty)
        return false;

    return ! (field == InterpField::note && raw == kNoteOff);
}

} // anonymous namespace

//==============================================================================
int Pattern::interpolate (InterpField field,
                          const std::function<bool (int row, int ch)>& inCell) noexcept
{
    if (rows.empty() || ! inCell)
        return 0;

    // a channel column exists only as far as the ragged rows go
    int widest = 1;
    for (const auto& r : rows)
        widest = juce::jmax (widest, (int) r.size());

    auto at = [this] (int row, int ch) noexcept -> Cell*
    {
        if (row < 0 || row >= (int) rows.size()
            || ch < 0 || ch >= (int) rows[(size_t) row].size())
            return nullptr;

        return &rows[(size_t) row][(size_t) ch];
    };

    const int top = fieldTop (field);
    int written = 0;

    for (int ch = 0; ch < widest; ++ch)
    {
        int anchorRow = -1;
        int anchorValue = kEmpty;

        for (int row = 0; row < (int) rows.size(); ++row)
        {
            // a cell outside the selection ends the run, so the ramp never
            // reaches across a hole and nothing outside it is written
            if (! inCell (row, ch))
            {
                anchorRow = -1;
                continue;
            }

            Cell* c = at (row, ch);
            if (c == nullptr)
                continue; // this row has no such channel: nothing to fill here

            if (field == InterpField::note && fieldValue (*c, field) == kNoteOff)
            {
                anchorRow = -1; // === closes the segment: blanks before it stay blank
                continue;
            }

            if (! isRampAnchor (*c, field))
                continue;

            const int value = juce::jlimit (0, top, fieldValue (*c, field));

            // every row between the two anchors is inside the selection already,
            // because a row outside it would have ended the run above
            if (anchorRow >= 0 && row - anchorRow > 1)
            {
                const double span = (double) (row - anchorRow);

                for (int fill = anchorRow + 1; fill < row; ++fill)
                {
                    Cell* blank = at (fill, ch);
                    if (blank == nullptr || fieldValue (*blank, field) != kEmpty)
                        continue;

                    const double t = (double) (fill - anchorRow) / span;
                    const long ramp = std::llround ((double) anchorValue
                                                    + (double) (value - anchorValue) * t);

                    setFieldValue (*blank, field, juce::jlimit (0, top, (int) ramp));
                    ++written;
                }
            }

            anchorRow = row;
            anchorValue = value;
        }
    }

    return written;
}

//==============================================================================
int Pattern::pasteCells (const CellClipboard& clip, int anchorRow,
                         int anchorChannel, int fieldMask) noexcept
{
    // the additive spelling is a mode, not a column: strip it from the working
    // mask and let it instead trim what each of the buffer's cells carries
    const bool additive = (fieldMask & kPasteAdditive) != 0;
    fieldMask &= ~kPasteAdditive;

    if (clip.entries.empty() || fieldMask == 0 || anchorRow < 0 || anchorChannel < 0)
        return 0;

    // which fields of the copied cell are worth writing at all, that is not
    // empty inside the buffer; an all-empty cell carries none of them
    const auto carrying = [](const Cell& src) noexcept
    {
        int bits = 0;
        if (src.note != kEmpty) bits |= kPasteNote;
        if (src.inst != kEmpty) bits |= kPasteInst;
        if (src.vol != kEmpty) bits |= kPasteVol;
        if (src.pan != kEmpty) bits |= kPastePan;
        if (src.cmd != kEmpty) bits |= kPasteCmd;
        if (src.cmdVal != kEmpty) bits |= kPasteCmdVal;
        if (src.delay != kEmpty) bits |= kPasteDelay;
        return bits;
    };

    int written = 0;

    for (const auto& entry : clip.entries)
    {
        const int row = anchorRow + entry.row;
        const int ch = anchorChannel + entry.ch;

        // what does not fit inside the pattern's visible size is discarded,
        // so a paste never grows the pattern in rows or channels
        if (row >= length || ch >= channels)
            continue;

        // a jagged row simply has no cell past its end
        if (row >= (int) rows.size() || ch >= (int) rows[(size_t) row].size())
            continue;

        auto& dst = rows[(size_t) row][(size_t) ch];
        const Cell& src = entry.cell;

        int mask = fieldMask;
        if (additive)
        {
            // only the buffer's own values reach: the empty ones stay out, so
            // the target keeps whatever it carried under them
            mask &= carrying (src);
            if (mask == 0)
                continue;
        }

        if ((mask & kPasteNote) != 0)
            dst.note = src.note;
        if ((mask & kPasteInst) != 0)
            dst.inst = src.inst;
        if ((mask & kPasteVol) != 0)
            dst.vol = src.vol;
        if ((mask & kPastePan) != 0)
            dst.pan = src.pan;
        if ((mask & kPasteCmd) != 0)
            dst.cmd = src.cmd;
        if ((mask & kPasteCmdVal) != 0)
            dst.cmdVal = src.cmdVal;
        if ((mask & kPasteDelay) != 0)
            dst.delay = src.delay;

        ++written;
    }

    return written;
}

//==============================================================================
Song::Song()
{
    reset();
}

void Song::reset()
{
    settings = Settings();
    patterns.clear();
    clips.clear();
    arps.clear();
    vibs.clear();
    clipboard.clear();
    nextPatternId = 1;
    activePatternId = 0;
    editPlayheadBeats = 0.0;
    transportMirrorBeats = -1.0;

    auto& p = addPattern ("Pattern 01", 64, 1);
    activePatternId = p.id;
    addClip (p.id, 0.0);

    // the song starts with one arpeggiator defined: id 1, the default ramp of
    // eight steps a fourth of an octave apart, not looped
    defineArp (1);

    // and with one vibrato defined beside it: id 1, the plain sinusoidal wobble at
    // half full pitchbend over a cycle of eight subticks, so the VIB tab has
    // a card to show and a V row has something to call as the song arrives
    defineVib (1);

    // and one tremolo defined beside those, too: id 1, the plain sinusoidal swing at
    // half the controller's whole range over a cycle of eight sub-ticks, so the TRM
    // tab has a card to show and a T row has something to call as the song arrives
    defineTrem (1);
}

//==============================================================================
Pattern& Song::addPattern (const juce::String& name, int length, int channels)
{
    Pattern p;
    p.id = nextPatternId++;
    p.name = name.isEmpty() ? ("Pattern " + juce::String (p.id).paddedLeft ('0', 2)) : name;
    p.length = length > 0 ? length : juce::jlimit (1, kMaxRows, settings.defaultPatternRows);
    p.channels = channels > 0 ? channels : 1;
    p.allocate();

    patterns.push_back (std::move (p));
    return patterns.back();
}

Pattern* Song::findPattern (int id) noexcept
{
    for (auto& p : patterns)
        if (p.id == id)
            return &p;

    return nullptr;
}

const Pattern* Song::findPattern (int id) const noexcept
{
    for (auto& p : patterns)
        if (p.id == id)
            return &p;

    return nullptr;
}

int Song::patternIndex (int id) const noexcept
{
    for (int i = 0; i < (int) patterns.size(); ++i)
        if (patterns[(size_t) i].id == id)
            return i;

    return -1;
}

bool Song::patternInUse (int id) const noexcept
{
    for (auto& c : clips)
        if (c.patternId == id)
            return true;

    return false;
}

void Song::removePattern (int id)
{
    if (patterns.size() <= 1)
        return;

    if (patternInUse (id))
        return;

    const int idx = patternIndex (id);

    if (idx < 0)
        return;

    patterns.erase (patterns.begin() + idx);

    if (activePatternId == id)
        activePatternId = patterns.front().id;
}

//==============================================================================
Clip* Song::findClip (const juce::Uuid& uid) noexcept
{
    for (auto& c : clips)
        if (c.uid == uid)
            return &c;

    return nullptr;
}

const Clip* Song::findClip (const juce::Uuid& uid) const noexcept
{
    for (auto& c : clips)
        if (c.uid == uid)
            return &c;

    return nullptr;
}

Clip* Song::clipAtBeat (double beats) noexcept
{
    for (auto& c : clips)
    {
        const double len = clipLengthBeats (c);

        if (beats >= c.startBeats && beats < c.startBeats + len)
            return &c;
    }

    return nullptr;
}

double Song::patternLengthBeats (int patternId) const noexcept
{
    if (auto* p = findPattern (patternId))
        return (double) p->length * beatsPerRow();

    return 0.0;
}

double Song::songEndBeats() const noexcept
{
    double end = 0.0;

    for (auto& c : clips)
        end = juce::jmax (end, c.startBeats + clipLengthBeats (c));

    return end;
}

Clip* Song::overlapFor (double startBeats, double lengthBeats, const juce::Uuid* clipToIgnore) const noexcept
{
    const double end = startBeats + lengthBeats;

    for (auto& c : clips)
    {
        if (clipToIgnore != nullptr && c.uid == *clipToIgnore)
            continue;

        const double cStart = c.startBeats;
        const double cEnd = cStart + clipLengthBeats (c);

        if (startBeats < cEnd - 1.0e-9 && end > cStart + 1.0e-9)
            return const_cast<Clip*> (&c);
    }

    return nullptr;
}

bool Song::canPlace (double startBeats, double lengthBeats, const juce::Uuid* clipToIgnore) const noexcept
{
    if (startBeats < 0.0 || lengthBeats <= 0.0)
        return false;

    return overlapFor (startBeats, lengthBeats, clipToIgnore) == nullptr;
}

Clip* Song::addClip (int patternId, double startBeats)
{
    if (findPattern (patternId) == nullptr)
        return nullptr;

    const double len = patternLengthBeats (patternId);

    if (startBeats < 0.0 || ! canPlace (startBeats, len))
        return nullptr;

    Clip c;
    c.patternId = patternId;
    c.startBeats = startBeats;
    clips.push_back (c);
    return &clips.back();
}

bool Song::moveClip (const juce::Uuid& uid, double newStartBeats)
{
    auto* c = findClip (uid);

    if (c == nullptr)
        return false;

    newStartBeats = juce::jmax (0.0, newStartBeats);
    const double len = clipLengthBeats (*c);

    if (! canPlace (newStartBeats, len, &uid))
        return false;

    c->startBeats = newStartBeats;
    return true;
}

bool Song::resizeClip (const juce::Uuid& uid, int newLengthRows)
{
    auto* c = findClip (uid);
    auto* p = c != nullptr ? findPattern (c->patternId) : nullptr;

    if (c == nullptr || p == nullptr)
        return false;

    newLengthRows = juce::jlimit (1, kMaxRows, newLengthRows);
    const double newLen = (double) newLengthRows * beatsPerRow();

    // The length lives on the shared pattern, so every instance of that pattern
    // must still fit after the resize.
    for (auto& other : clips)
    {
        if (other.patternId != p->id)
            continue;

        if (! canPlace (other.startBeats, newLen, &other.uid))
            return false;
    }

    p->setLength (newLengthRows);
    return true;
}

void Song::removeClip (const juce::Uuid& uid)
{
    for (size_t i = 0; i < clips.size(); ++i)
    {
        if (clips[i].uid == uid)
        {
            clips.erase (clips.begin() + (long) i);
            return;
        }
    }
}

bool Song::sliceClipAt (const juce::Uuid& uid, double atBeats)
{
    auto* c = findClip (uid);

    if (c == nullptr)
        return false;

    auto* p = findPattern (c->patternId);

    if (p == nullptr || p->length <= 1)
        return false;

    const int splitRow = (int) std::llround ((atBeats - c->startBeats) * (double) settings.rowsPerBeat);

    if (splitRow <= 0 || splitRow >= p->length)
        return false;

    // Build the two new patterns, each holding one half of the data.
    const int oldLength = p->length;
    const juce::String baseName = p->name;
    const double clipStart = c->startBeats;
    const juce::Uuid clipUid = c->uid;
    const std::vector<std::vector<Cell>> sourceRows = p->rows;

    Pattern a = *p;
    a.id = nextPatternId++;
    a.name = baseName + " A";
    a.length = splitRow;
    a.rows.assign (sourceRows.begin(), sourceRows.begin() + splitRow);

    Pattern b = *p;
    b.id = nextPatternId++;
    b.name = baseName + " B";
    b.rows.assign (sourceRows.begin() + splitRow, sourceRows.end());
    b.length = oldLength - splitRow;
    b.trimHiddenRows(); // keeps rows hidden past the old end, drops the empty ones

    const int idA = a.id;
    const int idB = b.id;

    const double startA = clipStart;
    const double startB = clipStart + (double) splitRow * beatsPerRow();

    patterns.push_back (std::move (a));
    patterns.push_back (std::move (b));

    Clip ca;
    ca.patternId = idA;
    ca.startBeats = startA;

    Clip cb;
    cb.patternId = idB;
    cb.startBeats = startB;

    removeClip (clipUid);
    clips.push_back (ca);
    clips.push_back (cb);

    activePatternId = idA;
    return true;
}

//==============================================================================
double Song::adaptiveGridRows (double pixelsPerBeat) const noexcept
{
    const double rowsPerBeat = (double) juce::jmax (1, settings.rowsPerBeat);
    const double rowPixels = pixelsPerBeat / rowsPerBeat;
    double rows = 1.0;

    while (rowPixels * rows < 14.0 && rows < (double) kMaxRows)
        rows *= 2.0;

    return rows;
}

double Song::currentGridRows (double pixelsPerBeat) const noexcept
{
    if (! settings.snapEnabled)
        return 0.0;

    if (settings.adaptiveGrid)
        return adaptiveGridRows (pixelsPerBeat);

    return (double) juce::jmax (1, settings.fixedGridRows);
}

double Song::snapBeats (double beats, double pixelsPerBeat) const noexcept
{
    const double rows = currentGridRows (pixelsPerBeat);

    if (rows <= 0.0)
        return juce::jmax (0.0, beats);

    const double gridBeats = rows * beatsPerRow();
    const double snapped = std::round (beats / gridBeats) * gridBeats;
    return juce::jmax (0.0, snapped);
}

double Song::snapRow (double beats, double pixelsPerBeat) const noexcept
{
    return snapBeats (beats, pixelsPerBeat) / beatsPerRow();
}

//==============================================================================
void Song::copyPattern (int patternId)
{
    if (auto* p = findPattern (patternId))
    {
        clipboard.valid = true;
        clipboard.patternId = patternId;
        clipboard.data = *p;
    }
}

Clip* Song::pasteAt (double startBeats, bool asUnique, const juce::Uuid* clipToIgnore)
{
    if (! clipboard.valid)
        return nullptr;

    const bool createCopy = asUnique || findPattern (clipboard.patternId) == nullptr;
    const double len = createCopy ? (double) clipboard.data.length * beatsPerRow()
                                  : patternLengthBeats (clipboard.patternId);

    // Validate before creating anything, so a rejected paste leaves no orphan pattern.
    if (startBeats < 0.0 || ! canPlace (startBeats, len, clipToIgnore))
        return nullptr;

    int targetId = clipboard.patternId;

    if (createCopy)
    {
        Pattern copy = clipboard.data;
        copy.id = nextPatternId++;
        copy.name = clipboard.data.name + " copy";
        patterns.push_back (std::move (copy));
        targetId = patterns.back().id;
    }

    Clip c;
    c.patternId = targetId;
    c.startBeats = startBeats;
    clips.push_back (c);
    activePatternId = targetId;
    return &clips.back();
}

//==============================================================================
std::vector<Pattern*> Song::patternsForScope (ToolScope scope, int patternId,
                                             const std::vector<std::pair<int, int>>& /*selection*/)
{
    std::vector<Pattern*> result;

    switch (scope)
    {
        case ToolScope::selection:
        case ToolScope::pattern:
            if (auto* p = findPattern (patternId))
                result.push_back (p);
            break;

        case ToolScope::songPatterns:
            for (auto& p : patterns)
                if (patternInUse (p.id))
                    result.push_back (&p);
            break;

        case ToolScope::allPatterns:
            for (auto& p : patterns)
                result.push_back (&p);
            break;
    }

    return result;
}

//==============================================================================
/** Arpeggiator macros are one grow-only table indexed by id - 1: naming an id
    extends the table with undefined entries rather than moving the ones already
    in it, so a macro that is cleared keeps the room its steps took and defining
    it again restores them. */
static bool arpInRange (int id) noexcept
{
    return id >= 1 && id <= kMaxArpMacros;
}

ArpMacro* Song::findArp (int id)
{
    if (! arpInRange (id))
        return nullptr;

    if ((int) arps.size() < id)
        arps.resize ((size_t) id);

    return &arps[(size_t) id - 1];
}

const ArpMacro* Song::peekArp (int id) const noexcept
{
    if (! arpInRange (id) || (int) arps.size() < id)
        return nullptr;

    return &arps[(size_t) id - 1];
}

bool Song::arpDefined (int id) const noexcept
{
    const auto* m = peekArp (id);
    return m != nullptr && m->defined();
}

bool Song::arpLooped (int id) const noexcept
{
    const auto* m = peekArp (id);
    return m != nullptr && m->defined() && m->looped;
}

int Song::firstUndefArpId() const noexcept
{
    for (int id = 1; id <= kMaxArpMacros; ++id)
        if (! arpDefined (id))
            return id;

    return 0;
}

int Song::lastDefArpId() const noexcept
{
    for (int id = kMaxArpMacros; id >= 1; --id)
        if (arpDefined (id))
            return id;

    return 0;
}

void Song::defineArp (int id)
{
    auto* m = findArp (id);

    if (m == nullptr)
        return;

    m->looped = false;
    m->setDefault();
}

void Song::clearArp (int id)
{
    auto* m = findArp (id);

    if (m != nullptr)
        m->steps.clear();
}

void Song::setArpLength (int id, int length)
{
    auto* m = findArp (id);

    if (m == nullptr)
        return;

    length = juce::jlimit (1, kMaxArpSteps, length);

    if ((int) m->steps.size() < length)
    {
        // growing keeps the steps already entered and adds steps that sound
        // nothing, so the shape a macro has is only ever where it was entered
        for (int i = (int) m->steps.size(); i < length; ++i)
            m->steps.push_back ((std::int16_t) kArpEmptyStep);
    }
    else
        m->steps.resize ((size_t) length);
}

void Song::setArpStep (int id, int index, int value) noexcept
{
    auto* m = findArp (id);

    if (m == nullptr)
        return;

    // a step is one jump off the base note, kept inside how far a step can reach,
    // or one of the two sentinels, which stand for themselves: the step that
    // sounds nothing, and the note-off that opens whatever the run was holding
    // there rather than stepping anywhere of its own
    if (value != kArpEmptyStep && value != kArpNoteOffStep)
        value = juce::jlimit (-kArpTopStep, kArpTopStep, value);

    if (index >= 0 && index < (int) m->steps.size())
        m->steps[(size_t) index] = (std::int16_t) value;
}

void Song::setArpLooped (int id, bool looped) noexcept
{
    auto* m = findArp (id);

    if (m != nullptr)
        m->looped = looped;
}

//==============================================================================
/** Vibrato macros are one grow-only table of their own, indexed by id - 1 exactly
    as the arpeggiator ones are: naming an id extends the table with absent entries
    rather than moving the ones already in it, so a macro that is removed keeps the
    room it had and the shape, amplitude and cycle it held, and defining it again
    starts over from the defaults. A macro holds a value for each of its three
    settings at all times, so being present is what makes it one: clearing it says
    so rather than erasing a setting, which is what keeps a macro the amplitude
    knob happens to have taken down to off a macro rather than no macro at all. */
static bool vibInRange (int id) noexcept
{
    return id >= 1 && id <= kMaxVibMacros;
}

VibMacro* Song::findVib (int id)
{
    if (! vibInRange (id))
        return nullptr;

    if ((int) vibs.size() < id)
        vibs.resize ((size_t) id);

    return &vibs[(size_t) id - 1];
}

const VibMacro* Song::peekVib (int id) const noexcept
{
    if (! vibInRange (id) || (int) vibs.size() < id)
        return nullptr;

    return &vibs[(size_t) id - 1];
}

bool Song::vibDefined (int id) const noexcept
{
    const auto* m = peekVib (id);
    return m != nullptr && m->defined();
}

int Song::vibType (int id) const noexcept
{
    const auto* m = peekVib (id);
    return m != nullptr && m->defined() ? m->type : 0;
}

int Song::vibAmplitude (int id) const noexcept
{
    const auto* m = peekVib (id);
    return m != nullptr && m->defined() ? m->amplitude : 0;
}

int Song::vibFrequency (int id) const noexcept
{
    const auto* m = peekVib (id);
    return m != nullptr && m->defined() ? m->frequency : 0;
}

int Song::firstUndefVibId() const noexcept
{
    for (int id = 1; id <= kMaxVibMacros; ++id)
        if (! vibDefined (id))
            return id;

    return 0;
}

int Song::lastDefVibId() const noexcept
{
    for (int id = kMaxVibMacros; id >= 1; --id)
        if (vibDefined (id))
            return id;

    return 0;
}

void Song::defineVib (int id)
{
    auto* m = findVib (id);

    if (m == nullptr)
        return;

    m->setDefault();
}

void Song::clearVib (int id)
{
    auto* m = findVib (id);

    if (m != nullptr)
        m->present = false;
}

void Song::setVibType (int id, int type) noexcept
{
    auto* m = findVib (id);

    // a setting only reaches a macro that is one, so an id nothing entered never
    // answers with a shape, an amplitude or a cycle of its own
    if (m == nullptr || ! m->defined())
        return;

    m->type = juce::jlimit (0, kVibTypeTop, type);
}

void Song::setVibAmplitude (int id, int amplitude) noexcept
{
    auto* m = findVib (id);

    if (m == nullptr || ! m->defined())
        return;

    m->amplitude = juce::jlimit (0, kVibAmpTop, amplitude);
}

void Song::setVibFrequency (int id, int frequency) noexcept
{
    auto* m = findVib (id);

    if (m == nullptr || ! m->defined())
        return;

    m->frequency = juce::jlimit (1, kVibFreqTop, frequency);
}

//==============================================================================
/** Tremolo macros are the same machine in a table of their own: one grow-only
    table indexed by id - 1, naming an id extending it with absent entries rather
    than moving the entries already in it, so a macro that is removed keeps the
    room it had and the shape, amplitude and cycle it held and defining it again
    starts from the defaults. Only the settings of a macro that is one are ever
    reached, the same rule that keeps an id nothing entered from answering with a
    shape, an amplitude or a cycle of its own, and only the cycle is kept off
    nothing: a swing that took no sub-tick at all would have no point to send. */
static bool tremInRange (int id) noexcept
{
    return id >= 1 && id <= kMaxTremMacros;
}

TremMacro* Song::findTrem (int id)
{
    if (! tremInRange (id))
        return nullptr;

    if ((int) trems.size() < id)
        trems.resize ((size_t) id);

    return &trems[(size_t) id - 1];
}

const TremMacro* Song::peekTrem (int id) const noexcept
{
    if (! tremInRange (id) || (int) trems.size() < id)
        return nullptr;

    return &trems[(size_t) id - 1];
}

bool Song::tremDefined (int id) const noexcept
{
    const auto* m = peekTrem (id);
    return m != nullptr && m->defined();
}

int Song::tremType (int id) const noexcept
{
    const auto* m = peekTrem (id);
    return m != nullptr && m->defined() ? m->type : 0;
}

int Song::tremAmplitude (int id) const noexcept
{
    const auto* m = peekTrem (id);
    return m != nullptr && m->defined() ? m->amplitude : 0;
}

int Song::tremFrequency (int id) const noexcept
{
    const auto* m = peekTrem (id);
    return m != nullptr && m->defined() ? m->frequency : 0;
}

int Song::firstUndefTremId() const noexcept
{
    for (int id = 1; id <= kMaxTremMacros; ++id)
        if (! tremDefined (id))
            return id;

    return 0;
}

int Song::lastDefTremId() const noexcept
{
    for (int id = kMaxTremMacros; id >= 1; --id)
        if (tremDefined (id))
            return id;

    return 0;
}

void Song::defineTrem (int id)
{
    auto* m = findTrem (id);

    if (m == nullptr)
        return;

    m->setDefault();
}

void Song::clearTrem (int id)
{
    auto* m = findTrem (id);

    if (m != nullptr)
        m->present = false;
}

void Song::setTremType (int id, int type) noexcept
{
    auto* m = findTrem (id);

    // a setting only reaches a macro that is one, so an id nothing entered never
    // answers with a shape, an amplitude or a cycle of its own
    if (m == nullptr || ! m->defined())
        return;

    m->type = juce::jlimit (0, kTremTypeTop, type);
}

void Song::setTremAmplitude (int id, int amplitude) noexcept
{
    auto* m = findTrem (id);

    if (m == nullptr || ! m->defined())
        return;

    m->amplitude = juce::jlimit (0, kTremAmpTop, amplitude);
}

void Song::setTremFrequency (int id, int frequency) noexcept
{
    auto* m = findTrem (id);

    if (m == nullptr || ! m->defined())
        return;

    m->frequency = juce::jlimit (1, kTremFreqTop, frequency);
}

//==============================================================================
juce::ValueTree Song::toValueTree() const
{
    juce::ValueTree root ("RETREKTOR");

    juce::ValueTree st ("SETTINGS");
    st.setProperty ("midRowIgnored", settings.midRowIgnored, nullptr);
    st.setProperty ("subticksPerRow", settings.subticksPerRow, nullptr);
    st.setProperty ("rowsPerBeat", settings.rowsPerBeat, nullptr);
    st.setProperty ("defaultPatternRows", settings.defaultPatternRows, nullptr);
    st.setProperty ("snapEnabled", settings.snapEnabled, nullptr);
    st.setProperty ("adaptiveGrid", settings.adaptiveGrid, nullptr);
    st.setProperty ("fixedGridRows", settings.fixedGridRows, nullptr);
    st.setProperty ("recordOctave", settings.recordOctave, nullptr);
    st.setProperty ("stepSkip", settings.stepSkip, nullptr);
    st.setProperty ("volumeCc", settings.volumeCc, nullptr);
    st.setProperty ("panCc", settings.panCc, nullptr);
    st.setProperty ("undoSteps", settings.undoSteps, nullptr);
    root.appendChild (st, nullptr);

    juce::ValueTree pt ("PATTERNS");

    for (auto& p : patterns)
    {
        juce::ValueTree pv ("PATTERN");
        pv.setProperty ("id", p.id, nullptr);
        pv.setProperty ("name", p.name, nullptr);
        pv.setProperty ("length", p.length, nullptr);
        pv.setProperty ("channels", p.channels, nullptr);

        // Column visibility is stored per channel column (one byte each: bits
        // for instrument, volume, pan, command), so hiding the volume of one
        // channel does not hide it in the others.
        const int visCols = p.visCount();
        juce::MemoryBlock visBlock;
        visBlock.setSize ((size_t) visCols);
        auto* vdst = static_cast<std::uint8_t*> (visBlock.getData());

        for (int c = 0; c < visCols; ++c)
            vdst[c] = c < (int) p.vis.size() ? p.vis[(size_t) c] : kVisDefault;

        pv.setProperty ("vis", visBlock.toBase64Encoding(), nullptr);

        // Packed row data: seven ints per cell over the capacity rectangle, so rows
        // and channel columns hidden past the pattern's visible size (kept by
        // resizing without deleting) survive a state round-trip. The delay column
        // is the seventh of them, so an older state's block reads six to the cell
        // and leaves every delay empty rather than putting a command's argument
        // where a delay would be.
        const int capRows = p.capacity();
        const int capCols = p.channelCapacity();
        static const Cell emptyCell;
        juce::MemoryBlock data;
        data.setSize ((size_t) capRows * (size_t) capCols * 7 * sizeof (juce::int16));

        auto* dst = static_cast<juce::int16*> (data.getData());
        size_t i = 0;

        for (int r = 0; r < capRows; ++r)
        {
            const auto& rowCells = p.rows[(size_t) juce::jmin (r, (int) p.rows.size() - 1)];

            for (int c = 0; c < capCols; ++c)
            {
                const Cell& cell = c < (int) rowCells.size() ? rowCells[(size_t) c] : emptyCell;
                dst[i++] = (juce::int16) cell.note;
                dst[i++] = (juce::int16) cell.inst;
                dst[i++] = (juce::int16) cell.vol;
                dst[i++] = (juce::int16) cell.pan;
                dst[i++] = (juce::int16) cell.cmd;
                dst[i++] = (juce::int16) cell.cmdVal;
                dst[i++] = (juce::int16) cell.delay;
            }
        }

        pv.setProperty ("capacity", capRows, nullptr);
        pv.setProperty ("capacityChannels", capCols, nullptr);
        pv.setProperty ("data", data.toBase64Encoding(), nullptr);
        pt.appendChild (pv, nullptr);
    }

    root.appendChild (pt, nullptr);

    juce::ValueTree ct ("CLIPS");

    for (auto& c : clips)
    {
        juce::ValueTree cv ("CLIP");
        cv.setProperty ("uid", c.uid.toString(), nullptr);
        cv.setProperty ("patternId", c.patternId, nullptr);
        cv.setProperty ("startBeats", c.startBeats, nullptr);
        ct.appendChild (cv, nullptr);
    }

    root.appendChild (ct, nullptr);

    juce::ValueTree at ("ARPS");

    for (int id = 1; id <= kMaxArpMacros; ++id)
    {
        const auto* m = peekArp (id);

        if (m == nullptr || ! m->defined())
            continue;

        juce::ValueTree av ("ARP");
        av.setProperty ("id", id, nullptr);
        av.setProperty ("loop", m->looped, nullptr);
        av.setProperty ("len", m->length(), nullptr); // readable alongside the block

        // a step is a signed semitone jump or one of the two sentinels, and a
        // note-off sits below what a single byte could name, so the block is two
        // bytes a step, little endian: the reader tells that from the one-byte
        // blocks older states wrote by the step count it reads beside it
        juce::MemoryBlock packed;
        packed.setSize ((size_t) m->length() * 2);
        auto* bdst = static_cast<std::uint8_t*> (packed.getData());

        for (int i = 0; i < m->length(); ++i)
        {
            const std::uint16_t raw = (std::uint16_t) (std::int16_t) m->steps[(size_t) i];
            bdst[i * 2] = (std::uint8_t) (raw & 0xff);
            bdst[i * 2 + 1] = (std::uint8_t) (raw >> 8);
        }

        av.setProperty ("steps", packed.toBase64Encoding(), nullptr);
        at.appendChild (av, nullptr);
    }

    root.appendChild (at, nullptr);

    // the vibrato macros the letter V calls get a section of their own, one entry
    // per macro that is present: the three settings are small enough to name as
    // properties rather than pack, so a macro reads as its shape, its amplitude in
    // hundredths of full pitchbend and its cycle length in subticks
    juce::ValueTree vt ("VIBS");

    for (int id = 1; id <= kMaxVibMacros; ++id)
    {
        const auto* m = peekVib (id);

        if (m == nullptr || ! m->defined())
            continue;

        juce::ValueTree vv ("VIB");
        vv.setProperty ("id", id, nullptr);
        vv.setProperty ("type", m->type, nullptr);
        vv.setProperty ("amp", m->amplitude, nullptr);
        vv.setProperty ("freq", m->frequency, nullptr);
        vt.appendChild (vv, nullptr);
    }

    root.appendChild (vt, nullptr);

    // the tremolo macros the letter T calls get a section of their own beside those,
    // one entry per macro that is present: the three settings are small enough to name
    // as properties rather than pack, so a macro reads as its shape, its amplitude in
    // volume points of the controller and its cycle length in sub-ticks
    juce::ValueTree tt ("TRMS");

    for (int id = 1; id <= kMaxTremMacros; ++id)
    {
        const auto* m = peekTrem (id);

        if (m == nullptr || ! m->defined())
            continue;

        juce::ValueTree tv ("TRM");
        tv.setProperty ("id", id, nullptr);
        tv.setProperty ("type", m->type, nullptr);
        tv.setProperty ("amp", m->amplitude, nullptr);
        tv.setProperty ("freq", m->frequency, nullptr);
        tt.appendChild (tv, nullptr);
    }

    root.appendChild (tt, nullptr);

    root.setProperty ("activePatternId", activePatternId, nullptr);
    root.setProperty ("nextPatternId", nextPatternId, nullptr);
    return root;
}

void Song::fromValueTree (const juce::ValueTree& root)
{
    if (! root.hasType ("RETREKTOR"))
        return;

    reset();
    patterns.clear();
    clips.clear();

    if (auto st = root.getChildWithName ("SETTINGS"); st.isValid())
    {
        settings.midRowIgnored = st.getProperty ("midRowIgnored", true);
        settings.subticksPerRow = st.getProperty ("subticksPerRow", 6);
        settings.rowsPerBeat = juce::jmax (1, (int) st.getProperty ("rowsPerBeat", 4));
        settings.defaultPatternRows = juce::jlimit (1, kMaxRows, (int) st.getProperty ("defaultPatternRows", 64));
        settings.snapEnabled = st.getProperty ("snapEnabled", true);
        settings.adaptiveGrid = st.getProperty ("adaptiveGrid", true);
        settings.fixedGridRows = juce::jmax (1, (int) st.getProperty ("fixedGridRows", 4));
        settings.recordOctave = juce::jlimit (0, 9, (int) st.getProperty ("recordOctave", 4));
        settings.stepSkip = juce::jlimit (0, 16, (int) st.getProperty ("stepSkip", 1));
        settings.volumeCc = juce::jlimit (0, 127, (int) st.getProperty ("volumeCc", 7));
        settings.panCc = juce::jlimit (0, 127, (int) st.getProperty ("panCc", 10));
        settings.undoSteps = juce::jlimit (0, kMaxUndoSteps, (int) st.getProperty ("undoSteps", 50));
    }

    if (auto pt = root.getChildWithName ("PATTERNS"); pt.isValid())
    {
        for (int i = 0; i < pt.getNumChildren(); ++i)
        {
            auto pv = pt.getChild (i);

            if (! pv.hasType ("PATTERN"))
                continue;

            Pattern p;
            p.id = pv.getProperty ("id", i + 1);
            p.name = pv.getProperty ("name", "Pattern").toString();
            p.length = juce::jlimit (1, kMaxRows, (int) pv.getProperty ("length", 64));
            p.channels = juce::jlimit (1, kMaxTrackerChannels, (int) pv.getProperty ("channels", 1));
            // older states stored one visibility flag for the whole pattern; the
            // current format stores a byte per channel column (see Pattern::vis)
            const bool legacyInst = pv.getProperty ("showInst", true);
            const bool legacyVol = pv.getProperty ("showVol", true);
            const bool legacyPan = pv.getProperty ("showPan", true);
            const bool legacyCmd = pv.getProperty ("showCmd", true);
            p.allocate();

            // rows and channel columns hidden past the visible size are part of the
            // state, so the block can cover a larger rectangle than length x
            // channels (older saves: capacities equal the visible size)
            const int capRows = juce::jlimit (1, kMaxRows,
                                              juce::jmax (p.length, (int) pv.getProperty ("capacity", p.length)));
            const int capCols = juce::jlimit (1, kMaxTrackerChannels,
                                              juce::jmax (p.channels, (int) pv.getProperty ("capacityChannels", p.channels)));

            if (capRows > (int) p.rows.size())
                p.rows.resize ((size_t) capRows);

            for (auto& r : p.rows)
                if (capCols > (int) r.size())
                    r.resize ((size_t) capCols);

            // restore per-column visibility; without the newer property the
            // whole-pattern flags of an older state apply to every column
            juce::MemoryBlock visBlock;
            const juce::String visText = pv.getProperty ("vis").toString();

            if (visText.isNotEmpty() && visBlock.fromBase64Encoding (visText))
            {
                const auto* vsrc = static_cast<const std::uint8_t*> (visBlock.getData());
                const int visCols = juce::jmin ((int) visBlock.getSize(), kMaxTrackerChannels);
                p.vis.resize ((size_t) visCols, kVisDefault);

                for (int c = 0; c < visCols; ++c)
                    p.vis[(size_t) c] = (std::uint8_t) (vsrc[c] & kVisAll);
            }
            else
            {
                std::uint8_t legacy = 0;

                if (legacyInst) legacy = (std::uint8_t) (legacy | kVisInst);
                if (legacyVol) legacy = (std::uint8_t) (legacy | kVisVol);
                if (legacyPan) legacy = (std::uint8_t) (legacy | kVisPan);
                if (legacyCmd) legacy = (std::uint8_t) (legacy | kVisCmd);

                p.vis.assign ((size_t) capCols, legacy);
            }

            if ((int) p.vis.size() < capCols)
                p.vis.resize ((size_t) capCols, kVisDefault);

            juce::MemoryBlock data;

            if (data.fromBase64Encoding (pv.getProperty ("data").toString()))
            {
                const auto* src = static_cast<const juce::int16*> (data.getData());
                const size_t count = data.getSize() / sizeof (juce::int16);
                // the block says which layout it is by its own length: this build
                // writes seven ints to a cell, an older one six, so the cell count
                // the pattern's capacity rectangle asks for tells the two apart
                // rather than the delay column being invented out of an older
                // state's last argument
                const size_t cells = (size_t) capRows * (size_t) capCols;
                const size_t stride = count == cells * 7 ? (size_t) 7 : (size_t) 6;
                size_t i2 = 0;

                for (int r = 0; r < capRows; ++r)
                {
                    for (int c = 0; c < capCols; ++c)
                    {
                        if (i2 + stride > count)
                            break;

                        auto& cell = p.rows[(size_t) r][(size_t) c];
                        cell.note = src[i2++];
                        cell.inst = src[i2++];
                        cell.vol = src[i2++];
                        cell.pan = src[i2++];
                        // an older state's macro calls (command values 128-143, the
                        // M0-M15 kinds) land on the one letter this build has for that
                        // half of the column's history, the arpeggiator call, so their
                        // old argument keeps naming an id. This build's own wheel values
                        // are the bend letter and the two slide letters, all values above
                        // the highest one that build could have written, so only those
                        // read back as themselves and nothing else can reach them by
                        // accident
                        const int rawCmd = (int) src[i2++];
                        cell.cmd = rawCmd >= kArpCmd && rawCmd <= kOlderCmdTop ? kArpCmd : rawCmd;
                        cell.cmdVal = src[i2++];
                        // the seventh int of the cell is the delay column's, which an
                        // older state never wrote: what it has no room for reads as the
                        // column naming nothing rather than as a delay of no percent
                        cell.delay = stride == 7 ? (int) src[i2++] : kEmpty;
                    }
                }
            }

            p.trimHiddenRows();
            p.trimHiddenChannels();
            patterns.push_back (std::move (p));
        }
    }

    if (auto ct = root.getChildWithName ("CLIPS"); ct.isValid())
    {
        for (int i = 0; i < ct.getNumChildren(); ++i)
        {
            auto cv = ct.getChild (i);

            if (! cv.hasType ("CLIP"))
                continue;

            Clip c;
            c.uid = juce::Uuid (cv.getProperty ("uid", juce::Uuid().toString()).toString());
            c.patternId = cv.getProperty ("patternId", 0);
            c.startBeats = cv.getProperty ("startBeats", 0.0);

            if (findPattern (c.patternId) != nullptr)
                clips.push_back (c);
        }
    }

    // an older state's MACROS section (the name/kind/two-parameter macros) is
    // not read at all: those kinds are gone, and the arpeggiator macros this build
    // runs are a different thing, named by id, so the section is simply dropped

    // the ARPS section names every macro that exists, so what the model held
    // before the read is dropped ahead of it: an undo that took a macro away has
    // to leave it undefined again, and a macro whose steps changed has to take
    // the state's steps rather than have them added behind the ones still in
    // place, which is what keeps walking the undo line back a true rewind of the
    // macros as well as of the rows
    for (auto& a : arps)
    {
        a.looped = false;
        a.steps.clear();
    }

    // the VIBS section names every macro that exists, so the vibrato table is
    // emptied the same way ahead of the read: an undo that took a vibrato away has
    // to leave it undefined again rather than keep the shape, amplitude and cycle
    // its knobs had
    for (auto& v : vibs)
        v.present = false;

    // the TRMS section names every tremolo that exists, so the tremolo table is
    // emptied the same way ahead of the read: an undo that took a tremolo away has
    // to leave it undefined again rather than keep the shape, amplitude and cycle
    // its knobs had
    for (auto& t : trems)
        t.present = false;

    if (auto at = root.getChildWithName ("ARPS"); at.isValid())
    {
        for (int i = 0; i < at.getNumChildren(); ++i)
        {
            auto av = at.getChild (i);

            if (! av.hasType ("ARP"))
                continue;

            const int id = juce::jlimit (1, kMaxArpMacros, (int) av.getProperty ("id", 0));
            auto* m = findArp (id);

            if (m == nullptr)
                continue;

            m->looped = av.getProperty ("loop", false);

            // len names the step count beside the block, which is what says how
            // the steps were packed: this build writes two bytes a step, since a
            // note-off is a value below what a single byte could name, and the
            // one-byte blocks older states wrote are widened step by step with
            // their own values intact rather than re-read as pairs
            const int declared = (int) av.getProperty ("len", 0);

            juce::MemoryBlock packed;
            const juce::String stepsText = av.getProperty ("steps").toString();

            if (stepsText.isNotEmpty() && packed.fromBase64Encoding (stepsText))
            {
                const int size = (int) packed.getSize();
                const auto* bsrc = static_cast<const std::uint8_t*> (packed.getData());

                if (declared > 0 && size == declared * 2)
                {
                    const int steps = juce::jmin (size / 2, kMaxArpSteps);

                    for (int s = 0; s < steps; ++s)
                    {
                        const int raw = (int) bsrc[s * 2] | ((int) bsrc[s * 2 + 1] << 8);
                        m->steps.push_back ((std::int16_t) (raw >= 0x8000 ? raw - 0x10000 : raw));
                    }
                }
                else
                {
                    const int steps = juce::jmin (size, kMaxArpSteps);

                    // an older state's steps come back as the same jumps it could
                    // write, and its step that sounds nothing keeps its place, so
                    // widening gives the macro it had rather than a new shape
                    for (int s = 0; s < steps; ++s)
                        m->steps.push_back ((std::int16_t) (std::int8_t) bsrc[s]);
                }
            }

            // len names the step count beside the block, so a state whose steps
            // block came up short is padded with the base note rather than played
            // with fewer steps than it says
            const int claimed = juce::jlimit (1, kMaxArpSteps, declared > 0 ? declared : (int) m->steps.size());

            while ((int) m->steps.size() < claimed)
                m->steps.push_back (0);

            if (m->steps.size() > (size_t) claimed)
                m->steps.resize ((size_t) claimed);
        }
    }

    if (auto vt = root.getChildWithName ("VIBS"); vt.isValid())
    {
        for (int i = 0; i < vt.getNumChildren(); ++i)
        {
            auto vv = vt.getChild (i);

            if (! vv.hasType ("VIB"))
                continue;

            const int id = juce::jlimit (1, kMaxVibMacros, (int) vv.getProperty ("id", 0));
            auto* m = findVib (id);

            if (m == nullptr)
                continue;

            // a state that named the macro makes it one, its own values being read
            // through the same clamps the knobs use, so a saved state cannot reach a
            // shape, an amplitude or a cycle outside what the tab can ask for
            m->present = true;
            m->type = juce::jlimit (0, kVibTypeTop, (int) vv.getProperty ("type", kVibTypeSin));
            m->amplitude = juce::jlimit (0, kVibAmpTop, (int) vv.getProperty ("amp", kVibDefaultAmp));
            m->frequency = juce::jlimit (1, kVibFreqTop, (int) vv.getProperty ("freq", kVibDefaultFreq));
        }
    }

    if (auto tt = root.getChildWithName ("TRMS"); tt.isValid())
    {
        for (int i = 0; i < tt.getNumChildren(); ++i)
        {
            auto tv = tt.getChild (i);

            if (! tv.hasType ("TRM"))
                continue;

            const int id = juce::jlimit (1, kMaxTremMacros, (int) tv.getProperty ("id", 0));
            auto* m = findTrem (id);

            if (m == nullptr)
                continue;

            // a state that named the macro makes it one, its own values being read
            // through the same clamps the knobs use, so a saved state cannot reach a
            // shape, an amplitude or a cycle outside what the tab can ask for
            m->present = true;
            m->type = juce::jlimit (0, kTremTypeTop, (int) tv.getProperty ("type", kTremTypeSin));
            m->amplitude = juce::jlimit (0, kTremAmpTop, (int) tv.getProperty ("amp", kTremDefaultAmp));
            m->frequency = juce::jlimit (1, kTremFreqTop, (int) tv.getProperty ("freq", kTremDefaultFreq));
        }
    }

    nextPatternId = juce::jmax (1, (int) root.getProperty ("nextPatternId", 1));
    activePatternId = (int) root.getProperty ("activePatternId", patterns.empty() ? 0 : patterns.front().id);

    if (patterns.empty())
    {
        auto& p = addPattern ("Pattern 01", 64, 1);
        activePatternId = p.id;
        addClip (p.id, 0.0);
    }

    if (findPattern (activePatternId) == nullptr)
        activePatternId = patterns.front().id;

    for (auto& p : patterns)
        nextPatternId = juce::jmax (nextPatternId, p.id + 1);

    // a state that named no arpeggiator at all gets the default one, the same
    // song reset gives
    if (lastDefArpId() == 0)
        defineArp (1);

    // and the same for the vibratos, so a state older than the letter V still
    // arrives with one wobble on its tab rather than an empty one
    if (lastDefVibId() == 0)
        defineVib (1);

    // and the same again for the tremolos, so a state older than the letter T still
    // arrives with one swing on its tab rather than an empty one
    if (lastDefTremId() == 0)
        defineTrem (1);
}

} // namespace rtr
