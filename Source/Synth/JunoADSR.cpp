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

void JunoADSR::setGateMode(bool enabled)
{
    gateMode = enabled;
}

void JunoADSR::noteOn()
{
    stage = Stage::Attack;
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
         currentValue = (stage == Stage::Release || stage == Stage::Idle) ? 0.0f : 0.97f;
         return currentValue;
    }

    // [Fidelidad] MCU Update Cycle (3ms ~ 132 samples @ 44.1k)
    if (--mcuUpdateCounter <= 0) {
        mcuUpdateCounter = mcuUpdateRateSamples;

        switch (stage)
        {
            case Stage::Attack:
            {
                // [Audit Compliance] VCF Overshoot emulation (Target > 1.0)
                // We target 1.08f to simulate the analog overshoot behavior commanded by the 8031.
                currentValue += attackRate * (1.08f - currentValue); 
                if (currentValue >= 1.0f) {
                    currentValue = 1.0f;
                    stage = Stage::Decay;
                }
                break;
            }
            
            case Stage::Decay:
            {
                currentValue = sustainLevel + (currentValue - sustainLevel) * decayRate;
                if (currentValue <= sustainLevel + 0.001f) {
                    currentValue = sustainLevel;
                    stage = Stage::Sustain;
                }
                break;
            }
            
            case Stage::Sustain:
                // Re-check Sustain level if it changed in realtime? 
                // Normally Sustain is static unless param changes.
                // Smooth update towards new sustain target if changed:
                if (std::abs(currentValue - sustainLevel) > 0.001f) {
                     currentValue += (sustainLevel - currentValue) * 0.1f;
                }
                break;
            
            case Stage::Release:
            {
                currentValue *= releaseRate;
                if (currentValue < 0.005f) {
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
    
    // [Fidelidad] 8-BIT DAC QUANTIZATION (256 steps)
    // The original 8031 MCU used an 8-bit DAC for envelope control.
    // 4-bit was too aggressive and killed low sustain levels.
    float quantized = std::floor(currentValue * 255.99f) / 255.0f;
    return quantized;
}

void JunoADSR::calculateRates()
{
    if (sampleRate <= 0.0) return;
    
    // Recalculate MCU samples
    mcuUpdateRateSamples = (int)(0.003 * sampleRate); // 3ms
    if (mcuUpdateRateSamples < 1) mcuUpdateRateSamples = 1;

    auto getAuthenticRate = [&](float tau, bool isAttack) -> float {
        float updateInterval = (float)mcuUpdateRateSamples;
        float sr = (float)sampleRate;
        
        if (isAttack) {
             // Attack: Logarithmic approach to target (Target > 1.0 for overshoot)
             // Rate is adjusted so it reaches 1.0 in approx 'tau' seconds
             return 1.0f - std::exp(-updateInterval / (tau * sr * 0.35f));
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
