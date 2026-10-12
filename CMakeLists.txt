cmake_minimum_required(VERSION 3.22)
project(SuburbanChannel VERSION 0.1.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.9
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(PLUGIN_FORMATS VST3 AU Standalone)
else()
    set(PLUGIN_FORMATS VST3 Standalone)
endif()

juce_add_plugin(SuburbanChannel
    COMPANY_NAME "YourName"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Ynam
    PLUGIN_CODE Sbch
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Suburban Channel")

target_sources(SuburbanChannel PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(SuburbanChannel PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SuburbanChannel
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Tests (no JUCE needed):  cmake -B build -DBGTR_BUILD_TESTS=ON && cmake --build build --target bgtr_dsp_test bgtr_processor_test
#                          ./build/bgtr_dsp_test && ./build/bgtr_processor_test
option(BGTR_BUILD_TESTS "Build the standalone DSP and processor tests" OFF)
if(BGTR_BUILD_TESTS)
    add_executable(bgtr_dsp_test tests/dsp_test.cpp)
    target_compile_features(bgtr_dsp_test PRIVATE cxx_std_17)

    # Runs the real PluginProcessor.cpp against a small functional stand-in for JUCE (tests/stub_fn)
    add_executable(bgtr_processor_test tests/processor_test.cpp Source/PluginProcessor.cpp)
    target_compile_features(bgtr_processor_test PRIVATE cxx_std_17)
    target_include_directories(bgtr_processor_test PRIVATE tests/stub_fn Source)
    target_compile_definitions(bgtr_processor_test PRIVATE SC_TEST_NO_EDITOR)
endif()
