#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace rtr
{

class PoolComponent final : public juce::Component,
                            private juce::Timer
{
public:
    explicit PoolComponent (ReTtrektorAudioProcessor& processor);
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

private:
    void timerCallback() override;
    void showContextMenu (int patternId);
    double placeBeat() const;
    int patternIndexAt (juce::Point<int>) const;
    juce::Rectangle<int> cellBounds (int index) const;
    int itemHeight = 42;
    int scrollY = 0;

    ReTtrektorAudioProcessor& proc;
    rtr::Song& song;

    bool dragging = false;
    int dragIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PoolComponent)
};

} // namespace rtr
