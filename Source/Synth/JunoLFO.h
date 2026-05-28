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
class CalibrationSettings;

class JunoLFO {
public:
    JunoLFO();
    
    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    
    void setDepth(float amount);     // 0 - 1
    void setDelay(float seconds);    // 0 - 5 seconds
    void setCalibrationSettings(CalibrationSettings* c) { cal = c; }
    
    void noteOn();
    void noteOff();
    
    // Process block-level gate update
    void updateGateState(bool gated, bool trigger);
    // Integer-accurate tick called at MCU rate (~234.2 Hz)
    void tick106();
    
    float process(float sampleRate);
    
    float getCurrentValue() const { return currentValue; }
    float getLastTri() const { return mLastTri; }
    float getAmpInt() const { return mAmpInt; }
    
private:
    double sampleRate = 44100.0;
    float depth = 1.0f;
    float delay = 0.0f;
    float currentValue = 0.0f;
    
    CalibrationSettings* cal = nullptr;
    
    // --- J106 integer LFO state (firmware-accurate) ---
    uint16_t mIntAccum     = 0;      // LFO accumulator 0x0000-0x1FFF
    uint16_t mIntCoeff     = 0;      // speed table coefficient
    bool     mIntRising    = true;   // direction
    bool     mIntPolarity  = false;  // toggles each direction change
    float    mIntTri       = 0.f;    // held triangle output [-1,+1]
    float    mLastTri      = 0.f;

    // --- J106 two-stage delay state ---
    uint16_t mHoldoffAccum  = 0;     // holdoff accumulator
    uint16_t mHoldoffInc    = 0;     // holdoff rate increment
    uint16_t mRampAccum     = 0;     // ramp accumulator
    uint16_t mRampInc       = 0;     // ramp table coefficient
    bool     mInHoldoff     = false;
    bool     mArmed         = true;  // armed for reset
    float    mAmpInt        = 0.f;   // held onset envelope [0,1]
    
    bool     mWasGated      = false;
};
