#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/TimelineComponent.h"
#include "ui/TrackerComponent.h"
#include "ui/PoolComponent.h"
#include "ui/ToolsPanel.h"
#include "ui/SettingsPanel.h"
#include "ui/MacrosPanel.h"
#include "ui/VibMacrosPanel.h"
#include "ui/TremoloMacrosPanel.h"

//==============================================================================
class ReTrektorAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             public juce::DragAndDropContainer,
                                             private juce::Timer
{
public:
    explicit ReTrektorAudioProcessorEditor (ReTrektorAudioProcessor&);
    ~ReTrektorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    void refreshAll();
    void commitEdit(); // the funnel a model change ends in: record, then refresh
    void selectPattern (int patternId);
    bool performUndo();
    bool performRedo();
    void followHostTransport();

private:
    // the macro panels sit between the tracker and the Settings tab, in the order
    // their own tab buttons are laid out in: the arpeggiator's, the vibrato's and
    // the tremolo's, the three letters a row can call
    enum class Tab { tracker = 0, macros, vibs, trems, settings };

    void timerCallback() override;
    void setTab (Tab tab);
    void doSlice();
    void syncBottomBar();
    void syncGridControls();
    void applySettingsToModel();
    void updatePositionLabel();

    ReTrektorAudioProcessor& proc;
    rtr::Song& song;
    rtr::theme::LookAndFeel lookAndFeel;

    // top bar
    juce::TextButton trackerTab { "TRACKER" };
    juce::TextButton macrosTab { "ARP" };
    juce::TextButton vibTab { "VIB" };
    juce::TextButton trmTab { "TRM" };
    juce::TextButton settingsTab { "SETTINGS" };
    juce::ToggleButton snapButton { "SNAP" };
    juce::ComboBox gridBox;
    juce::TextButton zoomOutButton { "-" };
    juce::TextButton zoomInButton { "+" };
    juce::Label positionLabel;

    // panels
    rtr::TimelineComponent timeline;
    rtr::TrackerComponent tracker;
    rtr::PoolComponent pool;
    rtr::ToolsPanel tools;
    rtr::SettingsPanel settings;
    rtr::MacrosPanel macros;
    rtr::VibMacrosPanel vibs;
    rtr::TremoloMacrosPanel trems;

    // bottom bar
    juce::Label octaveLabel, stepLabel, channelsLabel;
    juce::Slider octaveSlider;
    juce::Slider stepSlider;
    juce::Slider channelsSlider;
    juce::TextEditor nameEditor;
    juce::Label gridInfoLabel;

    Tab currentTab = Tab::tracker;
    bool syncingUi = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReTrektorAudioProcessorEditor)
};
