#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace rtr
{

class ToolsPanel final : public juce::Component
{
public:
    explicit ToolsPanel (ReTtrektorAudioProcessor& processor);
    ~ToolsPanel() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<std::pair<int, int>()> getSelection; // returns {start, end} or {-1,-1}
    std::function<void()> onModelChanged;

private:
    void applyTranspose (int semitones);
    void applyRemap();
    void applyInterpolate();
    std::vector<Pattern*> scopedPatterns();
    void applyToRows (Pattern& p, const std::function<void (Cell&, int row)>& fn);

    ReTtrektorAudioProcessor& proc;
    rtr::Song& song;

    juce::ComboBox scopeBox;
    juce::TextButton transposeDown12 { "-12" };
    juce::TextButton transposeDown1 { "-1" };
    juce::TextButton transposeUp1 { "+1" };
    juce::TextButton transposeUp12 { "+12" };
    juce::ComboBox remapBox;
    juce::TextButton remapApply { "Remap" };
    juce::TextButton interpolateButton { "Interpolate" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToolsPanel)
};

} // namespace rtr
