#include "TimelineComponent.h"
#include "Theme.h"

namespace rtr
{

static constexpr const char* kClipDragPrefix = "rtr-pattern:";

TimelineComponent::TimelineComponent (ReTrektorAudioProcessor& processor)
    : proc (processor), song (processor.getSong())
{
    setWantsKeyboardFocus (false);
    startTimerHz (30);
}

//==============================================================================
bool TimelineComponent::playbackLocked() const
{
    return proc.getTransport().playing;
}

void TimelineComponent::refresh()
{
    clampScroll();
    repaint();
}

void TimelineComponent::setSelectedPattern (int patternId)
{
    repaint();
    juce::ignoreUnused (patternId);
}

//==============================================================================
void TimelineComponent::resized()
{
    clampScroll();
}

void TimelineComponent::clampScroll()
{
    const double end = juce::jmax (song.songEndBeats(), 16.0);
    const double visible = (double) juce::jmax (1, getWidth()) / pixelsPerBeat;
    scrollBeats = juce::jlimit (0.0, juce::jmax (0.0, end - visible * 0.25), scrollBeats);
}

double TimelineComponent::minPixelsPerBeat() const
{
    // Zoom out far enough that the whole song still fits in the timeline.
    const double end = juce::jmax (song.songEndBeats(), 8.0);
    const double usable = (double) juce::jmax (120, getWidth() - 8);
    return juce::jlimit (0.02, 4.0, usable / end);
}

void TimelineComponent::setPixelsPerBeat (double newValue)
{
    pixelsPerBeat = juce::jlimit (juce::jmin (4.0, minPixelsPerBeat()), 420.0, newValue);
    clampScroll();
    repaint();
}

void TimelineComponent::zoomBy (double factor, double anchorBeat)
{
    const double oldPpb = pixelsPerBeat;
    const double newPpb = juce::jlimit (juce::jmin (4.0, minPixelsPerBeat()), 420.0, pixelsPerBeat * factor);

    if (juce::approximatelyEqual (oldPpb, newPpb))
        return;

    const double anchorX = (anchorBeat - scrollBeats) * oldPpb;
    pixelsPerBeat = newPpb;
    scrollBeats = anchorBeat - anchorX / newPpb;
    clampScroll();
    repaint();
}

//==============================================================================
juce::Rectangle<float> TimelineComponent::clipBounds (const Clip& clip) const
{
    const double len = song.clipLengthBeats (clip);
    const float x1 = xAtBeat (clip.startBeats);
    const float x2 = xAtBeat (clip.startBeats + len);

    return { x1, (float) rulerHeight + 4.0f, juce::jmax (3.0f, x2 - x1), (float) getHeight() - (float) rulerHeight - 8.0f };
}

void TimelineComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);

    auto bounds = getLocalBounds();
    auto ruler = bounds.removeFromTop (rulerHeight);
    auto clipsArea = bounds;

    drawGrid (g, clipsArea);
    drawClips (g, clipsArea);

    // drop preview for pool / clipboard drags
    if (dropHoverValid && dropPatternId >= 0)
    {
        const double len = song.patternLengthBeats (dropPatternId);
        const float x1 = xAtBeat (dropHoverBeat);
        const float x2 = xAtBeat (dropHoverBeat + len);
        juce::Rectangle<float> r (x1, (float) rulerHeight + 4.0f,
                                  juce::jmax (3.0f, x2 - x1),
                                  (float) getHeight() - (float) rulerHeight - 8.0f);

        const bool fits = song.canPlace (dropHoverBeat, len);
        const auto colour = fits ? theme::accent : theme::playhead;

        g.setColour (colour.withAlpha (0.20f));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (colour.withAlpha (0.85f));
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.5f);
    }

    drawRuler (g, ruler);
    drawPlayhead (g, getLocalBounds());
}

