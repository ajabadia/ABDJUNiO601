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
    void setDriftRate(float rate) { driftRate = rate; }
    void setLfoPitchDepth(float depth) { dcoLfoPitchDepth = depth; }
    
    // [Calibration]
    void setMixerGain(float gain);      // Master DCO gain (default 0.7)
    void setSubAmpScale(float scale);   // Sub weight (default 1.0)
    void setPWMOffset(float offset) { pwmOffset = offset; }
    void setNoiseGain(float gain) { noiseGain = gain; }
    void setVoiceVariance(float cents) { voiceVariance = cents; }
    void setGlobalDriftScale(float cents) { globalDriftScale = cents; }
    void setVoiceDriftScale(float cents) { voiceDriftScale = cents; }
    void setMasterClock(float hz) { masterClockHz = hz; }
    
    // Processing (receives LFO value from external LFO)
    float getNextSample(float lfoValue);
    
private:
    float globalDriftPhase = 0.0f; // [Fix] Thread-safe per-instance drift phase
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
    
    // [Calibration]
    float mixerGain = 0.7f;
    float subAmpScale = 1.0f;
    float pwmOffset = 0.0f;
    float noiseGain = 1.0f;
    float voiceVariance = 2.0f;
    float globalDriftScale = 0.5f;
    float voiceDriftScale = 0.3f;
    float driftRate = 0.008f;
    float dcoLfoPitchDepth = 0.4f;
    float masterClockHz = 8000000.0f;
    
    // PWM
    float pwmValue = 0.5f;
    PWMMode pwmMode = PWMMode::Manual;
    float lfoDepth = 0.0f;
    float currentPWM = 0.5f;      // Slewed
    
    // Drift (Multi-level authenticity)
    float driftAmount = 0.0f;
    float staticSpreadCents = 0.0f;
    float globalDriftCents = 0.0f;
    float voiceDriftCents = 0.0f;
    float voicePhase = 0.0f;
    float voiceRate = 0.02f;
    
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
