cmake_minimum_required(VERSION 3.22)
project(DM7Voice VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

# JUCE is pulled from GitHub. To use a local copy instead:
#   cmake -B build -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(DM7_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND DM7_FORMATS AU)
endif()

juce_add_plugin(DM7Voice
    COMPANY_NAME "MayWorks"
    COMPANY_WEBSITE "https://mayworks.com"
    BUNDLE_ID "com.mayworks.dm7voice"
    PLUGIN_MANUFACTURER_CODE Mywk
    PLUGIN_CODE Dm7v
    FORMATS ${DM7_FORMATS}
    PRODUCT_NAME "DM7 Voice"
    DESCRIPTION "SM7B-style dynamic broadcast mic tone"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx EQ
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(DM7Voice PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(DM7Voice PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(DM7Voice
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
