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
    bool keyPressed (const juce::KeyPress&) override;

    /** The tracker's selection: which cells it is holding. The scope dropdown
        decides what the tools reach, and the selection scope asks this cell by
        cell, so a tool can be limited to one column of one row. */
    std::function<bool (int row, int ch)> isCellSelected;
    // the channel the tracker's cursor sits in, or -1
    std::function<int()> getCursorChannel;
    std::function<void()> onModelChanged;

    // the Alt pair is answered here as well as in the editor, so a press aimed
    // at undo reaches whichever panel the host routed it to
    std::function<bool()> undoStep;
    std::function<bool()> redoStep;

    // the cell clipboard's Alt pair is answered here as well, with the editor's
    // own copy and paste injected rather than reached here, so the pair walks
    // the same cell clipboard the Ctrl pair and the tracker's menu use
    std::function<void()> copyStep;
    std::function<void()> pasteStep;

private:
    void applyTranspose (int semitones);
    void applyRemap();
    void applyInterpolate();
    std::vector<Pattern*> scopedPatterns();
    /** Whether a cell of a pattern is in reach of the current scope: the wider
        scopes name whole patterns and ignore the selection, while the
        selection scope reaches exactly the cells the tracker is holding. */
    bool cellInScope (int row, int ch) const;
    void applyToCells (Pattern& p, const std::function<void (Cell&, int row, int ch)>& fn);

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
    // what the interpolation ramps; how wide it reaches is the selection itself,
    // so the panel no longer needs a channel dropdown beside it
    juce::ComboBox interpolateFieldBox;
    juce::TextButton interpolateButton { "Interpolate" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToolsPanel)
};

} // namespace rtr
