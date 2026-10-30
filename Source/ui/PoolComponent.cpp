#include "PoolComponent.h"
#include "Theme.h"

namespace rtr
{

static constexpr const char* kClipDragPrefix = "rtr-pattern:";

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
juce::Rectangle<int> PoolComponent::cellBounds (int index) const
{
    const int headerH = 26;
    const int w = juce::jmax (60, getWidth() - 8);
    return { 4, headerH + 4 + index * itemHeight - scrollY, w, itemHeight - 4 };
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

    for (int i = 0; i < (int) song.patterns.size(); ++i)
    {
        auto& p = song.patterns[(size_t) i];
        auto r = cellBounds (i);

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
    }

    g.restoreState();
}

void PoolComponent::resized()
{
    const int maxScroll = juce::jmax (0, (int) song.patterns.size() * itemHeight - (getHeight() - 34));
    scrollY = juce::jlimit (0, maxScroll, scrollY);
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
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            dragging = true;
            const int pid = song.patterns[(size_t) dragIndex].id;
            container->startDragging (juce::String (kClipDragPrefix) + juce::String (pid), this, juce::ScaledImage(), true);
        }
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
    const int maxScroll = juce::jmax (0, (int) song.patterns.size() * itemHeight - (getHeight() - 34));
    scrollY = juce::jlimit (0, maxScroll, scrollY + (int) std::round (-wheel.deltaY * 60.0f));
    repaint();
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

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
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
                                        const bool sInst = p->showInst;
                                        const bool sVol = p->showVol;
                                        const bool sPan = p->showPan;
                                        const bool sCmd = p->showCmd;
                                        const auto rows = p->rows;

                                        auto& copy = song.addPattern (name, length, channels);
                                        copy.showInst = sInst;
                                        copy.showVol = sVol;
                                        copy.showPan = sPan;
                                        copy.showCmd = sCmd;
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

                                default:
                                    break;
                            }

                            if (onModelChanged)
                                onModelChanged();

                            repaint();
                        });
}

} // namespace rtr
