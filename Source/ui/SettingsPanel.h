#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace rtr
{

class SettingsPanel final : public juce::Component
{
public:
    explicit SettingsPanel (ReTrektorAudioProcessor& processor);
    ~SettingsPanel() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

    void refresh();

    std::function<void()> onSettingsChanged;

private:
    struct Row
    {
        juce::String label;
        juce::String hint;
        juce::ToggleButton* toggle = nullptr;
        juce::Slider* slider = nullptr;
    };

    juce::ToggleButton* addToggle (const juce::String& label, const juce::String& hint, bool value,
                                  std::function<void (bool)> onChange);
    juce::Slider* addSlider (const juce::String& label, const juce::String& hint, double min, double max,
                             double interval, double value, std::function<void (double)> onChange);

    int rowH() const;

    ReTrektorAudioProcessor& proc;
    rtr::Song& song;

    std::vector<Row> rows;
    std::vector<std::unique_ptr<juce::Component>> owned;

    juce::ToggleButton* hexToggle = nullptr;
    juce::ToggleButton* midRowToggle = nullptr;
    juce::Slider* subticksSlider = nullptr;
    juce::Slider* rowsPerBeatSlider = nullptr;
    juce::Slider* defaultRowsSlider = nullptr;
    juce::ToggleButton* pasteUniqueToggle = nullptr;
    juce::ToggleButton* snapToggle = nullptr;
    juce::ToggleButton* adaptiveToggle = nullptr;
    juce::Slider* fixedGridSlider = nullptr;
    juce::Slider* octaveSlider = nullptr;
    juce::Slider* stepSkipSlider = nullptr;
    juce::Slider* volumeCcSlider = nullptr;
    juce::Slider* panCcSlider = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsPanel)
};

} // namespace rtr
