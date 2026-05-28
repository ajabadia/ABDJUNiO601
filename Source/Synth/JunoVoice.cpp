#include <JuceHeader.h>
#include "JunoVoice.h"
#include <cmath>
#include <algorithm>
#include "../Core/SynthParams.h"
#include "../Core/JunoConstants.h"
#include "../Core/CalibrationSettings.h"
#include "J106VCATable.h"

using namespace JunoConstants;

static inline float getVCAMappedGain(float envVal, int curveType, const CalibrationSettings* cal)
{
    if (curveType == 2) {
        return envVal;
    }
    
    float idx = envVal * 255.f;
    int i0 = static_cast<int>(idx);
    i0 = std::clamp(i0, 0, 254);
    float frac = idx - i0;
    
    if (curveType == 1) {
        return kr106::kVCATable[(size_t)i0] + frac * (kr106::kVCATable[(size_t)i0 + 1] - kr106::kVCATable[(size_t)i0]);
    }
    
    // curveType == 0 (Default HW)
    if (cal != nullptr) {
        return cal->getVcaGain(i0) + frac * (cal->getVcaGain(i0 + 1) - cal->getVcaGain(i0));
    }
    return kr106::kVCATableHW[(size_t)i0] + frac * (kr106::kVCATableHW[(size_t)i0 + 1] - kr106::kVCATableHW[(size_t)i0]);
}

Voice::Voice() : ABD::VoiceBase() {
    lastOutputLevel = 0.0f;
}

void Voice::onPrepare() {
    int maxBlockSize = getBlockSize();
    dco.prepare(sr, maxBlockSize);
    voiceLFO.prepare(sr, maxBlockSize);
    voiceLFO.reset(); 
    
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sr;
    spec.maximumBlockSize = static_cast<juce::uint32>(maxBlockSize);
    spec.numChannels = 1;
    
    adsr.setSampleRate(sr);
    filter.setSampleRate(sr);
    filter.reset();
    smoothedVCALevel.reset(sr, 0.005);
    smoothedGate.reset(sr, 32.0f / (float)sr); // [Fidelity] 32-sample linear ramp (approx 0.7ms) to prevent clicks
    
    hpFilter.prepare(spec);
    hpFilter.reset();
    updateHPF();
    
    resCompFilter.prepare(spec);
    resCompFilter.reset();

    hpfShelfFilter.prepare(spec);
    hpfShelfFilter.reset(); 
    hpfShelfFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, 100.0f, 0.707f, 1.25f);
    
    noiseColorFilter.prepare(spec);
    noiseColorFilter.reset();
    noiseColorFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, 4000.0f, 0.707f, 0.707f); 

    smoothedCutoff.reset(sr, params.vcfSlewMs * 0.001);
    smoothedCutoff.setCurrentAndTargetValue(0.5f);
    smoothedResonance.reset(sr, 0.02);
    smoothedVCALevel.reset(sr, params.vcaSlewMs * 0.001);
    smoothedVCALevel.setCurrentAndTargetValue(0.5f);

    // [Fidelity] Staggered CV update: voice i sees its DAC write delayed by
    //   (i / kMaxVoices) × staggeredUpdateMaxMs
    // This replicates the Juno-106 MCU's round-robin multiplexed CV update pattern.
    // When staggeredUpdateMaxMs == 0, staggerDelaySamples is 0 (no delay).
    constexpr int kMaxVoices = 6;
    staggerDelaySamples = static_cast<int>(
        (static_cast<float>(voiceIndex) / kMaxVoices)
        * params.staggeredUpdateMaxMs * 0.001f * static_cast<float>(sr)
    );
    staggerCountdown    = 0; // Will be reset on first updateParams() call
    pendingCutoffTarget = params.vcfFreq;
    pendingVCATarget    = params.vcaLevel;
    
    tempBuffer.setSize(1, maxBlockSize);
}

void Voice::onNoteOn(int midiNote, float vel) {
    noteOn(midiNote, vel, false, currentUnisonCount);
}

