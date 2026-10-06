cmake_minimum_required(VERSION 3.22)

project(OpalVST3 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.3
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Opal
    COMPANY_NAME "SpiralTech"
    BUNDLE_ID "com.spiraltech.opal"
    PLUGIN_MANUFACTURER_CODE SpET
    PLUGIN_CODE Opal
    FORMATS VST3 Standalone
    PRODUCT_NAME "OPAL"
    DESCRIPTION "Input-reactive harmonic resonance field processor"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(Opal)

target_sources(Opal
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/DSP/OpalEngine.h
        Source/UI/OpalLookAndFeel.h
)

target_compile_definitions(Opal
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Opal
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)


juce_add_console_app(OpalDSPTests
    PRODUCT_NAME "OPAL DSP Tests"
)

juce_generate_juce_header(OpalDSPTests)

target_sources(OpalDSPTests
    PRIVATE
        Tests/OpalEngineTests.cpp
        Source/DSP/OpalEngine.h
)

target_compile_definitions(OpalDSPTests
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(OpalDSPTests
    PRIVATE
        juce::juce_audio_basics
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
