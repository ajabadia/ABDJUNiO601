cmake_minimum_required(VERSION 3.22)

project(ABDSimpleJuno106 VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED JUCE_PATH)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
        set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
    elseif(DEFINED ENV{JUCE_PATH})
        set(JUCE_PATH "$ENV{JUCE_PATH}")
    else()
        set(JUCE_PATH "C:/JUCE" CACHE PATH "Path to JUCE")
    endif()
endif()

message(STATUS "Using JUCE from: ${JUCE_PATH}")
if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at ${JUCE_PATH}. Please set JUCE_PATH to the JUCE directory.")
endif()

add_subdirectory(${JUCE_PATH} _juce)

option(BUILD_HEADLESS "Build headless version (no GUI)" OFF)

if(BUILD_HEADLESS)
    add_compile_definitions(JUCE_HEADLESS_PLUGIN=1)
    set(PLUGIN_FORMATS Standalone)
else()
    set(PLUGIN_FORMATS Standalone VST3 AU)
endif()

juce_add_plugin(ABDSimpleJuno106
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    NEEDS_DIRECT2D TRUE
    PLUGIN_MANUFACTURER_CODE Abda
    PLUGIN_CODE J106
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "ABDSimpleJuno106"
    NEEDS_WEBVIEW2 TRUE
)

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
    Source/Core/ServiceModeManager.cpp
    Source/Core/CalibrationManager.cpp

    Source/UI/WebView/WebViewEditor.h
    Source/UI/WebView/WebViewEditor.cpp

    Source/Synth/JunoADSR.h
    Source/Synth/JunoADSR.cpp
    Source/Synth/JunoDCO.h
    Source/Synth/JunoDCO.cpp
    Source/Synth/JunoLFO.h
    Source/Synth/JunoLFO.cpp
    Source/Synth/Voice.h
    Source/Synth/Voice.cpp
    Source/Synth/JunoVCF.h
    Source/Synth/JunoVCF.cpp
    Source/Synth/ChorusBBD.h
    Source/Synth/ChorusBBD.cpp
)

if(NOT BUILD_HEADLESS)
    target_sources(ABDSimpleJuno106 PRIVATE
        Source/Core/PluginEditor.h
        Source/Core/PluginEditor.cpp
        
        Source/UI/JunoBender.h
        Source/UI/JunoBender.cpp
        Source/UI/JunoUIHelpers.h
        Source/UI/PresetBrowser.h
        Source/UI/PresetBrowser.cpp
        Source/UI/ParameterDisplay.h
        
        Source/UI/Sections/JunoSysExDisplay.h
        Source/UI/Sections/JunoSysExDisplay.cpp

        Source/UI/Sections/JunoChorusSection.h
        Source/UI/Sections/JunoChorusSection.h
        Source/UI/Sections/JunoDCOSection.h
        Source/UI/Sections/JunoENVSection.h
        Source/UI/Sections/JunoHPFSection.h
        Source/UI/Sections/JunoLFOSection.h
        Source/UI/Sections/JunoVCASection.h
        Source/UI/Sections/JunoVCFSection.h
        Source/UI/Sections/JunoPerformanceSection.h
    )
endif()

# [RESTORE] WebUI Binary Data
juce_add_binary_data(ABDSimpleJuno106_WebUI SOURCES
    Source/UI/WebUI/index.html
    Source/UI/WebUI/script.js
    Source/UI/WebUI/service.js
    Source/UI/WebUI/style.css
    Source/UI/WebUI/service.css
    Source/UI/WebUI/fondo.png
    Source/UI/WebUI/fondo_sy.png
    Source/UI/WebUI/knob.png
    Source/UI/WebUI/slider_cap.png
    Source/UI/WebUI/slider_slot.png
    Source/UI/WebUI/led_off.png
    Source/UI/WebUI/led_on.png
    Source/UI/WebUI/switch_up.png
    Source/UI/WebUI/switch_down.png
    Source/UI/WebUI/SevenSegment.ttf
    Source/UI/WebUI/microgrammanormal.ttf
    Source/UI/WebUI/button_blue.png
    Source/UI/WebUI/button_blue_off.png
    Source/UI/WebUI/button_blue_on.png
    Source/UI/WebUI/button_grey_off.png
    Source/UI/WebUI/button_grey_on.png
    Source/UI/WebUI/button_orange.png
    Source/UI/WebUI/button_orange_off.png
    Source/UI/WebUI/button_orange_on.png
    Source/UI/WebUI/button_red_off.png
    Source/UI/WebUI/button_red_on.png
    Source/UI/WebUI/button_white.png
    Source/UI/WebUI/button_white_off.png
    Source/UI/WebUI/button_white_on.png
    
    Source/UI/WebUI/css/about.css
    Source/UI/WebUI/css/base.css
    Source/UI/WebUI/css/controls.css
    Source/UI/WebUI/css/engine.css
    Source/UI/WebUI/css/header.css
    Source/UI/WebUI/css/keyboard.css
    Source/UI/WebUI/css/performance.css
    Source/UI/WebUI/css/sidebar.css
    Source/UI/WebUI/css/settings.css
    Source/UI/WebUI/css/vars.css
)
message(STATUS "Registering 37 WebUI assets for BinaryData from Source/UI/WebUI/")


target_include_directories(ABDSimpleJuno106 PRIVATE 
    Source
    Source/Core
    Source/UI
    Source/UI/WebView
)

juce_generate_juce_header(ABDSimpleJuno106)

target_compile_definitions(ABDSimpleJuno106
    PUBLIC
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
        JUCE_DIRECT2D=1
)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(JUCE_LINUX_DEPS REQUIRED freetype2 alsa x11 xext xinerama webkit2gtk-4.0 gtk+-3.0)
    target_link_libraries(ABDSimpleJuno106 PRIVATE ${JUCE_LINUX_DEPS_LIBRARIES})
    target_include_directories(ABDSimpleJuno106 PRIVATE ${JUCE_LINUX_DEPS_INCLUDE_DIRS})
endif()

target_link_libraries(ABDSimpleJuno106
    PRIVATE
        ABDSimpleJuno106_WebUI
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

# --- Unit Tests Target ---
juce_add_console_app(JunoUnitTests
    PRODUCT_NAME "JunoUnitTests"
)

target_sources(JunoUnitTests PRIVATE
    Source/Core/JunoUnitTests.cpp
    Source/Synth/JunoDCO.cpp
    Source/Synth/JunoADSR.cpp
    Source/Synth/JunoVCF.cpp
    Source/Synth/ChorusBBD.cpp
    Source/Synth/Voice.cpp
    Source/Core/JunoSysExEngine.cpp
    Source/Core/ServiceModeManager.cpp
    Source/Core/CalibrationManager.cpp
)

target_link_libraries(JunoUnitTests PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

target_include_directories(JunoUnitTests PRIVATE Source)
juce_generate_juce_header(JunoUnitTests)