void Voice::noteOn(int midiNote, float vel, bool isLegato, int numVoicesInUnison) {
    currentNote = midiNote;
    velocity = vel;
    isGateOn = true;
    lastOutputLevel = 1.0f;
    
    if (tuningTable) {
        targetFrequency = tuningTable[juce::jlimit(0, 127, midiNote)];
    } else {
        targetFrequency = 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
    }
    
    if (params.polyMode == 3 && numVoicesInUnison > 1) {
        float spreadAmt = params.unisonDetune * kUnisonDetuneMaxSemitones * params.unisonSpread; 
        float center = (numVoicesInUnison - 1) * 0.5f;
        float detuneSemitones = (voiceIndex - center) * spreadAmt;
        targetFrequency *= std::pow(2.0f, detuneSemitones / 12.0f);
    }

    bool runGlide = params.portamentoOn;
    if (params.portamentoLegato) runGlide = runGlide && isLegato;
    
    adsr.setAttackRaw(params.attack);
    adsr.setDecayRaw(params.decay);
    adsr.setSustain(std::max(params.sustain, 0.0001f));
    adsr.setReleaseRaw(params.release);
    
    float adsrVar = params.adsrVariance;
    float voiceOffset = (static_cast<float>(voiceIndex) - 2.5f) / 2.5f;
    adsr.setTimeScale(1.0f + adsrVar * voiceOffset);
    
    stealPending = false;
    
    if (!isLegato) {
        bool wasIdle = (currentNote == -1);
        adsr.reset(); 
        adsr.noteOn();
        dco.reset(); 
        if (wasIdle) {
            filter.reset();
            hpFilter.reset();
        }
    }
    
    if (!runGlide) currentFrequency = targetFrequency;
    dco.setFrequency(currentFrequency);
    smoothedGate.setTargetValue(1.0f);
}

void Voice::onNoteOff(float) {
    noteOff();
}

void Voice::onReset() {
    filter.reset();
    forceStop();
}

void Voice::forceStop() { 
    adsr.reset(); 
    currentNote = -1; 
    note_ = -1;
    isActive_ = false;
    lastOutputLevel = 0.0f; 
    currentFrequency = targetFrequency;
    smoothedGate.setCurrentAndTargetValue(0.0f);
}

void Voice::noteOff() {
    isGateOn = false;
    adsr.noteOff();
    smoothedGate.setTargetValue(0.0f);
    stealPending = false;
}

void Voice::prepareForStealing() {
    adsr.setRelease(0.003f);
    adsr.noteOff();
    smoothedGate.setTargetValue(0.0f);
    stealPending = true;
}

void Voice::updateParams(const SynthParams& p) {
    params = p;

    // [Fidelity] Staggered CV update: buffer VCF/VCA targets; they will be
    // committed to the smoothers after staggerDelaySamples have elapsed.
    // Resonance is not staggered (it shares the VCF DAC line but is less
    // timing-critical and not separately multiplexed on the hardware).
    if (params.staggeredUpdateMaxMs > 0.0f) {
        pendingCutoffTarget = params.vcfFreq;
        pendingVCATarget    = params.vcaLevel;
        // Reset countdown only if it has already expired, so each new param
        // update re-arms the delay from zero (mirrors MCU write scheduling).
        if (staggerCountdown <= 0)
            staggerCountdown = staggerDelaySamples;
    } else {
        // Bypass stagger entirely when parameter is zero.
        smoothedCutoff.setTargetValue(params.vcfFreq);
        smoothedVCALevel.setTargetValue(params.vcaLevel);
    }
    smoothedResonance.setTargetValue(params.resonance);
    
    dco.setRange(static_cast<JunoDCO::Range>(p.dcoRange));
    dco.setSawLevel(p.sawOn ? 1.0f : 0.0f); 
    dco.setPulseLevel(p.pulseOn ? 1.0f : 0.0f); 
    dco.setSubLevel(p.subOscLevel);
    dco.setNoiseLevel(p.noiseLevel);
    dco.setPWM(p.pwmAmount);
    dco.setPWMMode(static_cast<JunoDCO::PWMMode>(p.pwmMode));
    dco.setLFODepth(p.lfoToDCO);
    dco.setMasterClock(p.masterClockHz);
    
    // [Fidelity] VCF always uses ADSR (VCA mode switch only affects VCA)
    adsr.setAttackRaw(p.attack);
    adsr.setDecayRaw(p.decay);
    adsr.setSustain(p.sustain);
    adsr.setReleaseRaw(p.release);
    
    float adsrVar = p.adsrVariance;
    float voiceOffset = (static_cast<float>(voiceIndex) - 2.5f) / 2.5f;
    adsr.setTimeScale(1.0f + adsrVar * voiceOffset);
    
    // [Fidelity] Ensure ADSR is never in Gate mode for the VCF
    adsr.setGateMode(false);
    adsr.setSlewMs(p.adsrSlewMs);
    adsr.setAttackFactor(p.adsrAttackFactor);
    adsr.setMcuRate(p.adsrMcuRate);
    adsr.setDacSteps(p.adsrDacSteps);
    adsr.setOvershoot(p.adsrOvershoot);
    
    dco.setCalibration(calibrationPtr);
    dco.setMixerGain(p.dcoMixerGain);
    dco.setSubAmpScale(p.subAmpScale);
    dco.setPWMOffset(p.pwmOffset);
    dco.setNoiseGain(p.noiseGain);
    dco.setVoiceVariance(p.voiceVariance);
    dco.setGlobalDriftScale(p.dcoGlobalDrift);
    dco.setVoiceDriftScale(p.dcoVoiceDrift);
    dco.setDriftRate(p.dcoDriftRate);
    dco.setLfoPitchDepth(p.dcoLfoPitchDepth);
    dco.setPwmCalibration(p.pwmMinDuty, p.pwmMaxDuty, p.pwmCenterDuty, p.pwmOffThreshold);
    dco.setPwmSlew(p.pwmSlewRateManual, p.pwmSlewRateLFO);
    dco.setNoiseAmpScale(p.noiseAmpScale);
    
    voiceLFO.setCalibrationSettings(calibrationPtr);
    voiceLFO.setDepth(1.0f);
    voiceLFO.setDelay(p.lfoDelay * p.lfoDelayMax);
    updateHPF();
}

