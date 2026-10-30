cmake_minimum_required(VERSION 3.22)

project(ReTrektorSmoke VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(../JUCE JUCE)

juce_add_console_app(ReTrektorSmoke PRODUCT_NAME "ReTrektorSmoke")

target_sources(ReTrektorSmoke PRIVATE SmokeTest.cpp)

target_compile_definitions(ReTrektorSmoke PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_PLUGINHOST_VST3=1
)

target_link_libraries(ReTrektorSmoke PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_basics
    juce::juce_audio_utils
    juce::juce_gui_basics
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

juce_add_console_app(ReTrektorCoreTest PRODUCT_NAME "ReTrektorCoreTest")

target_sources(ReTrektorCoreTest PRIVATE
    CoreTest.cpp
    ../Source/Model.cpp
    ../Source/PlaybackEngine.cpp
)

target_include_directories(ReTrektorCoreTest PRIVATE ../Source)

target_compile_definitions(ReTrektorCoreTest PRIVATE
    JUCE_USE_CURL=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
)

target_link_libraries(ReTrektorCoreTest PRIVATE
    juce::juce_audio_basics
    juce::juce_data_structures
    juce::juce_core
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

juce_add_gui_app(ReTrektorEditorShot PRODUCT_NAME "ReTrektorEditorShot")

target_sources(ReTrektorEditorShot PRIVATE
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

target_include_directories(ReTrektorEditorShot PRIVATE ../Source)

target_compile_definitions(ReTrektorEditorShot PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(ReTrektorEditorShot PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_extra
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

juce_add_gui_app(ReTrektorUiTest PRODUCT_NAME "ReTrektorUiTest")

target_sources(ReTrektorUiTest PRIVATE
    UiTest.cpp
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

target_include_directories(ReTrektorUiTest PRIVATE ../Source)

target_compile_definitions(ReTrektorUiTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(ReTrektorUiTest PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_extra
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
