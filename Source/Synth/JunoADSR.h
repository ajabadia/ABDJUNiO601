// Source/Synth/JunoADSR.h
#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <algorithm>

// ============================================================
// ADSR Mode Enum
// ============================================================
enum class ADSRMode
{
    kJ6 = 0,
    kJ60,
    kJ106
};

/**
 * JunoADSR - Authentic Juno Envelope (Analog RC & Digital MCU Modes)
 */
class JunoADSR {
public:
    enum class Stage {
        Idle,
        Attack,
        Decay,
        Sustain,  // Note: J6/J60 mode stays in Decay converging to sustain level
        Release
    };
    
    JunoADSR();
    
    // Setup
    void setSampleRate(double sampleRate);
    void reset();
    
    void setMode(ADSRMode newMode);
    ADSRMode getMode() const { return mode; }

    // Parameters (seconds)
    void setAttack(float seconds);
    void setDecay(float seconds);
    void setSustain(float level);      // 0-1
    void setRelease(float seconds);
    
    // ROM table indices / Raw slider setters
    void setAttackRaw(float slider);
    void setDecayRaw(float slider);
    void setReleaseRaw(float slider);
    void setTimeScale(float scale) { mTimeScale = scale; calculateRates(); }
    
    void setGateMode(bool enabled);    // ENV button
    void setSlewMs(float ms);          // [Calibration] Dynamic smoothing
    void setAttackFactor(float factor); 
    void setMcuRate(float ms) { mcuRateFactor = ms; calculateRates(); }
    void setDacSteps(float steps) { dacSteps = steps; }
    void setOvershoot(float level) { overshoot = level; }
    
    // Lifecycle
    void noteOn();
    void noteOff();
    
    // Processing
    float getNextSample();
    bool isActive() const { return stage != Stage::Idle; }
    
    Stage getCurrentStage() const { return stage; }
    float getCurrentValue() const { return currentValue; }
    
    // Analog curve helpers (slider 0-1 -> tau seconds)
    static float AttackTauJ6(float slider);
    static float DecRelTauJ6(float slider);
    static float AttackMsJ6(float slider) { return AttackTauJ6(slider) * 1791.8f; }
    static float DecRelMsJ6(float slider) { return DecRelTauJ6(slider) * 2397.9f; }

private:
    static constexpr uint16_t kEnvMax = 0x3FFF;
    static constexpr float kAttackTarget = 1.2f;    // RC charge overshoot
    static constexpr float kReleaseTarget = -0.1f;  // below zero (ensures completion)
    static constexpr float kSilence = 1e-5f;        // -100dB release cutoff
    static constexpr float kGateSlope = 1.f / 32.f;  // gate slew ramp

    double sampleRate = 44100.0;
    ADSRMode mode = ADSRMode::kJ106;

    // Parameters
    float attackTime = 0.01f;      // seconds
    float decayTime = 0.3f;        // seconds
    float sustainLevel = 0.7f;     // 0-1
    float releaseTime = 0.5f;      // seconds
    bool gateMode = false;         // ENV button state
    float slewMs = 1.5f;           // [Calibration] Output smoothing time
    float attackFactor = 0.35f;    // [Calibration] Attack curvature
    float mcuRateFactor = 4.2335f; // [Calibration] MCU update speed (ms)
    float dacSteps = 1024.0f;      // [Calibration] DAC resolution (steps)
    float overshoot = 1.08f;       // [Calibration] Attack overshoot target
    
    // State
    Stage stage = Stage::Idle;
    float currentValue = 0.0f;
    float gateEnv = 0.0f;
    
    // [Fidelidad] MCU Emulation & Ticks (Juno-106 mode)
    uint16_t mEnvInt = 0;        // current 14-bit envelope (0–0x3FFF)
    uint16_t mAtkInc = 0;        // attack increment per tick
    uint16_t mDecMul = 0;        // decay multiplier per tick
    uint16_t mRelMul = 0;        // release multiplier per tick
    uint16_t mSusInt = 0;        // sustain level as integer
    float mTickAccum = 0.f;      // fractional tick accumulator
    float mEnvPrev = 0.f;        // previous tick output (for interpolation)
    float mEnvNext = 0.f;        // current tick output (for interpolation)
    float mTickStep = 0.f;       // ticks per sample
    float mTimeScale = 1.0f;     // per-voice component tolerance
    
    // Analog coefficients (J6/J60 mode)
    float mAttackCoeff = 0.f;
    float mDecayCoeff = 0.f;
    float mReleaseCoeff = 0.f;

    float smoothedValue = 0.0f; 
    
    // Helpers
    void calculateRates();
    void tick106();
    static uint16_t calcDecay(uint16_t value, uint16_t coeff);
};
