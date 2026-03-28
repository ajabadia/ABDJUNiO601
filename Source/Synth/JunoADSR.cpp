#include "JunoADSR.h"
#include <cmath>

// ============================================================================
// JunoADSR Implementation
// Authentic Roland Juno-106 Envelope Reproduction
// ============================================================================

JunoADSR::JunoADSR()
{
    reset();
}

void JunoADSR::setSampleRate(double sr)
{
    if (sr > 0.0) {
        sampleRate = sr;
        calculateRates();
    }
}

void JunoADSR::reset()
{
    stage = Stage::Idle;
    currentValue = 0.0f;
}

void JunoADSR::setAttack(float seconds)
{
    attackTime = juce::jlimit(0.001f, 10.0f, seconds);
    calculateRates();
}

void JunoADSR::setDecay(float seconds)
{
    decayTime = juce::jlimit(0.001f, 10.0f, seconds);
    calculateRates();
}

void JunoADSR::setSustain(float level)
{
    sustainLevel = juce::jlimit(0.0f, 1.0f, level);
}

void JunoADSR::setRelease(float seconds)
{
    releaseTime = juce::jlimit(0.001f, 10.0f, seconds);
    calculateRates();
}

void JunoADSR::setGateMode(bool enabled) { gateMode = enabled; }
void JunoADSR::setSlewMs(float ms) { slewMs = juce::jlimit(0.1f, 10.0f, ms); }
void JunoADSR::setAttackFactor(float factor) { attackFactor = juce::jlimit(0.1f, 1.0f, factor); calculateRates(); }

void JunoADSR::noteOn()
{
    stage = Stage::Attack;
    mcuUpdateCounter = 0; // [Fix] Start MCU update cycle immediately on Note On
}

void JunoADSR::noteOff()
{
    if (stage != Stage::Idle) {
        stage = Stage::Release;
    }
}

float JunoADSR::getNextSample()
{
    if (gateMode) {
         float target = (stage == Stage::Release || stage == Stage::Idle) ? 0.0f : 0.97f;
         // [Fidelity] Fast analog-style slew (~2ms) to prevent digital clicks
         currentValue += (target - currentValue) * 0.03f;

         // [Fix] Gate mode must also terminate to Stage::Idle to allow voice reuse
         if (stage == Stage::Release && currentValue < 0.0001f) {
             currentValue = 0.0f;
             stage = Stage::Idle;
         }
    } else {
        // [Fidelidad] MCU Update Cycle (3ms ~ 132 samples @ 44.1k)
        if (--mcuUpdateCounter <= 0) {
        mcuUpdateCounter = mcuUpdateRateSamples;

        switch (stage)
        {
            case Stage::Attack:
            {
                // [Audit Compliance] VCF Overshoot emulation (Target > 1.0)
                // We target 'overshoot' (default 1.08f) to simulate the analog overshoot behavior commanded by the 8031.
                currentValue += attackRate * (overshoot - currentValue); 
                if (currentValue >= 1.0f) {
                    currentValue = 1.0f;
                    stage = Stage::Decay;
                }
                break;
            }
            
            case Stage::Decay:
            {
                // [Fidelity] Juno-106 "Shifted Space" logic:
                // Decay happens towards 0.0 in a space shifted by sustainLevel.
                float x = currentValue - sustainLevel;
                x *= decayRate; 
                currentValue = x + sustainLevel;

                if (std::abs(currentValue - sustainLevel) <= 0.001f) {
                    currentValue = sustainLevel;
                    stage = Stage::Sustain;
                }
                break;
            }
            
            case Stage::Sustain:
                // [Fidelity] In the Juno-106, Sustain changes are instantaneous 
                // but since we are digital, we ensure currentValue follows sustainLevel.
                currentValue = sustainLevel; 
                break;
            
            case Stage::Release:
            {
                // [Fidelity] Juno-106 "Shifted Space" logic:
                // Release happens towards 0.0 in a space shifted by 0.0.
                // This is effectively a decay towards 0.0.
                float x = currentValue; // No shift needed for target 0.0
                x *= releaseRate; 
                currentValue = x;

                if (currentValue < 0.0001f) { // [Fix] Lower threshold for high-precision release
                    currentValue = 0.0f;
                    stage = Stage::Idle;
                }
                break;
            }
            
            case Stage::Idle:
            default:
                break;
        }
    }
    }
    
    // [Fidelity Improvement] DYNAMIC DAC EMULATION
    float quantized = std::floor(currentValue * (dacSteps - 0.01f)) / (dacSteps - 1.0f);
    
    // [Fix] Analog-style Slew (dynamic via calibration) to kill 3ms digital stairs
    float alpha = 1.0f - std::exp(-1.0f / (std::max(0.1f, slewMs) * 0.001f * (float)sampleRate));
    smoothedValue += (quantized - smoothedValue) * alpha;
    return smoothedValue;
}

void JunoADSR::calculateRates()
{
    if (sampleRate <= 0.0) return;
    
    // Recalculate MCU samples
    mcuUpdateRateSamples = (int)(mcuRateFactor * 0.001 * sampleRate); 
    if (mcuUpdateRateSamples < 1) mcuUpdateRateSamples = 1;

    auto getAuthenticRate = [&](float tau, bool isAttack) -> float {
        float updateInterval = (float)mcuUpdateRateSamples;
        float sr = (float)sampleRate;
        
        if (isAttack) {
              // Attack: Logarithmic approach to target (Target > 1.0 for overshoot)
              // Rate is adjusted so it reaches 1.0 in approx 'tau' seconds
              return 1.0f - std::exp(-updateInterval / (tau * sr * attackFactor));
        } else {
             // Decay/Release: Exponential to target
             // Rate is adjusted so it reaches 37% in approx 'tau' seconds
             return std::exp(-updateInterval / (tau * sr));
        }
    };

    attackRate = getAuthenticRate(attackTime, true); 
    decayRate = getAuthenticRate(decayTime, false);
    releaseRate = getAuthenticRate(releaseTime, false);
}
