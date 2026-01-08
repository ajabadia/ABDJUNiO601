// Source/Synth/JunoLFO.cpp
#include "JunoLFO.h"

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
    delay = juce::jlimit(0.0f, 5.0f, seconds);
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
            delayEnvelope = juce::jlimit(0.0f, 1.0f, delayTimer / delay);
        } else {
            delayEnvelope = 1.0f;
        }
    }
    
    currentValue = globalLfoValue * depth * delayEnvelope;
    return currentValue;
}
