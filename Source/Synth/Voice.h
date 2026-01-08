#pragma once

#include <JuceHeader.h>
#include "JunoDCO.h"
#include "JunoLFO.h"
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
    
    // [reimplement.md] renderNextBlock now takes the global lfoBuffer (per sample)
    void renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, const std::vector<float>& lfoBuffer);
    
    void noteOn(int midiNote, float velocity, bool isLegato);
    void noteOff();
    void forceStop() { adsr.reset(); currentNote = -1; lastOutputLevel = 0.0f; }
    
    bool isActive() const { return currentNote != -1; }
    int getCurrentNote() const { return currentNote; }
    bool isGateOnActive() const { return isGateOn; }
    
    void updateParams(const SynthParams& params);
    void updateHPF();
    
    void setBender(float v);
    void setPortamentoEnabled(bool b);
    void setPortamentoTime(float v);
    void setPortamentoLegato(bool b);

private:
    // Components
    JunoDCO dco;
    JunoLFO lfo;
    JunoADSR adsr;
    
    juce::dsp::LadderFilter<float> filter;
    juce::dsp::IIR::Filter<float> hpFilter;
    
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
    
    SynthParams params;
    juce::AudioBuffer<float> tempBuffer;
    
    int releaseCounter = 0;
    static constexpr int kReleaseTimeoutMs = 2000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Voice)
};
