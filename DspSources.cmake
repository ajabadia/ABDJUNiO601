# ============================================================================
# ABDJUNiO601 - Listas compartidas de fuentes (single-source, anti-drift)
#
# Consumidas por DOS builds:
#   - CMakeLists.txt raiz (plugin nativo ABDSimpleJuno106)
#   - wasm/CMakeLists.txt (build WebAssembly con modulos vendados)
#
# HISTORIA: dos listas a mano derivaron 46 ficheros (JunoSysExEngine, gestores,
# importers... fuera del web). Ahora ambas viven aqui.
#
# JUNO_DSP_SOURCES      : motor/synth/importers del plugin nativo (sin UI: los
#                         10 .cpp de Source/UI/ viven en el bloque
#                         if(NOT BUILD_HEADLESS) del raiz, con su guarda).
# JUNO_DSP_SOURCES_WASM : subconjunto headless para Emscripten (3 modulos
#                         JUCE vendados: core, audio_basics, dsp).
#
# DIFERENCIA WASM (documentada, NO drift):
#   - PluginProcessor/Editor, WebView/Bridge*, UI: frontend nativo.
#   - PresetManager.cpp: necesita juce_data_structures (ValueTree) y APVTS,
#     no vendados en el WASM; la web gestiona presets via WasmBridge.
#   - Importers/*.cpp, JunoTapeDecoder, ServiceModeManager, CalibrationSettings
#     (el WASM usa CalibrationSettingsStub.cpp del shim): utilidades nativas.
#   - JunoTests/UnitTests: suites nativas.
# Regla: si anades un modulo DSP al nativo, debe ir tambien en WASM (o
# justificarlo en este bloque).
#
# NOTA: CMAKE_CURRENT_LIST_DIR apunta al directorio que contiene este fichero
# (la raiz del proyecto), sea quien sea el consumidor.
# ============================================================================

set(JUNO_DSP_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/CalibrationSettings.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/Importers/JunoCsvImporter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/Importers/JunoFormatConverter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/Importers/JunoSysexImporter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/Importers/JunoTapeImporter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/Importers/TalImporter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/JunoEngine.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/JunoSysExEngine.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/JunoTests.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/JunoVoiceManager.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/PerformanceState.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/PluginEditor.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/PluginProcessor.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/PresetManager.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/ProcessorMisc.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/ProcessorPresets.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/ProcessorState.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/ProcessorSysEx.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/ServiceModeManager.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Synth/ChorusBBD.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Synth/JunoADSR.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Synth/JunoDCO.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Synth/JunoLFO.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Synth/JunoTapeEcho.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Synth/JunoVCF.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Synth/JunoVoice.cpp"
)

set(JUNO_DSP_SOURCES_WASM
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/JunoEngine.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/JunoVoiceManager.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Core/PerformanceState.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Synth/ChorusBBD.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Synth/JunoADSR.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Synth/JunoDCO.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Synth/JunoLFO.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Synth/JunoTapeEcho.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Synth/JunoVCF.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Synth/JunoVoice.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Source/Wasm/WasmBridge.cpp"
)
