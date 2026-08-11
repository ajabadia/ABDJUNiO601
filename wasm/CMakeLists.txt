cmake_minimum_required(VERSION 3.20)
project(abdjunio601_wasm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SRC_ROOT ${CMAKE_SOURCE_DIR}/../Source)
set(SHIM_ROOT ${CMAKE_SOURCE_DIR}/juce_shim)

set(JUCE_CORE_SRC
    ${CMAKE_SOURCE_DIR}/juce_core/juce_core.cpp
)

set(JUCE_DSP_SRC
    ${CMAKE_SOURCE_DIR}/juce_dsp/juce_dsp.cpp
)

set(JUCE_AUDIO_BASICS_SRC
    ${CMAKE_SOURCE_DIR}/juce_audio_basics/juce_audio_basics.cpp
)

set(DSP_SRC
    ${SRC_ROOT}/Core/JunoEngine.cpp
    ${SRC_ROOT}/Core/JunoVoiceManager.cpp
    ${SRC_ROOT}/Core/PerformanceState.cpp
    ${SRC_ROOT}/Synth/JunoVoice.cpp
    ${SRC_ROOT}/Synth/JunoDCO.cpp
    ${SRC_ROOT}/Synth/JunoVCF.cpp
    ${SRC_ROOT}/Synth/JunoADSR.cpp
    ${SRC_ROOT}/Synth/JunoLFO.cpp
    ${SRC_ROOT}/Synth/ChorusBBD.cpp
    ${SRC_ROOT}/Synth/JunoTapeEcho.cpp
    ${SRC_ROOT}/Wasm/WasmBridge.cpp
)

set(STUB_SRC
    ${SHIM_ROOT}/CalibrationSettingsStub.cpp
    ${SHIM_ROOT}/wasm_link_stubs.cpp
)

add_executable(abdjunio601_wasm
    ${DSP_SRC}
    ${STUB_SRC}
    ${JUCE_CORE_SRC}
    ${JUCE_DSP_SRC}
    ${JUCE_AUDIO_BASICS_SRC}
)

target_include_directories(abdjunio601_wasm PRIVATE
    ${SRC_ROOT}
    ${SRC_ROOT}/Core
    ${SRC_ROOT}/Synth
    ${SRC_ROOT}/Wasm
    ${SHIM_ROOT}
    ${CMAKE_SOURCE_DIR}/juce_core
    ${CMAKE_SOURCE_DIR}/juce_dsp
    ${CMAKE_SOURCE_DIR}/juce_audio_basics
    C:/JUCE/modules
)

target_compile_definitions(abdjunio601_wasm PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUNO_TARGET_MODEL=0
    JUNO_IS_WASM=1
    NDEBUG
)

target_compile_options(abdjunio601_wasm PRIVATE
    "-include" "wasm_compat.h"
    -O3
    -fwasm-exceptions
    -msimd128
)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s INITIAL_MEMORY=134217728 -s MODULARIZE=1 -s EXPORT_ES6=1 -s EXPORT_NAME=createABDJUNiO601 -s \"EXPORTED_FUNCTIONS=['_wasm_init_engine','_wasm_process_audio','_wasm_set_parameter','_wasm_set_model','_wasm_note_on','_wasm_note_off','_wasm_panic','_malloc','_free']\" -s \"EXPORTED_RUNTIME_METHODS=['ccall','cwrap','getValue','setValue','HEAPF32']\" -s MALLOC=emmalloc -s SINGLE_FILE=1 -s MAXIMUM_MEMORY=268435456 -s STACK_SIZE=4194304 -s NO_EXIT_RUNTIME=1 -s ENVIRONMENT=web,worker,shell -s ASSERTIONS=1 -fwasm-exceptions --no-entry")

set_target_properties(abdjunio601_wasm PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../Source/UI/WebUI/wasm"
    SUFFIX ".js"
)
