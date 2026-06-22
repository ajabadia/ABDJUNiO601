cmake_minimum_required(VERSION 3.22)

project(ABDSimpleJuno106 VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Model Selection ---
# JUNO_TARGET_MODEL: 0=SuperSix(Hybrid), 1=Juno-106, 2=Juno-60, 3=Juno-6
if(NOT DEFINED JUNO_TARGET_MODEL)
    set(JUNO_TARGET_MODEL 0 CACHE STRING "Model target: 0=SuperSix, 1=J106, 2=J60, 3=J6")
endif()

if(JUNO_TARGET_MODEL EQUAL 1)
    set(PRODUCT_DISPLAY_NAME "ABD JUNiO 601")
    set(PRODUCT_PLUGIN_CODE "J106")
elseif(JUNO_TARGET_MODEL EQUAL 2)
    set(PRODUCT_DISPLAY_NAME "ABD JUNiO 06")
    set(PRODUCT_PLUGIN_CODE "J060")
elseif(JUNO_TARGET_MODEL EQUAL 3)
    set(PRODUCT_DISPLAY_NAME "ABD JUNiO SIX")
    set(PRODUCT_PLUGIN_CODE "J006")
else()
    set(PRODUCT_DISPLAY_NAME "ABD JUNiO Super SIX")
    set(PRODUCT_PLUGIN_CODE "JSSX")
endif()

message(STATUS "Building model: ${PRODUCT_DISPLAY_NAME} (JUNO_TARGET_MODEL=${JUNO_TARGET_MODEL})")

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

# JUNO_TARGET_MODEL remains directory-scope so both plugin and test targets
# can use it for JunoModelConfig.h product name resolution.
add_compile_definitions(JUNO_TARGET_MODEL=${JUNO_TARGET_MODEL})

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
    PRODUCT_NAME "${PRODUCT_DISPLAY_NAME}"
    NEEDS_WEBVIEW2 TRUE
)

target_sources(ABDSimpleJuno106 PRIVATE
    Source/Core/ABDSimpleJuno106AudioProcessor.h
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
    Source/Core/JunoProtocol.h
    Source/Core/JunoTests.h
    Source/Core/JunoTests.cpp
    Source/Core/PerformanceState.h
    Source/Core/PerformanceState.cpp
    Source/Core/ServiceModeManager.cpp
    Source/Core/CalibrationSettings.cpp
    Source/Core/ProcessorState.cpp
    Source/Core/ProcessorPresets.cpp
    Source/Core/ProcessorSysEx.cpp
    Source/Core/ProcessorMisc.cpp

    Source/Core/Importers/TalImporter.h
    Source/Core/Importers/TalImporter.cpp
    Source/Core/Importers/JunoTapeImporter.h
    Source/Core/Importers/JunoTapeImporter.cpp
    Source/Core/Importers/JunoSysexImporter.h
    Source/Core/Importers/JunoSysexImporter.cpp
    Source/Core/Importers/JunoFormatConverter.h
    Source/Core/Importers/JunoFormatConverter.cpp
    Source/Core/Importers/JunoCsvImporter.h
    Source/Core/Importers/JunoCsvImporter.cpp

    Source/Core/BaseClass/PresetManagerBase.h
    Source/Core/BaseClass/VoiceBase.h
    Source/Core/BaseClass/VoiceAllocator.h
    Source/Core/BaseClass/ADSRGeneric.h
    Source/Core/BaseClass/LFOGeneric.h

    Source/UI/WebView/WebViewEditor.h
    Source/UI/WebView/WebViewEditor.cpp
    Source/UI/WebView/BridgeActions.h
    Source/UI/WebView/BridgeActions.cpp
    Source/UI/WebView/BridgeImport.h
    Source/UI/WebView/BridgeImport.cpp
    Source/UI/WebView/BridgeService.h
    Source/UI/WebView/BridgeService.cpp
    Source/UI/WebView/BridgeMenu.h
    Source/UI/WebView/BridgeMenu.cpp

    Source/Synth/JunoADSR.h
    Source/Synth/JunoADSR.cpp
    Source/Synth/JunoDCO.h
    Source/Synth/JunoDCO.cpp
    Source/Synth/JunoLFO.h
    Source/Synth/JunoLFO.cpp
    Source/Synth/JunoVoice.h
    Source/Synth/JunoVoice.cpp
    Source/Synth/JunoVCF.h
    Source/Synth/JunoVCF.cpp
    Source/Synth/JunoTapeEcho.h
    Source/Synth/JunoTapeEcho.cpp
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
        Source/UI/PresetBrowserHeaderBar.h
        Source/UI/PresetBrowserHeaderBar.cpp
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
        Source/UI/Sections/JunoBankSection.cpp
    )
