cmake_minimum_required(VERSION 3.24)
project(DAProductionTelescopeFX VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
# Fixed JUCE revision for reproducible builds. License: GPLv3/commercial from JUCE.
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.13
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(DA_FORMATS VST3)
if(APPLE)
    list(APPEND DA_FORMATS AU)
endif()

juce_add_plugin(DA_Telescope
    COMPANY_NAME "D.A Production"
    BUNDLE_ID "com.daproduction.telescopefx"
    PLUGIN_MANUFACTURER_CODE DApr
    PLUGIN_CODE TsFx
    FORMATS ${DA_FORMATS}
    PRODUCT_NAME "Telescope FX"
    DESCRIPTION "Original spatial echo, motion and reverb effect"
    VST3_CATEGORIES Fx Spatial Delay Reverb
    AU_MAIN_TYPE kAudioUnitType_Effect
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(DA_Telescope)

juce_add_binary_data(DA_TelescopeAssets SOURCES
    Assets/Runtime/gui_background.png
    Assets/Runtime/knob.png
    Assets/Runtime/galaxy.png
    Assets/Fonts/poppins_regular.ttf
)

target_sources(DA_Telescope PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/FactoryPresets.h
)

target_compile_definitions(DA_Telescope PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(DA_Telescope PRIVATE
    DA_TelescopeAssets
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