void Voice::updateHPF(int position) {
    int activePos = (position >= 0) ? position : params.hpfFreq;
    switch (activePos) {
        case 0: // Bass Boost mode: the main hpf is bypass, shelving is handled in render loop
        case 1: // Flat mode: bypass
            hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeAllPass(sr, 1000.0f); 
            break;
        case 2: hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, params.hpfFreq2, params.hpfQ); break;
        case 3: hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, params.hpfFreq3, params.hpfQ); break;
        default: hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeAllPass(sr, 1000.0f); break;
    }
}

void Voice::forceUpdate() {
    updateParams(params);
    smoothedCutoff.setCurrentAndTargetValue(params.vcfFreq);
    smoothedResonance.setCurrentAndTargetValue(params.resonance);
    smoothedVCALevel.setCurrentAndTargetValue(params.vcaLevel);
    filter.reset();
    hpFilter.reset();
}

void Voice::renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) {
    if (!lfoBufferPtr) return;
    renderNextBlock(buffer, startSample, numSamples, *lfoBufferPtr, currentNeighborCrosstalk, currentUnisonCount);
}

void Voice::renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, const std::vector<float>& lfoBuffer, float neighborCrosstalk, int numVoicesInUnison) {
    if (!(adsr.isActive() || lastOutputLevel > 0.0001f)) {
        isActive_ = false;
        return;
    }
    isActive_ = true;
    if (numSamples > tempBuffer.getNumSamples()) numSamples = tempBuffer.getNumSamples();
    
    updateHPF(params.hpfCyclePos);
    float bendedFrequency = updatePitch(numSamples);
    dco.setFrequency(bendedFrequency);
    
    float* voiceData = tempBuffer.getWritePointer(0);
    renderVoiceCycles(voiceData, numSamples, lfoBuffer, neighborCrosstalk);
    processFinalOutput(buffer, startSample, numSamples, voiceData, numVoicesInUnison);
}

