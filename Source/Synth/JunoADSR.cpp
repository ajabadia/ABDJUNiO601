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
         currentValue = (stage == Stage::Release || stage == Stage::Idle) ? 0.0f : 1.0f;
         return currentValue;
    }

    switch (stage)
    {
        case Stage::Attack:
        {
            // Exponential Attack approach
            currentValue += attackRate * (1.1f - currentValue);
            if (currentValue >= 1.0f) {
                currentValue = 1.0f;
                stage = Stage::Decay;
            }
            break;
        }

        case Stage::Decay:
        {
            currentValue += (sustainLevel - currentValue) * decayRate;
            if (std::abs(currentValue - sustainLevel) < 0.001f) {
                currentValue = sustainLevel;
                stage = Stage::Sustain;
            }
            break;
        }

        case Stage::Sustain:
            if (std::abs(currentValue - sustainLevel) > 0.001f) {
                    currentValue += (sustainLevel - currentValue) * 0.01f;
            }
            break;

        case Stage::Release:
        {
            currentValue *= releaseRate;
            if (currentValue < 0.0001f) {
                currentValue = 0.0f;
                stage = Stage::Idle;
            }
            break;
        }

        case Stage::Idle:
        default:
            break;
    }
    
    return currentValue;
}

void JunoADSR::calculateRates()
{
    if (sampleRate <= 0.0) return;
    
    auto getRate = [&](float tau, bool isAttack) -> float {
        float sr = (float)sampleRate;
        if (isAttack) {
             return 1.0f - std::exp(-1.0f / (tau * sr * 0.3f));
        } else {
             return 1.0f - std::exp(-1.0f / (tau * sr * 0.5f));
        }
    };

    attackRate = getRate(attackTime, true);
    decayRate = getRate(decayTime, false);
    releaseRate = 1.0f - getRate(releaseTime, false); // Multiplicative factor
}
