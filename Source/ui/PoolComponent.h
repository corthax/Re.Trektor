#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include <vector>

namespace rtr
{

class PoolComponent final : public juce::Component,
                            private juce::Timer
{
public:
    explicit PoolComponent (ReTrektorAudioProcessor& processor);
    ~PoolComponent() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    void refresh();

    std::function<void (int)> onPatternSelected;
    std::function<void()> onModelChanged;

    // the Alt pair is answered here as well as in the editor, and the pool is
    // what a pattern dragged from it is attached to, which is where a key press
    // goes while that drag is still modal
    std::function<bool()> undoStep;
    std::function<bool()> redoStep;

    // the cell clipboard's Alt spellings are answered here as well, with the
    // editor's own copy, paste and cut injected rather than reached here, so a
    // press aimed at any of them is heard while the pool's own drag is still
    // modal and the steps land on the clipboard the Ctrl pair and the menu use
    std::function<void()> copyStep;
    std::function<void()> pasteStep;
    std::function<void()> cutStep;

    // the clip slice at the playhead, Alt+S, with the editor's slice injected:
    // the Ctrl+X spelling the timeline used answered for the host instead of the
    // plug-in, so the Alt letter is what every panel answers, including while the
    // pool is still mid-drag and the editor is never consulted at all
    std::function<void()> sliceStep;

    // the panel hands keyboard focus back when its own menu or the rename alert
    // has closed and when a drag it started is released, since a popup and a
    // dragged item are both windows with no parent component and closing one
    // leaves nothing focused
    std::function<void()> focusRescue;

    /** Optional snap function supplied by the editor (uses the timeline grid). */
    std::function<double (double)> snapBeat;

    /** Expandable grid: pool items can grow to preview the pattern's note
        grid (first rows x channels). Toggled from the pattern context menu. */
    bool isExpanded (int index) const;
    int poolContentHeight() const;
    void toggleGridPreview (int patternId);

    /** A pool drag floats only the item's cell at the cursor (its grid preview
        included when the item is expanded). Handing JUCE a null image would
        snapshot the whole panel instead, so the drag preview is built here:
        dragCellBounds is the part of the cell that is on screen, and dragPreviewImage
        renders exactly that, semi-transparent, at twice the size so it stays
        crisp when it is drawn back down. */
    juce::Rectangle<int> dragCellBounds (int index) const;
    juce::ScaledImage dragPreviewImage (int index);

private:
    void timerCallback() override;
    void showContextMenu (int patternId);
    double placeBeat() const;
    int patternIndexAt (juce::Point<int>) const;
    juce::Rectangle<int> cellBounds (int index) const;
    int itemHeight = 42;
    int scrollY = 0;

    void drawGridPreview (juce::Graphics& g, const Pattern& p,
                          const juce::Rectangle<int>& r) const;

    ReTrektorAudioProcessor& proc;
    rtr::Song& song;

    bool dragging = false;
    int dragIndex = -1;

    std::vector<int> expandedIds; // pattern ids currently showing the grid preview

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PoolComponent)
};

} // namespace rtr
