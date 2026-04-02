#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <algorithm>
#include "JunoDCO.h"
#include "JunoLFO.h"
#include "../ABD-SynthEngine/Core/Voices/VoiceBase.h"
#include "JunoADSR.h"
#include "../ABD-SynthEngine/Core/LFO/LFOGeneric.h"
#include "../Core/SynthParams.h"
#include "JunoVCF.h"

/**
 * Voice - A single 106 voice instance.
 * 
 * Manages the complete signal chain for one voice:
 * DCO -> HPF -> VCF -> VCA -> Outputs.
 * Integrates independent LFO, ADSR, and Thermal Drift logic.
 */
class Voice : public ABD::VoiceBase {
public:
    Voice();
    
    // ABD::VoiceBase Overrides
    using VoiceBase::noteOn;
    using VoiceBase::noteOff;
    void onPrepare() override;
    void onReset() override;
    void onNoteOn(int midiNote, float velocity) override;
    void onNoteOff(float velocity) override;
    
    /**
     * Renders the next block of audio for this voice.
     * 
     * @param buffer      The output audio buffer to render into.
     * @param startSample The starting sample index in the buffer.
     * @param numSamples  The number of samples to process.
     */
    void renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;
    
    // Setters for dynamic render state (called by VoiceAllocator or Manager)
    void setLfoBuffer(const std::vector<float>* buffer) { lfoBufferPtr = buffer; }
    void setCrosstalk(float crosstalk) { currentNeighborCrosstalk = crosstalk; }
    void setUnisonCount(int count) { currentUnisonCount = count; }

    // Legacy noteOn wrapper for compatibility if needed (but we should move to VoiceAllocator)
    void noteOn(int midiNote, float velocity, bool isLegato, int numVoicesInUnison);
    void noteOff();
    void forceStop();
    void prepareForStealing(); 
    
    bool isActive() const { return currentNote != -1; }
    int getCurrentNote() const { return currentNote; }
    bool isGateOnActive() const { return isGateOn; }
    float lastActiveOutputLevel() const { return lastOutputLevel; }
    
    void updateParams(const SynthParams& params);
    void forceUpdate(); // [Fix] Instant parameter update (no smoothing) for patch load
    void updateHPF(int position = -1);
    
    void setBender(float v);
    void setPortamentoEnabled(bool b);
    void setPortamentoTime(float v);
    void setPortamentoLegato(bool b);
    void setVoiceIndex(int i) { voiceIndex = i; }
    void setTuningTable(const float* table) { tuningTable = table; }

private:
    float updatePitch(int numSamples);
    void renderVoiceCycles(float* voiceData, int numSamples, const std::vector<float>& lfoBuffer, float neighborCrosstalk);
    void processFinalOutput(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, float* voiceData, int numVoicesInUnison);
    
    // New helper for modular rendering
    void renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, 
                         const std::vector<float>& lfoBuffer, float neighborCrosstalk, int numVoicesInUnison);

    // Components
    JunoDCO dco;
    JunoADSR adsr;
    JunoLFO voiceLFO;
    
    JunoVCF filter;
    juce::dsp::IIR::Filter<float> hpFilter;
    juce::dsp::IIR::Filter<float> resCompFilter;
    juce::dsp::IIR::Filter<float> hpfShelfFilter;
    juce::dsp::IIR::Filter<float> noiseColorFilter;
    
    // Smoothing
    juce::LinearSmoothedValue<float> smoothedCutoff;
    juce::LinearSmoothedValue<float> smoothedResonance;
    juce::LinearSmoothedValue<float> smoothedVCALevel;

    // State
    double sampleRate = 44100.0;
    int currentNote = -1;
    float velocity = 0.0f;
    float currentFrequency = 440.0f;
    float targetFrequency = 440.0f;
    float currentNoteSlew = 0.0f; // For portamento
    float targetNote = 0.0f;      // For portamento
    
    bool isGateOn = false;
    float lastOutputLevel = 0.0f;
    uint8_t lastEnvByte = 0;
    juce::Random noiseGen;
    float lastNoise = 0.0f;
    
    float thermalDrift = 0.0f;   // [Fidelidad] Independent voice heat
    float thermalTarget = 0.0f;
    int thermalCounter = 0;
    
    // [Fidelidad] Unison Detune requires index knowledge
    // State from Manager set every block
    const std::vector<float>* lfoBufferPtr = nullptr;
    float currentNeighborCrosstalk = 0.0f;
    int currentUnisonCount = 1;

    int voiceIndex = 0;
    const float* tuningTable = nullptr;

    SynthParams params;
    juce::AudioBuffer<float> tempBuffer;
    bool stealPending = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Voice)
};
