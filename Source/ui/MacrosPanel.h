#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include <array>

namespace rtr
{

/** Editor for the custom tracker command macros (arpeggiator / glide / slide). */
class MacrosPanel final : public juce::Component
{
public:
    explicit MacrosPanel (ReTrektorAudioProcessor& processor);
    ~MacrosPanel() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    void refresh();

    std::function<void()> onModelChanged;

    // the Alt pair is answered here as well as in the editor, so a press aimed
    // at undo reaches whichever panel the host routed it to
    std::function<bool()> undoStep;
    std::function<bool()> redoStep;

    // the cell clipboard's Alt spellings are answered here as well, with the
    // editor's own copy, paste and cut injected rather than reached here, so the
    // steps walk the same cell clipboard the Ctrl pair and the tracker's menu use
    std::function<void()> copyStep;
    std::function<void()> pasteStep;
    std::function<void()> cutStep;

    // the clip slice at the playhead, Alt+S, with the editor's slice injected so
    // the one spelling still splits the timeline; its old Ctrl+X spelling was
    // answered by the host rather than by the plug-in, so the Alt letter is what
    // every panel answers
    std::function<void()> sliceStep;

private:
    static constexpr int kMaxRows = 16;

    struct RowControls
    {
        std::unique_ptr<juce::TextEditor> name;
        std::unique_ptr<juce::ComboBox> kind;
        std::unique_ptr<juce::Slider> p1;
        std::unique_ptr<juce::Slider> p2;
    };

    void rebuild();
    void syncValues();
    int rowStep() const;

    ReTrektorAudioProcessor& proc;
    rtr::Song& song;

    std::array<RowControls, kMaxRows> controls;
    juce::TextButton addButton { "+ Add macro" };
    juce::TextButton removeButton { "- Remove last" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MacrosPanel)
};

} // namespace rtr
