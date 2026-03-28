// Source/Synth/JunoLFO.h
#pragma once

#include <JuceHeader.h>

/**
 * JunoLFO - Per-voice LFO delay ramp handler
 * 
 * NOTE: Currently not used. The LFO phase is generated globally in PluginProcessor
 * and the delay envelope is also applied globally (wasAnyNoteHeld / masterLfoDelayEnvelope).
 * This class is preserved for future restoration of per-voice delay behaviour,
 * where each voice has its own fade-in ramp after noteOn.
 */
class JunoLFO {
public:
    JunoLFO();
    
    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    
    void setDepth(float amount);     // 0 - 1
    void setDelay(float seconds);    // 0 - 5 seconds
    void setDelayCurve(float curve) { lfoDelayCurve = curve; }
    
    void noteOn();
    void noteOff();
    
    // Processes the global LFO value applying the voice-specific delay ramp
    float process(float globalLfoValue);
    
    float getCurrentValue() const { return currentValue; }
    
private:
    double sampleRate = 44100.0;
    float depth = 1.0f;
    float delay = 0.0f;
    float lfoDelayCurve = 5.0f;
    
    float delayTimer = 0.0f;
    float delayEnvelope = 0.0f;
    bool noteActive = false;
    
    float currentValue = 0.0f;
};
