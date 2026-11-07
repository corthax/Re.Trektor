#include "TrackerComponent.h"
#include "Theme.h"
#include "AltShortcutKeys.h"

namespace rtr
{

namespace
{
    constexpr int kFieldPad = 5;
    constexpr int kChannelGap = 12;

    bool isHexDigitChar (juce::juce_wchar c)
    {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    int noteOffsetForKey (juce::juce_wchar c)
    {
        switch (c)
        {
            case 'z': return 0;
            case 's': return 1;
            case 'x': return 2;
            case 'd': return 3;
            case 'c': return 4;
            case 'v': return 5;
            case 'g': return 6;
            case 'b': return 7;
            case 'h': return 8;
            case 'n': return 9;
            case 'j': return 10;
            case 'm': return 11;
            case 'q': return 12;
            case '2': return 13;
            case 'w': return 14;
            case '3': return 15;
            case 'e': return 16;
            case 'r': return 17;
            case '5': return 18;
            case 't': return 19;
            case '6': return 20;
            case 'y': return 21;
            case '7': return 22;
            case 'u': return 23;
            default: return -1;
        }
    }
}

//==============================================================================
TrackerComponent::TrackerComponent (ReTrektorAudioProcessor& processor)
    : proc (processor), song (processor.getSong())
{
    setOpaque (true);
    setWantsKeyboardFocus (true);

    rowsEditor.setFont (theme::monoFont (12.0f));
    rowsEditor.setJustification (juce::Justification::centred);
    rowsEditor.setInputRestrictions (4, "0123456789");
    rowsEditor.setColour (juce::TextEditor::backgroundColourId, theme::panelLight);
    rowsEditor.setColour (juce::TextEditor::textColourId, theme::text);
    rowsEditor.setColour (juce::TextEditor::outlineColourId, theme::outline);
    rowsEditor.setColour (juce::TextEditor::focusedOutlineColourId, theme::accent);
    rowsEditor.setIndents (2, 1);
    rowsEditor.setText (juce::String (pattern().length), false);
    rowsEditor.onReturnKey = [this]
    {
        commitRowsEditor();
        grabKeyboardFocus();
    };
    rowsEditor.onFocusLost = [this] { commitRowsEditor(); };
    addAndMakeVisible (rowsEditor);

    startTimerHz (30);
}

void TrackerComponent::commitRowsEditor()
{
    const int newRows = juce::jlimit (1, kMaxRows, rowsEditor.getText().getIntValue());
    rowsEditor.setText (juce::String (newRows), false);

    auto& p = pattern();

    if (newRows != p.length)
    {
        // make sure all instances of this pattern still fit on the timeline
        bool fits = true;

        for (auto& clip : song.clips)
        {
            if (clip.patternId != p.id)
                continue;

            const double oldLen = song.patternLengthBeats (p.id);
            const double newLen = (double) newRows * song.beatsPerRow();
            const juce::Uuid ignore = clip.uid;

            if (! song.canPlace (clip.startBeats, newLen, &ignore))
            {
                fits = false;
                break;
            }

            juce::ignoreUnused (oldLen);
        }

        if (! fits)
        {
            rowsEditor.setText (juce::String (p.length), false);
            return;
        }

        p.setLength (newRows);
        cursorRow = juce::jlimit (0, p.length - 1, cursorRow);

        if (onModelChanged)
            onModelChanged();

        repaint();
    }
}

//==============================================================================
Pattern& TrackerComponent::pattern()
{
    auto* p = song.activePattern();

    if (p == nullptr)
    {
        // should not happen, but keep a safe fallback
        song.addPattern();
        p = song.activePattern();
    }

    return *p;
}

const Pattern& TrackerComponent::pattern() const
{
    return const_cast<TrackerComponent*> (this)->pattern();
}

void TrackerComponent::refresh()
{
    auto& p = pattern();
    cursorRow = juce::jlimit (0, p.length - 1, cursorRow);
    cursorChannel = juce::jlimit (0, p.channels - 1, cursorChannel);
    // the selected cells are left alone: the accessors only report cells inside
    // the pattern that is showing, so shrinking or switching patterns cannot
    // lose a selection the user comes back to
    rowsEditor.setText (juce::String (p.length), false);
    layoutFields();
    clampScroll();
    repaint();
}

//==============================================================================
void TrackerComponent::layoutFields()
{
    auto& p = pattern();
    fieldRects.clear();
    channelRects.clear();

    const float charW = theme::monoFont (13.0f).getStringWidthFloat ("0");

    // field widths follow the widest text a field can show, so a decimal volume
    // of 0-127 (three digits) is never clipped to two digits
    const int noteW = (int) (charW * 4.0f) + kFieldPad;
    const int instW = (int) (charW * (float) fieldGlyphs (Field::inst)) + kFieldPad;
    const int volW = (int) (charW * (float) fieldGlyphs (Field::vol)) + kFieldPad;
    const int panW = (int) (charW * (float) fieldGlyphs (Field::pan)) + kFieldPad;
    const int cmdW = (int) (charW * (float) (fieldGlyphs (Field::cmdNum) + fieldGlyphs (Field::cmdVal))) + kFieldPad;

    int x = gutterWidth - scrollX;

    for (int ch = 0; ch < p.channels; ++ch)
    {
        const int startX = x;

        auto add = [&] (Field f, int w)
        {
            fieldRects.push_back ({ f, ch, { x, 0, w, rowHeight } });
            x += w;
        };

        add (Field::note, noteW);

        if (p.showColumn (ch, 0))
            add (Field::inst, instW);

        if (p.showColumn (ch, 1))
            add (Field::vol, volW);

        if (p.showColumn (ch, 2))
            add (Field::pan, panW);

        if (p.showColumn (ch, 3))
        {
            const int half = cmdW / 2;
            fieldRects.push_back ({ Field::cmdNum, ch, { x, 0, half, rowHeight } });
            fieldRects.push_back ({ Field::cmdVal, ch, { x + half, 0, cmdW - half, rowHeight } });
            x += cmdW;
        }

        // the channel's cell spans the fields it shows, and stops before the
        // gap that separates the channels, so a selected cell reads as its own
        // column rather than as a band across the pattern
        channelRects.push_back ({ startX, 0, x - startX, rowHeight });

        x += kChannelGap;
    }

    totalWidth = x + scrollX;
}

void TrackerComponent::resized()
{
    layoutFields();
    rowsEditor.setBounds (4, headerHeight - 21, gutterWidth - 12, 17);
}

//==============================================================================
juce::Rectangle<int> TrackerComponent::fieldBounds (int channel, Field field) const
{
    for (auto& fr : fieldRects)
        if (fr.channel == channel && fr.field == field)
            return fr.bounds;

    return {};
}

juce::Rectangle<int> TrackerComponent::toggleBounds (int channel, int index) const
{
    auto note = fieldBounds (channel, Field::note);

    if (note.isEmpty())
        return {};

    const int w = juce::jmax (7, (note.getWidth() - 2) / 4);
    return { note.getX() + 1 + index * w, 18, juce::jmax (6, w - 1), 13 };
}

int TrackerComponent::fieldIndexAtX (int x) const
{
    if (x < gutterWidth)
        return -1; // the gutter belongs to the row numbers, not to a column

    for (int i = 0; i < (int) fieldRects.size(); ++i)
    {
        auto b = visibleColumn (fieldRects[(size_t) i].bounds);

        if (x >= b.getX() - 2 && x < b.getRight() + 2)
            return i;
    }

    return -1;
}

/** The channel whose column an x sits in, for the selection rectangle. The
    gutter stands for the first channel and everything past the last field
    stands for the last one, so a drag that wanders off the columns still
    names cells instead of dropping the edge of the rectangle. */
int TrackerComponent::channelAtX (int x) const
{
    const int idx = fieldIndexAtX (x);

    if (idx >= 0)
        return fieldRects[(size_t) idx].channel;

    if (x <= gutterWidth || fieldRects.empty())
        return 0;

    return fieldRects.back().channel;
}

int TrackerComponent::rowAtY (int y) const noexcept
{
    if (y < headerHeight)
        return -1;

    return scrollRow + (y - headerHeight) / rowHeight;
}

int TrackerComponent::visibleRowCount() const noexcept
{
    // the bottom strip holds the horizontal scrollbar when the pattern's
    // columns are wider than the window, and then takes rows away
    return juce::jmax (1, (getHeight() - headerHeight - scrollbarStrip()) / rowHeight + 1);
}

int TrackerComponent::scrollbarStrip() const noexcept
{
    return totalWidth > getWidth() ? scrollbarHeight : 0;
}

int TrackerComponent::maxScrollX() const noexcept
{
    return juce::jmax (0, totalWidth - getWidth());
}

juce::Rectangle<int> TrackerComponent::scrollbarArea() const
{
    if (scrollbarStrip() == 0)
        return {};

    return { 0, getHeight() - scrollbarHeight, getWidth(), scrollbarHeight };
}

int TrackerComponent::thumbWidthFor (int trackWidth) const noexcept
{
    const int colsTotal = juce::jmax (1, totalWidth - gutterWidth);
    const int colsVisible = juce::jmax (1, getWidth() - gutterWidth);
    const int minWidth = juce::jmin (16, trackWidth);

    return juce::jlimit (minWidth, juce::jmax (1, trackWidth),
                         (int) ((double) trackWidth * colsVisible / colsTotal));
}

juce::Rectangle<int> TrackerComponent::thumbBounds() const
{
    const auto bar = scrollbarArea();

    if (bar.isEmpty())
        return {};

    const int trackX = gutterWidth + 2;
    const int trackWidth = juce::jmax (1, getWidth() - trackX - 2);
    const int thumbW = thumbWidthFor (trackWidth);
    const int travel = juce::jmax (1, trackWidth - thumbW);
    const int maxScroll = juce::jmax (1, maxScrollX());
    const int scroll = juce::jlimit (0, maxScroll, scrollX);

    return { trackX + (int) std::lround ((double) travel * scroll / maxScroll),
             bar.getY() + 3, thumbW, bar.getHeight() - 6 };
}

int TrackerComponent::scrollXFromThumbX (int thumbX) const noexcept
{
    const int trackX = gutterWidth + 2;
    const int trackWidth = juce::jmax (1, getWidth() - trackX - 2);
    const int travel = juce::jmax (1, trackWidth - thumbWidthFor (trackWidth));

    const double frac = (double) (thumbX - trackX) / (double) travel;
    return juce::jlimit (0, maxScrollX(), (int) std::lround (frac * maxScrollX()));
}

void TrackerComponent::setScrollX (int x)
{
    scrollX = juce::jlimit (0, maxScrollX(), x);
    layoutFields();
}

void TrackerComponent::clampScroll()
{
    scrollRow = juce::jlimit (0, juce::jmax (0, pattern().length - visibleRowCount()), scrollRow);
    scrollX = juce::jlimit (0, maxScrollX(), scrollX);
    layoutFields();
}

juce::Rectangle<int> TrackerComponent::visibleColumn (juce::Rectangle<int> r) const
{
    // a scrolled column may reach into the row-number gutter: keep it clipped
    // to the columns area so the row numbers stay readable
    if (r.isEmpty() || r.getRight() <= gutterWidth)
        return {};

    if (r.getX() < gutterWidth)
        r.setBounds (gutterWidth, r.getY(), r.getRight() - gutterWidth, r.getHeight());

    if (r.getRight() > getWidth())
        r.setBounds (r.getX(), r.getY(), getWidth() - r.getX(), r.getHeight());

    if (r.isEmpty())
        return {};

    return r;
}

juce::Rectangle<int> TrackerComponent::cursorBounds() const
{
    const int row = cursorRow;
    const int y = headerHeight + (row - scrollRow) * rowHeight;
    auto b = fieldBounds (cursorChannel, cursorField);
    return { b.getX(), y, b.getWidth(), rowHeight };
}

//==============================================================================
void TrackerComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
    layoutFields();

