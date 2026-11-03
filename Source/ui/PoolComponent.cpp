#include "PoolComponent.h"
#include "Theme.h"
#include <algorithm>

namespace rtr
{

static constexpr const char* kClipDragPrefix = "rtr-pattern:";
static constexpr int kExpandedPitch = 138; // expanded item pitch (grid preview)

PoolComponent::PoolComponent (ReTrektorAudioProcessor& processor)
    : proc (processor), song (processor.getSong())
{
    setOpaque (true);
    setWantsKeyboardFocus (false);
    startTimerHz (12);
}

void PoolComponent::refresh()
{
    repaint();
}

void PoolComponent::timerCallback()
{
    if (song.patterns.size() > 0)
        repaint();
}

//==============================================================================
bool PoolComponent::isExpanded (int index) const
{
    if (index < 0 || index >= (int) song.patterns.size())
        return false;

    const int id = song.patterns[(size_t) index].id;
    return std::find (expandedIds.begin(), expandedIds.end(), id) != expandedIds.end();
}

int PoolComponent::poolContentHeight() const
{
    int total = 0;

    for (int i = 0; i < (int) song.patterns.size(); ++i)
        total += isExpanded (i) ? kExpandedPitch : itemHeight;

    return total;
}

void PoolComponent::toggleGridPreview (int patternId)
{
    if (auto it = std::find (expandedIds.begin(), expandedIds.end(), patternId); it != expandedIds.end())
        expandedIds.erase (it);
    else
        expandedIds.push_back (patternId);
}

juce::Rectangle<int> PoolComponent::cellBounds (int index) const
{
    int y = 30; // header (26) + 4

    for (int i = 0; i < index; ++i)
        y += isExpanded (i) ? kExpandedPitch : itemHeight;

    const int pitch = isExpanded (index) ? kExpandedPitch : itemHeight;
    return { 4, y - scrollY, juce::jmax (60, getWidth() - 8), pitch - 4 };
}

int PoolComponent::patternIndexAt (juce::Point<int> p) const
{
    for (int i = 0; i < (int) song.patterns.size(); ++i)
        if (cellBounds (i).contains (p))
            return i;

    return -1;
}

//==============================================================================
void PoolComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);

    auto header = getLocalBounds().removeFromTop (26);
    g.setColour (theme::panelLight);
    g.fillRect (header);
    g.setColour (theme::textDim);
    g.setFont (theme::monoFont (11.0f));
    g.drawText ("PATTERN POOL", header.reduced (8, 0), juce::Justification::centredLeft, false);

    // "+" button
    juce::Rectangle<int> plus (header.getRight() - 24, 4, 18, 18);
    g.setColour (theme::panel);
    g.fillRoundedRectangle (plus.toFloat(), 3.0f);
    g.setColour (theme::outline);
    g.drawRoundedRectangle (plus.toFloat(), 3.0f, 1.0f);
    g.setColour (theme::text);
    g.setFont (theme::monoFont (13.0f));
    g.drawText ("+", plus, juce::Justification::centred, false);

    g.saveState();
    g.reduceClipRegion (getLocalBounds().withTrimmedTop (26));
    g.setFont (theme::monoFont (11.5f));

    int y = 30; // first item top (header 26 + 4)

    for (int i = 0; i < (int) song.patterns.size(); ++i)
    {
        auto& p = song.patterns[(size_t) i];
        const bool exp = isExpanded (i);
        const int pitch = exp ? kExpandedPitch : itemHeight;
        juce::Rectangle<int> r (4, y - scrollY, juce::jmax (60, getWidth() - 8), pitch - 4);
        y += pitch;

        if (r.getBottom() < 26 || r.getY() > getHeight())
            continue;

        const bool selected = p.id == song.activePatternId;
        const bool used = song.patternInUse (p.id);

        g.setColour (selected ? theme::clipFillSelected : theme::panelLight);
        g.fillRoundedRectangle (r.toFloat(), 3.0f);
        g.setColour (selected ? theme::accent : theme::outline);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 3.0f, selected ? 1.4f : 1.0f);

        // id badge
        juce::Rectangle<int> badge (r.getX() + 4, r.getY() + 5, 22, 16);
        g.setColour (theme::background.withAlpha (0.65f));
        g.fillRoundedRectangle (badge.toFloat(), 2.0f);
        g.setColour (selected ? theme::accent : theme::textDim);
        g.drawText (juce::String (p.id).paddedLeft ('0', 2), badge, juce::Justification::centred, false);

        g.setColour (selected ? theme::text : theme::text.withAlpha (0.85f));
        g.drawText (p.name, r.getX() + 30, r.getY() + 4, r.getWidth() - 36, 17,
                    juce::Justification::centredLeft, true);

        juce::String info;
        info << p.length << " rows  ·  " << p.channels << (p.channels == 1 ? " ch" : " ch");

        if (! used)
            info << "  · pool";

        g.setColour (theme::textDim);
        g.setFont (theme::monoFont (10.0f));
        g.drawText (info, r.getX() + 30, r.getY() + 21, r.getWidth() - 36, 14,
                    juce::Justification::centredLeft, false);
        g.setFont (theme::monoFont (11.5f));

        if (exp)
            drawGridPreview (g, p, r);
    }

    g.restoreState();
}