endif()

# [RESTORE] WebUI Binary Data
juce_add_binary_data(ABDSimpleJuno106_WebUI SOURCES
    Source/UI/WebUI/index.html
    Source/UI/WebUI/modals.js
    Source/UI/WebUI/bridge-core.js
    Source/UI/WebUI/ui-sliders.js
    Source/UI/WebUI/ui-modals.js
    Source/UI/WebUI/smart-import.js
    Source/UI/WebUI/ui-keyboard.js
    Source/UI/WebUI/theme-manager.js
    Source/UI/WebUI/service.js
    Source/UI/WebUI/browser.js
    Source/UI/WebUI/style.css
    Source/UI/WebUI/service.css
    Source/UI/WebUI/fondo.png
    Source/UI/WebUI/interior.jpg
    Source/UI/WebUI/interior_2.jpg
    Source/UI/WebUI/interior_3.jpg
    Source/UI/WebUI/fondo_sy.png
    Source/UI/WebUI/re201_big_knob.png
    Source/UI/WebUI/silver_re201_normal.png
    Source/UI/WebUI/switch_toggle.png
    Source/UI/WebUI/re201_peak_led.png
    Source/UI/WebUI/re201_vumeter_background.png
    Source/UI/WebUI/knob.png
    Source/UI/WebUI/wood_left.png
    Source/UI/WebUI/wood_right.png
    Source/UI/WebUI/wood_60_left.png
    Source/UI/WebUI/wood_60_right.png
    Source/UI/WebUI/wood_6_left.png
    Source/UI/WebUI/wood_6_right.png
    Source/UI/WebUI/metal_106_left.png
    Source/UI/WebUI/metal_106_right.png
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
    
    Source/UI/WebUI/assets/led/0.png
    Source/UI/WebUI/assets/led/1.png
    Source/UI/WebUI/assets/led/2.png
    Source/UI/WebUI/assets/led/3.png
    Source/UI/WebUI/assets/led/4.png
    Source/UI/WebUI/assets/led/5.png
    Source/UI/WebUI/assets/led/6.png
    Source/UI/WebUI/assets/led/7.png
    Source/UI/WebUI/assets/led/8.png
    Source/UI/WebUI/assets/led/9.png
    Source/UI/WebUI/assets/led/A.png
    Source/UI/WebUI/assets/led/B.png
    Source/UI/WebUI/assets/led/C.png
    Source/UI/WebUI/assets/led/D.png
    Source/UI/WebUI/assets/led/DASH.png
    Source/UI/WebUI/assets/led/E.png
    Source/UI/WebUI/assets/led/F.png
    Source/UI/WebUI/assets/led/G.png
    Source/UI/WebUI/assets/led/H.png
    Source/UI/WebUI/assets/led/I.png
    Source/UI/WebUI/assets/led/J.png
    Source/UI/WebUI/assets/led/K.png
    Source/UI/WebUI/assets/led/L.png
    Source/UI/WebUI/assets/led/M.png
    Source/UI/WebUI/assets/led/N.png
    Source/UI/WebUI/assets/led/O.png
    Source/UI/WebUI/assets/led/P.png
    Source/UI/WebUI/assets/led/Q.png
    Source/UI/WebUI/assets/led/R.png
    Source/UI/WebUI/assets/led/S.png
    Source/UI/WebUI/assets/led/SPACE.png
    Source/UI/WebUI/assets/led/T.png
    Source/UI/WebUI/assets/led/U.png
    Source/UI/WebUI/assets/led/V.png
    Source/UI/WebUI/assets/led/W.png
    Source/UI/WebUI/assets/led/X.png
    Source/UI/WebUI/assets/led/Y.png
    Source/UI/WebUI/assets/led/Z.png
    
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
message(STATUS "Registering 40 WebUI assets for BinaryData from Source/UI/WebUI/")


target_include_directories(ABDSimpleJuno106 PRIVATE 
    Source
    Source/Core
    Source/Core/BaseClass
    Source/UI
    Source/UI/WebView
    Source/Core/Importers
    "${CMAKE_CURRENT_BINARY_DIR}/ABDSimpleJuno106_artefacts/JuceLibraryCode"
)

juce_generate_juce_header(ABDSimpleJuno106)

# ── Copy kV4Hz.bin to the build output directory ────────────────────
# The runtime loader (DacHzTable.h) looks for this file next to the
# executable. If absent it falls back to the compile-time constexpr table.
add_custom_command(TARGET ABDSimpleJuno106 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/Synth/kV4Hz.bin"
        "$<TARGET_FILE_DIR:ABDSimpleJuno106>"
    COMMENT "Copying kV4Hz.bin to build output"
)

# Also copy for the test runner
if(TARGET JunoUnitTests)
    add_custom_command(TARGET JunoUnitTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Source/Synth/kV4Hz.bin"
            "$<TARGET_FILE_DIR:JunoUnitTests>"
        COMMENT "Copying kV4Hz.bin to test output"
    )
endif()

target_compile_definitions(ABDSimpleJuno106
    PUBLIC
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
        JUCE_DIRECT2D=1
)

# COMPILING_JUNO* defines are target-scoped to ABDSimpleJuno106 only.
# JunoUnitTests runs in Super Six (hybrid) mode — no fixed compile-time defines,
# so GET_MODEL_* macros use runtime values from SynthParams members.
if(JUNO_TARGET_MODEL EQUAL 1)
    target_compile_definitions(ABDSimpleJuno106 PRIVATE COMPILING_JUNO106)
elseif(JUNO_TARGET_MODEL EQUAL 2)
    target_compile_definitions(ABDSimpleJuno106 PRIVATE COMPILING_JUNO60)
elseif(JUNO_TARGET_MODEL EQUAL 3)
    target_compile_definitions(ABDSimpleJuno106 PRIVATE COMPILING_JUNO6)
endif()

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
juce_generate_juce_header(JunoUnitTests)

target_sources(JunoUnitTests PRIVATE
    Source/Core/JunoUnitTests.cpp
    Source/Core/Tests/DCOTests.cpp
    Source/Core/Tests/PresetBrowserTests.cpp
    Source/Core/Tests/ADSRTests.cpp
    Source/Core/Tests/VCFTests.cpp
    Source/Core/Tests/ChorusTests.cpp
    Source/Core/Tests/SysExTests.cpp
    Source/Core/Tests/TapeTests.cpp
    Source/Core/Tests/FormatConverterTests.cpp
    Source/Core/Tests/SmartTapeTests.cpp
    Source/Core/Tests/MemoryTests.cpp
    Source/Core/Tests/UnisonTests.cpp
    Source/Core/Tests/DCBCorrectorTests.cpp
    Source/Core/Tests/SmartImportHtmlTests.cpp
    Source/Core/Tests/ModelRoutingTests.cpp
    Source/Core/Tests/TapeEchoTests.cpp
    Source/Core/Tests/MidiLearnTests.cpp
    Source/Synth/JunoTapeEcho.cpp
    Source/Synth/JunoDCO.cpp
    Source/Synth/JunoADSR.cpp
    Source/Synth/JunoVCF.cpp
    Source/Synth/ChorusBBD.cpp
    Source/Synth/JunoLFO.cpp
    Source/Synth/JunoVoice.cpp
    Source/Core/JunoSysExEngine.cpp
    Source/Core/CalibrationSettings.cpp
    Source/Core/PresetManager.cpp
    Source/Core/JunoVoiceManager.cpp
    Source/Core/PerformanceState.cpp
    Source/Core/Importers/TalImporter.cpp
    Source/Core/Importers/JunoTapeImporter.cpp
    Source/Core/Importers/JunoSysexImporter.cpp
    Source/Core/Importers/JunoFormatConverter.cpp
    Source/Core/Importers/JunoCsvImporter.cpp
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

target_include_directories(JunoUnitTests PRIVATE
    Source
    Source/Core
    Source/Core/BaseClass
    Source/Core/Importers
    Source/Synth
    "${CMAKE_CURRENT_BINARY_DIR}/JunoUnitTests_artefacts/JuceLibraryCode"
)
juce_generate_juce_header(JunoUnitTests)
