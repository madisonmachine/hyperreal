cmake_minimum_required(VERSION 3.22)
project(HYPERREAL VERSION 0.3.0 LANGUAGES C CXX)

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

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packages/ui/dist/assets/app.js")
    message(FATAL_ERROR "Build the local UI first: npm ci && npm run build in packages/ui")
endif()

juce_add_binary_data(HYPERREAL_Assets SOURCES
    packages/ui/dist/index.html
    packages/ui/dist/assets/app.js
    packages/ui/dist/assets/style.css
)
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
    apps/plugin/PluginEditor.cpp
    apps/plugin/PluginEditor.h
    packages/engine/SampleEngine.cpp
    packages/engine/SampleEngine.h
)

target_compile_definitions(HYPERREAL PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(HYPERREAL PRIVATE
    HYPERREAL_Assets
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

enable_testing()
add_executable(hyperreal_engine_smoke
    tests/dsp/SampleEngineSmoke.cpp
    packages/engine/SampleEngine.cpp
)
target_compile_definitions(hyperreal_engine_smoke PRIVATE JUCE_USE_CURL=0)
target_link_libraries(hyperreal_engine_smoke PRIVATE
    juce::juce_audio_formats
    juce::juce_recommended_config_flags
)
add_test(NAME hyperreal_engine_smoke COMMAND hyperreal_engine_smoke)
add_executable(hyperreal_engine_benchmark
    tests/perf/EngineDenseTrigger.cpp
    packages/engine/SampleEngine.cpp
)
target_compile_definitions(hyperreal_engine_benchmark PRIVATE JUCE_USE_CURL=0)
target_link_libraries(hyperreal_engine_benchmark PRIVATE
    juce::juce_audio_formats
    juce::juce_recommended_config_flags
)
if(WIN32)
    target_link_libraries(hyperreal_engine_benchmark PRIVATE psapi)
endif()
add_executable(hyperreal_processor_state_smoke
    tests/state/ProcessorStateSmoke.cpp
)
target_link_libraries(hyperreal_processor_state_smoke PRIVATE HYPERREAL juce::juce_audio_utils juce::juce_gui_extra juce::juce_webview2)
add_test(NAME hyperreal_processor_state_smoke COMMAND hyperreal_processor_state_smoke)
