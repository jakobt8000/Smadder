cmake_minimum_required(VERSION 3.22)

project(Smadder VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Universal binary (Apple Silicon + Intel) on macOS
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

# JUCE: use a local checkout if JUCE_DIR is given, otherwise download it
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(SMADDER_FORMATS AU VST3)
else()
    set(SMADDER_FORMATS VST3)
endif()

juce_add_plugin(Smadder
    COMPANY_NAME "Jakob2ft"
    PLUGIN_MANUFACTURER_CODE Jk2f
    PLUGIN_CODE Smdr
    FORMATS ${SMADDER_FORMATS}
    PRODUCT_NAME "Smadder"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Distortion Delay
    AU_MAIN_TYPE kAudioUnitType_Effect
    BUNDLE_ID dk.jakob2ft.smadder)

target_sources(Smadder PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Smadder PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Smadder
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional: headless GUI snapshot tool used during development (cmake -DSMADDER_TESTS=ON)
if(SMADDER_TESTS)
    juce_add_console_app(SmadderSnapshot PRODUCT_NAME "SmadderSnapshot")
    target_sources(SmadderSnapshot PRIVATE tests/snapshot.cpp)
    target_compile_definitions(SmadderSnapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(SmadderSnapshot PRIVATE Smadder juce::juce_gui_extra)
endif()