    auto& p = pattern();

    auto area = getLocalBounds();
    auto header = area.removeFromTop (headerHeight);

    drawHeader (g, header);

    const auto bar = scrollbarArea();

    if (! bar.isEmpty())
        area.removeFromBottom (scrollbarHeight);

    g.saveState();
    g.reduceClipRegion (area);
    drawRows (g, area);
    g.restoreState();

    // horizontal scrollbar: the channel columns can be far wider than the
    // window, so a draggable thumb at the bottom edge reaches the rest
    if (! bar.isEmpty())
        drawScrollbar (g, bar);

    // vertical scrollbar hint
    if (p.length > visibleRowCount())
    {
        const int trackH = area.getHeight();
        const int thumbH = juce::jmax (24, trackH * visibleRowCount() / p.length);
        const int maxScroll = juce::jmax (1, p.length - visibleRowCount());
        const int y = area.getY() + (trackH - thumbH) * scrollRow / maxScroll;

        g.setColour (theme::outline);
        g.fillRoundedRectangle ((float) getWidth() - 4.0f, (float) y, 3.0f, (float) thumbH, 1.5f);
    }
}

void TrackerComponent::drawHeader (juce::Graphics& g, juce::Rectangle<int> header)
{
    auto& p = pattern();

    g.setColour (theme::panel);
    g.fillRect (header);
    g.setColour (theme::outline);
    g.drawHorizontalLine (header.getBottom() - 1, 0.0f, (float) getWidth());

    g.setFont (theme::monoFont (11.0f));
    g.setColour (theme::textDim);
    g.drawText ("ROWS", 4, 2, gutterWidth - 8, 13, juce::Justification::centredLeft, false);

    g.setColour (theme::outline);
    g.drawVerticalLine (gutterWidth - 2, 0.0f, (float) header.getBottom());

    for (int ch = 0; ch < p.channels; ++ch)
    {
        auto noteRect = visibleColumn (fieldBounds (ch, Field::note));

        if (noteRect.isEmpty())
            continue;

        const int headerTop = 2;
        g.setFont (theme::monoFont (11.0f));
        g.setColour (ch == cursorChannel ? theme::accent : theme::textDim);
        g.drawText (juce::String (ch).paddedLeft ('0', 2),
                    noteRect.getX(), headerTop, noteRect.getWidth(), 14,
                    juce::Justification::centred, false);

        // Column visibility chips, always shown so a hidden column can be
        // restored. Hiding one inverts the chip instead of dimming it: the letter
        // is cut out of a plate in the letter's own colour, so a hidden chip is
        // exactly as legible as a shown one and only its fill says which state
        // the field is in.
        const char* labels[4] = { "I", "V", "P", "C" };
        const bool states[4] = { p.showColumn (ch, 0), p.showColumn (ch, 1),
                                 p.showColumn (ch, 2), p.showColumn (ch, 3) };

        for (int i = 0; i < 4; ++i)
        {
            auto btn = visibleColumn (toggleBounds (ch, i));

            if (btn.isEmpty())
                continue;

            const bool shown = states[i];
            const auto plate = shown ? theme::panelLight : theme::text;
            const auto ink = shown ? theme::text : theme::panelLight;

            g.setColour (plate);
            g.fillRoundedRectangle (btn.toFloat(), 2.0f);
            g.setColour (theme::textDim);
            g.drawRoundedRectangle (btn.toFloat(), 2.0f, 1.0f);
            g.setColour (ink);
            g.setFont (theme::monoFont (8.5f));
            g.drawText (labels[i], btn, juce::Justification::centred, false);
        }
    }
}

void TrackerComponent::drawScrollbar (juce::Graphics& g, juce::Rectangle<int> bar)
{
    const int trackX = gutterWidth + 2;
    const int trackWidth = juce::jmax (1, getWidth() - trackX - 2);

    // the gutter side is kept free: row numbers stay put while scrolling
    g.setColour (theme::background.darker (0.35f));
    g.fillRect (trackX, bar.getY() + 2, trackWidth, bar.getHeight() - 4);

    g.setColour (theme::outline.withAlpha (0.6f));
    g.drawRoundedRectangle (juce::Rectangle<float> ((float) trackX, (float) bar.getY() + 2.0f,
                                                   (float) trackWidth, (float) bar.getHeight() - 4.0f),
                            2.0f, 1.0f);

    const auto thumb = thumbBounds();

    g.setColour (theme::panelLight);
    g.fillRoundedRectangle (thumb.toFloat(), 3.0f);
    g.setColour (draggingBar ? theme::accent : theme::outline);
    g.drawRoundedRectangle (thumb.toFloat(), 3.0f, 1.0f);

    g.setColour (theme::textDim);

    for (int i = -1; i <= 1; ++i)
    {
        const int lineY = bar.getCentreY() + i * 3;
        g.drawHorizontalLine (lineY, (float) thumb.getX() + 4.0f,
                              (float) juce::jmax (thumb.getX() + 4, thumb.getRight() - 4));
    }
}

void TrackerComponent::drawRows (juce::Graphics& g, juce::Rectangle<int> area)
{
    auto& p = pattern();
    const int rowsPerBeat = juce::jmax (1, song.settings.rowsPerBeat);
    const bool hex = song.settings.hexDisplay;

    // carry tables for display
    struct Eff { int inst = -1, vol = -1, pan = -1, cmd = -1; };
    std::vector<std::vector<Eff>> eff ((size_t) p.channels);

    for (int ch = 0; ch < p.channels; ++ch)
    {
        eff[(size_t) ch].resize ((size_t) p.length);
        Eff c;

        for (int r = 0; r < p.length; ++r)
        {
            const auto& cell = p.rows[(size_t) r][(size_t) ch];

            if (cell.inst != kEmpty) c.inst = cell.inst;
            if (cell.vol != kEmpty) c.vol = cell.vol;
            if (cell.pan != kEmpty) c.pan = cell.pan;
            if (cell.cmd != kEmpty) c.cmd = cell.cmd;

            eff[(size_t) ch][(size_t) r] = c;
        }
    }

    const int firstRow = juce::jlimit (0, juce::jmax (0, p.length - 1), scrollRow);
    const int lastRow = juce::jmin (p.length - 1, firstRow + visibleRowCount());

    auto beep = juce::Colour (theme::background).brighter (0.035f);

    for (int row = firstRow; row <= lastRow; ++row)
    {
        const int y = headerHeight + (row - scrollRow) * rowHeight;
        juce::Rectangle<int> rowRect (0, y, getWidth(), rowHeight);

        if ((row / rowsPerBeat) % 2 == 1)
        {
            g.setColour (beep);
            g.fillRect (rowRect);
        }

        if (row == playRow)
        {
            g.setColour (theme::playhead.withAlpha (0.12f));
            g.fillRect (rowRect);
        }

        if (row == cursorRow)
        {
            g.setColour (theme::rowHighlight.brighter (0.06f));
            g.fillRect (rowRect.withTrimmedLeft (gutterWidth));
        }

        // row number
        g.setColour (row % (rowsPerBeat * 4) == 0 ? theme::textDim.brighter (0.2f) : theme::textDim.withAlpha (0.7f));
        g.setFont (theme::monoFont (12.0f));
        g.drawText (hex ? juce::String::toHexString (row).toUpperCase().paddedLeft ('0', 3)
                        : juce::String (row).paddedLeft ('0', 3),
                    4, y, gutterWidth - 10, rowHeight, juce::Justification::centredRight, false);

        // cursor row marker in gutter
        if (row == cursorRow)
        {
            g.setColour (theme::accent.withAlpha (0.8f));
            g.fillRect (1, y + 1, 2, rowHeight - 2);
        }

        for (int ch = 0; ch < p.channels; ++ch)
        {
            const auto& cell = p.rows[(size_t) row][(size_t) ch];
            const auto& carried = eff[(size_t) ch][(size_t) row];

            // the selection is per cell, so it tints one channel column of this
            // row instead of a band across the whole pattern
            if (isCellSelected (row, ch))
            {
                const auto bounds = ch < (int) channelRects.size()
                                        ? channelRects[(size_t) ch]
                                        : juce::Rectangle<int>();
                const auto span = visibleColumn ({ bounds.getX(), y, bounds.getWidth(), rowHeight });

                if (! span.isEmpty())
                {
                    g.setColour (theme::selection.withAlpha (0.45f));
                    g.fillRect (span);
                }
            }

            const bool isCursorCh = row == cursorRow && ch == cursorChannel;
            const bool notePresent = cell.note != kEmpty;

            for (auto& fr : fieldRects)
            {
                if (fr.channel != ch)
                    continue;

                auto r = visibleColumn ({ fr.bounds.getX(), y, fr.bounds.getWidth(), rowHeight });

                if (r.isEmpty())
                    continue;

                const bool cursorHere = isCursorCh && fr.field == cursorField;

                if (cursorHere)
                {
                    g.setColour (theme::selection.brighter (0.15f));
                    g.fillRect (r.reduced (1, 1));
                    g.setColour (theme::cursor);
                    g.drawRect (r.reduced (1, 1), 1);
                }

                juce::String text;
                juce::Colour colour = theme::text;
                bool dim = false;

                switch (fr.field)
                {
                    case Field::note:
                        if (cell.note == kEmpty)
                        {
                            text = "---";
                            colour = theme::textDim.withAlpha (0.55f);
                        }
                        else if (cell.note == kNoteOff)
                        {
                            text = "===";
                            colour = theme::noteOff;
                        }
                        else
                        {
                            text = noteName (cell.note);
                            colour = theme::note;
                        }

                        break;

                    case Field::inst:
                        if (cell.inst != kEmpty)
                        {
                            text = "-" + instString (cell.inst, hex, ! hex);
                            colour = theme::inst;
                        }
                        else if (carried.inst >= 0 && notePresent)
                        {
                            text = "-" + instString (carried.inst, hex, ! hex);
                            colour = theme::inst;
                            dim = true;
                        }
                        else
                        {
                            text = "--";
                            colour = theme::textDim.withAlpha (0.45f);
                        }

                        break;

                    case Field::vol:
                        if (cell.vol != kEmpty)
                        {
                            text = valueString (cell.vol, hex, fieldGlyphs (Field::vol));
                            colour = theme::vol;
                        }
                        else if (carried.vol >= 0 && notePresent)
                        {
                            text = valueString (carried.vol, hex, fieldGlyphs (Field::vol));
                            colour = theme::vol;
                            dim = true;
                        }
                        else
                        {
                            text = hex ? "--" : "---";
                            colour = theme::textDim.withAlpha (0.45f);
                        }

                        break;

                    case Field::pan:
                        if (cell.pan != kEmpty)
                        {
                            text = valueString (cell.pan, hex, 3);
                            colour = theme::pan;
                        }
                        else if (carried.pan >= 0 && notePresent)
                        {
                            text = valueString (carried.pan, hex, 3);
                            colour = theme::pan;
                            dim = true;
                        }
                        else
                        {
                            text = "---";
                            colour = theme::textDim.withAlpha (0.45f);
                        }

                        break;

                    case Field::cmdNum:
                    case Field::cmdVal:
                    {
                        const bool hasOwn = cell.cmd != kEmpty;
                        const int cmdNum = hasOwn ? cell.cmd : carried.cmd;
                        // a row that names only an argument still shows it, under
                        // the command number the column carries down from above
                        const int cmdVal = cell.cmdVal;

                        if (fr.field == Field::cmdNum)
                        {
                            if (cmdNum >= 0)
                            {
                                text = valueString (cmdNum, hex, fieldGlyphs (Field::cmdNum));
                                colour = cmdNum >= kMacroCmdBase ? theme::cmdMacro : theme::cmdCC;
                                dim = ! hasOwn;
                            }
                            else
                            {
                                text = hex ? "--" : "---";
                                colour = theme::textDim.withAlpha (0.45f);
                            }
                        }
                        else
                        {
                            if (cmdVal >= 0)
                            {
                                text = valueString (cmdVal, hex, fieldGlyphs (Field::cmdVal));
                                colour = theme::cmdCC;
                            }
                            else
                            {
                                text = hex ? "--" : "---";
                                colour = theme::textDim.withAlpha (0.45f);
                            }
                        }

                        if (cursorHere && editing)
                            text = editBuffer;

                        break;
                    }
                }

                if (cursorHere && editing && fr.field != Field::cmdNum)
                    text = editBuffer;

                g.setFont (theme::monoFont (13.0f));
                g.setColour (dim ? colour.withAlpha (0.42f) : colour);
                g.drawText (text, r.withTrimmedLeft (2), juce::Justification::centredLeft, false);
            }
        }

        // beat separator
        if (row % rowsPerBeat == 0)
        {
            g.setColour (theme::beatLine.withAlpha (0.5f));
            g.drawHorizontalLine (y, 0.0f, (float) getWidth());
        }
    }

    // bottom line
    const int lastY = headerHeight + (lastRow - scrollRow + 1) * rowHeight;
    g.setColour (theme::outline.withAlpha (0.4f));
    g.drawHorizontalLine (lastY, 0.0f, (float) getWidth());
}

//==============================================================================
void TrackerComponent::timerCallback()
{
    const int newPlayRow = currentPlayRow();

    if (newPlayRow != playRow)
    {
        playRow = newPlayRow;

        if (playRow >= 0)
        {
            // keep the play row visible
            if (playRow < scrollRow || playRow > scrollRow + visibleRowCount() - 2)
                scrollRow = juce::jlimit (0, juce::jmax (0, pattern().length - visibleRowCount()), playRow - 2);
        }

        repaint();
    }
}

int TrackerComponent::currentPlayRow() const
{
    const auto transport = proc.getTransport();

    if (! transport.playing)
        return -1;

    for (auto& clip : song.clips)
    {
        if (clip.patternId != song.activePatternId)
            continue;

        const double len = song.clipLengthBeats (clip);

        if (transport.ppq >= clip.startBeats && transport.ppq < clip.startBeats + len)
            return juce::jlimit (0, pattern().length - 1,
                                 (int) ((transport.ppq - clip.startBeats) * song.settings.rowsPerBeat));
    }

    return -1;
}

//==============================================================================
void TrackerComponent::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();

