cmake_minimum_required(VERSION 3.22)

# VocalTune: toy box voice lab. Voice changer + talk box + tune, as VST3 / AU.
project(VOCALTUNE VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE lives next to this file (CI clones it; see .github/workflows/build.yml), or pass -DJUCE_DIR=...
set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE" CACHE PATH "Path to the JUCE source tree")
add_subdirectory("${JUCE_DIR}" JUCE)

if(APPLE)
    set(VT_FORMATS VST3 AU Standalone)
else()
    set(VT_FORMATS VST3)   # Linux builds only exist to compile-check and test the shared code
endif()

juce_add_plugin(VocalTune
    COMPANY_NAME "Bass Bible"
    COMPANY_WEBSITE ""
    BUNDLE_ID "com.bassbible.vocaltune"
    PLUGIN_MANUFACTURER_CODE Bbtl
    PLUGIN_CODE VcTn
    FORMATS ${VT_FORMATS}
    PRODUCT_NAME "VocalTune"
    DESCRIPTION "Toy box voice lab: voice changer, talk box and tune"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/brand/icon-1024.png"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Pitch Shift
    AU_MAIN_TYPE kAudioUnitType_Effect
    HARDENED_RUNTIME_ENABLED FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "VocalTune's standalone app listens to your microphone so you can talk through it.")

juce_add_binary_data(VocalTuneResources SOURCES
    Resources/fonts/Fredoka-SemiBold.ttf
    Resources/fonts/LuckiestGuy-Regular.ttf)

set(VT_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Presets.cpp
    Source/dsp/SpectralEngine.cpp)

target_sources(VocalTune PRIVATE ${VT_SOURCES})

set(VT_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
set(VT_LINUX_DEFS
    JUCE_USE_XINPUT=0 JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0
    JUCE_USE_XRENDER=0 JUCE_USE_XSHM=0 JUCE_ALSA=0 JUCE_JACK=0)

target_compile_definitions(VocalTune PUBLIC ${VT_DEFS})
if(NOT APPLE)
    target_compile_definitions(VocalTune PUBLIC ${VT_LINUX_DEFS})
endif()

target_link_libraries(VocalTune
    PRIVATE
        VocalTuneResources
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- tests + offline renderer (cmake -DVT_BUILD_TESTS=ON)
option(VT_BUILD_TESTS "Build the tests" OFF)
if(VT_BUILD_TESTS)
    juce_add_console_app(VocalTuneTests PRODUCT_NAME "VocalTuneTests")
    target_sources(VocalTuneTests PRIVATE tests/PluginTests.cpp ${VT_SOURCES})
    target_include_directories(VocalTuneTests PRIVATE Source)
    target_compile_definitions(VocalTuneTests PRIVATE ${VT_DEFS}
        JUCE_MODAL_LOOPS_PERMITTED=1
        JucePlugin_Name="VocalTune" JucePlugin_VersionString="1.0.0")
    if(NOT APPLE)
        target_compile_definitions(VocalTuneTests PRIVATE ${VT_LINUX_DEFS})
    endif()
    target_link_libraries(VocalTuneTests PRIVATE
        VocalTuneResources juce::juce_audio_processors juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
