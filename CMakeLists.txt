cmake_minimum_required(VERSION 3.22)

project(ReTtrektor VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(JUCE)

juce_add_plugin(ReTtrektor
    COMPANY_NAME "Corthax"
    BUNDLE_ID "com.corthax.retrektor"
    PLUGIN_MANUFACTURER_CODE Cort
    PLUGIN_CODE Rtrk
    FORMATS VST3
    PRODUCT_NAME "Re.Trektor"
    IS_MIDI_EFFECT TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Fx"
)

juce_generate_juce_header(ReTtrektor)

target_sources(ReTtrektor PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Model.h
    Source/Model.cpp
    Source/PlaybackEngine.h
    Source/PlaybackEngine.cpp
    Source/ui/Theme.h
    Source/ui/TimelineComponent.cpp
    Source/ui/TimelineComponent.h
    Source/ui/TrackerComponent.cpp
    Source/ui/TrackerComponent.h
    Source/ui/PoolComponent.cpp
    Source/ui/PoolComponent.h
    Source/ui/ToolsPanel.cpp
    Source/ui/ToolsPanel.h
    Source/ui/SettingsPanel.cpp
    Source/ui/SettingsPanel.h
    Source/ui/MacrosPanel.cpp
    Source/ui/MacrosPanel.h
)

target_compile_definitions(ReTtrektor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=0
)

target_link_libraries(ReTtrektor PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_extra
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
