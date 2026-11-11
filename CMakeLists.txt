# SPDX-License-Identifier: AGPL-3.0-or-later

cmake_minimum_required(VERSION 3.22)

# Must be set before project(). Xcode 27 no longer supports targets below macOS 12.
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")

project(Gnomon VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(GNOMON_COPY_PLUGIN "Install the plugin into the system plug-in folders after building" ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.5
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Gnomon
    COMPANY_NAME             "Gnomon"
    BUNDLE_ID                com.gnomon.gnomon
    PLUGIN_MANUFACTURER_CODE Gnmn
    PLUGIN_CODE              Eucl
    PRODUCT_NAME             "Gnomon"
    DESCRIPTION              "Euclidean MIDI drum sequencer"

    # Ableton Live can't host third-party MIDI effects, so this presents itself as an
    # instrument that outputs MIDI (and silent audio). Route its MIDI to another track.
    IS_SYNTH          TRUE
    NEEDS_MIDI_INPUT  TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT    FALSE

    FORMATS VST3
    COPY_PLUGIN_AFTER_BUILD ${GNOMON_COPY_PLUGIN})

target_sources(Gnomon
    PRIVATE
        Source/PluginEditor.cpp
        Source/PluginProcessor.cpp)

target_include_directories(Gnomon PRIVATE Source)

target_compile_definitions(Gnomon
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Gnomon
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
