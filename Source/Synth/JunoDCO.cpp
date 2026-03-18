// Source/Synth/JunoDCO.cpp
#include "JunoDCO.h"
#include "../Core/JunoConstants.h"
#include <cmath>

using namespace JunoConstants;

JunoDCO::JunoDCO() {
    updateRangeMultiplier();
    reset();
}

void JunoDCO::prepare(double sr, int maxBlockSize) {
    sampleRate = sr;
    
    juce::dsp::ProcessSpec spec { sr, (juce::uint32)maxBlockSize, 1 };
    noiseFilter.prepare(spec);
    noiseFilter.reset(); 
    noiseFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, 4000.0f, 0.707f, 0.707f); // -3dB peak at 4kHz
    reset();
}

void JunoDCO::reset() {
    pulsePhase = 0.0;
    staticSpreadCents = (noiseGen.nextFloat() * 2.0f - 1.0f) * kDcoDriftMaxSpreadCents;
    voicePhase = noiseGen.nextFloat() * juce::MathConstants<float>::twoPi;
    voiceRate = 0.01f + noiseGen.nextFloat() * 0.04f;
    currentPWM = pwmValue;
    // [Fidelity] Random phase for Sub-Osc (8253 Flip-flop starts in unknown state)
    subFlipFlop = noiseGen.nextBool(); 
}

void JunoDCO::setFrequency(float hz) {
    baseFrequency = hz;
}

void JunoDCO::setRange(Range r) {
    range = r;
    updateRangeMultiplier();
}

void JunoDCO::updateRangeMultiplier() {
    switch (range) {
        case Range::Range16: rangeMultiplier = 0.5f; break;  // -1 octave
        case Range::Range8:  rangeMultiplier = 1.0f; break;  // Normal
        case Range::Range4:  rangeMultiplier = 2.0f; break;  // +1 octave
    }
}

void JunoDCO::setPulseLevel(float level) {
    pulseLevel = juce::jlimit(0.0f, 1.0f, level);
}

void JunoDCO::setSawLevel(float level) {
    sawLevel = juce::jlimit(0.0f, 1.0f, level);
}

void JunoDCO::setSubLevel(float level) {
    subLevel = juce::jlimit(0.0f, 1.0f, level);
}

void JunoDCO::setNoiseLevel(float level) {
    noiseLevel = juce::jlimit(0.0f, 1.0f, level);
}

void JunoDCO::setPWM(float value) {
    pwmValue = juce::jlimit(0.0f, 1.0f, value);
}

void JunoDCO::setPWMMode(PWMMode mode) {
    pwmMode = mode;
}

void JunoDCO::setLFODepth(float depth) {
    lfoDepth = juce::jlimit(0.0f, 1.0f, depth);
}

void JunoDCO::setDrift(float amount) {
    driftAmount = juce::jlimit(0.0f, 1.0f, amount);
}