    // bottom strip: grab the horizontal scrollbar
    if (scrollbarArea().contains (e.getPosition()))
    {
        draggingBar = true;

        const auto thumb = thumbBounds();
        dragOffsetX = thumb.contains (e.getPosition()) ? e.x - thumb.getX()
                                                       : thumb.getWidth() / 2;

        setScrollX (scrollXFromThumbX (e.x - dragOffsetX));
        repaint();
        return;
    }

    const int row = rowAtY (e.y);

    // header: toggle column visibility
    if (row < 0)
    {
        if (e.y < headerHeight && e.x > gutterWidth)
        {
            auto& p = pattern();

            const auto chipField = [] (int i)
            {
                constexpr Field fields[4] = { Field::inst, Field::vol, Field::pan, Field::cmdNum };
                return i >= 0 && i < 4 ? fields[i] : Field::note;
            };

            for (int ch = 0; ch < p.channels; ++ch)
            {
                for (int i = 0; i < 4; ++i)
                {
                    if (! toggleBounds (ch, i).contains (e.getPosition()))
                        continue;

                    // one chip hides one field of one channel only
                    p.toggleColumn (ch, i);

                    if (cursorChannel == ch && ! p.showColumn (ch, i)
                        && (cursorField == chipField (i) || (i == 3 && cursorField == Field::cmdVal)))
                    {
                        // the cursor cannot sit in a field just hidden: park it
                        cursorField = Field::note;
                        editing = false;
                        editBuffer.clear();
                    }

                    layoutFields();
                    repaint();

                    if (onModelChanged)
                        onModelChanged();

                    return;
                }
            }
        }

        return;
    }

