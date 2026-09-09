cmake_minimum_required(VERSION 3.22)

project(PitchShiftVST3 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Incluir JUCE con clonado superficial rapido
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.9
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(PitchShiftVST3
    COMPANY_NAME "Xaina DSP"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Xain
    PLUGIN_CODE Pshf
    FORMATS VST3 Standalone
    PRODUCT_NAME "PitchShift Guitar & Bass Transposer"
)

target_sources(PitchShiftVST3
    PRIVATE
        Source/PitchShiftAudioProcessor.h
        Source/PitchShiftAudioProcessor.cpp
        Source/PitchShiftAudioProcessorEditor.h
        Source/PitchShiftAudioProcessorEditor.cpp
)

target_compile_definitions(PitchShiftVST3
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(PitchShiftVST3
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