void PoolComponent::resized()
{
    const int maxScroll = juce::jmax (0, poolContentHeight() - (getHeight() - 34));
    scrollY = juce::jlimit (0, maxScroll, scrollY);
}

juce::Rectangle<int> PoolComponent::dragCellBounds (int index) const
{
    return cellBounds (index).getIntersection (getLocalBounds());
}

juce::ScaledImage PoolComponent::dragPreviewImage (int index)
{
    const auto r = dragCellBounds (index);

    if (r.isEmpty())
        return {};

    // twice the size, so the chip stays sharp when it is drawn back down
    static constexpr double kDragScale = 2.0;

    auto snapshot = createComponentSnapshot (r, true, (float) kDragScale)
                        .convertedToFormat (juce::Image::ARGB);

    // a translucent chip reads as a copy lifted out of the pool
    snapshot.multiplyAllAlphas (0.75f);

    return { snapshot, kDragScale };
}

//==============================================================================
void PoolComponent::mouseDown (const juce::MouseEvent& e)
{
    // plus button
    if (e.y < 26)
    {
        if (e.x > getWidth() - 26)
        {
            auto& p = song.addPattern();
            song.activePatternId = p.id;

            if (onPatternSelected)
                onPatternSelected (p.id);

            if (onModelChanged)
                onModelChanged();

            repaint();
        }

        return;
    }

    const int idx = patternIndexAt (e.getPosition());

    if (idx < 0)
        return;

    song.activePatternId = song.patterns[(size_t) idx].id;

    if (onPatternSelected)
        onPatternSelected (song.activePatternId);

    if (e.mods.isPopupMenu())
    {
        showContextMenu (song.activePatternId);
    }
    else
    {
        dragIndex = idx;
        dragging = false;
    }

    repaint();
}

void PoolComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (dragIndex < 0 || dragging)
        return;

    if (e.getDistanceFromDragStart() > 6)
    {
        auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);

        if (container == nullptr)
            return;

        const int pid = song.patterns[(size_t) dragIndex].id;

        // only the item being dragged should float under the cursor: a null
        // image would make JUCE snapshot the whole pool panel
        const auto grabRect = dragCellBounds (dragIndex);

        if (grabRect.isEmpty())
            return;

        const auto preview = dragPreviewImage (dragIndex);

        // JUCE negates this value and clamps it into the image, so the spot of
        // the cell that was grabbed is the spot kept under the pointer
        auto imageOffset = grabRect.getPosition() - e.getPosition();

        dragging = true;

        container->startDragging (juce::String (kClipDragPrefix) + juce::String (pid), this,
                                  preview, true, &imageOffset, &e.source);
    }
}

void PoolComponent::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    dragIndex = -1;
}

void PoolComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int idx = patternIndexAt (e.getPosition());

    if (idx < 0)
        return;

    const int pid = song.patterns[(size_t) idx].id;

    if (proc.getTransport().playing)
        return;

    const double beat = snapBeat != nullptr ? snapBeat (song.editPlayheadBeats) : song.editPlayheadBeats;

    if (song.addClip (pid, beat) != nullptr)
    {
        song.activePatternId = pid;

        if (onModelChanged)
            onModelChanged();
    }

    repaint();
}

void PoolComponent::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const int maxScroll = juce::jmax (0, poolContentHeight() - (getHeight() - 34));
    scrollY = juce::jlimit (0, maxScroll, scrollY + (int) std::round (-wheel.deltaY * 60.0f));
    repaint();
}

