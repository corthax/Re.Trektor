cmake_minimum_required(VERSION 3.22)

project(ReTrektor VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(JUCE)

# Default build: a pure MIDI effect with no audio buses. Some hosts hide
# plug-ins without audio pins, so this option builds the same plug-in as an
# instrument with a silent stereo output instead.
option(RETREKTOR_INSTRUMENT_FALLBACK
       "Build as an instrument with a silent stereo output (host compatibility fallback)" OFF)

set(RTR_PLUGIN_KIND IS_MIDI_EFFECT TRUE)
set(RTR_VST3_CATEGORIES "Fx")

# JUCE writes the VST3 bundle's moduleinfo.json by building a helper for the
# target and running it, which only works when the target is the machine doing
# the building. The Windows cross build (see the Windows build note in
# README.md) asks for the manifest in its own build recipe instead, so this is
# switched off when the two platforms differ: the manifest the Linux-tree
# helper then writes is put into the Windows bundle by the build recipe.
if (CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME)
    set(RTR_VST3_AUTO_MANIFEST TRUE)
else()
    message("Cross-building: the VST3 moduleinfo.json is written by the build recipe rather than by a helper the build runs")
    set(RTR_VST3_AUTO_MANIFEST FALSE)
endif()

if (RETREKTOR_INSTRUMENT_FALLBACK)
    set(RTR_PLUGIN_KIND IS_SYNTH TRUE)
    set(RTR_VST3_CATEGORIES "Instrument")
endif()

juce_add_plugin(ReTrektor
    COMPANY_NAME "Corthax"
    BUNDLE_ID "com.corthax.retrektor"
    PLUGIN_MANUFACTURER_CODE Cort
    PLUGIN_CODE Rtrk
    FORMATS VST3
    PRODUCT_NAME "Re.Trektor"
    ${RTR_PLUGIN_KIND}
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_AUTO_MANIFEST ${RTR_VST3_AUTO_MANIFEST}
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "${RTR_VST3_CATEGORIES}"
)

if (RETREKTOR_INSTRUMENT_FALLBACK)
    target_compile_definitions(ReTrektor PUBLIC RETREKTOR_INSTRUMENT_FALLBACK=1)
endif()

juce_generate_juce_header(ReTrektor)

# Source/Portability.cpp redefines the libm float routines whose newest symbol
# versions older hosts lack (see README.md). Its aliases are written in
# assembly, which the LTO plugin resolves before it has seen the functions they
# name, so that one file is compiled without LTO.
set_source_files_properties(Source/Portability.cpp PROPERTIES
    COMPILE_FLAGS "-fno-lto")

target_sources(ReTrektor PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Model.h
    Source/Model.cpp
    Source/PlaybackEngine.h
    Source/PlaybackEngine.cpp
    Source/Portability.cpp
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
    Source/ui/VibMacrosPanel.cpp
    Source/ui/VibMacrosPanel.h
    Source/ui/TremoloMacrosPanel.cpp
    Source/ui/TremoloMacrosPanel.h
)

target_compile_definitions(ReTrektor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=0
)

target_link_libraries(ReTrektor PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_extra
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
