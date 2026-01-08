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
            // [reimplement.md] Analyzed Hardware: Attack targets 1.5V (internal) to ensure 
            // linear-like snap in the 0-1.0V range, then clamps.
            // Formula: val += rate * (target - val)
            currentValue += attackRate * (1.5f - currentValue);
            
            if (currentValue >= 1.0f) {
                currentValue = 1.0f;
                stage = Stage::Decay;
            }
            break;
        }
        
        case Stage::Decay:
        {
            // Standard exponential decay to Sustain
            currentValue += decayRate * (sustainLevel - currentValue);
            
            // Tolerance threshold
            if (std::abs(currentValue - sustainLevel) < 0.001f) {
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
            // [reimplement.md] Release targets -0.2f to ensure tail usually finishes cleanly 
            // rather than hanging mathematically forever.
            currentValue += releaseRate * (-0.2f - currentValue);
            
            if (currentValue <= 0.0f) {
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
    
    // Attack: Target 1.5, Threshold 1.0. 
    // Remaining ratio at threshold = (1.5 - 1.0) / 1.5 = 0.3333...
    double attackTau = (double)attackTime * sampleRate;
    attackRate = 1.0f - std::pow(0.333333f, 1.0f / static_cast<float>(attackTau));
    attackRate = juce::jlimit(0.0f, 1.0f, attackRate);
    
    // Decay: Target Sustain. We want to reach within 0.001 produced deviation.
    // Standard exp logic.
    double decayTau = (double)decayTime * sampleRate;
    // We assume calculating rate for a full 1.0 drop reference for consistency? 
    // Or closer: (1.0 - sustain) is the drop. We want to settle.
    // Let's use standard time-constant approx: reaching 99.9% in decayTime.
    // tau of RC circuit logic: rate = 1 - e^(-1/tauSamples).
    // If input 'seconds' is meant to be 5*tau (full settlement), we adjust.
    // Existing code used a specific threshold logic. Let's stick to valid approximation:
    // Rate to clear 99.9% difference.
    decayRate = 1.0f - std::pow(0.001f, 1.0f / static_cast<float>(decayTau));
    decayRate = juce::jlimit(0.0f, 1.0f, decayRate);
    
    // Release: Target -0.2. Start (worst case) 1.0. Range 1.2.
    // Threshold 0.0. Distance to target at threshold = 0.2.
    // Ratio = 0.2 / 1.2 = 1/6 = 0.16666...
    double releaseTau = (double)releaseTime * sampleRate;
    releaseRate = 1.0f - std::pow(0.166667f, 1.0f / static_cast<float>(releaseTau));
    releaseRate = juce::jlimit(0.0f, 1.0f, releaseRate);
}
