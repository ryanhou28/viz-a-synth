cmake_minimum_required(VERSION 3.18)

# Project definition
project(VizASynth VERSION 0.1.0)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Export compile commands for IDEs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Fetch JUCE from GitHub
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# Plugin configuration
juce_add_plugin(VizASynth
    COMPANY_NAME "VizASynth"
    PLUGIN_MANUFACTURER_CODE Vzas
    PLUGIN_CODE Vzs1
    FORMATS VST3 Standalone
    PRODUCT_NAME "Viz-A-Synth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES Instrument Synth
)

# Automatically find all source files
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

target_sources(VizASynth PRIVATE ${SOURCES})

# Include directories
target_include_directories(VizASynth
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/DSP
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Visualization
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Visualization/SignalFlow
        ${CMAKE_CURRENT_SOURCE_DIR}/src/UI
)

# Compile definitions
target_compile_definitions(VizASynth
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
)

# Link libraries
target_link_libraries(VizASynth
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_data_structures
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