void TimelineComponent::drawGrid (juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto transport = proc.getTransport();
    const double ppqPerBar = transport.ppqPerBar > 0.0 ? transport.ppqPerBar : 4.0;

    // beat lines
    const double firstBeat = std::floor (scrollBeats);
    const double visibleBeats = (double) area.getWidth() / pixelsPerBeat;

    g.setColour (theme::outline.withAlpha (0.45f));

    for (double b = firstBeat; b < scrollBeats + visibleBeats; b += 1.0)
    {
        const float x = xAtBeat (b);

        if (x >= area.getX() && x <= area.getRight())
            g.drawVerticalLine ((int) x, (float) area.getY(), (float) area.getBottom());
    }

    // bar lines
    g.setColour (theme::outline);

    for (double b = std::floor (scrollBeats / ppqPerBar) * ppqPerBar;
         b < scrollBeats + visibleBeats;
         b += ppqPerBar)
    {
        const float x = xAtBeat (b);

        if (x >= area.getX() && x <= area.getRight())
            g.drawVerticalLine ((int) x, (float) area.getY(), (float) area.getBottom());
    }

    // snap grid (when enabled)
    if (song.settings.snapEnabled)
    {
        const double rows = song.currentGridRows (pixelsPerBeat);
        const double gridBeats = rows * song.beatsPerRow();

        if (gridBeats > 0.0 && gridBeats * pixelsPerBeat > 6.0)
        {
            g.setColour (theme::outline.withAlpha (0.22f));

            for (double b = std::floor (scrollBeats / gridBeats) * gridBeats; b < scrollBeats + visibleBeats; b += gridBeats)
            {
                const float x = xAtBeat (b);

                if (x >= area.getX() && x <= area.getRight())
                    g.drawVerticalLine ((int) x, (float) area.getY() + area.getHeight() * 0.5f, (float) area.getBottom());
            }
        }
    }
}

void TimelineComponent::drawRuler (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (theme::panel);
    g.fillRect (area);
    g.setColour (theme::outline);
    g.drawHorizontalLine (area.getBottom() - 1, (float) area.getX(), (float) area.getRight());

    g.setFont (theme::monoFont (11.0f));
    g.setColour (theme::textDim);

    const auto transport = proc.getTransport();
    const double ppqPerBar = transport.ppqPerBar > 0.0 ? transport.ppqPerBar : 4.0;
    const double visibleBeats = (double) area.getWidth() / pixelsPerBeat;
    const double firstBar = std::floor (scrollBeats / ppqPerBar);

    for (double bar = firstBar; bar * ppqPerBar < scrollBeats + visibleBeats; bar += 1.0)
    {
        const double beat = bar * ppqPerBar;
        const float x = xAtBeat (beat);

        if (x < area.getX() - 10 || x > area.getRight())
            continue;

        g.setColour (theme::outline);
        g.drawVerticalLine ((int) x, (float) area.getY(), (float) area.getY() + 5.0f);
        g.setColour (theme::textDim);
        g.drawText (juce::String ((int) bar + 1), (int) x + 3, area.getY(), 42, area.getHeight(),
                    juce::Justification::centredLeft, false);
    }

    // transport readout
    const double ppq = transport.ppq;
    const int bar = (int) std::floor (ppq / ppqPerBar) + 1;
    const int beat = (int) std::floor (std::fmod (ppq, ppqPerBar)) + 1;
    const int row = (int) std::floor (std::fmod (ppq, song.beatsPerRow() * 4.0) / song.beatsPerRow()) + 1;

    juce::String pos;
    pos << (transport.playing ? "PLAY " : "STOP ")
        << juce::String (bar).paddedLeft ('0', 2) << ":"
        << juce::String (beat) << ":" << juce::String (row).paddedLeft ('0', 2)
        << "   " << juce::String (transport.bpm, 1) << " BPM";

    g.setColour (transport.playing ? theme::note : theme::textDim);
    g.setFont (theme::monoFont (11.0f));
    g.drawText (pos, area.withTrimmedRight (6), juce::Justification::centredRight, false);
}

