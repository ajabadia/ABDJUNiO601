#include <JuceHeader.h>
#include "JunoDCO.h"
#include "../Core/JunoConstants.h"
#include <cmath>

using namespace JunoConstants;

JunoDCO::JunoDCO() {
    masterClockHz = 8000000.0f;
    updateRangeMultiplier();
    reset();
}

void JunoDCO::prepare(double sr, int maxBlockSize) {
    sampleRate = sr;
    
    juce::dsp::ProcessSpec spec { sr, (juce::uint32)maxBlockSize, 1 };
    noiseFilter.prepare(spec);
    noiseFilter.reset(); 
    noiseFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, 12000.0f); // Fast analog-style roll-off
    reset();
}

void JunoDCO::reset() {
    pulsePhase = 0.0;
    // [Build 29] Use calibrated voice variance instead of hardcoded kDcoDriftMaxSpreadCents
    staticSpreadCents = (noiseGen.nextFloat() * 2.0f - 1.0f) * voiceVariance;
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

void JunoDCO::setDrift(float amount) { driftAmount = amount; }
void JunoDCO::setMixerGain(float gain) { mixerGain = gain; }
void JunoDCO::setSubAmpScale(float scale) { subAmpScale = scale; }

float JunoDCO::getNextSample(float lfoValue) {
    // === ANALOG DRIFT (Multi-level authenticity) ===
    // [Fidelity] Independent levels: Fixed spread, Global slow drift, Per-voice slow drift
    
    // 2. Slow voice drift
    voicePhase += juce::MathConstants<float>::twoPi * voiceRate / (float)sampleRate;
    if (voicePhase > juce::MathConstants<float>::twoPi) voicePhase -= juce::MathConstants<float>::twoPi;
    
    voiceDriftCents = std::sin(voicePhase) * (voiceDriftScale * 0.5f) * driftAmount;
    
    // 3. (Global drift for this voice would be set externally or simulated here)
    // For simplicity, we'll simulate a slow global drift (rate calibrated)
    globalDriftPhase += juce::MathConstants<float>::twoPi * driftRate / (float)sampleRate;
    if (globalDriftPhase > juce::MathConstants<float>::twoPi) globalDriftPhase -= juce::MathConstants<float>::twoPi;
    globalDriftCents = std::sin(globalDriftPhase) * (globalDriftScale * 0.5f) * driftAmount;

    float totalDriftCents = staticSpreadCents * driftAmount + globalDriftCents + voiceDriftCents;
    
    // === FREQUENCY with RANGE, LFO, and DRIFT ===
    float freq = baseFrequency * rangeMultiplier;
    
    // [Fidelity] LFO to Pitch Sensitivity (Calibrated)
    float lfoSemitones = lfoValue * lfoDepth * dcoLfoPitchDepth; 
    freq *= std::pow(2.0f, (lfoSemitones + (totalDriftCents / 100.0f)) / 12.0f);

    // [Fidelity] 8253 TIMER QUANTIZATION (STRICT IMPL)
    // The Juno-106 DCO is driven by an Intel 8253 Programmable Interval Timer.
    // Master Clock = 8MHz. Divider = Freq * 256 (Prescaler/Integrator steps).
    // The counter is a 16-bit integer. This causes frequency stepping.
    
    if (freq > 0.0f) {
        // [Audit Compliance] Intel 8253 Timer Emulation (Juno-106)
        // Master Clock: 8MHz. Pre-dividers (Range): 16'=1/8, 8'=1/4, 4'=1/2.
        // Clock entering 16-bit 8253 at 8': 2.0MHz.
        float timerClock = masterClockHz / (4.0f / rangeMultiplier);
        
        // Calculate required ticks relative to the hardware clock for this range
        float rawTicks = timerClock / (freq);
        
        // Quantize to 16-bit Integer Counter
        uint32_t quantizedTicks = (uint32_t)(rawTicks + 0.5f);
        if (quantizedTicks < 1) quantizedTicks = 1;
        if (quantizedTicks > 65535) quantizedTicks = 65535;
        
        // The resulting frequency is determined by the integer divider
        freq = timerClock / (float)quantizedTicks;
    }
    
    // DCO phase update happens below using current freq.
    
    // === UPDATE PHASE ===
    if (sampleRate <= 0.0) return 0.0f;
    
    double dt = freq / sampleRate;
    pulsePhase += dt;
    
    if (pulsePhase >= 1.0) {
        pulsePhase -= 1.0;
        subFlipFlop = !subFlipFlop; // Always toggle (Authentic Aliasing/Divider behavior)
    }
    
    // Switch Ramp TC 1.45ms: coeff = 1 - exp(-1 / (0.00145 * sampleRate))
    float switchRampTime = 0.00145f;
    if (cal != nullptr) {
        float msVal = cal->getValue("oscSwitchRampMs");
        if (msVal > 0.0f) {
            switchRampTime = msVal * 0.001f;
        }
    }
    mSwitchRamp = 1.0f - std::exp(-1.0f / (switchRampTime * (float)sampleRate));

    mSawGain   += (((sawLevel > 0.0f)   ? 1.0f : 0.0f) - mSawGain)   * mSwitchRamp;
    mPulseGain += (((pulseLevel > 0.0f) ? 1.0f : 0.0f) - mPulseGain) * mSwitchRamp;
    mSubGain   += (((subLevel > 0.0f)   ? 1.0f : 0.0f) - mSubGain)   * mSwitchRamp;

    // PolyBLEP 4º orden (from KR106Oscillators.h)
    auto polyBlep4 = [](float t, float dt_param) -> float {
        float dt2 = dt_param + dt_param;
        if (t < dt_param) {
            float n = t / dt_param;
            float n2 = n * n;
            return 0.25f * n2 * n2 - 0.66666667f * n2 * n + 1.33333333f * n - 1.0f;
        } else if (t < dt2) {
            float u = 1.0f - (t - dt_param) / dt_param;
            float u2 = u * u;
            return -0.08333333f * u2 * u2;
        } else if (t > 1.0f - dt_param) {
            float n = (t - 1.0f) / dt_param;
            float n2 = n * n;
            return -0.25f * n2 * n2 - 0.66666667f * n2 * n + 1.33333333f * n + 1.0f;
        } else if (t > 1.0f - dt2) {
            float u = 1.0f + (t - 1.0f + dt_param) / dt_param;
            float u2 = u * u;
            return 0.08333333f * u2 * u2;
        }
        return 0.0f;
    };

    const float blepAtReset = polyBlep4((float)pulsePhase, (float)dt);

    float output = 0.0f;

    // 1. SAW
    if (mSawGain > 1e-4f) {
        float sawAmp = (cal != nullptr) ? cal->getValue("sawMixAmp") : 0.6f;
        float saw = 2.0f * (float)pulsePhase - 1.0f;
        saw -= blepAtReset; // step at reset is 2.0 (saw -= blepAtReset, NOT +=)
        output += saw * sawAmp * mSawGain;
    }
    
    // 2. PULSE
    if (mPulseGain > 1e-4f) {
        float pulseAmp = (cal != nullptr) ? cal->getValue("pulseMixAmp") : 0.5f;
        float targetPWM = pwmCenterDuty;
        if (pwmMode == PWMMode::Manual) {
            targetPWM = pwmCenterDuty + pwmOffset + (pwmValue - 0.5f) * 2.0f * (pwmMaxDuty - pwmCenterDuty);
        } else {
            targetPWM = juce::jlimit(pwmMinDuty, pwmMaxDuty, pwmCenterDuty + pwmOffset + lfoValue * pwmValue * 0.45f);
        }
        
        if (targetPWM > (1.0f - pwmOffThreshold)) targetPWM = 1.0f;
        if (targetPWM < pwmOffThreshold) targetPWM = 0.0f;
        
        float slewRate = (pwmMode == PWMMode::Manual) ? pwmSlewRateManual : pwmSlewRateLFO;
        currentPWM += (targetPWM - currentPWM) * slewRate;

        // J106: pulse width invert
        float effPW = currentPWM;
        effPW = 1.0f - effPW;
        effPW = std::clamp(effPW, 0.01f, 0.99f);

        float pulse = (pulsePhase < effPW) ? -1.0f : 1.0f;
        pulse -= blepAtReset;
        float pw2 = (float)pulsePhase - effPW;
        if (pw2 < 0.0f) pw2 += 1.0f;
        pulse += polyBlep4(pw2, (float)dt);

        output += pulse * pulseAmp * mPulseGain;
    }
    
    // 3. SUB
    if (mSubGain > 1e-4f) {
        float subAmp = (cal != nullptr) ? cal->getValue("subMixAmp") : 0.75f;
        float subLevelVal = subLevel;
        // Interpolación en tabla de 11 puntos
        if (cal != nullptr) {
            float subIdx = subLevel * 10.0f;
            int subI0 = static_cast<int>(subIdx);
            subI0 = std::clamp(subI0, 0, 9);
            float subFrac = subIdx - subI0;
            subLevelVal = cal->getSubLevel(subI0) + subFrac * (cal->getSubLevel(subI0 + 1) - cal->getSubLevel(subI0));
        }

        const float subSign = subFlipFlop ? -1.0f : 1.0f;
        float sub = subSign * (1.0f - std::abs(blepAtReset));
        output += sub * subAmp * subLevelVal * mSubGain;
    }
    
    // 4. NOISE
    if (noiseLevel > 0.0f) {
        float noiseAmp = (cal != nullptr) ? cal->getValue("noiseMixAmp") : 1.2f;
        float noise = (noiseGen.nextFloat() * 2.0f - 1.0f);
        noise = noiseFilter.processSample(noise);

        // AudioTaper for Noise: (exp(k*x)-1)/(exp(k)-1)
        float taperScale = 3.0f;
        if (cal != nullptr) {
            taperScale = cal->getValue("audioTaperScale");
        }
        float expTaper = (std::exp(taperScale * noiseLevel) - 1.0f) / (std::exp(taperScale) - 1.0f);

        output += noise * expTaper * noiseAmp * noiseAmpScale * noiseGain;
    }
    
    // [Fidelity] Output Gain scaled to prevent VCF saturation
    return output * mixerGain;
}
