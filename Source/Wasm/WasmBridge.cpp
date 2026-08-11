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
#include "../Core/JunoVoiceManager.h"
#include "../Core/PerformanceState.h"
#include "../Synth/ChorusBBD.h"
#include "../Synth/JunoLFO.h"
#include "../Synth/JunoTapeEcho.h"
#include "../Synth/JunoArpeggiator.h"
#include "../Synth/JunoVoice.h"
#include "../Synth/JunoDCO.h"
#include "../Synth/JunoADSR.h"
#include "../Synth/JunoVCF.h"
#include "../Synth/JunoHPF.h"

#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <string>

// ------------------------------------------------------------------
// Globals
// ------------------------------------------------------------------
static JunoEngine* gEngine = nullptr;
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
    gParams.numVoices      = (int)getMap("numVoices", 8.0f);
    if (gParams.numVoices <= 0) gParams.numVoices = 8;

    gParams.dcoRange       = (int)getMap("dcoRange", 1.0f);
    gParams.pwmAmount      = getMap("pwmAmount", 0.0f);
    gParams.pwmMode        = (int)getMap("pwmMode", 0.0f);
    gParams.sawOn          = getMap("sawOn", 1.0f) > 0.5f;
    gParams.pulseOn        = getMap("pulseOn", 0.0f) > 0.5f;
    gParams.subOscLevel    = getMap("subOscLevel", 0.0f);
    gParams.noiseLevel     = getMap("noiseLevel", 0.0f);
    gParams.lfoToDCO       = getMap("lfoToDCO", 0.0f);

    gParams.vcfFreq        = getMap("vcfFreq", 1.0f);
    gParams.resonance      = getMap("resonance", 0.0f);
    gParams.envAmount      = getMap("envAmount", 0.5f);
    gParams.kybdTracking   = getMap("vcfKeyTrack", 0.0f);
    gParams.lfoToVCF       = getMap("lfoToVCF", 0.0f);

    gParams.attack         = getMap("attack", 0.01f);
    gParams.decay          = getMap("decay", 0.3f);
    gParams.sustain        = getMap("sustain", 0.7f);
    gParams.release        = getMap("release", 0.5f);

    gParams.masterVolume   = getMap("masterVolume", 1.0f);
    gParams.vcaLevel       = getMap("vcaLevel", 0.8f);

    gParams.lfoRate        = getMap("lfoRate", 0.5f);
    gParams.lfoDelay       = getMap("lfoDelay", 0.0f);

    gParams.chorus1        = getMap("chorus1", 0.0f) > 0.5f;
    gParams.chorus2        = getMap("chorus2", 0.0f) > 0.5f;
    gParams.chorusMode     = (int)getMap("chorusMode", 0.0f);

    gParams.vcaMode        = (int)getMap("vcaMode", 1.0f);
    gParams.polyMode       = (int)std::lround(getMap("polyMode", 0.0f) * 2.0f) + 1;

    gParams.portamentoOn   = getMap("portamentoOn", 0.0f) > 0.5f;
    gParams.portamentoTime = getMap("portamentoTime", 0.2f);
    gParams.benderValue    = getMap("bender", 0.0f);

    gParams.hpfFreq        = (int)getMap("hpfFreq", getMap("hpfCutoff", 0.0f));

    // Dynamic model routing per component
    // JS sends 0.0 (J6), 0.5 (J60), 1.0 (J106) normalized values.
    // C++ expects int 0/1/2 — multiply by 2 and round to match native APVTS.
    gParams.modelDCO       = (int)std::lround(getMap("modelDCO", 1.0f) * 2.0f);
    gParams.modelHPF       = (int)std::lround(getMap("modelHPF", 1.0f) * 2.0f);
    gParams.modelVCF       = (int)std::lround(getMap("modelVCF", 1.0f) * 2.0f);
    gParams.modelADSR      = (int)std::lround(getMap("modelADSR", 1.0f) * 2.0f);
    gParams.modelChorus    = (int)std::lround(getMap("modelChorus", 1.0f) * 2.0f);
    gParams.modelPoly      = (int)std::lround(getMap("modelPoly", 1.0f) * 2.0f);
    gParams.modelArp       = (int)std::lround(getMap("modelArp", 1.0f) * 2.0f);
    gParams.modelPorta     = (int)std::lround(getMap("modelPorta", 1.0f) * 2.0f);
    gParams.modelUnison    = (int)std::lround(getMap("modelUnison", 1.0f) * 2.0f);

    // Tape Echo / Delay FX (SuperSix Engine)
    gParams.delayEnabled     = getMap("delayEnabled", 0.0f) > 0.5f;
    gParams.delaySetting     = (int)std::lround(getMap("delaySetting", 0.0f) * 11.0f);
    gParams.delayRepeatRate  = getMap("delayRepeatRate", 0.5f);
    gParams.delayIntensity   = getMap("delayIntensity", 0.5f);
    gParams.delayBass        = getMap("delayBass", 0.5f);
    gParams.delayTreble      = getMap("delayTreble", 0.5f);
    gParams.delayReverbVol   = getMap("delayReverbVol", 0.5f);
    gParams.delayEchoVol     = getMap("delayEchoVol", 0.5f);
    gParams.delayEchoCancel  = getMap("delayEchoCancel", 0.0f) > 0.5f;
    gParams.delaySyncEnabled = getMap("delaySyncEnabled", 0.0f) > 0.5f;
    gParams.delaySyncDivision= (int)std::lround(getMap("delaySyncDivision", 0.25f) * 8.0f);
    gParams.delayReverbType  = (int)std::lround(getMap("delayReverbType", 0.0f) * 2.0f);
    gParams.delayWowFlutter  = getMap("delayWowFlutter", 0.5f);
    gParams.delayReverbDecay = getMap("delayReverbDecay", 0.5f);
    gParams.delayEchoIsolator= getMap("delayEchoIsolator", 0.5f);

    // Arpeggiator Engine
    gParams.arpEnabled       = getMap("arpEnabled", 0.0f) > 0.5f;
    gParams.arpMode          = (int)getMap("arpMode", 0.0f);
    gParams.arpRange         = (int)getMap("arpRange", 0.0f);
    gParams.arpRate          = getMap("arpRate", 0.5f);
    gParams.arpSync          = getMap("arpSync", 0.0f) > 0.5f;
    gParams.arpDivision      = (int)getMap("arpDivision", 2.0f);

    gParams.noiseFloorMul  = getMap("noiseFloor", 1.0f);
    gParams.mainsRippleMul = getMap("mainsRipple", 1.0f);
    gParams.masterNoise    = getMap("masterNoise", -80.0f);
    gParams.thermalIntensity = getMap("thermalIntensity", 1.5f);
    gParams.thermalInertia = getMap("thermalInertia", 1024.0f);
    gParams.thermalMigration = getMap("thermalMigration", 0.0005f);

    gParams.unisonDetune   = getMap("detuningAmount", 0.5f);
}

#include <cstdio>

void wasm_init_engine(float sampleRate, int blockSize, int model)
{
    if (!gEngine) {
        gEngine = new JunoEngine();
    }
    gEngine->setTargetModel(model);
    
    // Force default Space Echo calibration
    JunoEngine::TapeEchoCal cal;
    gEngine->setTapeEchoCalibration(cal);
    
    gEngine->prepare(static_cast<double>(sampleRate), blockSize);

    gParamMap.clear();
    gIntParamMap.clear();
    gParams = SynthParams();
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

    // Flat contiguous non-interleaved copy (L L ... R R ...) for fast zero-copy JS subarray copies
    const float* srcL = buffer.getReadPointer(0);
    const float* srcR = buffer.getReadPointer(1);
    std::memcpy(output, srcL, numSamples * sizeof(float));
    std::memcpy(output + numSamples, srcR, numSamples * sizeof(float));
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