void TimelineComponent::drawClips (juce::Graphics& g, juce::Rectangle<int> area)
{
    for (auto& clip : song.clips)
    {
        auto r = clipBounds (clip);

        if (r.getRight() < area.getX() || r.getX() > area.getRight())
            continue;

        const bool selected = clip.patternId == song.activePatternId;
        const auto* pattern = song.findPattern (clip.patternId);

        juce::Colour fill = selected ? theme::clipFillSelected : theme::clipFill;

        g.setColour (fill);
        g.fillRoundedRectangle (r, 3.0f);

        g.setColour (selected ? theme::accent : theme::clipOutline);
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, selected ? 1.6f : 1.0f);

        // clip label
        if (pattern != nullptr && r.getWidth() > 26.0f)
        {
            auto labelArea = r.reduced (4.0f, 3.0f).withHeight (13.0f);
            g.setColour (selected ? theme::text : theme::textDim);
            g.setFont (theme::monoFont (10.5f));

            juce::String label;
            label << pattern->id << " " << pattern->name;
            g.drawText (label, labelArea.toNearestInt(), juce::Justification::topLeft, false);

            // row ticks
            if (pixelsPerBeat > 26.0)
            {
                g.setColour (theme::outline.withAlpha (0.7f));
                const double rowsPerBeat = (double) juce::jmax (1, song.settings.rowsPerBeat);
                const int tickStep = juce::jmax (1, (int) rowsPerBeat);

                for (int row = tickStep; row < pattern->length; row += tickStep)
                {
                    const double beat = clip.startBeats + (double) row / rowsPerBeat;
                    const float x = xAtBeat (beat);

                    if (x > r.getX() + 1 && x < r.getRight() - 1)
                        g.drawVerticalLine ((int) x, r.getY() + 16.0f, r.getBottom() - 1.0f);
                }
            }
        }

        // resize handle
        g.setColour ((selected ? theme::accent : theme::clipOutline).withAlpha (0.9f));
        g.fillRect (juce::Rectangle<float> (r.getRight() - 3.0f, r.getY() + 2.0f, 2.0f, r.getHeight() - 4.0f));
    }

}

void TimelineComponent::drawPlayhead (juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto transport = proc.getTransport();
    const double beat = transport.playing ? transport.ppq : song.editPlayheadBeats;
    const float x = xAtBeat (beat);

    if (x < area.getX() || x > area.getRight())
        return;

    g.setColour (theme::playhead);
    g.drawVerticalLine ((int) x, (float) area.getY(), (float) area.getBottom());

    juce::Path marker;
    marker.addTriangle ((float) x - 4.0f, (float) area.getY(), (float) x + 4.0f, (float) area.getY(), (float) x, (float) area.getY() + 6.0f);
    g.fillPath (marker);
}

//==============================================================================
void TimelineComponent::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    dragChangedModel = false;
    dragClip = juce::Uuid();

    if (e.mods.isMiddleButtonDown())
    {
        dragMode = DragMode::pan;
        panAnchor = e.getPosition();
        panAnchorBeats = scrollBeats;
        return;
    }

    const double beat = beatAtX ((float) e.x);

    // Which clip is under the mouse?
    Clip* hit = nullptr;

    for (auto& clip : song.clips)
    {
        if (clipBounds (clip).contains (e.position))
        {
            hit = &clip;
            break;
        }
    }

    if (hit != nullptr)
    {
        song.activePatternId = hit->patternId;

        if (onPatternSelected)
            onPatternSelected (hit->patternId);

        auto r = clipBounds (*hit);

        if (e.mods.isPopupMenu())
        {
            dragMode = DragMode::none;
            showContextMenu (e, hit);
            repaint();
            return;
        }

        if (playbackLocked())
        {
            dragMode = DragMode::none;
            repaint();
            return;
        }

        dragClip = hit->uid;
        dragOriginalStart = hit->startBeats;
        dragOriginalRows = song.findPattern (hit->patternId) != nullptr ? song.findPattern (hit->patternId)->length : 1;

        if (e.position.x > r.getRight() - 6.0f)
        {
            dragMode = DragMode::resizeClip;
        }
        else
        {
            dragMode = DragMode::moveClip;
            dragGrabOffsetBeats = beat - hit->startBeats;
        }

        repaint();
        return;
    }

    // empty space
    if (e.mods.isPopupMenu())
    {
        dragMode = DragMode::none;
        showContextMenu (e, nullptr);
        return;
    }

    dragMode = DragMode::none;

    if (onPatternSelected)
        onPatternSelected (song.activePatternId);

    repaint();
}