float Voice::updatePitch(int numSamples) {
    if (params.portamentoOn && std::abs(currentFrequency - targetFrequency) > 0.1f) {
        // [Fidelity Bugfix C7] Linear glide in semitones using portamentoRateST calculated in PluginProcessor
        float currentST = 12.0f * std::log2(currentFrequency / 440.0f);
        float targetST = 12.0f * std::log2(targetFrequency / 440.0f);
        float diffST = targetST - currentST;
        float glideStep = (params.portamentoRateST / (float)sr) * numSamples;
        if (std::abs(diffST) > glideStep) {
            currentST += (diffST > 0.f) ? glideStep : -glideStep;
            currentFrequency = 440.0f * std::pow(2.0f, currentST / 12.0f);
        } else {
            currentFrequency = targetFrequency;
        }
    } else {
        currentFrequency = targetFrequency;
    }

    // [Fidelity] Update independent voice thermal drift
    // This creates the organic pitch 'wandering' over time
    thermalCounter -= numSamples;
    if (thermalCounter <= 0) {
        thermalCounter = (int)std::max(64.0f, params.thermalInertia);
        thermalTarget = (noiseGen.nextFloat() * 2.0f - 1.0f); // Pitch target wander
    }
    float migration = juce::jlimit(0.00001f, 0.1f, params.thermalMigration * (numSamples / 128.0f));
    thermalDrift += (thermalTarget - thermalDrift) * migration;
    
    float bendedFrequency = currentFrequency * std::pow(2.0f, (params.tune + params.masterPitchCents) / 1200.0f);
    if (params.a4Reference != 440.0f) bendedFrequency *= (params.a4Reference / 440.0f);

    if (params.benderValue != 0.0f && params.benderToDCO > 0.0f) {
        bendedFrequency *= std::pow(2.0f, params.benderValue * (params.benderToDCO * (float)params.benderRange / 12.0f));
    }

    float driftFactor = 1.0f - (params.polyMode == 3 ? params.unisonDetune : 0.0f);
    // Combine Global Drift param + Independent Voice Wander member
    float thermalAmount = (params.thermalDrift * 0.01f + thermalDrift) * 0.1f * driftFactor * params.thermalIntensity;
    bendedFrequency *= std::pow(2.0f, thermalAmount / 12.0f);
    return bendedFrequency;
}

