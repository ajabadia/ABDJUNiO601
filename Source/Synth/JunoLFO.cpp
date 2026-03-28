// Source/Synth/JunoLFO.cpp
#include "JunoLFO.h"
#include "../Core/JunoConstants.h"

JunoLFO::JunoLFO() {
    reset();
}

void JunoLFO::prepare(double sr, int maxBlockSize) {
    sampleRate = sr;
    reset();
}

void JunoLFO::reset() {
    delayTimer = 0.0f;
    delayEnvelope = 0.0f;
    currentValue = 0.0f;
    noteActive = false;
}

void JunoLFO::setDepth(float amount) {
    depth = juce::jlimit(0.0f, 1.0f, amount);
}

void JunoLFO::setDelay(float seconds) {
    delay = juce::jlimit(0.0f, JunoConstants::Curves::kLfoDelayMax, seconds);
}

void JunoLFO::noteOn() {
    noteActive = true;
    delayTimer = 0.0f;
    delayEnvelope = 0.0f;
}

void JunoLFO::noteOff() {
    noteActive = false;
}

float JunoLFO::process(float globalLfoValue) {
    if (noteActive && delayEnvelope < 1.0f) {
        if (delay > 0.0f) {
            delayTimer += 1.0f / static_cast<float>(sampleRate);
            // [Enrichment] RC-style Exponential Onset
            float linearProgress = juce::jlimit(0.0f, 1.0f, delayTimer / delay);
            // [Enrichment] RC-style Exponential Onset (Curve calibrated)
            delayEnvelope = 1.0f - std::exp(-lfoDelayCurve * linearProgress); 
        } else {
            delayEnvelope = 1.0f;
        }
    }
    
    currentValue = globalLfoValue * depth * delayEnvelope;
    return currentValue;
}
