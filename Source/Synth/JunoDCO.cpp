// Source/Synth/JunoDCO.cpp
#include "JunoDCO.h"
#include <cmath>

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
    driftMigrator = 0.0f;
    driftTarget = 0.0f;
    driftCounter = 0;
    currentPWM = pwmValue;
    // [Fidelidad] Fase aleatoria para el Sub-Osc (Flip-flop del 8253 arranca en estado desconocido)
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
    // === ANALOG DRIFT (Random Walk) ===
    // [Fidelidad] Drift lento y sutil (Authentic: Temperature varies slowly, not 6ms jitter)
    // Update every ~0.1s (4410 samples @ 44.1k)
    if (++driftCounter > 4096) {
        driftCounter = 0;
        driftTarget = (noiseGen.nextFloat() * 2.0f - 1.0f); 
    }
    
    // Smoothly migrate towards target (Thermal Inertia)
    driftMigrator += (driftTarget - driftMigrator) * 0.001f;
    
    // Scale by user drift amount (Max 2 cents = 0.02 semitones)
    // Authentic Juno DCOs are very stable, minimal jitter.
    float driftSemitones = driftMigrator * driftAmount * 0.02f; 
    
    // === FREQUENCY with RANGE, LFO, and DRIFT ===
    float freq = baseFrequency * rangeMultiplier;
    
    // Apply LFO to pitch (vibrato)
    float lfoSemitones = (lfoValue * 0.5f + 0.5f) * lfoDepth * 0.5f; 
    freq *= std::pow(2.0f, (lfoSemitones + driftSemitones) / 12.0f);

    // [Fidelidad] 8253 TIMER QUANTIZATION (STRICT IMPL)
    // The Juno-106 DCO is driven by an Intel 8253 Programmable Interval Timer.
    // Master Clock = 8MHz. Divider = Freq * 256 (Prescaler/Integrator steps).
    // The counter is a 16-bit integer. This causes frequency stepping.
    
    static constexpr float kMasterClock = 8000000.0f; 

    if (freq > 0.0f) {
        // Calculate required ticks (Float)
        float rawTicks = kMasterClock / (freq * 256.0f);
        
        // Quantize to Integer (The Counter Register)
        // [Audit Fix] Strict integer casting/rounding
        uint32_t quantizedTicks = (uint32_t)(rawTicks + 0.5f);
        
        if (quantizedTicks < 1) quantizedTicks = 1;
        // 16-bit Timer Limit (though freq would be super low)
        if (quantizedTicks > 65535) quantizedTicks = 65535;
        
        // Recalculate EXACT Frequency driven by the Timer
        freq = kMasterClock / (quantizedTicks * 256.0f);
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
    // V9 Fix: Consolidate PolyBLEP for all waves (Kill metallic aliasing)
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
        float targetPWM = 0.5f;
        if (pwmMode == PWMMode::Manual) {
            // [Fidelidad] Offset de fábrica +7% para evitar ancho muerto en 50%
            targetPWM = 0.07f + pwmValue * 0.86f; 
        } else {
            targetPWM = juce::jlimit(0.05f, 0.95f, 0.5f + lfoValue * pwmValue * 0.45f);
        }
        
        // [Fidelidad] PWM Slew Calibrated
        // Manual: 220ms (0.0009f), LFO: 470ms (0.00047f)
        float slewRate = (pwmMode == PWMMode::Manual) ? 0.0009f : 0.00047f;
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
        // [Fidelidad] Sub-Osc is a square wave from 8253 divider.
        // It toggles at pulsePhase == 0.5 and remains continuous at pulsePhase == 0.0.
        // Thus, we only need PolyBLEP at the 0.5 threshold.
        float subThreshold = 0.5f; 
        float sub = (pulsePhase < subThreshold) == subFlipFlop ? 1.0f : -1.0f;
        
        // PolyBLEP at 0.5 transition
        float relativePhase = (float)pulsePhase - subThreshold;
        if (relativePhase < 0.0f) relativePhase += 1.0f;
        
        // The jump magnitude is 2.0 (1 to -1 or -1 to 1) 
        // If subFlipFlop is T: jumps 1->-1 at 0.5. If F: jumps -1->1 at 0.5.
        float blep = polyBlep(relativePhase, (float)dt);
        if (subFlipFlop) sub -= 2.0f * blep;
        else sub += 2.0f * blep;
        
        output += sub * subLevel;
    }
    
    // === 4. NOISE ===
    if (noiseLevel > 0.0f) {
        float noise = (noiseGen.nextFloat() * 2.0f - 1.0f);
        // [Fidelidad] Noise color (Peaking filter a 4kHz)
        noise = noiseFilter.processSample(noise);
        output += noise * noiseLevel;
    }
    
    // [Fidelidad] Output Gain restored (removed 0.5f factor)
    return output;  
}