    const int fieldIdx = fieldIndexAtX (e.x);

    if (fieldIdx < 0 || row >= pattern().length)
        return;

    if (e.mods.isPopupMenu())
    {
        cursorRow = row;
        cursorChannel = fieldRects[(size_t) fieldIdx].channel;
        cursorField = fieldRects[(size_t) fieldIdx].field;
        showContextMenu (e);
        return;
    }

    cursorRow = juce::jlimit (0, pattern().length - 1, row);
    cursorChannel = fieldRects[(size_t) fieldIdx].channel;
    cursorField = fieldRects[(size_t) fieldIdx].field;
    editing = false;
    editBuffer.clear();

    // a click now selects the cell it lands on rather than the whole row, and
    // the modifier it is holding says what this click and the drag that follows
    // do to the set of selected cells: Shift unions, Ctrl subtracts, a plain
    // click hands the selection over to this cell and the rectangle it sweeps
    selectOp = gestureOp (e);
    selAnchorRow = cursorRow;
    selAnchorChannel = cursorChannel;
    selDragRow = cursorRow;
    selDragChannel = cursorChannel;
    applyCellRect (cursorRow, cursorChannel, cursorRow, cursorChannel, selectOp);
    selecting = true;

    repaint();
}

void TrackerComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingBar)
    {
        setScrollX (scrollXFromThumbX (e.x - dragOffsetX));
        repaint();
        return;
    }

    if (! selecting)
        return;

    const int row = juce::jlimit (0, pattern().length - 1, rowAtY (e.y));
    const int ch = channelAtX (e.x);

    // the corner that started the gesture keeps its place, so sweeping back
    // shortens the rectangle being drawn instead of turning it around, and a
    // modifier pressed or released mid-gesture changes the operation from there
    const auto op = gestureOp (e);

    if (row == selDragRow && ch == selDragChannel && op == selectOp)
        return;

    applyCellRect (selAnchorRow, selAnchorChannel, row, ch, op);
    selDragRow = row;
    selDragChannel = ch;
    selectOp = op;
    repaint();
}