void TimelineComponent::mouseDrag (const juce::MouseEvent& e)
{
    switch (dragMode)
    {
        case DragMode::pan:
        {
            const double delta = (double) (panAnchor.x - e.getPosition().x) / pixelsPerBeat;
            scrollBeats = juce::jmax (0.0, panAnchorBeats + delta);
            clampScroll();
            repaint();
            break;
        }

        case DragMode::moveClip:
        {
            if (playbackLocked())
                break;

            auto* clip = song.findClip (dragClip);

            if (clip == nullptr)
                break;

            double target = beatAtX ((float) e.x) - dragGrabOffsetBeats;
            target = song.snapBeats (target, pixelsPerBeat);

            if (song.moveClip (dragClip, target))
            {
                dragChangedModel = true;
                repaint();
            }

            break;
        }

        case DragMode::resizeClip:
        {
            if (playbackLocked())
                break;

            auto* clip = song.findClip (dragClip);

            if (clip == nullptr)
                break;

            const double endBeat = beatAtX ((float) e.x);
            double rows = (endBeat - clip->startBeats) / song.beatsPerRow();

            if (song.settings.snapEnabled)
            {
                const double grid = juce::jmax (1.0, song.currentGridRows (pixelsPerBeat));
                rows = std::round (rows / grid) * grid;
            }

            const int newRows = juce::jlimit (1, kMaxRows, (int) std::llround (rows));

            if (newRows != dragOriginalRows)
            {
                if (song.resizeClip (dragClip, newRows))
                {
                    dragOriginalRows = newRows;
                    dragChangedModel = true;
                    repaint();
                }
            }

            break;
        }

        default:
            break;
    }
}

void TimelineComponent::mouseUp (const juce::MouseEvent&)
{
    dragMode = DragMode::none;
    dragClip = juce::Uuid();

    if (dragChangedModel)
    {
        dragChangedModel = false;

        if (onModelChanged)
            onModelChanged();
    }
    repaint();
}

void TimelineComponent::mouseMove (const juce::MouseEvent& e)
{
    for (auto& clip : song.clips)
    {
        if (clipBounds (clip).contains (e.position))
        {
            auto r = clipBounds (clip);
            setMouseCursor (e.position.x > r.getRight() - 6.0f ? juce::MouseCursor::LeftRightResizeCursor
                                                              : juce::MouseCursor::NormalCursor);
            return;
        }
    }

    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void TimelineComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (playbackLocked())
        return;

    const double beat = song.snapBeats (beatAtX ((float) e.x), pixelsPerBeat);
    Clip* hit = nullptr;

    for (auto& clip : song.clips)
        if (clipBounds (clip).contains (e.position))
            hit = &clip;

    if (hit == nullptr)
    {
        // create a new pattern and place it here
        auto& p = song.addPattern();
        if (song.addClip (p.id, beat) != nullptr)
        {
            song.activePatternId = p.id;

            if (onPatternSelected)
                onPatternSelected (p.id);

            if (onModelChanged)
                onModelChanged();
        }
        else
        {
            song.removePattern (p.id);
        }
    }

    repaint();
}

void TimelineComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCtrlDown() || e.mods.isCommandDown() || e.mods.isShiftDown())
    {
        const double anchor = beatAtX ((float) e.x);
        zoomBy (wheel.deltaY > 0 ? 1.15 : 1.0 / 1.15, anchor);
    }
    else if (e.mods.isAltDown())
    {
        scrollBeats = juce::jmax (0.0, scrollBeats - wheel.deltaY * 8.0);
        clampScroll();
        repaint();
    }
    else
    {
        scrollBeats = juce::jmax (0.0, scrollBeats - wheel.deltaY * 6.0);
        clampScroll();
        repaint();
    }
}

void TimelineComponent::timerCallback()
{
    // repaint the playhead while playing
    repaint();
}

