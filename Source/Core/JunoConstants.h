#pragma once
#include <cmath>
#include <algorithm>

/**
 * JunoConstants.h
 * Centralized constants for JUNiO 601 emulations.
 */
namespace JunoConstants
{
    // --- Numerical Helpers ---
    static inline float curveMap(float val, float minV, float maxV, float exponent = 3.0f) {
        // [Fidelity] Roland Juno-106 hardware sliders use a non-linear scaling 
        // that is roughly quadratic (approx x^2 or x^3) for better precision in short times.
        // [BUG FIX] Changed min threshold from 1.5ms to 2.5ms to prevent silence/clicks in fast patches.
        float safeMin = std::max(minV, 0.0025f); 
        if (val <= 0.0f) return safeMin;
        if (val >= 1.0f) return maxV;
        return safeMin + (maxV - safeMin) * std::pow(val, exponent);
    }
    // --- Global DSP Constants ---
    constexpr float kPwmOffThreshold = 0.98f;
    constexpr float kPwmSlewRateManual = 0.0009f;
    constexpr float kPwmSlewRateLFO = 0.00047f;
    constexpr float kUnisonDetuneMaxSemitones = 0.5f;

    // --- Versioning ---
    static constexpr int kCurrentSaveFormatVersion = 2;
}