void Voice::renderVoiceCycles(float* voiceData, int numSamples, const std::vector<float>& lfoBuffer, float neighborCrosstalk) {
    const float bleedLin = std::pow(10.0f, params.vcaBleed / 20.0f);

    // [Fidelity] Staggered CV update: commit pending targets when countdown expires.
    if (params.staggeredUpdateMaxMs > 0.0f) {
        if (staggerCountdown > 0) {
            staggerCountdown -= numSamples;
        }
        if (staggerCountdown <= 0) {
            smoothedCutoff.setTargetValue(pendingCutoffTarget);
            smoothedVCALevel.setTargetValue(pendingVCATarget);
        }
    }

    // Update LFO gate state at block level
    voiceLFO.updateGateState(isGateOn, stealPending);

    // Tick voiceLFO at tick rate (~234.2 Hz) inside the sample processing loop
    // TickPeriod = 4.2335ms
    float tickRateMs = 4.2335f;
    if (calibrationPtr != nullptr) {
        tickRateMs = calibrationPtr->getValue("lfoTickRateMs");
    }
    double tickRateHz = 1000.0 / (double)tickRateMs;
    double samplesPerTick = sr / tickRateHz;
    static double voiceLfoTimeAccumulator = 0.0;

    for (int i = 0; i < numSamples; ++i) {
        voiceLfoTimeAccumulator += 1.0;
        if (voiceLfoTimeAccumulator >= samplesPerTick) {
            voiceLfoTimeAccumulator -= samplesPerTick;
            voiceLFO.tick106();
        }
        
        float envVal = adsr.getNextSample();
        float voiceLfoValue = voiceLFO.process(params.lfoRate);
        
        // [Fidelity Build 101] Scaled Mixer with Sub & Noise multipliers
        dco.setSubLevel(params.subOscLevel * params.subGainScale);
        float dcoSample = dco.getNextSample(voiceLfoValue);
        float noiseSample = (noiseGen.nextFloat() * 2.0f - 1.0f) * params.noiseLevel * params.noiseGainScale;
        
        float rippleNoise = (noiseGen.nextFloat() - 0.5f) * params.vcaRippleDepth * envVal;
        
        // [Fidelity Build 101] Oscillators + Bleed + Crosstalk
        float signal = dcoSample + noiseSample + (bleedLin * 0.1f) + neighborCrosstalk * params.vcaCrosstalk + rippleNoise;
        
        int activeHpfPos = (params.hpfCyclePos >= 0) ? params.hpfCyclePos : params.hpfFreq;
        if (activeHpfPos == 0) {
            signal = hpfShelfFilter.processSample(signal); // Apply +3dB Bass Boost shelving
        } else {
            signal = hpFilter.processSample(signal); // Apply 225Hz/700Hz HPF or AllPass
        }
        
        float envMod = params.envAmount * envVal * params.vcfEnvRange;
        bool envInverted = (params.vcfPolarity == 1);
        float lfoVCF = params.lfoToVCF * params.vcfLfoDepth;
        
        signal = filter.processSample(signal, params.vcfFreq, params.resonance,
                                   params.envAmount * params.vcfEnvRange, envVal, envInverted,
                                   lfoVCF, voiceLfoValue,
                                   params.kybdTracking, currentFrequency,
                                   params.benderValue, params.benderToVCF,
                                   params.vcfSelfOscThreshold,
                                   params.vcfSaturation, params.vcfSelfOscInt,
                                   params.vcfWidth, calibrationPtr);
        
        juce::dsp::util::snapToZero(signal);
        
        // [Fidelity Bugfix] VCA Mode branching: 0 = ENV (ADSR), 1 = GATE (Steady)
        float vcaLevelNorm = std::max(smoothedVCALevel.getNextValue(), 0.0001f); 
        float vcaGain = vcaLevelNorm;

        if (params.vcaMode == 0) { // ENV (ADSR)
             float mappedEnv = getVCAMappedGain(envVal, static_cast<int>(params.vcaCurveType), calibrationPtr);
             vcaGain *= std::max(mappedEnv, 0.0001f);
        } else { // GATE (Steady while note is held)
             // Use smoothed gate to prevent digital clicks (2ms ramp)
             vcaGain *= std::max(smoothedGate.getNextValue(), 0.0001f);
        }
        
        float velScale = std::pow(1.0f - params.velocitySens + (params.velocitySens * velocity), params.vcaVelSensScale);
        vcaGain *= velScale * params.vcaMasterGain;
        
        // [Sprint 10 Fidelity] Accurate IR3109 Passband Loss Compensation.
        // Resonance in IR3109 reduces passband gain. We COMPENSATE (+) instead of SUBTRACT (-).
        float resComp = 1.0f + (params.resonance * params.resonance * params.vcfResoComp * params.vcfResoCompBoost);
        
        // Final Output [Build 101]: kVoiceOutputGain (1.5) is now moved to params.vcaMasterGain
        voiceData[i] = (signal + params.vcaDcOffset + params.vcaOffset) * vcaGain * resComp;
    }
}


void Voice::processFinalOutput(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, float* voiceData, int numVoicesInUnison) {
    float pan = (numVoicesInUnison > 1) ? ((voiceIndex / (float)(numVoicesInUnison - 1)) * 2.0f - 1.0f) : 0.0f;
    float gainL = std::sqrt(0.5f * (1.0f - pan * params.unisonStereoWidth));
    float gainR = std::sqrt(0.5f * (1.0f + pan * params.unisonStereoWidth));

    float currentBlockMax = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        float sample = voiceData[i];
        
        // [Fidelity Protection] Prevent buffer silencers (NaN/Inf)
        if (std::isnan(sample) || std::isinf(sample)) {
             sample = 0.0f;
             // juce::Logger::writeToLog("[JUNiO] Voice NaN detected!"); // Optional: overly spammy
        }
        
        sample = std::tanh(sample * 1.0f);
        if (std::abs(sample) > currentBlockMax) currentBlockMax = std::abs(sample);
        buffer.addSample(0, startSample + i, sample * gainL);
        if (buffer.getNumChannels() > 1) buffer.addSample(1, startSample + i, sample * gainR);
    }
    lastOutputLevel = currentBlockMax;
    if (!adsr.isActive() && lastOutputLevel < params.vcaKillThreshold) {
         currentNote = -1; note_ = -1; isActive_ = false; isGateOn = false;
    }
}

void Voice::setBender(float v) { params.benderValue = v; }
void Voice::setPortamentoEnabled(bool b) { params.portamentoOn = b; }
void Voice::setPortamentoTime(float v) { params.portamentoTime = v; }
void Voice::setPortamentoLegato(bool b) { params.portamentoLegato = b; }
