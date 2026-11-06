#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace rtr
{

class TimelineComponent final : public juce::Component,
                                public juce::DragAndDropTarget,
                                private juce::Timer
{
public:
    explicit TimelineComponent (ReTrektorAudioProcessor& processor);
    ~TimelineComponent() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    void setSelectedPattern (int patternId);
    void setPixelsPerBeat (double newValue);
    double getPixelsPerBeat() const noexcept { return pixelsPerBeat; }
    void zoomBy (double factor, double anchorBeat);

    /** Snaps a beat position to the timeline's current grid (honouring the
        snap toggle and adaptive/fixed grid settings). */
    double snapBeat (double beat) const noexcept { return song.snapBeats (beat, pixelsPerBeat); }

    double beatAtX (float x) const noexcept { return scrollBeats + (double) x / pixelsPerBeat; }
    float xAtBeat (double beat) const noexcept { return (float) ((beat - scrollBeats) * pixelsPerBeat); }

    void refresh();

    std::function<void()> onModelChanged;
    std::function<void (int)> onPatternSelected;

    // the Alt pair is answered here as well as in the editor, so a press aimed
    // at undo reaches the panel the host routed it to; these are the editor's
    // two steps, injected rather than called directly so the whole view
    // refreshes and no extra point of the undo line is recorded
    std::function<bool()> undoStep;
    std::function<bool()> redoStep;

    bool playbackLocked() const;

private:
    enum class DragMode { none, moveClip, resizeClip, pan };

    void timerCallback() override;
    void showContextMenu (const juce::MouseEvent&, Clip* clip);

    juce::Rectangle<float> clipBounds (const Clip&) const;
    void clampScroll();
    void jumpPlayheadToClickedX (int x);
    double minPixelsPerBeat() const;
    void drawRuler (juce::Graphics&, juce::Rectangle<int> area);
    void drawClips (juce::Graphics&, juce::Rectangle<int> area);
    void drawPlayhead (juce::Graphics&, juce::Rectangle<int> area);
    void drawGrid (juce::Graphics&, juce::Rectangle<int> area);

    ReTrektorAudioProcessor& proc;
    rtr::Song& song;

    double pixelsPerBeat = 48.0;
    double scrollBeats = 0.0;
    int rulerHeight = 20;

    DragMode dragMode = DragMode::none;
    juce::Uuid dragClip;
    double dragGrabOffsetBeats = 0.0;
    double dragOriginalStart = 0.0;
    int dragOriginalRows = 0;
    bool dragChangedModel = false;

    int dropPatternId = -1;
    bool dropHoverValid = false;
    double dropHoverBeat = 0.0;

    juce::Point<int> panAnchor;
    double panAnchorBeats = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimelineComponent)
};

} // namespace rtr