void TrackerComponent::mouseUp (const juce::MouseEvent&)
{
    selecting = false;
    draggingBar = false;
}

void TrackerComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isShiftDown())
    {
        setScrollX (scrollX - (int) (wheel.deltaY * 120.0f));
        repaint();
        return;
    }

    const int delta = (int) std::round (-wheel.deltaY * 6.0f);
    scrollRow = juce::jlimit (0, juce::jmax (0, pattern().length - 1), scrollRow + delta);
    repaint();
}

void TrackerComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int row = rowAtY (e.y);
    const int fieldIdx = fieldIndexAtX (e.x);

    if (row < 0 || fieldIdx < 0)
        return;

    cursorRow = row;
    cursorChannel = fieldRects[(size_t) fieldIdx].channel;
    cursorField = fieldRects[(size_t) fieldIdx].field;

    clearCurrentField();
}

//==============================================================================
void TrackerComponent::showContextMenu (const juce::MouseEvent& e)
{
    // the four clears share one submenu so the top level stays about what the
    // cursor is on: the ids do not change, only where the items are presented,
    // and the callback below still answers 1..4 by them
    juce::PopupMenu menu;

    juce::PopupMenu clearMenu;
    clearMenu.addItem (4, "Selection", hasSelection());
    clearMenu.addItem (1, "Cell");
    clearMenu.addItem (2, "Channel");
    clearMenu.addItem (3, "Pattern");
    menu.addSubMenu ("Clear", clearMenu);

    menu.addItem (5, "Copy selection", hasSelection());
    menu.addItem (6, "Cut selection", hasSelection());

    juce::PopupMenu pasteMenu;
    pasteMenu.addItem (7, "Everything");
    pasteMenu.addItem (8, "Note");
    pasteMenu.addItem (9, "Instrument");
    pasteMenu.addItem (10, "Volume");
    pasteMenu.addItem (11, "Pan");
    pasteMenu.addItem (12, "Command");
    pasteMenu.addItem (13, "Command Value");
    menu.addSubMenu ("Paste", pasteMenu, hasCellClipboard());

    // the undo pair sits last so the menu reads as the actions the cursor is on
    // first and the steps along the state line after them; the ids continue the
    // paste submenu's because one callback sees all of them by number alone,
    // and the two counts grey the pair out at the ends of the line
    menu.addItem (14, "Undo", proc.getUndoDepth() > 0);
    menu.addItem (15, "Redo", proc.getUndoRedoCount() > 0);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition (),
                        [this] (int result)
                        {
                            auto& p = pattern();

                            // 1..4 are the "Clear" submenu's leaves, 5 and 6 are
                            // the top-level copy and cut, 7..13 the "Paste"
                            // submenu's and 14/15 the top-level undo pair
                            if (result == 1)
                            {
                                clearCurrentField();
                            }
                            else if (result == 2)
                            {
                                for (int r = 0; r < p.length; ++r)
                                    for (auto& fr : fieldRects)
                                        if (fr.channel == cursorChannel)
                                            clearFieldAt (r, cursorChannel, fr.field);

                                markChanged();
                            }
                            else if (result == 3)
                            {
                                p.clearAll();
                                markChanged();
                            }
                            else if (result == 4 && hasSelection())
                            {
                                // only the selected cells lose their contents: a
                                // cell-level selection means the rows through
                                // them and the columns beside them stay put
                                clearSelectedCells();
                                markChanged();
                            }
                            else if (result == 5 && hasSelection())
                            {
                                copySelectionToClipboard();
                            }
                            else if (result == 6 && hasSelection())
                            {
                                // the item the Alt+X key answers as well: the
                                // cells go to the clipboard and the ones it took
                                // are emptied, so a following paste moves them
                                cutSelectionToClipboard();
                            }
                            else if (result >= 7 && result <= 13 && hasCellClipboard())
                            {
                                // the submenu items map to the paste filter bits:
                                // 7 is Everything, 8..13 name one field each
                                static const int pasteMasks[] = { kPasteAll, kPasteNote,
                                                                  kPasteInst, kPasteVol,
                                                                  kPastePan, kPasteCmd,
                                                                  kPasteCmdVal };
                                pasteClipboardAtCursor (pasteMasks[result - 7]);
                            }
                            else if (result == 14)
                            {
                                // the same shape as the Ctrl+Z key: a half-typed
                                // value is what the press closes, and pressing
                                // again walks back. Stepping the line is the
                                // editor's, which refreshes everything and leaves
                                // the undo point the model is in where it was
                                if (! cancelPendingEdit())
                                    undoStep();
                            }
                            else if (result == 15)
                            {
                                redoStep();
                            }

                            // the menu is closed either way, including when the
                            // press dismissed it or picked a greyed item, so hand
                            // keyboard focus back: the popup was a window with no
                            // parent component and closing it leaves nothing
                            // focused, so a press is only heard by the editor's
                            // own shortcuts and the note entry here dies until the
                            // grid is clicked again
                            if (focusRescue)
                                focusRescue();

                            repaint();
                        });
}

