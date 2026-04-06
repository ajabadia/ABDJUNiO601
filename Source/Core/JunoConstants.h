#pragma once
#include <cmath>

/**
 * JunoConstants.h
 * Centralized constants for JUNiO 601 emulations.
 */
namespace JunoConstants
{
    // --- Numerical Helpers ---
    static inline float curveMap(float val, float minV, float maxV, float exponent = 2.2f) {
        // [Fidelity] Roland Juno-106 hardware sliders use a non-linear scaling 
        // that is roughly quadratic (approx x^2 or x^3) for better precision in short times.
        if (val <= 0.0f) return minV;
        if (val >= 1.0f) return maxV;
        return minV + (maxV - minV) * std::pow(val, exponent);
    }
    // --- Global DSP Constants ---
    constexpr float kVoiceCrosstalkAmount = 0.007f;
    constexpr float kVoiceOutputGain = 1.5f;
    constexpr float kGlobalThermalDriftScale = 1.5f;
    constexpr float kThermalInertia = 1024; // Samples between drift updates
    constexpr float kThermalMigration = 0.0005f;

    // --- DCO & PWM ---
    constexpr float kDcoDriftMaxSpreadCents = 2.0f;
    constexpr float kDcoDriftMaxGlobalCents = 0.5f;
    constexpr float kDcoDriftMaxVoiceCents = 0.3f;
    constexpr float kMasterClockHz = 8000000.0f;
    constexpr float kDcoMixerSaturationThreshold = 0.6f;
    
    // PWM Constants (Juno-106: 50% center, 95% max)
    constexpr float kPwmCenterDuty = 0.5f;
    constexpr float kPwmMaxDuty = 0.95f;
    constexpr float kPwmMinDuty = 0.05f;
    constexpr float kPwmOffThreshold = 0.98f;
    constexpr float kPwmSlewRateManual = 0.0009f;
    constexpr float kPwmSlewRateLFO = 0.00047f;

    // --- Suboscillator & Noise ---
    static constexpr float kSubAmpScale  = 1.25f;    // [Sprint 10] Increased prominence for hardware punch
    static constexpr float kDcoMixerGain = 0.58f;   // [Sprint 10] Scaled down to prevent VCF saturation with all waves on
    static constexpr float kNoiseAmpScale = 0.450f; // [Sprint 10] Slightly reduced to match original mixing floor
    constexpr float kUnisonDetuneMaxSemitones = 0.5f; // Characteristic 106 spread

    // --- HPF Constants ---
    namespace HPF
    {
        static constexpr float kFreq2 = 225.0f;
        static constexpr float kFreq3 = 700.0f;
        static constexpr float kShelfFreq = 70.0f;
        static constexpr float kShelfGainDb = 3.0f;
    }

    // --- Voice Kill Thresholds ---
    constexpr float kVoiceKillThreshold = 0.004f; // ~0.4% output level

    // --- Time & Frequency Curves ---
    namespace Curves
    {
        static constexpr float kLfoMinHz = 0.1f;
        static constexpr float kLfoMaxHz = 30.0f;
        static constexpr float kLfoDelayMax = 3.0f; // [Manual] 0-3s
        
        static constexpr float kVcfMinHz = 8.0f;    // [Sprint 10] Authentic 106 sub-floor
        static constexpr float kVcfMaxHz = 16000.0f; // [Sprint 10] Practical BBD/OTA limit
        
        static constexpr float kAttackMin = 0.0015f;
        static constexpr float kAttackMax = 3.0f;
        static constexpr float kDecayMin = 0.0015f;
        static constexpr float kDecayMax = 12.0f;
        static constexpr float kReleaseMin = 0.0015f;
        static constexpr float kReleaseMax = 12.0f;

        static constexpr float kPortamentoMin = 0.005f;
        static constexpr float kPortamentoMax = 2.0f;
    }

    // --- Chorus Constants ---
    namespace Chorus
    {
        static constexpr float kRateI = 0.45f;
        static constexpr float kRateII = 0.8f;
        static constexpr float kRateBoth = 7.7f;
        static constexpr float kDelayBaseMs = 15.0f;
        static constexpr float kModDepthMs = 4.0f;

        static constexpr float kHissLevelHalfDb = -58.0f;
        static constexpr float kHissLevelFullDb = -52.0f;
    }

    // --- Versioning ---
    static constexpr int kCurrentSaveFormatVersion = 2;
}
