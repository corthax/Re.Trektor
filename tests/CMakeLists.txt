cmake_minimum_required(VERSION 3.22)

project(ReTtrektorSmoke VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(../JUCE JUCE)

juce_add_console_app(ReTtrektorSmoke PRODUCT_NAME "ReTtrektorSmoke")

target_sources(ReTtrektorSmoke PRIVATE SmokeTest.cpp)

target_compile_definitions(ReTtrektorSmoke PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_PLUGINHOST_VST3=1
)

target_link_libraries(ReTtrektorSmoke PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_basics
    juce::juce_audio_utils
    juce::juce_gui_basics
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

juce_add_console_app(ReTtrektorCoreTest PRODUCT_NAME "ReTtrektorCoreTest")

target_sources(ReTtrektorCoreTest PRIVATE
    CoreTest.cpp
    ../Source/Model.cpp
    ../Source/PlaybackEngine.cpp
)

target_include_directories(ReTtrektorCoreTest PRIVATE ../Source)

target_compile_definitions(ReTtrektorCoreTest PRIVATE
    JUCE_USE_CURL=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
)

target_link_libraries(ReTtrektorCoreTest PRIVATE
    juce::juce_audio_basics
    juce::juce_data_structures
    juce::juce_core
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

juce_add_gui_app(ReTtrektorEditorShot PRODUCT_NAME "ReTtrektorEditorShot")

target_sources(ReTtrektorEditorShot PRIVATE
    EditorShot.cpp
    ../Source/PluginProcessor.cpp
    ../Source/PluginEditor.cpp
    ../Source/Model.cpp
    ../Source/PlaybackEngine.cpp
    ../Source/ui/TimelineComponent.cpp
    ../Source/ui/TrackerComponent.cpp
    ../Source/ui/PoolComponent.cpp
    ../Source/ui/ToolsPanel.cpp
    ../Source/ui/SettingsPanel.cpp
    ../Source/ui/MacrosPanel.cpp
)

target_include_directories(ReTtrektorEditorShot PRIVATE ../Source)

target_compile_definitions(ReTtrektorEditorShot PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(ReTtrektorEditorShot PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_extra
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
