#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include <array>

namespace rtr
{

/** Editor for the arpeggiator macros the Command column's letter A calls. */
class MacrosPanel final : public juce::Component
{
public:
    explicit MacrosPanel (ReTrektorAudioProcessor& processor);
    ~MacrosPanel() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

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
    struct RowControls
    {
        std::unique_ptr<juce::Slider> len;
        std::unique_ptr<juce::ToggleButton> loop;
    };

    void rebuild();
    void syncValues();
    int cardHeight (int length) const; // one control line plus the step lines it needs
    int blockTop (int id) const; // screen y of a macro's card, scroll included
    int contentHeight() const;
    void clampScroll();

    ReTrektorAudioProcessor& proc;
    rtr::Song& song;

    std::array<RowControls, kMaxArpMacros> controls;

    // the steps are drawn rather than made into one control each, which is what
    // keeps a macro of 64 of them from putting 64 sliders on the panel, so they
    // are entered by keyboard at the cursor the arrows and clicks move
    int stepId = 0;   // macro the step cursor is on (0 = none)
    int stepIndex = 0; // step of that macro the cursor is on
    bool typingStep = false;
    juce::String stepText;

    int scrollY = 0;

    void setStepCursor (int id, int index);
    void commitStepText();
    int prevDefArpId (int id) const; // nearest defined macro above a card
    int nextDefArpId (int id) const; // nearest defined macro below one

    juce::TextButton addButton { "+ Add macro" };
    juce::TextButton removeButton { "- Remove last" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MacrosPanel)
};

} // namespace rtr
