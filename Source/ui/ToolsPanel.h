#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace rtr
{

/** Labels for the transpose direction buttons. juce::String takes a plain
    narrow string as native bytes, so the UTF-8 for these codepoints is handed
    to it explicitly: a raw arrow in the source came out as three separate
    characters on screen. */
inline juce::String arrowUpGlyph() { return juce::String::fromUTF8 (u8"\u2191"); }
inline juce::String arrowDownGlyph() { return juce::String::fromUTF8 (u8"\u2193"); }

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
    // the amount is an editable field the two direction buttons read, so
    // transposing is one value plus one click rather than four fixed steps
    juce::Slider transposeAmount { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    juce::TextButton transposeUp { arrowUpGlyph() };
    juce::TextButton transposeDown { arrowDownGlyph() };
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
