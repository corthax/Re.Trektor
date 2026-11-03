#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace rtr
{

class ToolsPanel final : public juce::Component
{
public:
    explicit ToolsPanel (ReTrektorAudioProcessor& processor);
    ~ToolsPanel() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<std::pair<int, int>()> getSelection; // returns {start, end} or {-1,-1}
    std::function<int()> getCursorChannel; // channel the cursor sits in, or -1
    std::function<void()> onModelChanged;

private:
    void applyTranspose (int semitones);
    void applyRemap();
    void applyInterpolate();
    std::vector<Pattern*> scopedPatterns();
    // rows the current scope selects inside a pattern: { start, end },
    // or { -1, -1 } when the selection scope has no row selection
    std::pair<int,int> rowRange (const Pattern& p) const;
    void applyToRows (Pattern& p, const std::function<void (Cell&, int row)>& fn);

    ReTrektorAudioProcessor& proc;
    rtr::Song& song;

    juce::ComboBox scopeBox;
    juce::TextButton transposeDown12 { "-12" };
    juce::TextButton transposeDown1 { "-1" };
    juce::TextButton transposeUp1 { "+1" };
    juce::TextButton transposeUp12 { "+12" };
    juce::ComboBox remapFromBox;
    juce::ComboBox remapToBox;
    juce::TextButton remapApply { "Remap" };
    // what the interpolation ramps and how wide it reaches
    juce::ComboBox interpolateFieldBox;
    juce::ComboBox interpolateWhereBox;
    juce::TextButton interpolateButton { "Interpolate" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToolsPanel)
};

} // namespace rtr
