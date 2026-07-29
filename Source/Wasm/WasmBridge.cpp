/**
 * WasmBridge.cpp — Global DSP engine singleton + parameter mapping for WASM.
 *
 * Maintains:
 *   - A global JunoEngine instance (gEngine)
 *   - A global SynthParams instance (gParams)
 *   - A std::map<std::string, float> for generic parameter storage (gParamMap)
 *   - A syncParamsFromMap() step that copies mapped keys into gParams fields
 *
 * The wasm_set_parameter / wasm_get_parameter functions operate on the map.
 * wasm_process_audio calls syncParamsFromMap() then gEngine->process().
 *
 * When the calibration pointer is nullptr (no CalibrationSettings available
 * in the headless WASM build), the DSP code uses safe default values.
 */
#include "WasmBridge.h"
#include "../Core/JunoEngine.h"
#include "../Core/SynthParams.h"
#include "../Core/JunoModelConfig.h"

#include <cstring>
#include <map>
#include <memory>
#include <string>

// ------------------------------------------------------------------
// Globals
// ------------------------------------------------------------------
static std::unique_ptr<JunoEngine> gEngine;
static SynthParams gParams;
static std::map<std::string, float> gParamMap;
static std::map<std::string, int> gIntParamMap;
static float gOutputChannels[2][16384]; // max block size per channel

// ------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------
static float getMap(const char* key, float fallback = 0.0f)
{
    auto it = gParamMap.find(key);
    return (it != gParamMap.end()) ? it->second : fallback;
}

static int getIntMap(const char* key, int fallback = 0)
{
    auto it = gIntParamMap.find(key);
    return (it != gIntParamMap.end()) ? it->second : fallback;
}

// ------------------------------------------------------------------
// Parameter synchronisation: gParamMap + gIntParamMap -> gParams
// ------------------------------------------------------------------
static void syncParamsFromMap()
{
    gParams.dcoRange       = (int)getMap("dcoRange");
    gParams.pwmAmount      = getMap("pwmAmount");
    gParams.sawOn          = getMap("sawOn") > 0.5f;
    gParams.pulseOn        = getMap("pulseOn") > 0.5f;
    gParams.subOscLevel    = getMap("subOscLevel");
    gParams.noiseLevel     = getMap("noiseLevel");
    gParams.lfoToDCO       = getMap("lfoToDCO");

    gParams.vcfFreq        = getMap("vcfFreq");
    gParams.resonance      = getMap("resonance");
    gParams.envAmount      = getMap("envAmount");
    gParams.kybdTracking   = getMap("vcfKeyTrack");
    gParams.lfoToVCF       = getMap("lfoToVCF");

    gParams.attack         = getMap("attack");
    gParams.decay          = getMap("decay");
    gParams.sustain        = getMap("sustain");
    gParams.release        = getMap("release");

    gParams.masterVolume   = getMap("masterVolume");
    gParams.vcaLevel       = getMap("vcaLevel");

    gParams.lfoRate        = getMap("lfoRate");

    gParams.chorus1        = getMap("chorus1");
    gParams.chorus2        = getMap("chorus2");
    gParams.chorusMode     = (int)getMap("chorusMode");

    gParams.vcaMode        = (int)getMap("vcaMode");
    gParams.polyMode       = (int)getMap("polyMode");

    gParams.portamentoTime = getMap("portamentoTime");

    gParams.hpfFreq        = (int)getMap("hpfCutoff");

    gParams.noiseFloorMul  = getMap("noiseFloor");
    gParams.thermalIntensity = getMap("thermalIntensity");
    gParams.thermalInertia = getMap("thermalInertia");
    gParams.thermalMigration = getMap("thermalMigration");

    gParams.unisonDetune   = getMap("detuningAmount");
}

// ------------------------------------------------------------------
// Public C API
// ------------------------------------------------------------------

void wasm_init_engine(float sampleRate, int blockSize, int model)
{
    gEngine = std::make_unique<JunoEngine>();
    gEngine->setTargetModel(model);
    gEngine->prepare(static_cast<double>(sampleRate), blockSize);

    gParamMap.clear();
    gIntParamMap.clear();
    std::memset(&gParams, 0, sizeof(gParams));
}

void wasm_process_audio(float* output, int numSamples, int lfoTrig)
{
    if (!gEngine || !output || numSamples <= 0)
        return;

    syncParamsFromMap();

    if (numSamples > 16384) numSamples = 16384;

    float* channels[2] = { gOutputChannels[0], gOutputChannels[1] };
    juce::AudioBuffer<float> buffer(channels, 2, numSamples);
    buffer.clear();

    gEngine->process(gParams, buffer, numSamples, lfoTrig != 0);

    for (int i = 0; i < numSamples; ++i)
    {
        output[i * 2 + 0] = buffer.getSample(0, i);
        output[i * 2 + 1] = buffer.getSample(1, i);
    }
}

void wasm_set_parameter(const char* key, float value)
{
    gParamMap[std::string(key)] = value;
}

float wasm_get_parameter(const char* key)
{
    return getMap(key, 0.0f);
}

void wasm_note_on(int channel, int midiNote, float velocity)
{
    if (gEngine)
        gEngine->noteOn(channel, midiNote, velocity);
}

void wasm_note_off(int channel, int midiNote, float velocity)
{
    if (gEngine)
        gEngine->noteOff(channel, midiNote, velocity);
}

void wasm_pitch_bend(int channel, int value)
{
    if (gEngine)
    {
        float bend = (value - 8192.0f) / 8192.0f;
        gEngine->setBenderAmount(bend);
    }
}

void wasm_set_portamento(int enabled)
{
    if (gEngine)
        gEngine->setPortamentoEnabled(enabled != 0);
}

void wasm_set_portamento_time(float time)
{
    gParamMap["portamentoTime"] = time;
    if (gEngine)
        gEngine->setPortamentoTime(time);
}

void wasm_set_bender_amount(float amount)
{
    if (gEngine)
        gEngine->setBenderAmount(amount);
}

void wasm_panic()
{
    if (gEngine)
        gEngine->panic();
}

void wasm_set_model(int model)
{
    if (gEngine)
        gEngine->setTargetModel(model);
}
