#include "JunoADSR.h"
#include <cmath>

// ============================================================================
// JunoADSR Implementation
// Custom high-fidelity exponential state machine (Authentic Juno-106 behavior)
// ============================================================================

JunoADSR::JunoADSR()
    : sampleRate(44100.0), 
      attackTime(0.01f), decayTime(0.3f), sustainLevel(0.7f), releaseTime(0.5f),
      gateMode(false), attackRate(0.0f), decayRate(0.0f), releaseRate(0.0f),
      stage(Stage::Idle), currentValue(0.0f)
{
    calculateRates();
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
    calculateRates();
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
    currentValue = 0.0f;
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
         // Simple Gate with 1ms smoothing
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
            // Exponential attack: currentValue approaches 1.0
            currentValue += attackRate * (1.0f - currentValue);
            
            if (currentValue >= 0.999f) {
                currentValue = 1.0f;
                stage = Stage::Decay;
            }
            break;
        }
        
        case Stage::Decay:
        {
            // Exponential decay from 1.0 towards sustainLevel
            currentValue -= decayRate * (currentValue - sustainLevel);
            
            if (currentValue <= (sustainLevel + 0.001f)) {
                currentValue = sustainLevel;
                stage = Stage::Sustain;
            }
            break;
        }
        
        case Stage::Sustain:
        {
            currentValue = sustainLevel;
            break;
        }
        
        case Stage::Release:
        {
            // Exponential release from current value towards 0.0
            currentValue *= (1.0f - releaseRate);
            
            if (currentValue <= 0.001f) {
                currentValue = 0.0f;
                stage = Stage::Idle;
            }
            break;
        }
        
        case Stage::Idle:
        default:
        {
            currentValue = 0.0f;
            break;
        }
    }
    
    return currentValue;
}

void JunoADSR::calculateRates()
{
    if (sampleRate <= 0.0) return;
    
    // Attack: reach 99.9% in attackTime
    double attackTau = (double)attackTime * sampleRate;
    attackRate = 1.0f - std::pow(0.001f, 1.0f / static_cast<float>(attackTau));
    attackRate = juce::jlimit(0.0f, 1.0f, attackRate);
    
    // Decay: reach sustainLevel + 0.001 asymptotically 
    double decayTau = (double)decayTime * sampleRate;
    float decayRange = 1.0f - sustainLevel;
    if (decayRange > 0.001f) {
        decayRate = 1.0f - std::pow(0.001f / decayRange, 1.0f / static_cast<float>(decayTau));
    } else {
        decayRate = 0.0f;
    }
    decayRate = juce::jlimit(0.0f, 1.0f, decayRate);
    
    // Release: go from current value to 0.001
    double releaseTau = (double)releaseTime * sampleRate;
    releaseRate = 1.0f - std::pow(0.001f, 1.0f / static_cast<float>(releaseTau));
    releaseRate = juce::jlimit(0.0f, 1.0f, releaseRate);
}
