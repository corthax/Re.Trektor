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

juce::String valueString (int value, bool hex, int digits)
{
    if (value == kEmpty)
        return juce::String::repeatedString ("-", juce::jmax (1, digits));

    if (hex)
        return juce::String::toHexString (value).toUpperCase().paddedLeft ('0', digits);

    return juce::String (value).paddedLeft (digits == 2 ? '0' : ' ', digits);
}

juce::String instString (int inst, bool hex, bool decimalInstruments)
{
    if (inst == kEmpty)
        return "--";

    if (hex && ! decimalInstruments)
        return juce::String::toHexString (inst).toUpperCase();

    return juce::String (inst);
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
    macros.clear();
    clipboard.clear();
    nextPatternId = 1;
    activePatternId = 0;
    editPlayheadBeats = 0.0;
    transportMirrorBeats = -1.0;

    auto& p = addPattern ("Pattern 01", 64, 1);
    activePatternId = p.id;
    addClip (p.id, 0.0);

    macros.push_back ({ "Arp 7x+4", 0, 7, 4 });
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
juce::ValueTree Song::toValueTree() const
{
    juce::ValueTree root ("RETREKTOR");

    juce::ValueTree st ("SETTINGS");
    st.setProperty ("hexDisplay", settings.hexDisplay, nullptr);
    st.setProperty ("midRowIgnored", settings.midRowIgnored, nullptr);
    st.setProperty ("subticksPerRow", settings.subticksPerRow, nullptr);
    st.setProperty ("rowsPerBeat", settings.rowsPerBeat, nullptr);
    st.setProperty ("defaultPatternRows", settings.defaultPatternRows, nullptr);
    st.setProperty ("pasteAsUnique", settings.pasteAsUnique, nullptr);
    st.setProperty ("snapEnabled", settings.snapEnabled, nullptr);
    st.setProperty ("adaptiveGrid", settings.adaptiveGrid, nullptr);
    st.setProperty ("fixedGridRows", settings.fixedGridRows, nullptr);
    st.setProperty ("recordOctave", settings.recordOctave, nullptr);
    st.setProperty ("stepSkip", settings.stepSkip, nullptr);
    root.appendChild (st, nullptr);

    juce::ValueTree pt ("PATTERNS");

    for (auto& p : patterns)
    {
        juce::ValueTree pv ("PATTERN");
        pv.setProperty ("id", p.id, nullptr);
        pv.setProperty ("name", p.name, nullptr);
        pv.setProperty ("length", p.length, nullptr);
        pv.setProperty ("channels", p.channels, nullptr);
        pv.setProperty ("showInst", p.showInst, nullptr);
        pv.setProperty ("showVol", p.showVol, nullptr);
        pv.setProperty ("showPan", p.showPan, nullptr);
        pv.setProperty ("showCmd", p.showCmd, nullptr);

        // Packed row data: 6 ints per cell over the capacity rectangle, so rows
        // and channel columns hidden past the pattern's visible size (kept by
        // resizing without deleting) survive a state round-trip.
        const int capRows = p.capacity();
        const int capCols = p.channelCapacity();
        static const Cell emptyCell;
        juce::MemoryBlock data;
        data.setSize ((size_t) capRows * (size_t) capCols * 6 * sizeof (juce::int16));

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

    juce::ValueTree mt ("MACROS");

    for (auto& m : macros)
    {
        juce::ValueTree mv ("MACRO");
        mv.setProperty ("name", m.name, nullptr);
        mv.setProperty ("kind", m.kind, nullptr);
        mv.setProperty ("param1", m.param1, nullptr);
        mv.setProperty ("param2", m.param2, nullptr);
        mt.appendChild (mv, nullptr);
    }

    root.appendChild (mt, nullptr);

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
    macros.clear();

    if (auto st = root.getChildWithName ("SETTINGS"); st.isValid())
    {
        settings.hexDisplay = st.getProperty ("hexDisplay", false);
        settings.midRowIgnored = st.getProperty ("midRowIgnored", true);
        settings.subticksPerRow = st.getProperty ("subticksPerRow", 6);
        settings.rowsPerBeat = juce::jmax (1, (int) st.getProperty ("rowsPerBeat", 4));
        settings.defaultPatternRows = juce::jlimit (1, kMaxRows, (int) st.getProperty ("defaultPatternRows", 64));
        settings.pasteAsUnique = st.getProperty ("pasteAsUnique", false);
        settings.snapEnabled = st.getProperty ("snapEnabled", true);
        settings.adaptiveGrid = st.getProperty ("adaptiveGrid", true);
        settings.fixedGridRows = juce::jmax (1, (int) st.getProperty ("fixedGridRows", 4));
        settings.recordOctave = juce::jlimit (0, 9, (int) st.getProperty ("recordOctave", 4));
        settings.stepSkip = juce::jlimit (0, 16, (int) st.getProperty ("stepSkip", 1));
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
            p.showInst = pv.getProperty ("showInst", true);
            p.showVol = pv.getProperty ("showVol", true);
            p.showPan = pv.getProperty ("showPan", true);
            p.showCmd = pv.getProperty ("showCmd", true);
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

            juce::MemoryBlock data;

            if (data.fromBase64Encoding (pv.getProperty ("data").toString()))
            {
                const auto* src = static_cast<const juce::int16*> (data.getData());
                const size_t count = data.getSize() / sizeof (juce::int16);
                size_t i2 = 0;

                for (int r = 0; r < capRows; ++r)
                {
                    for (int c = 0; c < capCols; ++c)
                    {
                        if (i2 + 6 > count)
                            break;

                        auto& cell = p.rows[(size_t) r][(size_t) c];
                        cell.note = src[i2++];
                        cell.inst = src[i2++];
                        cell.vol = src[i2++];
                        cell.pan = src[i2++];
                        cell.cmd = src[i2++];
                        cell.cmdVal = src[i2++];
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

    if (auto mt = root.getChildWithName ("MACROS"); mt.isValid())
    {
        for (int i = 0; i < mt.getNumChildren(); ++i)
        {
            auto mv = mt.getChild (i);

            if (! mv.hasType ("MACRO"))
                continue;

            Macro m;
            m.name = mv.getProperty ("name", "Macro").toString();
            m.kind = mv.getProperty ("kind", 0);
            m.param1 = mv.getProperty ("param1", 0);
            m.param2 = mv.getProperty ("param2", 0);
            macros.push_back (m);
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

    if (macros.empty())
        macros.push_back ({ "Arp 7x+4", 0, 7, 4 });
}

} // namespace rtr
