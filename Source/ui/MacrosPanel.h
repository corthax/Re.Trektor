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

    void refresh();

    std::function<void()> onModelChanged;

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
