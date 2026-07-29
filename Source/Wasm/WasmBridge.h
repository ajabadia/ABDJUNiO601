/**
 * WasmBridge.h — Public C exports for ABDJUNiO601 DSP WASM module.
 *
 * Exposes a C-compatible API that JavaScript can call via cwrap/ccall.
 * The bridge wraps a global JunoEngine singleton and SynthParams instance,
 * translating flat string-keyed parameters to the SynthParams struct fields.
 *
 * All functions are extern "C" and use only primitive types so the
 * Emscripten linker can export them by name.
 */
#pragma once

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

extern "C"
{

    /**
     * Initialize the DSP engine.
     * Must be called once before any other WASM DSP functions.
     * @param sampleRate  Target sample rate (e.g. 44100)
     * @param blockSize   Max expected block size
     * @param model       Target model (0=SuperSix, 1=J106, 2=J60, 3=J6)
     */
    void EMSCRIPTEN_KEEPALIVE wasm_init_engine(float sampleRate, int blockSize, int model);

    /**
     * Process one audio block.
     * @param output       Pointer to interleaved float output buffer (numSamples * 2 channels)
     * @param numSamples   Number of sample frames
     * @param lfoTrig      Non-zero to trigger LFO reset
     */
    void EMSCRIPTEN_KEEPALIVE wasm_process_audio(float* output, int numSamples, int lfoTrig);

    /**
     * Set a synthesizer parameter by string key.
     * @param key    Null-terminated parameter name (e.g. "dcoRange", "vcfFreq")
     * @param value  Normalized value (0.0 – 1.0)
     */
    void EMSCRIPTEN_KEEPALIVE wasm_set_parameter(const char* key, float value);

    /**
     * Get a synthesizer parameter value.
     * @param  key  Null-terminated parameter name
     * @return      Current value (raw, not normalized)
     */
    float EMSCRIPTEN_KEEPALIVE wasm_get_parameter(const char* key);

    /**
     * Send a MIDI note-on event.
     */
    void EMSCRIPTEN_KEEPALIVE wasm_note_on(int channel, int midiNote, float velocity);

    /**
     * Send a MIDI note-off event.
     */
    void EMSCRIPTEN_KEEPALIVE wasm_note_off(int channel, int midiNote, float velocity);

    /**
     * Send a pitch bend event (0–16383, center 8192).
     */
    void EMSCRIPTEN_KEEPALIVE wasm_pitch_bend(int channel, int value);

    /**
     * Set portamento enabled/disabled.
     */
    void EMSCRIPTEN_KEEPALIVE wasm_set_portamento(int enabled);

    /**
     * Set portamento time (normalized 0..1).
     */
    void EMSCRIPTEN_KEEPALIVE wasm_set_portamento_time(float time);

    /**
     * Set bender amount (normalized 0..1).
     */
    void EMSCRIPTEN_KEEPALIVE wasm_set_bender_amount(float amount);

    /**
     * Panic — stop all notes immediately.
     */
    void EMSCRIPTEN_KEEPALIVE wasm_panic();

    /**
     * Switch the target model at runtime.
     * @param model  0=SuperSix, 1=J106, 2=J60, 3=J6
     */
    void EMSCRIPTEN_KEEPALIVE wasm_set_model(int model);

} // extern "C"
