// Source/Synth/JunoLFO.h
#pragma once
#include <JuceHeader.h>

/**
 * JunoLFO - Monophonic modulation handler with per-voice delay
 */
class JunoLFO {
public:
    JunoLFO();
    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setDepth(float amount); 
    void setDelay(float seconds);
    void noteOn();
    void noteOff();
    float process(float globalLfoValue);
    float getCurrentValue() const { return currentValue; }
    
private:
    double sampleRate = 44100.0;
    float depth = 1.0f;
    float delay = 0.0f;
    float delayTimer = 0.0f;
    float delayEnvelope = 0.0f;
    bool noteActive = false;
    float currentValue = 0.0f;
};