void TrackerComponent::markChanged()
{
    if (onModelChanged)
        onModelChanged();

    if (onPatternEdited)
        onPatternEdited();
}

void TrackerComponent::clearFieldAt (int row, int channel, Field field)
{
    if (row < 0 || row >= pattern().length)
        return;

    auto& cell = pattern().cell (row, channel);

    switch (field)
    {
        case Field::note: cell.note = kEmpty; break;
        case Field::inst: cell.inst = kEmpty; break;
        case Field::vol: cell.vol = kEmpty; break;
        case Field::pan: cell.pan = kEmpty; break;
        case Field::cmdNum:
        case Field::cmdVal:
            cell.cmd = kEmpty;
            cell.cmdVal = kEmpty;
            break;
    }
}

void TrackerComponent::clearCurrentField()
{
    clearFieldAt (cursorRow, cursorChannel, cursorField);
    editing = false;
    editBuffer.clear();
    markChanged();
    repaint();
}

//==============================================================================
//== Cell selection ===========================================================
TrackerComponent::SelectOp TrackerComponent::gestureOp (const juce::MouseEvent& e) const
{
    // Ctrl takes cells away and Shift adds them to what is already selected;
    // when both happen to be held the subtraction wins, so a gesture is never
    // adding and removing at once
    const bool removing = e.mods.isCtrlDown() || e.mods.isCommandDown();
    const bool adding = e.mods.isShiftDown() && ! removing;

    if (removing)
        return TrackerComponent::SelectOp::remove;

    if (adding)
        return TrackerComponent::SelectOp::add;

    return TrackerComponent::SelectOp::replace;
}

/** Applies a rectangle of cells, both ends inclusive and in any order, to the
    selection set. Ends outside the pattern that is showing are clamped away. */
void TrackerComponent::applyCellRect (int rowA, int chA, int rowB, int chB,
                                      TrackerComponent::SelectOp op)
{
    const auto& p = pattern();

    if (p.length <= 0 || p.channels <= 0)
        return;

    const int row0 = juce::jlimit (0, p.length - 1, juce::jmin (rowA, rowB));
    const int row1 = juce::jlimit (0, p.length - 1, juce::jmax (rowA, rowB));
    const int ch0 = juce::jlimit (0, p.channels - 1, juce::jmin (chA, chB));
    const int ch1 = juce::jlimit (0, p.channels - 1, juce::jmax (chA, chB));

    if (row0 > row1 || ch0 > ch1)
        return;

    if (op == TrackerComponent::SelectOp::replace)
    {
        for (auto& words : selBits)
            words = kNoCells;
    }

    if ((int) selBits.size() < p.length)
        selBits.resize ((size_t) p.length, kNoCells);

    for (int row = row0; row <= row1; ++row)
    {
        auto& words = selBits[(size_t) row];

        for (int w = 0; w < kSelWords; ++w)
        {
            const int firstBit = w * 64;
            if (firstBit > ch1)
                break;

            const int lo = juce::jmax (ch0, firstBit);
            const int hi = juce::jmin (ch1, firstBit + 63);
            const int run = hi - lo + 1;

            // the channels the rectangle covers inside this word of the row
            const std::uint64_t mask = run >= 64
                                        ? ~std::uint64_t { 0 }
                                        : ((std::uint64_t { 1 } << run) - 1)
                                          << (lo - firstBit);

            if (op == TrackerComponent::SelectOp::remove)
                words[(size_t) w] &= ~mask;
            else
                words[(size_t) w] |= mask;
        }
    }
}

bool TrackerComponent::isCellSelected (int row, int ch) const noexcept
{
    const auto& p = pattern();

    // a cell only exists inside the pattern that is showing, so a selection
    // made over a wider pattern never reads onto a narrower one
    if (row < 0 || row >= p.length || ch < 0 || ch >= p.channels)
        return false;

    if (row >= (int) selBits.size())
        return false;

    const auto& words = selBits[(size_t) row];
    return (words[(size_t) (ch / 64)] & (std::uint64_t { 1 } << (ch % 64))) != 0;
}

bool TrackerComponent::hasSelection() const noexcept
{
    const auto& p = pattern();
    const int rows = juce::jmin ((int) selBits.size(), p.length);

    for (int row = 0; row < rows; ++row)
    {
        const auto& words = selBits[(size_t) row];

        for (int w = 0; w < kSelWords; ++w)
        {
            const int firstBit = w * 64;
            if (firstBit >= p.channels)
                break;

            const int run = juce::jmin (64, p.channels - firstBit);
            const std::uint64_t mask = run >= 64
                                        ? ~std::uint64_t { 0 }
                                        : ((std::uint64_t { 1 } << run) - 1);

            if ((words[(size_t) w] & mask) != 0)
                return true;
        }
    }

    return false;
}

void TrackerComponent::addCellsToSelection (int row0, int ch0, int row1, int ch1)
{
    applyCellRect (row0, ch0, row1, ch1, TrackerComponent::SelectOp::add);
    repaint();
}

void TrackerComponent::clearSelection()
{
    for (auto& words : selBits)
        words = kNoCells;

    repaint();
}

//==============================================================================
//== Cell clipboard ===========================================================
/** Ctrl+C / "Copy selection": the selected cells become clipboard entries that
    keep their offsets inside the copied rectangle measured from the top-left
    selected cell, so the paste anchored at the cursor lays them out around it
    again — and a Ctrl-drag hole stays a hole between entries, which means a
    paste never writes through it. Copying with nothing selected leaves the
    clipboard from before alone. */
void TrackerComponent::copySelectionToClipboard()
{
    if (! hasSelection())
        return;

    auto& p = pattern();
    cellClipboard.entries.clear();

    int minRow = p.length;
    int minCh = p.channels;

    for (int r = 0; r < p.length; ++r)
        for (int c = 0; c < p.channels; ++c)
        {
            if (! isCellSelected (r, c))
                continue;

            minRow = juce::jmin (minRow, r);
            minCh = juce::jmin (minCh, c);
        }

    for (int r = minRow; r < p.length; ++r)
        for (int c = minCh; c < p.channels; ++c)
            if (isCellSelected (r, c))
                cellClipboard.entries.push_back ({ r - minRow, c - minCh, p.cell (r, c) });
}

