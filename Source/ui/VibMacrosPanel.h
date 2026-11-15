#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include <array>
#include <functional>

namespace rtr
{

/** Editor for the vibrato macros the Command column's letter V calls: one card
    per macro that is present, each holding the three settings a wobble runs -
    its shape as a dropdown, its amplitude and its cycle length as knobs - and
    added to and removed from the same way the arpeggiator macros are, the lowest
    id that has none being what a fresh one takes and the one the last gesture
    picked out being what the remove button puts away. A macro holds a value for
    each of its three settings at all times, so the card only says whether it is
    one: an amplitude knob turned down to off still has a card, and a macro that
    was only ever asked to swing at off is a macro rather than no macro at all. */
class VibMacrosPanel final : public juce::Component
{
public:
    explicit VibMacrosPanel (ReTrektorAudioProcessor& processor);
    ~VibMacrosPanel() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void refresh();

    // where the scroll has got to, which is what a test reads back to see that a
    // drag of the drawn strip moved the cards rather than only the wheel
    int getScrollY() const { return scrollY; }

    /** A macro's shape as the tab draws it: the dropdown's own spelling, one of
        the six, which is what a test reads back rather than a second copy of the
        painting and what the model's own number is read against. */
    juce::String vibShapeLabel (int type) const;

    /** A macro's amplitude as the tab spells it: the share of full pitchbend the
        knob is set to, two decimals wide, which is what the model holds as
        hundredths and the wheel reads as a share of its own half-height. */
    juce::String vibAmplitudeLabel (int amplitude) const;

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
        std::unique_ptr<juce::ComboBox> shape;
        std::unique_ptr<juce::Slider> amp;
        std::unique_ptr<juce::Slider> cycle;
    };

    void rebuild();
    void syncValues();
    int contentHeight() const;
    int cardTop (int id) const; // screen y of a macro's card, scroll included
    void clampScroll();

    ReTrektorAudioProcessor& proc;
    rtr::Song& song;

    std::array<RowControls, kMaxVibMacros> controls;

    // the macro the last gesture picked out, which is what the remove button puts
    // away and what the arrows walk over when there is nothing else to move to
    int selectedId = 0;

    int scrollY = 0;

    // the cards that do not fit scroll out, so the strip on the right is what
    // shows where they are: the wheel moves it and so does a drag of its thumb,
    // which is asked for in one place because a drag has to answer on exactly
    // the bounds the panel draws
    int scrollbarX() const;
    int thumbHeight() const;
    int thumbTop() const;
    bool draggingThumb = false;
    void scrollToPointer (int y);

    int prevDefVibId (int id) const; // nearest macro above a card
    int nextDefVibId (int id) const; // nearest macro below one

    juce::TextButton addButton { "+ Add macro" };
    juce::TextButton removeButton { "- Remove selected" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VibMacrosPanel)
};

} // namespace rtr
