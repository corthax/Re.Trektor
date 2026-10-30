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

    void refresh();

    std::function<void (int)> onPatternSelected;
    std::function<void()> onModelChanged;

    /** Optional snap function supplied by the editor (uses the timeline grid). */
    std::function<double (double)> snapBeat;

    /** Expandable grid: pool items can grow to preview the pattern's note
        grid (first rows x channels). Toggled from the pattern context menu. */
    bool isExpanded (int index) const;
    int poolContentHeight() const;
    void toggleGridPreview (int patternId);

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
