#include "TrackerComponent.h"
#include "Theme.h"
#include "AltShortcutKeys.h"

namespace rtr
{

namespace
{
    constexpr int kFieldPad = 5;
    constexpr int kChannelGap = 12;

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

    /** Whether the argument a row names puts the command it belongs to away
        rather than naming a new one: the arpeggiator's release spells itself two
        ways, the 0 beside the call letter and the OFF the entry writes for an o,
        and a 0 under a carried call is the release as much as the row that named
        both letters is. The vibrato letter takes the same two spellings, its 0 being
        the id that calls no macro and its OFF being the release the entry writes, and
        the two slide letters take the OFF spelling the same way, where it answers for
        the wheel being put back at its centre rather than for a slide of no rows at
        all, and the entry writes the same 0 beside a carried or named slide letter for
        it, which is why a 0 there releases too - a 0 beside the bend letter keeps naming
        the wheel's own bottom, the point the wheel reads rather than a command being put
        away. The cut letter takes the same two spellings as well, its 0 being the release
        rather than a note held for no subticks at all and its OFF being the one the entry
        writes for an o, which is what lets the notes under a release row sustain again.
        Only the STP spelling asks to be carried on, since a row beside it only
        stops the move the wheel was making and leaves the letter in force under it;
        a vibrato has no move to stop, so the spelling means nothing beside its letter,
        and neither has a cut, whose letter keeps gating the notes under it rather than
        answering them. A row that puts a command away leaves the column carrying
        nothing below it. */
    bool commandArgumentReleases (int cmd, int cmdVal)
    {
        if (cmdVal == kArgOff)
            return true;

        if (cmdVal != 0)
            return false;

        return cmd == kArpCmd || cmd == kVibCmd || cmd == kSlideUpCmd
            || cmd == kSlideDownCmd || cmd == kCutCmd;
    }

    /** Whether a row that named a command leaves one in force for the rows
        below it: the releases above leave the column calling nothing, which is
        what stops the tracker drawing a carried letter and argument over the
        rows below it and matches the engine answering no row there that names
        only an argument. Every other command, a bend letter, a slide letter or
        a plain CC, leaves its letter and its argument in force, so the rows
        under a slide carry the span that slide is still taking toward its
        extreme and the rows under an STP carry the letter that was holding the
        wheel at its own point. */
    bool commandLeavesCarry (const Cell& cell)
    {
        return cell.cmd != kEmpty && ! commandArgumentReleases (cell.cmd, cell.cmdVal);
    }

