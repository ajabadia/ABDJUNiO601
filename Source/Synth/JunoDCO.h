// Source/Synth/JunoDCO.h
#pragma once

#include <JuceHeader.h>

/**
 * JunoDCO - Complete Authentic Juno-106 DCO
 * 
 * AUTHENTIC CONTROLS (from front panel):
 * - RANGE: 16', 8', 4' (octave selector)
 * - LFO: LFO modulation depth to pitch
 * - PWM: Pulse width (MAN) or PWM depth (LFO)
 * - LFO/MAN: PWM mode selector
 * - Waveforms: Pulse, Saw (both can be active)
 * - SUB: Sub-oscillator level
 * - NOISE: Noise generator level
 * 
 * JUCE COMPONENTS USED:
 * - juce::dsp::Oscillator for Sawtooth
 * - juce::Random for Noise
 * - Custom for Pulse (PWM slew support)
 * - Custom for Sub-osc (flip-flop authentic)
 */
class JunoDCO {
public:
    enum class Range {
        Range16 = 0,  // -1 octave (×0.5)
        Range8 = 1,   // Normal (×1.0) - DEFAULT
        Range4 = 2    // +1 octave (×2.0)
    };
    
    enum class PWMMode {
        Manual = 0,   // PWM slider = pulse width directly
        LFO = 1       // PWM slider = LFO modulation depth
    };
    
    JunoDCO();
    
    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    
    // Frequency
    void setFrequency(float hz);
    
    // RANGE (16', 8', 4')
    void setRange(Range range);
    
    // Waveform levels (0-1)
    void setPulseLevel(float level);
    void setSawLevel(float level);
    void setSubLevel(float level);
    void setNoiseLevel(float level);
    
    // PWM
    void setPWM(float value);           // 0-1 (slider value)
    void setPWMMode(PWMMode mode);      // LFO or MAN
    
    // LFO to DCO
    void setLFODepth(float depth);      // 0-1 (LFO slider)
    
    // Character
    void setDrift(float amount);        // 0-1 (analog drift)
    
    // Processing (receives LFO value from external LFO)
    float getNextSample(float lfoValue);
    
private:
    // JUCE Components
    juce::Random noiseGen;
    
    // Manual oscillators
    double pulsePhase = 0.0;
    
    // Spec
    double sampleRate = 44100.0;
    float baseFrequency = 440.0f;
    
    // RANGE
    Range range = Range::Range8;
    float rangeMultiplier = 1.0f;
    
    // Waveform levels
    float pulseLevel = 0.5f;
    float sawLevel = 0.5f;
    float subLevel = 0.0f;
    float noiseLevel = 0.0f;
    
    // PWM
    float pwmValue = 0.5f;
    PWMMode pwmMode = PWMMode::Manual;
    float lfoDepth = 0.0f;
    float currentPWM = 0.5f;      // Slewed
    
    // Drift (Random Walk)
    float driftAmount = 0.0f;
    float driftMigrator = 0.0f;   // Current erratic tuning offset
    float driftTarget = 0.0f;     // Target for random walk
    int driftCounter = 0;         // Decimator for drift updates
    // Sub-osc flip-flop (authentic)
    bool subFlipFlop = false;
    // JUCE DSP
    juce::dsp::Oscillator<float> sawOsc;
    juce::dsp::IIR::Filter<float> noiseFilter;
    // noiseGen removed (duplicate)
    
    // Helpers
    void updateRangeMultiplier();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JunoDCO)
};
