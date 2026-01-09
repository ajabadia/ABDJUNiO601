#include "JunoADSR.h"
#include <cmath>

// ============================================================================
// JunoADSR Implementation
// Custom high-fidelity exponential state machine (Authentic Juno-106 behavior)
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
         float target = (stage == Stage::Release || stage == Stage::Idle) ? 0.0f : 1.0f;
         currentValue += (target - currentValue) * 0.05f;
         if (std::abs(target - currentValue) < 0.001f) {
             currentValue = target;
             if (target == 0.0f) stage = Stage::Idle;
         }
         return currentValue;
    }

    switch (stage)
    {
        case Stage::Attack:
        {
            currentValue += attackRate * (1.05f - currentValue);
            if (currentValue >= 1.0f) {
                currentValue = 1.0f;
                stage = Stage::Decay;
            }
            break;
        }
        
        case Stage::Decay:
        {
            currentValue = sustainLevel + (currentValue - sustainLevel) * decayRate;
            if (currentValue - sustainLevel < 0.001f) {
                currentValue = sustainLevel;
                stage = Stage::Sustain;
            }
            break;
        }
        
        case Stage::Sustain:
        {
            // Sustain is handled by the condition in the Decay stage
            break;
        }
        
        case Stage::Release:
        {
            currentValue *= releaseRate;
            if (currentValue < 0.001f) {
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
    
    attackRate = 1.0f - std::exp(-1.0f / (attackTime * sampleRate * 0.5f));
    decayRate = std::exp(-1.0f / (decayTime * sampleRate));
    releaseRate = std::exp(-1.0f / (releaseTime * sampleRate));
}
