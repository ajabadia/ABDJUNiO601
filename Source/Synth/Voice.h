#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <algorithm>
#include "JunoDCO.h"
#include "JunoADSR.h"
#include "../Core/SynthParams.h"

/**
 * Voice
 * 
 * Represents one of the 6 voices of the JUNiO 601.
 */
class Voice {
public:
    Voice();
    
    void prepare(double sampleRate, int maxBlockSize);
    
    // The voice now processes a buffer containing the GLOBAL LFO signal
    void renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, const std::vector<float>& lfoBuffer, float neighborCrosstalk);
    
    void noteOn(int midiNote, float velocity, bool isLegato);
    void noteOff();
    void forceStop();
    
    bool isActive() const { return currentNote != -1; }
    int getCurrentNote() const { return currentNote; }
    bool isGateOnActive() const { return isGateOn; }
    float lastActiveOutputLevel() const { return lastOutputLevel; }
    
    void updateParams(const SynthParams& params);
    void forceUpdate(); // [Fix] Instant parameter update (no smoothing) for patch load
    void updateHPF();
    
    void setBender(float v);
    void setPortamentoEnabled(bool b);
    void setPortamentoTime(float v);
    void setPortamentoLegato(bool b);
    void setVoiceIndex(int i) { voiceIndex = i; }

private:
    // Components
    JunoDCO dco;
    // LFO has been removed from the voice; it's now global in PluginProcessor
    JunoADSR adsr;
    
    juce::dsp::LadderFilter<float> filter;
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
    
    bool isGateOn = false;
    float lastOutputLevel = 0.0f;
    float lastModOctaves = 0.0f;
    uint8_t lastEnvByte = 0;
    juce::Random noiseGen;
    
    float thermalDrift = 0.0f;   // [Fidelidad] Independent voice heat
    float thermalTarget = 0.0f;
    int thermalCounter = 0;
    
    // [Fidelidad] Unison Detune requires index knowledge
    int voiceIndex = 0;

    SynthParams params;
    juce::AudioBuffer<float> tempBuffer;
    
    // [Fix] Removed releaseCounter/timeout - Allow natural envelope decay
    // int releaseCounter = 0;
    // static constexpr int kReleaseTimeoutMs = 30000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Voice)
};
