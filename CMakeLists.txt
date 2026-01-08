cmake_minimum_required(VERSION 3.22)

project(ABDSimpleJuno106 VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE path
# JUCE path
set(JUCE_PATH "C:/JUCE" CACHE PATH "Path to JUCE")
add_subdirectory(${JUCE_PATH} _juce)

# Options
option(BUILD_HEADLESS "Build headless version (no GUI)" OFF)

if(BUILD_HEADLESS)
    add_compile_definitions(JUCE_HEADLESS_PLUGIN=1)
    set(PLUGIN_FORMATS Standalone) # Headless usually Standalone (or VST3 with no editor)
else()
    set(PLUGIN_FORMATS Standalone VST3 AU)
endif()

juce_add_plugin(ABDSimpleJuno106
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    PLUGIN_MANUFACTURER_CODE Abda
    PLUGIN_CODE J106
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "ABDSimpleJuno106"
)

# Core Sources (Always included)
target_sources(ABDSimpleJuno106 PRIVATE
    Source/Core/PluginProcessor.h
    Source/Core/PluginProcessor.cpp
    Source/Core/PresetManager.h
    Source/Core/PresetManager.cpp
    Source/Core/SynthParams.h
    Source/Core/JunoVoiceManager.h
    Source/Core/JunoVoiceManager.cpp
    Source/Core/JunoTapeDecoder.h
    Source/Core/MidiLearnHandler.h
    Source/Core/JunoSysEx.h
    Source/Core/JunoSysExEngine.h
    Source/Core/JunoSysExEngine.cpp
    Source/Core/PerformanceState.h
    Source/Core/PerformanceState.cpp

    Source/Synth/JunoADSR.h
    Source/Synth/JunoADSR.cpp
    Source/Synth/JunoDCO.h
    Source/Synth/JunoDCO.cpp
    Source/Synth/JunoLFO.h
    Source/Synth/JunoLFO.cpp
    Source/Synth/Voice.h
    Source/Synth/Voice.cpp
)

# UI Sources (Only if NOT Headless)
if(NOT BUILD_HEADLESS)
    target_sources(ABDSimpleJuno106 PRIVATE
        Source/Core/PluginEditor.h
        Source/Core/PluginEditor.cpp
        
        Source/UI/JunoBender.h
        Source/UI/JunoBender.cpp
        Source/UI/JunoUIHelpers.h
        Source/UI/PresetBrowser.h
        Source/UI/PresetBrowser.cpp
        
        Source/UI/Sections/JunoChorusSection.h
        Source/UI/Sections/JunoControlSection.h
        Source/UI/Sections/JunoControlSection.cpp
        Source/UI/Sections/JunoDCOSection.h
        Source/UI/Sections/JunoENVSection.h
        Source/UI/Sections/JunoHPFSection.h
        Source/UI/Sections/JunoLFOSection.h
        Source/UI/Sections/JunoVCASection.h
        Source/UI/Sections/JunoVCFSection.h
        Source/UI/Sections/JunoPerformanceSection.h
        # Source/UI/Sections/JunoSidePanel.h # Removed
        # Source/UI/Sections/JunoSidePanel.cpp # Removed
    )
endif()

target_include_directories(ABDSimpleJuno106 PRIVATE Source)

juce_generate_juce_header(ABDSimpleJuno106)

target_compile_definitions(ABDSimpleJuno106
    PUBLIC
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# Platform Specific Configuration
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(JUCE_LINUX_DEPS REQUIRED freetype2 alsa x11 xext xinerama webkit2gtk-4.0 gtk+-3.0)
    target_link_libraries(ABDSimpleJuno106 PRIVATE ${JUCE_LINUX_DEPS_LIBRARIES})
    target_include_directories(ABDSimpleJuno106 PRIVATE ${JUCE_LINUX_DEPS_INCLUDE_DIRS})
endif()

target_link_libraries(ABDSimpleJuno106
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_audio_devices
        juce::juce_audio_basics
        juce::juce_gui_extra
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_events
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
