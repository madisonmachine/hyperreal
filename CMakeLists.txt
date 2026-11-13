cmake_minimum_required(VERSION 3.22)
project(HYPERREAL VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG d60b7a78030661574a615b586d9325ba7a7fa7dd
    GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(HYPERREAL
    COMPANY_NAME "HYPERREAL"
    BUNDLE_ID "com.hyperreal.audio"
    PLUGIN_MANUFACTURER_CODE Hypr
    PLUGIN_CODE Hr01
    FORMATS VST3 Standalone
    PRODUCT_NAME "HYPERREAL"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_WEBVIEW2 TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument
)

target_sources(HYPERREAL PRIVATE
    apps/plugin/PluginProcessor.cpp
    apps/plugin/PluginProcessor.h
)

target_compile_definitions(HYPERREAL PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2=1
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(HYPERREAL PRIVATE
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
