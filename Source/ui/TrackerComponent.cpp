#include "TrackerComponent.h"
#include "Theme.h"

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
    rowsEditor.setText (juce::String (p.length), false);
    layoutFields();
    repaint();
}

//==============================================================================
void TrackerComponent::layoutFields()
{
    auto& p = pattern();
    fieldRects.clear();

    const float charW = theme::monoFont (13.0f).getStringWidthFloat ("0");

    const int noteW = (int) (charW * 4.0f) + kFieldPad;
    const int instW = (int) (charW * 2.0f) + kFieldPad;
    const int volW = (int) (charW * 2.0f) + kFieldPad;
    const int panW = (int) (charW * 3.0f) + kFieldPad;
    const int cmdW = (int) (charW * 6.0f) + kFieldPad;

    int x = gutterWidth - scrollX;

    for (int ch = 0; ch < p.channels; ++ch)
    {
        auto add = [&] (Field f, int w)
        {
            fieldRects.push_back ({ f, ch, { x, 0, w, rowHeight } });
            x += w;
        };

        add (Field::note, noteW);

        if (p.showInst)
            add (Field::inst, instW);

        if (p.showVol)
            add (Field::vol, volW);

        if (p.showPan)
            add (Field::pan, panW);

        if (p.showCmd)
        {
            const int half = cmdW / 2;
            fieldRects.push_back ({ Field::cmdNum, ch, { x, 0, half, rowHeight } });
            fieldRects.push_back ({ Field::cmdVal, ch, { x + half, 0, cmdW - half, rowHeight } });
            x += cmdW;
        }

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
    for (int i = 0; i < (int) fieldRects.size(); ++i)
    {
        auto b = fieldRects[(size_t) i].bounds;

        if (x >= b.getX() - 2 && x < b.getRight() + 2)
            return i;
    }

    return -1;
}

int TrackerComponent::rowAtY (int y) const noexcept
{
    if (y < headerHeight)
        return -1;

    return scrollRow + (y - headerHeight) / rowHeight;
}

int TrackerComponent::visibleRowCount() const noexcept
{
    return juce::jmax (1, (getHeight() - headerHeight) / rowHeight + 1);
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

    g.saveState();
    g.reduceClipRegion (area);
    drawRows (g, area);
    g.restoreState();

    // scrollbar hint
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
        auto noteRect = fieldBounds (ch, Field::note);

        if (noteRect.isEmpty())
            continue;

        const int headerTop = 2;
        g.setFont (theme::monoFont (11.0f));
        g.setColour (ch == cursorChannel ? theme::accent : theme::textDim);
        g.drawText (juce::String (ch).paddedLeft ('0', 2),
                    noteRect.getX(), headerTop, noteRect.getWidth(), 14,
                    juce::Justification::centred, false);

        // Column visibility chips, always shown so a hidden column can be restored.
        const char* labels[4] = { "I", "V", "P", "C" };
        const bool states[4] = { p.showInst, p.showVol, p.showPan, p.showCmd };

        for (int i = 0; i < 4; ++i)
        {
            auto btn = toggleBounds (ch, i);

            if (btn.isEmpty())
                continue;

            g.setColour (states[i] ? theme::panelLight : theme::background);
            g.fillRoundedRectangle (btn.toFloat(), 2.0f);
            g.setColour (states[i] ? theme::textDim : theme::outline);
            g.drawRoundedRectangle (btn.toFloat(), 2.0f, 1.0f);
            g.setColour (states[i] ? theme::text : theme::outline);
            g.setFont (theme::monoFont (8.5f));
            g.drawText (labels[i], btn, juce::Justification::centred, false);
        }
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

        if (hasSelection())
        {
            const int s = juce::jmin (selStartRow, selEndRow);
            const int e = juce::jmax (selStartRow, selEndRow);

            if (row >= s && row <= e)
            {
                g.setColour (theme::selection.withAlpha (0.45f));
                g.fillRect (rowRect.withTrimmedLeft (gutterWidth));
            }
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

            const bool isCursorCh = row == cursorRow && ch == cursorChannel;
            const bool notePresent = cell.note != kEmpty;

            for (auto& fr : fieldRects)
            {
                if (fr.channel != ch)
                    continue;

                juce::Rectangle<int> r (fr.bounds.getX(), y, fr.bounds.getWidth(), rowHeight);
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
                            text = valueString (cell.vol, hex, 2);
                            colour = theme::vol;
                        }
                        else if (carried.vol >= 0 && notePresent)
                        {
                            text = valueString (carried.vol, hex, 2);
                            colour = theme::vol;
                            dim = true;
                        }
                        else
                        {
                            text = "--";
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
                        const int cmdVal = hasOwn ? cell.cmdVal : kEmpty;

                        if (fr.field == Field::cmdNum)
                        {
                            if (cmdNum >= 0)
                            {
                                text = valueString (cmdNum, hex, 2);
                                colour = theme::cmdCC;
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
                                text = valueString (cmdVal, hex, 2);
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

    const int row = rowAtY (e.y);

    // header: toggle column visibility
    if (row < 0)
    {
        if (e.y < headerHeight && e.x > gutterWidth)
        {
            auto& p = pattern();

            for (int ch = 0; ch < p.channels; ++ch)
            {
                for (int i = 0; i < 4; ++i)
                {
                    if (! toggleBounds (ch, i).contains (e.getPosition()))
                        continue;

                    switch (i)
                    {
                        case 0: p.showInst = ! p.showInst; break;
                        case 1: p.showVol = ! p.showVol; break;
                        case 2: p.showPan = ! p.showPan; break;
                        case 3: p.showCmd = ! p.showCmd; break;
                        default: break;
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

    if (e.mods.isShiftDown())
    {
        if (selStartRow < 0)
            selStartRow = cursorRow;

        selEndRow = cursorRow;
    }
    else
    {
        selStartRow = cursorRow;
        selEndRow = cursorRow;
        selecting = true;
    }

    repaint();
}

void TrackerComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (! selecting)
        return;

    const int row = juce::jlimit (0, pattern().length - 1, rowAtY (e.y));

    if (row >= 0 && row != selEndRow)
    {
        selEndRow = row;
        repaint();
    }
}

void TrackerComponent::mouseUp (const juce::MouseEvent&)
{
    selecting = false;
}

void TrackerComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isShiftDown())
    {
        scrollX = juce::jmax (0, scrollX - (int) (wheel.deltaY * 120.0f));
        layoutFields();
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
    juce::PopupMenu menu;
    menu.addItem (1, "Clear cell");
    menu.addItem (2, "Clear channel");
    menu.addItem (3, "Clear pattern");
    menu.addItem (4, "Clear selection", hasSelection());

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [this] (int result)
                        {
                            auto& p = pattern();

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
                                const int s = juce::jmin (selStartRow, selEndRow);
                                const int en = juce::jmax (selStartRow, selEndRow);

                                for (int r = s; r <= en; ++r)
                                    for (auto& fr : fieldRects)
                                        clearFieldAt (r, fr.channel, fr.field);

                                markChanged();
                            }

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

void TrackerComponent::clearSelection()
{
    selStartRow = -1;
    selEndRow = -1;
    repaint();
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

int TrackerComponent::clampValue (Field field, int value) const
{
    switch (field)
    {
        case Field::inst: return juce::jlimit (0, 15, value);
        case Field::vol: return juce::jlimit (0, 127, value);
        case Field::pan: return juce::jlimit (0, 127, value);
        case Field::cmdNum: return juce::jlimit (0, 127, value);
        case Field::cmdVal: return juce::jlimit (0, 127, value);
        default: return value;
    }
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

            if (cell.cmd == kEmpty)
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
        auto visibleFields = [&]() -> std::vector<Field>
        {
            std::vector<Field> f { Field::note };

            if (p.showInst) f.push_back (Field::inst);
            if (p.showVol) f.push_back (Field::vol);
            if (p.showPan) f.push_back (Field::pan);
            if (p.showCmd) { f.push_back (Field::cmdNum); f.push_back (Field::cmdVal); }

            return f;
        };

        auto fields = visibleFields();
        int idx = 0;

        for (int i = 0; i < (int) fields.size(); ++i)
            if (fields[(size_t) i] == cursorField)
                idx = i;

        idx += deltaField;

        if (idx >= (int) fields.size())
        {
            idx = 0;
            cursorChannel = (cursorChannel + 1) % p.channels;
        }
        else if (idx < 0)
        {
            idx = (int) fields.size() - 1;
            cursorChannel = (cursorChannel + p.channels - 1) % p.channels;
        }

        cursorField = fields[(size_t) idx];
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
}

//==============================================================================
bool TrackerComponent::keyPressed (const juce::KeyPress& key)
{
    const int code = key.getKeyCode();
    const auto text = key.getTextCharacter();
    const bool shift = key.getModifiers().isShiftDown();
    const bool cmdCtrl = key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown();

    if (cmdCtrl)
        return false; // let the editor handle clipboard shortcuts

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