/** The cell menu's Clear / Selection and the cut below: the selected cells lose
    their data and nothing else moves. The painting, the cursor, the rows through
    them and the columns beside them all stay as they were. The callers decide
    what the change means to the rest of the view: the menu item records its
    point and repaints on its own, and the cut runs as one step. */
void TrackerComponent::clearSelectedCells()
{
    auto& p = pattern();

    for (int r = 0; r < p.length; ++r)
        for (int c = 0; c < p.channels; ++c)
            if (isCellSelected (r, c))
                p.cell (r, c).clear();
}

/** Alt+X / "Cut selection": the copy above and then emptying exactly the cells
    it took, so the cut data is already on the clipboard by the time a following
    Alt+V or a paste item looks for an anchor, and the cells move to the cursor
    rather than repeat there. The cursor and the painted selection stay where
    they were for exactly that reason. Only the wipe is a change to the song, so
    a cut records one point of the undo line rather than two. */
void TrackerComponent::cutSelectionToClipboard()
{
    if (! hasSelection())
        return;

    copySelectionToClipboard();
    clearSelectedCells();

    markChanged();
    repaint();
}

/** Ctrl+V / the paste submenu: writes the clipboard at the cursor, filtered by
    the kPaste* bits, and only touches the model when something was written. */
int TrackerComponent::pasteClipboardAtCursor (int fieldMask)
{
    const int written = pattern().pasteCells (cellClipboard, cursorRow, cursorChannel, fieldMask);

    if (written > 0)
        markChanged();

    return written;
}

void TrackerComponent::setCursorRow (int row)
{
    cursorRow = juce::jlimit (0, pattern().length - 1, row);
    ensureCursorVisible();
    repaint();
}

//==============================================================================
int TrackerComponent::maxDigitsFor (Field field) const
{
    const bool hex = song.settings.hexDisplay;

    switch (field)
    {
        case Field::inst: return hex ? 1 : 2;
        case Field::vol: return hex ? 2 : 3;
        case Field::pan: return hex ? 2 : 3;
        case Field::cmdNum: return hex ? 2 : 3;
        case Field::cmdVal: return hex ? 2 : 3;
        default: return 0;
    }
}

int TrackerComponent::fieldGlyphs (Field field) const
{
    const bool hex = song.settings.hexDisplay;

    switch (field)
    {
        case Field::note: return 4; // "A#" plus an octave of up to two characters
        case Field::inst: return hex ? 2 : 3; // "-" prefix plus one/two digits
        case Field::vol: return hex ? 2 : 3; // 0-255 hex, 0-127 decimal
        case Field::pan: return 3; // hex shows three padded digits, decimal up to 127
        case Field::cmdNum: return hex ? 2 : 3; // CC number up to 143
        case Field::cmdVal: return hex ? 2 : 3; // argument 0-127
        default: return 1;
    }
}

int TrackerComponent::clampValue (Field field, int value) const
{
    switch (field)
    {
        case Field::inst: return juce::jlimit (0, 15, value);
        case Field::vol: return juce::jlimit (0, 127, value);
        case Field::pan: return juce::jlimit (0, 127, value);
        case Field::cmdNum: return juce::jlimit (0, kMacroCmdBase + kMaxMacros - 1, value);
        case Field::cmdVal: return juce::jlimit (0, 127, value);
        default: return value;
    }
}

int TrackerComponent::carriedCommandNumber (int row, int ch) const
{
    auto& p = pattern();

    if (ch < 0 || ch >= p.channels)
        return -1;

    for (int r = juce::jmin (row, (int) p.rows.size()) - 1; r >= 0; --r)
    {
        const auto& cell = p.rows[(size_t) r][(size_t) ch];

        if (cell.cmd != kEmpty)
            return cell.cmd;
    }

    return -1;
}

void TrackerComponent::applyValue (Field field, int value)
{
    auto& cell = pattern().cell (cursorRow, cursorChannel);

    switch (field)
    {
        case Field::inst: cell.inst = clampValue (field, value); break;
        case Field::vol: cell.vol = clampValue (field, value); break;
        case Field::pan: cell.pan = clampValue (field, value); break;
        case Field::cmdNum:
            cell.cmd = clampValue (field, value);

            if (cell.cmdVal == kEmpty)
                cell.cmdVal = 0;

            break;

        case Field::cmdVal:
            cell.cmdVal = clampValue (field, value);

            // typing an argument below a row that named a command leaves the cell
            // implicit, so the row plays that carried command; only a column the
            // editor has not named a command in yet gets an explicit 0
            if (cell.cmd == kEmpty && carriedCommandNumber (cursorRow, cursorChannel) < 0)
                cell.cmd = 0;

            break;

        default: break;
    }
}

void TrackerComponent::commitEditBuffer()
{
    if (! editing || editBuffer.isEmpty())
    {
        editing = false;
        editBuffer.clear();
        return;
    }

    const bool hex = song.settings.hexDisplay;
    const int value = hex ? editBuffer.getHexValue32() : editBuffer.getIntValue();
    applyValue (cursorField, value);

    editing = false;
    editBuffer.clear();
    markChanged();

    const bool advance = cursorField != Field::cmdNum;

    if (advance)
        advanceRow (juce::jmax (1, song.settings.stepSkip));

    repaint();
}

void TrackerComponent::cancelEdit()
{
    editing = false;
    editBuffer.clear();
    repaint();
}

bool TrackerComponent::cancelPendingEdit()
{
    if (! editing)
        return false;

    cancelEdit();
    return true;
}

//==============================================================================
void TrackerComponent::moveCursor (int deltaRow, int deltaField)
{
    auto& p = pattern();
    editing = false;
    editBuffer.clear();

    if (deltaRow != 0)
        cursorRow = juce::jlimit (0, p.length - 1, cursorRow + deltaRow);

    if (deltaField != 0)
    {
        // fields depend on the channel: visibility is per channel now, and the
        // last visible field of the previous channel can differ between them
        auto fieldsFor = [&] (int ch) -> std::vector<Field>
        {
            std::vector<Field> f { Field::note };

            if (p.showColumn (ch, 0)) f.push_back (Field::inst);
            if (p.showColumn (ch, 1)) f.push_back (Field::vol);
            if (p.showColumn (ch, 2)) f.push_back (Field::pan);
            if (p.showColumn (ch, 3)) { f.push_back (Field::cmdNum); f.push_back (Field::cmdVal); }

            return f;
        };

        auto fields = fieldsFor (cursorChannel);
        int idx = 0;

        for (int i = 0; i < (int) fields.size(); ++i)
            if (fields[(size_t) i] == cursorField)
                idx = i;

        idx += deltaField;

        if (idx >= (int) fields.size())
        {
            cursorChannel = (cursorChannel + 1) % p.channels;
            cursorField = Field::note; // first field of the next channel
        }
        else if (idx < 0)
        {
            cursorChannel = (cursorChannel + p.channels - 1) % p.channels;
            cursorField = fieldsFor (cursorChannel).back(); // last field of the previous one
        }
        else
        {
            cursorField = fields[(size_t) idx];
        }
    }

    ensureCursorVisible();
    repaint();
}