//==============================================================================
void PoolComponent::drawGridPreview (juce::Graphics& g, const Pattern& p,
                                     const juce::Rectangle<int>& r) const
{
    const int rows = juce::jmin (p.length, 12);
    const int cols = juce::jmin (p.channels, 4);
    const int gx = r.getX() + 30;
    const int gw = juce::jmax (0, r.getWidth() - 36);
    const int gy = r.getY() + 37; // below id badge / name / info line
    const int cellH = 8;
    const int cellW = gw / cols;

    // channel separators
    g.setColour (theme::outline.withAlpha (0.6f));
    for (int c = 1; c < cols; ++c)
        g.drawVerticalLine ((float) (gx + c * cellW), (float) gy, (float) (gy + rows * cellH));

    // subtle beat lines every rowsPerBeat rows
    const int rpb = juce::jmax (1, song.settings.rowsPerBeat);
    g.setColour (theme::beatLine);
    for (int row = rpb; row < rows; row += rpb)
        g.drawHorizontalLine ((float) (gy + row * cellH - 1), (float) gx, (float) (gx + gw));

    g.setFont (theme::monoFont (7.5f));

    for (int row = 0; row < rows; ++row)
    {
        const int y = gy + row * cellH;

        for (int c = 0; c < cols; ++c)
        {
            const auto& cell = p.cell (row, c);
            const int x = gx + c * cellW;

            if (cell.note == kEmpty)
            {
                g.setColour (theme::textDim.withAlpha (0.4f));
                g.drawText ("---", x + 2, y + 1, cellW - 5, cellH - 2,
                            juce::Justification::centredLeft, false);
            }
            else if (cell.note == kNoteOff)
            {
                g.setColour (theme::noteOff);
                g.drawText ("===", x + 2, y + 1, cellW - 5, cellH - 2,
                            juce::Justification::centredLeft, false);
            }
            else
            {
                g.setColour (theme::note);
                g.drawText (noteName (cell.note), x + 2, y + 1, cellW - 5, cellH - 2,
                            juce::Justification::centredLeft, false);
            }

            if (cell.cmd != kEmpty)
            {
                g.setColour (cell.cmd >= kMacroCmdBase ? theme::cmdMacro : theme::cmdCC);
                g.fillRect (x + cellW - 3, y + 1, 2, 2);
            }
        }
    }
}

//==============================================================================
double PoolComponent::placeBeat() const
{
    const double beat = song.editPlayheadBeats;
    return snapBeat != nullptr ? snapBeat (beat) : beat;
}

void PoolComponent::showContextMenu (int patternId)
{
    const bool locked = proc.getTransport().playing;
    const bool inUse = song.patternInUse (patternId);

    juce::PopupMenu menu;
    menu.addItem (1, "Edit pattern");
    menu.addItem (2, "Place at playhead", ! locked);
    menu.addSeparator();
    menu.addItem (3, "Copy pattern", ! locked);
    menu.addItem (4, "Copy pattern data", ! locked);
    menu.addItem (5, "Paste at playhead (instance)", ! locked && song.clipboard.valid);
    menu.addItem (6, "Paste at playhead (unique)", ! locked && song.clipboard.valid);
    menu.addSeparator();
    menu.addItem (7, "Duplicate pattern");
    menu.addItem (8, "Rename pattern");
    menu.addItem (9, "Delete pattern", ! inUse);
    menu.addSeparator();
    menu.addItem (10, "Toggle grid preview");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition (),
                        [this, patternId] (int result)
                        {
                            switch (result)
                            {
                                case 1:
                                    song.activePatternId = patternId;

                                    if (onPatternSelected)
                                        onPatternSelected (patternId);
                                    break;

                                case 2:
                                    if (song.addClip (patternId, placeBeat()) != nullptr)
                                        song.activePatternId = patternId;
                                    break;

                                case 3:
                                    song.copyPattern (patternId);
                                    break;

                                case 4:
                                    song.copyPattern (patternId);
                                    song.clipboard.data.name = song.clipboard.data.name + " data";
                                    break;

                                case 5:
                                case 6:
                                    if (song.clipboard.valid)
                                        song.pasteAt (placeBeat(), result == 6);
                                    break;

                                case 7:
                                {
                                    if (auto* p = song.findPattern (patternId))
                                    {
                                        const juce::String name = p->name + " copy";
                                        const int length = p->length;
                                        const int channels = p->channels;
                                        const auto vis = p->vis;
                                        const auto rows = p->rows;

                                        auto& copy = song.addPattern (name, length, channels);
                                        copy.vis = vis;
                                        copy.rows = rows;
                                        copy.length = length;
                                        song.activePatternId = copy.id;

                                        if (onPatternSelected)
                                            onPatternSelected (copy.id);
                                    }

                                    break;
                                }

                                case 8:
                                {
                                    auto* p = song.findPattern (patternId);

                                    if (p == nullptr)
                                        break;

                                    auto* aw = new juce::AlertWindow ("Rename pattern", "Pattern name:",
                                                                     juce::MessageBoxIconType::NoIcon);
                                    aw->addTextEditor ("name", p->name, {});
                                    aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
                                    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

                                    aw->enterModalState (true, juce::ModalCallbackFunction::create (
                                        [this, aw, patternId] (int r)
                                        {
                                            if (r == 1)
                                            {
                                                if (auto* pat = song.findPattern (patternId))
                                                {
                                                    pat->name = aw->getTextEditorContents ("name");

                                                    if (onModelChanged)
                                                        onModelChanged();
                                                }
                                            }

                                            delete aw;
                                            repaint();
                                        }),
                                        false);

                                    break;
                                }

                                case 9:
                                    song.removePattern (patternId);
                                    break;

                                case 10:
                                    toggleGridPreview (patternId);
                                    break;

                                default:
                                    break;
                            }

                            if (onModelChanged)
                                onModelChanged();

                            repaint();
                        });
}

} // namespace rtr