float JunoDCO::getNextSample(float lfoValue) {
    // === ANALOG DRIFT (Multi-level authenticity) ===
    // [Fidelity] Independent levels: Fixed spread, Global slow drift, Per-voice slow drift
    
    // 2. Slow voice drift
    voicePhase += juce::MathConstants<float>::twoPi * voiceRate / (float)sampleRate;
    if (voicePhase > juce::MathConstants<float>::twoPi) voicePhase -= juce::MathConstants<float>::twoPi;
    
    voiceDriftCents = std::sin(voicePhase) * kDcoDriftMaxVoiceCents * driftAmount;
    
    // 3. (Global drift for this voice would be set externally or simulated here)
    // For simplicity, we'll simulate a 0.015Hz global drift here if driftAmount > 0
    globalDriftPhase += juce::MathConstants<float>::twoPi * 0.015f / (float)sampleRate;
    if (globalDriftPhase > juce::MathConstants<float>::twoPi) globalDriftPhase -= juce::MathConstants<float>::twoPi;
    globalDriftCents = std::sin(globalDriftPhase) * kDcoDriftMaxGlobalCents * driftAmount;

    float totalDriftCents = staticSpreadCents * driftAmount + globalDriftCents + voiceDriftCents;
    
    // === FREQUENCY with RANGE, LFO, and DRIFT ===
    float freq = baseFrequency * rangeMultiplier;
    
    // Apply LFO to pitch (vibrato)
    float lfoSemitones = (lfoValue * 0.5f + 0.5f) * lfoDepth * 0.4f; // Slightly scaled
    freq *= std::pow(2.0f, (lfoSemitones + (totalDriftCents / 100.0f)) / 12.0f);

    // [Fidelity] 8253 TIMER QUANTIZATION (STRICT IMPL)
    // The Juno-106 DCO is driven by an Intel 8253 Programmable Interval Timer.
    // Master Clock = 8MHz. Divider = Freq * 256 (Prescaler/Integrator steps).
    // The counter is a 16-bit integer. This causes frequency stepping.
    
    if (freq > 0.0f) {
        // Calculate required ticks (Float)
        float rawTicks = kMasterClockHz / (freq * 256.0f);
        
        // Quantize to Integer (The Counter Register)
        // [Audit Fix] Strict integer casting/rounding
        uint32_t quantizedTicks = (uint32_t)(rawTicks + 0.5f);
        
        if (quantizedTicks < 1) quantizedTicks = 1;
        // 16-bit Timer Limit
        if (quantizedTicks > 65535) quantizedTicks = 65535;
        
        // Recalculate EXACT Frequency driven by the Timer
        freq = kMasterClockHz / (quantizedTicks * 256.0f);
    }
    
    // updateRangeMultiplier() handles the range. baseFrequency is bended.
    // DCO phase update happens below using current freq.
    
    // === UPDATE PHASE ===
    if (sampleRate <= 0.0) return 0.0f;
    
    double dt = freq / sampleRate;
    pulsePhase += dt;
    
    if (pulsePhase >= 1.0) {
        pulsePhase -= 1.0;
        subFlipFlop = !subFlipFlop; // Always toggle (Authentic Aliasing/Divider behavior)
    }
    
    float output = 0.0f;
    // PolyBLEP for all waves (Kill metallic aliasing)
    auto polyBlep = [](float t, float dt_param) -> float {
        if (t < dt_param) { // Near start 0
            float x = t / dt_param;
            return 2.0f * x - x * x - 1.0f;
        }
        else if (t > 1.0f - dt_param) { // Near end 1
            float x = (t - 1.0f) / dt_param;
            return x * x + 2.0f * x + 1.0f;
        }
        return 0.0f;
    };

    if (sawLevel > 0.0f) {
        // Falling saw: jumps from -1.0 to 1.0 at phase 0.0 (Magnitude +2.0)
        float saw = 1.0f - 2.0f * (float)pulsePhase;
        // Corrected Sign & Magnitude: Subtracting 2x the BLEP residue for -2.0 jump
        saw -= 2.0f * polyBlep((float)pulsePhase, (float)dt);
        output += saw * sawLevel;
    }
    
    // === 2. PULSE with PWM (PolyBLEP) ===
    if (pulseLevel > 0.0f) {
        float targetPWM = kPwmCenterDuty;
        if (pwmMode == PWMMode::Manual) {
            // [Fidelity] Juno-106: 50% at center, 95% at max
            targetPWM = kPwmCenterDuty + (pwmValue - 0.5f) * 2.0f * (kPwmMaxDuty - kPwmCenterDuty);
        } else {
            // LFO depth applies to the 50% center
            targetPWM = juce::jlimit(kPwmMinDuty, kPwmMaxDuty, kPwmCenterDuty + lfoValue * pwmValue * 0.45f);
        }
        
        // PWM "Off" mode: force waveform level if too narrow
        if (targetPWM > kPwmOffThreshold) targetPWM = 1.0f;
        if (targetPWM < (1.0f - kPwmOffThreshold)) targetPWM = 0.0f;
        
        // [Fidelity] PWM Slew Calibrated
        float slewRate = (pwmMode == PWMMode::Manual) ? kPwmSlewRateManual : kPwmSlewRateLFO;
        currentPWM += (targetPWM - currentPWM) * slewRate;
        
        float pulse = (pulsePhase < currentPWM) ? 1.0f : -1.0f;
        
        // Rising edge at 0 (Jump +2.0)
        pulse += 2.0f * polyBlep((float)pulsePhase, (float)dt);
        
        // Falling edge at currentPWM (Jump -2.0)
        float relativePhase = (float)pulsePhase - currentPWM;
        if (relativePhase < 0.0f) relativePhase += 1.0f;
        pulse -= 2.0f * polyBlep(relativePhase, (float)dt);
        
        output += pulse * pulseLevel;
    }
    
    // === 3. SUB-OSCILLATOR (PolyBLEP) ===
    if (subLevel > 0.0f) {
        // [Fidelity] Sub-Osc is a square wave from 8253 divider.
        // It toggles at pulsePhase == 0.5 and remains continuous at pulsePhase == 0.0.
        // Thus, we only need PolyBLEP at the 0.5 threshold.
        float subThreshold = 0.5f; 
        float sub = (pulsePhase < subThreshold) == subFlipFlop ? 1.0f : -1.0f;
        
        // PolyBLEP at 0.5 transition
        float relativePhase = (float)pulsePhase - subThreshold;
        if (relativePhase < 0.0f) relativePhase += 1.0f;
        
        // The jump magnitude is 2.0 (1 to -1 or -1 to 1) 
        float blep = polyBlep(relativePhase, (float)dt);
        if (subFlipFlop) sub -= 2.0f * blep;
        else sub += 2.0f * blep;
        
        output += sub * subLevel * kSubAmpScale;
    }
    
    // === 4. NOISE ===
    if (noiseLevel > 0.0f) {
        float noise = (noiseGen.nextFloat() * 2.0f - 1.0f);
        // [Fidelity] Noise color (Peaking filter at 4kHz)
        noise = noiseFilter.processSample(noise);
        output += noise * noiseLevel;
    }
    
    // [Fidelity] Output Gain restored
    return output;  
}