void TrackerComponent::advanceRow (int amount)
{
    cursorRow = juce::jlimit (0, pattern().length - 1, cursorRow + amount);
    ensureCursorVisible();
}

void TrackerComponent::ensureCursorVisible()
{
    const int visible = visibleRowCount();

    if (cursorRow < scrollRow)
        scrollRow = cursorRow;
    else if (cursorRow >= scrollRow + visible - 1)
        scrollRow = juce::jmax (0, cursorRow - visible + 2);

    // the least horizontal scroll that keeps the cursor's field on screen, so
    // keyboard entry into a far channel is not blind
    const auto b = fieldBounds (cursorChannel, cursorField);

    if (b.isEmpty())
        return;

    if (b.getRight() > getWidth())
        setScrollX (scrollX + (b.getRight() - getWidth()));
    else if (b.getX() < gutterWidth)
        setScrollX (scrollX - (gutterWidth - b.getX()));
}

//==============================================================================
bool TrackerComponent::keyPressed (const juce::KeyPress& key)
{
    const int code = key.getKeyCode();
    const auto text = key.getTextCharacter();
    const bool shift = key.getModifiers().isShiftDown();
    const bool cmdCtrl = key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown();
    const bool alt = key.getModifiers().isAltDown();

    // the tracker answers the Alt pair itself rather than handing it up, because
    // a key press is sent to the modal component and only walks up from there:
    // while the cell menu is open the press is answered here and the editor is
    // never consulted, so a pair the editor alone owned would be swallowed
    if (answerUndoRedoKey (key, undoStep, redoStep,
                           [this] { return cancelPendingEdit(); }))
        return true;

    // the clipboard's Alt spellings are answered here too rather than handed up,
    // for the same reason the undo pair had to be: the cell menu routes the press
    // to this panel, and the host answers the Ctrl spelling with its own
    // clipboard. Alt+V pastes with everything, the plain spelling's meaning, so
    // what was copied lands wherever the paste filters put it, and Alt+X cuts
    // what is selected, which is that copy followed by emptying exactly the cells
    // it took. All three letters are notes this panel types
    if (answerClipboardKey (key, copyStep, pasteStep, cutStep))
        return true;

    // any other letter held with Alt stays out of the tracker's own key entry,
    // which would otherwise type it as a note before the editor got the event,
    // and the Ctrl pair is left for the editor as the clipboard shortcuts need
    if (cmdCtrl || alt)
        return false; // let the editor handle the clipboard and undo shortcuts

    auto& p = pattern();

    //== Editing numeric buffers ==============================================
    if (editing)
    {
        if (code == juce::KeyPress::escapeKey)
        {
            cancelEdit();
            return true;
        }

        if (code == juce::KeyPress::returnKey)
        {
            commitEditBuffer();
            return true;
        }

        const bool hex = song.settings.hexDisplay;
        const bool validChar = hex ? (isHexDigitChar (text))
                                   : juce::CharacterFunctions::isDigit (text);

        if (validChar)
        {
            editBuffer << juce::String::charToString (text).toUpperCase();

            if (editBuffer.length() >= maxDigitsFor (cursorField))
            {
                commitEditBuffer();
            }
            else
            {
                repaint();
            }

            return true;
        }

        cancelEdit();
    }

    //== Octave ===============================================================
    if (text == '/' || code == juce::KeyPress::F1Key)
    {
        song.settings.recordOctave = juce::jlimit (0, 9, song.settings.recordOctave - 1);
        markChanged();
        return true;
    }

    if (text == '*' || code == juce::KeyPress::F2Key)
    {
        song.settings.recordOctave = juce::jlimit (0, 9, song.settings.recordOctave + 1);
        markChanged();
        return true;
    }

    //== Navigation ===========================================================
    if (code == juce::KeyPress::upKey) { moveCursor (-1, 0); return true; }
    if (code == juce::KeyPress::downKey) { moveCursor (1, 0); return true; }
    if (code == juce::KeyPress::leftKey) { moveCursor (0, -1); return true; }
    if (code == juce::KeyPress::rightKey) { moveCursor (0, 1); return true; }
    if (code == juce::KeyPress::pageUpKey) { moveCursor (-16, 0); return true; }
    if (code == juce::KeyPress::pageDownKey) { moveCursor (16, 0); return true; }
    if (code == juce::KeyPress::homeKey) { cursorRow = 0; ensureCursorVisible(); repaint(); return true; }
    if (code == juce::KeyPress::endKey) { cursorRow = p.length - 1; ensureCursorVisible(); repaint(); return true; }
    if (code == juce::KeyPress::tabKey) { moveCursor (0, shift ? -1 : 1); return true; }
    if (code == juce::KeyPress::returnKey) { advanceRow (juce::jmax (1, song.settings.stepSkip)); return true; }
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey) { clearCurrentField(); return true; }

    //== Value fields =========================================================
    if (cursorField != Field::note)
    {
        const bool hex = song.settings.hexDisplay;
        const bool validChar = hex ? isHexDigitChar (text)
                                   : juce::CharacterFunctions::isDigit (text);

        if (validChar)
        {
            editBuffer = juce::String::charToString (text).toUpperCase();
            editing = true;
            repaint();

            if (editBuffer.length() >= maxDigitsFor (cursorField))
                commitEditBuffer();

            return true;
        }

        return false;
    }

    //== Note entry ===========================================================
    if (text == '`' || text == '~')
    {
        p.cell (cursorRow, cursorChannel).note = kNoteOff;
        markChanged();
        advanceRow (juce::jmax (1, song.settings.stepSkip));
        repaint();
        return true;
    }

    const int offset = noteOffsetForKey (juce::CharacterFunctions::toLowerCase (text));

    if (offset >= 0)
    {
        const int note = juce::jlimit (0, 127, (song.settings.recordOctave + 1) * 12 + offset);
        p.cell (cursorRow, cursorChannel).note = note;
        markChanged();
        advanceRow (juce::jmax (1, song.settings.stepSkip));
        repaint();
        return true;
    }

    return false;
}

void TrackerComponent::focusGained (FocusChangeType)
{
    repaint();
}

void TrackerComponent::focusLost (FocusChangeType)
{
    cancelEdit();
    repaint();
}

} // namespace rtr