//==============================================================================
void TimelineComponent::showContextMenu (const juce::MouseEvent& e, Clip* clip)
{
    juce::PopupMenu menu;

    const bool locked = playbackLocked();
    const bool canPaste = ! locked && song.clipboard.valid;
    const double pasteBeat = song.snapBeats (song.editPlayheadBeats, pixelsPerBeat);

    if (clip != nullptr)
    {
        const juce::Uuid uid = clip->uid;
        const int patternId = clip->patternId;

        menu.addItem (1, "Copy pattern", ! locked);
        menu.addSeparator();
        menu.addItem (2, "Paste as instance at playhead", canPaste);
        menu.addItem (3, "Paste as unique pattern at playhead", canPaste);
        menu.addSeparator();
        menu.addItem (4, "Slice at playhead", ! locked && song.settings.snapEnabled);
        menu.addItem (5, "Duplicate", ! locked);
        menu.addSeparator();
        menu.addItem (6, "Delete instance", ! locked);

        const bool inUse = song.patternInUse (patternId);
        menu.addItem (7, "Delete pattern (remove from pool)", ! inUse);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [this, uid, patternId, pasteBeat] (int result)
                            {
                                switch (result)
                                {
                                    case 1:
                                        song.copyPattern (patternId);
                                        break;

                                    case 2:
                                        if (auto* c = song.pasteAt (pasteBeat, false))
                                            song.activePatternId = c->patternId;
                                        break;

                                    case 3:
                                        if (auto* c = song.pasteAt (pasteBeat, true))
                                            song.activePatternId = c->patternId;
                                        break;

                                    case 4:
                                        if (song.settings.snapEnabled)
                                            song.sliceClipAt (uid, pasteBeat);
                                        break;

                                    case 5:
                                    {
                                        if (auto* original = song.findClip (uid))
                                        {
                                            const double start = original->startBeats + song.clipLengthBeats (*original);

                                            if (auto* copy = song.addClip (patternId, start))
                                            {
                                                song.activePatternId = copy->patternId;
                                            }
                                        }

                                        break;
                                    }

                                    case 6:
                                        song.removeClip (uid);
                                        break;

                                    case 7:
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
    else
    {
        const double beat = song.snapBeats (beatAtX ((float) e.x), pixelsPerBeat);

        menu.addItem (3, "Paste as instance here", canPaste);
        menu.addItem (4, "Paste as unique pattern here", canPaste);
        menu.addSeparator();
        menu.addItem (10, "Create new pattern here", ! locked);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [this, beat] (int result)
                            {
                                if (result == 3 || result == 4)
                                {
                                    if (auto* c = song.pasteAt (beat, result == 4))
                                        song.activePatternId = c->patternId;
                                }
                                else if (result == 10)
                                {
                                    auto& p = song.addPattern();

                                    if (song.addClip (p.id, beat) == nullptr)
                                        song.removePattern (p.id);
                                    else
                                        song.activePatternId = p.id;
                                }

                                if (onModelChanged)
                                    onModelChanged();

                                repaint();
                            });
    }
}

//==============================================================================
bool TimelineComponent::isInterestedInDragSource (const SourceDetails& details)
{
    return details.description.toString().startsWith (kClipDragPrefix);
}

void TimelineComponent::itemDragEnter (const SourceDetails& details)
{
    const juce::String desc = details.description.toString();
    dropPatternId = desc.startsWith (kClipDragPrefix)
                        ? desc.fromFirstOccurrenceOf (kClipDragPrefix, false, false).getIntValue()
                        : -1;
    dropHoverValid = dropPatternId >= 0;
    dropHoverBeat = song.snapBeats (beatAtX ((float) details.localPosition.x), pixelsPerBeat);
    repaint();
}

void TimelineComponent::itemDragMove (const SourceDetails& details)
{
    dropHoverBeat = song.snapBeats (beatAtX ((float) details.localPosition.x), pixelsPerBeat);
    repaint();
}

void TimelineComponent::itemDragExit (const SourceDetails&)
{
    dropHoverValid = false;
    dropPatternId = -1;
    repaint();
}

void TimelineComponent::itemDropped (const SourceDetails& details)
{
    dropHoverValid = false;
    dropPatternId = -1;

    if (playbackLocked())
        return;

    const juce::String desc = details.description.toString();

    if (! desc.startsWith (kClipDragPrefix))
        return;

    const int patternId = desc.fromFirstOccurrenceOf (kClipDragPrefix, false, false).getIntValue();
    const double beat = song.snapBeats (beatAtX ((float) details.localPosition.x), pixelsPerBeat);

    if (song.addClip (patternId, beat) != nullptr)
    {
        song.activePatternId = patternId;

        if (onModelChanged)
            onModelChanged();
    }

    repaint();
}

} // namespace rtr