    /** Steps what a column carries for its command by the row it is carried into,
        the letter and the argument together: a row that named a command leaves that
        one, a release leaving none, and a row that named only an argument belongs
        to the command already in force above it, so it answers with that command
        and leaves the argument it named itself in force for the rows below rather
        than the one the letter above called with - unless that argument is itself
        a release, an OFF read under a carried command or a 0 read under a carried
        call or a carried slide letter, which puts that command away there exactly
        the way the engine clears the command it answers with, and is what stops a
        letter and an argument being drawn over the rows under it. */
    void stepCommandCarry (int& cmd, int& cmdVal, const Cell& cell)
    {
        if (cell.cmd != kEmpty)
        {
            if (commandLeavesCarry (cell))
            {
                cmd = cell.cmd;
                cmdVal = cell.cmdVal;
            }
            else
            {
                cmd = -1;
                cmdVal = -1;
            }

            return;
        }

        if (cmd >= 0 && cell.cmdVal != kEmpty)
        {
            if (commandArgumentReleases (cmd, cell.cmdVal))
            {
                cmd = -1;
                cmdVal = -1;
                return;
            }

            cmdVal = cell.cmdVal;
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
            // the two halves each get room for their own widest spelling rather
            // than sharing the cell by halves: the number's own half keeps two
            // digits plus a two-pixel inner trim and the argument's half keeps
            // three, the release's OFF among them, each with its own pad, so
            // halving one wider total would cut the number's cell and leave the
            // argument's room in the wrong place
            const int numW = (int) (charW * (float) fieldGlyphs (Field::cmdNum) + 1.0f) + 2;
            const int valW = (int) (charW * (float) fieldGlyphs (Field::cmdVal) + 1.0f) + kFieldPad;

            fieldRects.push_back ({ Field::cmdNum, ch, { x, 0, numW, rowHeight } });
            fieldRects.push_back ({ Field::cmdVal, ch, { x + numW, 0, valW, rowHeight } });
            x += numW + valW;
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

    // carry tables for display
    struct Eff { int inst = -1, vol = -1, pan = -1, cmd = -1, cmdVal = -1; };
    std::vector<std::vector<Eff>> eff ((size_t) p.channels);

    for (int ch = 0; ch < p.channels; ++ch)
    {
        eff[(size_t) ch].resize ((size_t) p.length);
        Eff c;

        for (int r = 0; r < p.length; ++r)
        {
            const auto& cell = p.rows[(size_t) r][(size_t) ch];

            // the row is drawn from what the column carries into it rather than
            // from what it leaves for the rows below, which is what a release
            // written as only its argument still reads as the letter above it on
            // its own row and as nothing on the ones under it
            eff[(size_t) ch][(size_t) r] = c;

            if (cell.inst != kEmpty) c.inst = cell.inst;
            if (cell.vol != kEmpty) c.vol = cell.vol;
            if (cell.pan != kEmpty) c.pan = cell.pan;

            // the argument is part of what the column carries, which is what a
            // row under a command that named one shows it as well as the letter,
            // and a row naming only an argument carries the one it entered down
            // over the ones below it instead of the argument of the letter above
            stepCommandCarry (c.cmd, c.cmdVal, cell);
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
        g.drawText (juce::String (row).paddedLeft ('0', 3),
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
                            text = "-" + instString (cell.inst);
                            colour = theme::inst;
                        }
                        else if (carried.inst >= 0 && notePresent)
                        {
                            text = "-" + instString (carried.inst);
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
                            text = valueString (cell.vol, fieldGlyphs (Field::vol));
                            colour = theme::vol;
                        }
                        else if (carried.vol >= 0 && notePresent)
                        {
                            text = valueString (carried.vol, fieldGlyphs (Field::vol));
                            colour = theme::vol;
                            dim = true;
                        }
                        else
                        {
                            text = "---";
                            colour = theme::textDim.withAlpha (0.45f);
                        }

                        break;

                    case Field::pan:
                        if (cell.pan != kEmpty)
                        {
                            text = valueString (cell.pan, 3);
                            colour = theme::pan;
                        }
                        else if (carried.pan >= 0 && notePresent)
                        {
                            text = valueString (carried.pan, 3);
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
                        const bool hasOwnNum = cell.cmd != kEmpty;
                        const bool hasOwnVal = cell.cmdVal != kEmpty;
                        const int cmdNum = hasOwnNum ? cell.cmd : carried.cmd;
                        // the argument belongs to what the column carries as much
                        // as the letter does, so a row under a command that named
                        // one shows both dim rather than only the letter; a row
                        // that named a command of its own answers with that row's
                        // own argument, which names nothing beside a bare letter
                        const int cmdVal = hasOwnVal ? cell.cmdVal
                                                     : hasOwnNum ? kEmpty
                                                                : carried.cmdVal;

                        if (fr.field == Field::cmdNum)
                        {
                            if (cmdNum >= 0)
                            {
                                if (cmdNum == kArpCmd)
                                {
                                    // the arpeggiator call is drawn as its letter,
                                    // in orange, rather than as its number
                                    text = "A";
                                    colour = theme::cmdMacro;
                                }
                                else if (cmdNum == kBendCmd)
                                {
                                    // the bend pair is drawn as its letter, in the
                                    // lilac the theme leaves for a command kind that
                                    // is neither a plain CC nor a call
                                    text = "B";
                                    colour = theme::cmdOther;
                                }
                                else if (cmdNum == kSlideUpCmd || cmdNum == kSlideDownCmd)
                                {
                                    // a slide of the wheel is drawn as its letter too,
                                    // in the same lilac: U slides up to the wheel's
                                    // top and D down to its bottom, the argument beside
                                    // it naming how many rows the slide takes
                                    text = cmdNum == kSlideUpCmd ? "U" : "D";
                                    colour = theme::cmdOther;
                                }
                                else if (cmdNum == kVibCmd)
                                {
                                    // a vibrato call is drawn as its letter, in the same
                                    // lilac the wheel's own commands are read in, and its
                                    // argument is the vibrato macro the VIB MACRO tab shows
                                    text = "V";
                                    colour = theme::cmdOther;
                                }
                                else if (cmdNum == kCutCmd)
                                {
                                    // the cut is drawn as its letter too, in the same
                                    // lilac: it cuts the note its row entered rather than
                                    // calling anything, so its argument beside it counts
                                    // how many subticks that note is held for. Unlike the
                                    // wheel's and a macro's letters, this one keeps being
                                    // carried down under it, so the rows below a named one
                                    // draw the letter and their carried length dim, which
                                    // is what says that the notes entering those rows are
                                    // cut to the length the column carries rather than left
                                    // sustained until something else ends them
                                    text = "C";
                                    colour = theme::cmdOther;
                                }
                                else
                                {
                                    text = valueString (cmdNum, fieldGlyphs (Field::cmdNum));
                                    colour = theme::cmdCC;
                                }

                                dim = ! hasOwnNum;
                            }
                            else
                            {
                                text = "---";
                                colour = theme::textDim.withAlpha (0.45f);
                            }
                        }
                        else
                        {
                            if (cmdVal >= 0)
                            {
                                // a macro id is the number the Macros tab shows and a
                                // bend's is where on the wheel its centre of 64 is read,
                                // so both keep the decimal spelling the whole view is
                                // written in; so does the argument of a plain CC, a
                                // slide's rows, a vibrato's macro id and a cut's subticks,
                                // each of them just a count
                                const bool arpCall = cmdNum == kArpCmd;
                                const bool bendCall = cmdNum == kBendCmd;
                                const bool slideCall = cmdNum == kSlideUpCmd
                                                      || cmdNum == kSlideDownCmd;
                                const bool vibCall = cmdNum == kVibCmd;
                                const bool cutCall = cmdNum == kCutCmd;

                                if (( arpCall && cmdVal == 0 ) || cmdVal == kArgOff
                                    || ( slideCall && cmdVal == 0 )
                                    || ( vibCall && cmdVal == 0 )
                                    || ( cutCall && cmdVal == 0 ))
                                    // the release reads as what it does rather than
                                    // as a number: an arpeggiator's going off, spelled
                                    // by the 0 the row named or by the OFF the entry
                                    // writes for an o, and a wheel letter beside an
                                    // OFF, which puts the wheel back at its centre
                                    // instead of making it climb - a 0 read under a
                                    // slide letter means the same thing, which is why
                                    // the column stops carrying the letter below it, a
                                    // vibrato's release spells itself the same two ways
                                    // as a call's do, and a cut's release spells itself
                                    // the same two ways as well: the 0 that cuts a note
                                    // for no subticks at all reads as the gate being put
                                    // away, so the notes entering the rows below a release
                                    // are left sustained and the column carries no letter
                                    // over them, which is why a row under a release names
                                    // nothing rather than a dim letter and a dim length
                                    text = "OFF";
                                else if (cmdVal == kArgStp)
                                    // the STP spelling reads as the hold it asks for:
                                    // the wheel left sitting on the point it had got
                                    // to rather than moved over the rows below
                                    text = "STP";
                                else
                                    text = valueString (cmdVal, fieldGlyphs (Field::cmdVal));

                                colour = arpCall ? theme::cmdMacro
                                                 : ( bendCall || slideCall || vibCall
                                                   || cutCall ) ? theme::cmdOther
                                                                : theme::cmdCC;

                                // an argument the row did not name is the one the
                                // column carries down from above, drawn dimmer like
                                // every other carried value
                                dim = ! hasOwnVal;
                            }
                            else
                            {
                                text = "---";
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

    // the two paste submenus are the same list in the two modes: the plain one
    // writes every field the buffer carries, so a cell that is empty there
    // clears the target, and the additive one lets only the fields the buffer
    // actually holds reach, which leaves the target's own value where the
    // buffer was empty. the leaves repeat rather than a single extra item, so
    // the user picks the field to paste and the mode after it, in that order
    juce::PopupMenu pasteMenu;
    pasteMenu.addItem (7, "Everything");
    pasteMenu.addItem (8, "Note");
    pasteMenu.addItem (9, "Instrument");
    pasteMenu.addItem (10, "Volume");
    pasteMenu.addItem (11, "Pan");
    pasteMenu.addItem (12, "Command");
    pasteMenu.addItem (13, "Command Value");
    menu.addSubMenu ("Paste", pasteMenu, hasCellClipboard());

    // the additive submenu repeats the same seven leaves under a run of ids
    // after the undo pair's, so both runs map by one subtraction each and
    // neither range reaches into the other's
    juce::PopupMenu additiveMenu;
    additiveMenu.addItem (17, "Everything");
    additiveMenu.addItem (18, "Note");
    additiveMenu.addItem (19, "Instrument");
    additiveMenu.addItem (20, "Volume");
    additiveMenu.addItem (21, "Pan");
    additiveMenu.addItem (22, "Command");
    additiveMenu.addItem (23, "Command Value");
    menu.addSubMenu ("Paste (Additive)", additiveMenu, hasCellClipboard());

    // the undo pair sits last so the menu reads as the actions the cursor is on
    // first and the steps along the state line after them; its ids continue the
    // first paste submenu's run because one callback sees all of them by number
    // alone, and the additive submenu starts past them. the two counts grey the
    // pair out at the ends of the line
    menu.addItem (14, "Undo", proc.getUndoDepth() > 0);
    menu.addItem (15, "Redo", proc.getUndoRedoCount() > 0);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition (),
                        [this] (int result)
                        {
                            auto& p = pattern();

                            // 1..4 are the "Clear" submenu's leaves, 5 and 6 are
                            // the top-level copy and cut, 7..13 and 17..23 the
                            // two paste submenus' and 14/15 the top-level undo
                            // pair
                            static const int pasteMasks[] = { kPasteAll, kPasteNote,
                                                              kPasteInst, kPasteVol,
                                                              kPastePan, kPasteCmd,
                                                              kPasteCmdVal };

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
                                // 7 is Everything, 8..13 name one field each. the
                                // plain mode writes all of them, so an empty field
                                // in the buffer clears the target's own value
                                pasteClipboardAtCursor (pasteMasks[result - 7]);
                            }
                            else if (result >= 17 && result <= 23 && hasCellClipboard())
                            {
                                // the same leaves again, the same masks, only with
                                // the additive mode bit: inside each copied cell a
                                // field reaches the target only where the buffer
                                // actually holds something, so an empty field
                                // leaves the target's value alone, and a cell that
                                // is empty in every field writes nothing at all,
                                // which leaves no undo point behind
                                pasteClipboardAtCursor (pasteMasks[result - 17] | kPasteAdditive);
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
    switch (field)
    {
        case Field::inst: return 2; // -0 .. -15
        case Field::vol: return 3; // 0-127
        case Field::pan: return 3; // 0-127
        case Field::cmdNum: return 3; // a plain CC 0-127
        case Field::cmdVal: return 3; // 0-127, or the release's OFF and a hold's STP
        default: return 0;
    }
}

int TrackerComponent::fieldGlyphs (Field field) const
{
    switch (field)
    {
        case Field::note: return 4; // "A#" plus an octave of up to two characters
        case Field::inst: return 3; // "-" prefix plus up to two digits
        case Field::vol: return 3; // 0-127
        case Field::pan: return 3; // 0-127
        case Field::cmdNum: return 3; // a plain CC, or one letter for a command kind
        case Field::cmdVal: return 3; // the argument is always decimal, so a macro id, a point on the wheel, the release's OFF or a hold's STP need three
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

        case Field::cmdNum:
            // digits stay a plain CC: the command letters are written by the
            // letters themselves, so typing a bigger number than the column asks
            // for cannot invent one of them and an older build's macro values
            // cannot be typed back at all
            return juce::jlimit (0, kCcTop, value);

        case Field::cmdVal: return juce::jlimit (0, 127, value);
        default: return value;
    }
}

int TrackerComponent::carriedCommandNumber (int row, int ch) const
{
    auto& p = pattern();

    if (ch < 0 || ch >= p.channels)
        return -1;

    // the walk goes down from the top of the column to the row rather than up from
    // the row, because what a row leaves in force also depends on the arguments the
    // rows above it named on their own, which a bottom-up walk cannot see; a row
    // naming only a release under a carried command, an OFF or a 0 under a call or
    // a slide letter, releases it there and leaves the column carrying nothing for
    // the rows below
    int cmd = -1;
    int cmdVal = -1;
    const int top = juce::jmin (row, (int) p.rows.size());

    for (int r = 0; r < top; ++r)
        stepCommandCarry (cmd, cmdVal, p.rows[(size_t) r][(size_t) ch]);

    return cmd;
}

int TrackerComponent::carriedCommandValue (int row, int ch) const
{
    auto& p = pattern();

    if (ch < 0 || ch >= p.channels)
        return -1;

    // the same walk the drawing steps the column by, so a test reads the argument
    // the view carries rather than a pixel
    int cmd = -1;
    int cmdVal = -1;
    const int top = juce::jmin (row, (int) p.rows.size());

    for (int r = 0; r < top; ++r)
        stepCommandCarry (cmd, cmdVal, p.rows[(size_t) r][(size_t) ch]);

    return cmdVal;
}

int TrackerComponent::commandKindAt (int row, int ch) const
{
    auto& p = pattern();

    if (ch < 0 || ch >= p.channels)
        return -1;

    // the argument field belongs to whichever command its row answers against: a
    // row that named a command of its own answers against that one, and a row that
    // named only an argument answers against the command the column carries above
    // it, so this names the kind a row's argument belongs to whichever of the two
    // the row is, which is what tells the entry whether an OFF or an STP spelling
    // means anything at the cursor at all
    if (row < (int) p.rows.size() && p.rows[(size_t) row][(size_t) ch].cmd != kEmpty)
        return p.rows[(size_t) row][(size_t) ch].cmd;

    return carriedCommandNumber (row, ch);
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

            // a 0 typed beside a slide letter is the release the o would have
            // written rather than a slide over no rows at all: the narrowest slide
            // the field can name takes one row, so the entry lands the OFF argument
            // the drawing reads and the engine answers by putting the wheel back at
            // its centre. A 0 beside the call letter keeps the release spelling the
            // arpeggiator already has, and one beside the bend letter keeps naming
            // the wheel's own bottom rather than switching anything off. A cut of
            // no subticks would hold a note for nothing at all, so a 0 beside or
            // under the cut letter lands the same OFF the o writes for it, the entry
            // spelling the release rather than a length below the field's own
            // narrowest one
            if (cell.cmdVal == 0)
            {
                const int kind = commandKindAt (cursorRow, cursorChannel);

                if (kind == kSlideUpCmd || kind == kSlideDownCmd || kind == kCutCmd)
                    cell.cmdVal = kArgOff;
            }

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

    const int value = editBuffer.getIntValue();
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

    // the clip slice at the playhead is answered here as well, and before the
    // Alt-held letters are kept out of the panel's own key entry, since this
    // panel's note entry types s as C sharp: the press has to be claimed first
    // or the cursor cell gets a note the user never meant to leave there
    if (answerSliceKey (key, sliceStep))
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

        const bool validChar = juce::CharacterFunctions::isDigit (text);

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
        // the command letters are letters rather than numbers: as the first
        // character of the command number field each claims that field, so a
        // decimal field spells the CC 10 or CC 11 a column asks for as 10 and
        // 11 and nothing about a typed number's spelling reaches one of the
        // letters, a digit already being typed having been appended by the
        // editing path above, and the argument typed next names what that
        // letter's own field means: a macro id for A, a point on the wheel for B,
        // a span in rows for U and D, a vibrato macro id for V and a count of
        // subticks for C
        if (cursorField == Field::cmdNum)
        {
            const int letterCmd = text == 'a' || text == 'A' ? kArpCmd
                                : text == 'b' || text == 'B' ? kBendCmd
                                : text == 'u' || text == 'U' ? kSlideUpCmd
                                : text == 'd' || text == 'D' ? kSlideDownCmd
                                : text == 'v' || text == 'V' ? kVibCmd
                                : text == 'c' || text == 'C' ? kCutCmd
                                                             : kEmpty;

            if (letterCmd != kEmpty)
            {
                // the letter writes the command value it names rather than a
                // number that gets clamped, which is what keeps a typed digit
                // run from inventing one of them
                auto& claim = p.cell (cursorRow, cursorChannel);
                claim.cmd = letterCmd;

                // an entry has to leave an argument behind: a call starts at the
                // release, id 0, a bend starts at the wheel's centre and a slide
                // starts at the default span of rows its extreme is counted over,
                // a cut starts at the default count of subticks its note is held
                // for, so a row that names only the letter spells the command it
                // asks for rather than locking the wheel to its bottom, bending
                // nowhere, sliding over no rows at all or holding a note for no
                // subticks, which would read as the release instead
                if (claim.cmdVal == kEmpty)
                    claim.cmdVal = letterCmd == kBendCmd ? kBendCentreVal
                                 : letterCmd == kSlideUpCmd || letterCmd == kSlideDownCmd ? kSlideDefaultSpan
                                 : letterCmd == kCutCmd ? kCutDefaultTicks
                                                        : 0;

                markChanged();
                repaint();
                return true;
            }
        }

        // the argument field spells the wheel's own states as letters rather than
        // as numbers, since the field only ever spells 0..127 and neither of the
        // two is a number: an o lands the OFF argument, which switches the
        // pitchbending off and puts the wheel back at its centre, and an s lands
        // the STP one, which stops whatever move the wheel was making and leaves
        // it sitting on the point it had got to. An o also goes off an
        // arpeggiator run, a vibrato or a cut, the release the 0 beside any of
        // those letters already spells, and a cut's OFF being the gate being put
        // away rather than a note held for no subticks at all, which is what lets
        // the notes entering the rows under a release sustain again. An s asks
        // for nothing beside a call, a vibrato or a cut, since none of them has
        // a move of the wheel to stop - a cut's letter gates the notes below it
        // rather than making the wheel climb, so there is nothing there to hold -
        // so a letter neither claim owns where the cursor sits is left alone
        // rather than writing a value or typing a note beside it, and the cell's
        // value stays as it was
        if (cursorField == Field::cmdVal)
        {
            const int kind = commandKindAt (cursorRow, cursorChannel);
            const bool wheelKind = kind == kBendCmd || kind == kSlideUpCmd || kind == kSlideDownCmd;
            const bool cutKind = kind == kCutCmd;
            const bool releaseKind = kind == kArpCmd || kind == kVibCmd || wheelKind || cutKind;

            const int letterVal = text == 'o' || text == 'O' ? ( releaseKind ? kArgOff : kEmpty )
                                : text == 's' || text == 'S' ? ( wheelKind ? kArgStp : kEmpty )
                                                             : kEmpty;

            if (letterVal != kEmpty)
            {
                p.cell (cursorRow, cursorChannel).cmdVal = letterVal;
                markChanged();
                advanceRow (juce::jmax (1, song.settings.stepSkip));
                repaint();
                return true;
            }
        }

        const bool validChar = juce::CharacterFunctions::isDigit (text);

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
