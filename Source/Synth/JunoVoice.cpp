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
    
    hpf.prepare((float)sr);
    hpf.setPosition(params.hpfFreq, params.hpfFreq1, params.hpfFreq2, params.hpfFreq3, params.hpfBassBoostGain);

    resCompFilter.prepare(spec);
    resCompFilter.reset();

    noiseColorFilter.prepare(spec);
    noiseColorFilter.reset();
    noiseColorFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, 4000.0f, 0.707f, 0.707f); 

    smoothedCutoff.reset(sr, params.vcfSlewMs * 0.001);
    smoothedCutoff.setCurrentAndTargetValue(0.5f);
    smoothedResonance.reset(sr, 0.02);
    smoothedVCALevel.reset(sr, params.vcaSlewMs * 0.001);
    smoothedVCALevel.setCurrentAndTargetValue(0.5f);

    mVcfPhase = 0.0f;
    mVcaPhase = 0.0f;
    mFwTickAccum = 0.0f;
    mVcfDacUpdated = false;
    mVcaDacUpdated = false;
    
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
    
    if (GET_MODEL_UNISON(params) == 2 && params.polyMode == 3 && numVoicesInUnison > 1) {
        float spreadAmt = params.unisonDetune * kUnisonDetuneMaxSemitones * params.unisonSpread; 
        float center = (numVoicesInUnison - 1) * 0.5f;
        float detuneSemitones = (voiceIndex - center) * spreadAmt;
        targetFrequency *= std::pow(2.0f, detuneSemitones / 12.0f);
    }

    bool runGlide = params.portamentoOn && (GET_MODEL_PORTA(params) == 2);
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
            hpf.reset();
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

    // Compute dynamic voice variance offsets from LCG seed
    uint32_t seed = static_cast<uint32_t>(voiceIndex) * 2654435761u + 0x46756E6Bu;
    auto rng = [&seed]() -> float {
        seed = seed * 196314165u + 907633515u;
        return static_cast<float>(seed) / static_cast<float>(0xFFFFFFFF) * 2.0f - 1.0f;
    };
    float uFrq = rng();
    float uWidth = rng();
    float uGain = rng();

    mVcfFrqTrim = uFrq * p.voiceVcfFrqSpread;
    mVcfWidthTrim = 1.0f + uWidth * (p.voiceVcfWidthSpread / 1200.0f);
    mVcaGainScale = 1.0f + uGain * p.voiceVcaGainSpread;

    // [Fidelity] Staggered CV update: compute phase offsets based on voice slot
    if (params.staggeredUpdateMaxMs == 0.0f) {
        mVcfPhase = 0.0f;
        mVcaPhase = 0.0f;
        mVcfDacPending = params.vcfFreq;
        mVcaDacPending = params.vcaLevel;
        mVcfDacNext = params.vcfFreq;
        mVcaDacNext = params.vcaLevel;
    } else {
        int slot = voiceIndex % 6;
        float tickPeriodMs = std::max(0.5f, params.adsrMcuRate);
        float staggerScale = params.staggeredUpdateMaxMs / 1.4371f;
        float vcfDelayMs = slot * 0.2874f * staggerScale;
        float vcaDelayMs = (slot * 0.2874f + 0.1253f) * staggerScale;

        mVcfPhase = juce::jlimit(0.0f, 1.0f, vcfDelayMs / tickPeriodMs);
        mVcaPhase = juce::jlimit(0.0f, 1.0f, vcaDelayMs / tickPeriodMs);
        
        mVcfDacPending = params.vcfFreq;
        mVcaDacPending = params.vcaLevel;
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
    int adsrModel = GET_MODEL_ADSR(p);
    if (adsrModel == 0) {
        adsr.setMode(ADSRMode::kJ6);
    } else if (adsrModel == 1) {
        adsr.setMode(ADSRMode::kJ60);
    } else {
        adsr.setMode(ADSRMode::kJ106);
    }
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

    // VCF Model and Resonance Curve
    int vcfModel = GET_MODEL_VCF(p);
    if (vcfModel == 0 || vcfModel == 1) { // J6 or J60
        filter.setModelAndResCurve(false, true); // J6/60 ResK curve, OTA Saturation on
    } else {
        filter.setModelAndResCurve(true, true); // J106 ResK curve
    }
    filter.setOversample(static_cast<int>(p.oversampling));
    
    dco.setCalibration(calibrationPtr);
    dco.setMixerGain(p.dcoMixerGain);
    dco.setPWMOffset(p.pwmOffset);
    dco.setNoiseGain(p.noiseGain);
    dco.setVoiceVariance(p.voiceVariance);
    dco.setGlobalDriftScale(p.dcoGlobalDrift);
    dco.setVoiceDriftScale(p.dcoVoiceDrift);
    dco.setDriftRate(p.dcoDriftRate);
    dco.setLfoPitchDepth(p.dcoLfoPitchDepth);
    dco.setPwmCalibration(p.pwmMinDuty, p.pwmMaxDuty, p.pwmCenterDuty, p.pwmOffThreshold);
    dco.setPwmSlew(p.pwmSlewRateManual, p.pwmSlewRateLFO);
    dco.setNoiseGainScale(p.noiseGainScale);
    
    voiceLFO.setCalibrationSettings(calibrationPtr);
    voiceLFO.setDepth(1.0f);
    voiceLFO.setDelay(p.lfoDelay * p.lfoDelayMax);
    updateHPF();
}

void Voice::updateHPF(int position) {
    int activePos = (position >= 0) ? position : params.hpfFreq;
    int hpfModel = GET_MODEL_HPF(params);
    if (hpfModel == 0) {
        hpf.setMode(HPFMode::J6Continuous);
    } else if (hpfModel == 1) {
        hpf.setMode(HPFMode::J60);
    } else {
        hpf.setMode(HPFMode::J106);
    }
    hpf.setPosition(activePos, params.hpfFreq1, params.hpfFreq2, params.hpfFreq3, params.hpfBassBoostGain);
}

void Voice::forceUpdate() {
    updateParams(params);
    smoothedCutoff.setCurrentAndTargetValue(params.vcfFreq);
    smoothedResonance.setCurrentAndTargetValue(params.resonance);
    smoothedVCALevel.setCurrentAndTargetValue(params.vcaLevel);
    mVcfDacPending = params.vcfFreq;
    mVcfDacNext = params.vcfFreq;
    mVcaDacPending = params.vcaLevel;
    mVcaDacNext = params.vcaLevel;
    filter.reset();
    hpf.reset();
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
    float targetPitch = 12.0f * std::log2(targetFrequency / 440.0f);
    float currentPitch = 12.0f * std::log2(currentFrequency / 440.0f);
    
    if (params.portamentoOn && (GET_MODEL_PORTA(params) == 2) && std::abs(currentFrequency - targetFrequency) > 0.1f) {
        float t = params.portamentoTime; // 0..1 slider
        float i_idx = t * 127.0f;
        float coeff = 0.0f;
        if (i_idx > 0.0f) {
            if (i_idx <= 25.0f) {
                coeff = 255.0f - 8.0f * (i_idx - 1.0f);
            } else if (i_idx <= 47.0f) {
                coeff = 63.0f - 2.0f * (i_idx - 25.0f);
            } else {
                coeff = std::round(18.0f * std::pow(0.9625f, i_idx - 48.0f));
            }
        }
        
        float tickRate = 1000.0f / std::max(0.5f, params.adsrMcuRate);
        float semiPerSec = coeff * tickRate / 256.0f;
        float glideStep = (semiPerSec / (float)sr) * numSamples;
        
        float diff = targetPitch - currentPitch;
        if (glideStep > 0.0f && std::abs(diff) > glideStep) {
            currentPitch += (diff > 0.0f) ? glideStep : -glideStep;
            currentFrequency = 440.0f * std::pow(2.0f, currentPitch / 12.0f);
        } else {
            currentFrequency = targetFrequency;
        }
    } else {
        currentFrequency = targetFrequency;
    }

    // Update drift walk LFOs
    float dtSeconds = (float)numSamples / (float)sr;
    constexpr float kWalkRateHz[3] = { 0.07f, 0.13f, 0.31f };
    float walkSum = 0.0f;
    for (int i = 0; i < 3; ++i) {
        mWalkPhase[i] += juce::MathConstants<float>::twoPi * kWalkRateHz[i] * dtSeconds;
        if (mWalkPhase[i] > juce::MathConstants<float>::twoPi) {
            mWalkPhase[i] -= juce::MathConstants<float>::twoPi;
        }
        walkSum += std::sin(mWalkPhase[i]);
    }
    
    // Scale LFOs to driftWalkIntensity cents (driftAmt is global thermalDrift * intensity)
    float driftAmt = params.thermalDrift * 0.01f * params.thermalIntensity;
    mWalkValue = (walkSum / 3.0f) * driftAmt * params.driftWalkIntensity;
    
    float staticCents = mStaticOffsetUnit * driftAmt * 12.0f; // static spread is ±12 cents at max
    float totalDriftCents = staticCents + mWalkValue;
    
    float bendedFrequency = currentFrequency * std::pow(2.0f, (params.tune + params.masterPitchCents) / 1200.0f);
    if (params.a4Reference != 440.0f) bendedFrequency *= (params.a4Reference / 440.0f);

    if (params.benderValue != 0.0f && params.benderToDCO > 0.0f) {
        bendedFrequency *= std::pow(2.0f, params.benderValue * (params.benderToDCO * (float)params.benderRange / 12.0f));
    }

    float driftFactor = 1.0f - (params.polyMode == 3 ? params.unisonDetune : 0.0f);
    float thermalAmount = totalDriftCents * driftFactor;
    bendedFrequency *= std::pow(2.0f, thermalAmount / 1200.0f); // bendedFrequency is scaled by cents
    return bendedFrequency;
}

void Voice::renderVoiceCycles(float* voiceData, int numSamples, const std::vector<float>& lfoBuffer, float neighborCrosstalk) {
    juce::ignoreUnused(lfoBuffer);
    const float bleedLin = std::pow(10.0f, params.vcaBleed / 20.0f);

    float tickRateMs = std::max(0.5f, params.adsrMcuRate);
    double tickRateHz = 1000.0 / (double)tickRateMs;
    float tickStep = (float)(tickRateHz / sr);

    // Update LFO gate state at block level
    voiceLFO.updateGateState(isGateOn, stealPending);

    // Tick voiceLFO at tick rate (~234.2 Hz) inside the sample processing loop
    double samplesPerTick = sr / tickRateHz;
    static double voiceLfoTimeAccumulator = 0.0;

    for (int i = 0; i < numSamples; ++i) {
        // [Fidelity] Staggered CV Update logic
        mFwTickAccum += tickStep;
        if (mFwTickAccum >= 1.0f) {
            mFwTickAccum -= 1.0f;
            mVcfDacUpdated = false;
            mVcaDacUpdated = false;
        }

        if (!mVcfDacUpdated && mFwTickAccum >= mVcfPhase) {
            mVcfDacNext = mVcfDacPending;
            mVcfDacUpdated = true;
            smoothedCutoff.setTargetValue(mVcfDacNext);
        }

        if (!mVcaDacUpdated && mFwTickAccum >= mVcaPhase) {
            mVcaDacNext = mVcaDacPending;
            mVcaDacUpdated = true;
            smoothedVCALevel.setTargetValue(mVcaDacNext);
        }

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
        
        float rippleNoise = (noiseGen.nextFloat() - 0.5f) * params.vcaRippleDepth * envVal;
        
        // [Fidelity Build 101] Oscillators + Bleed + Crosstalk
        float signal = dcoSample + (bleedLin * 0.1f) + neighborCrosstalk * params.vcaCrosstalk + rippleNoise;
        
        // HPF: hardware-accurate (pos 0=BassBoost, 1=Flat, 2=236Hz, 3=754Hz)
        signal = hpf.process(signal);
        
        float envMod = params.envAmount * envVal * params.vcfEnvRange;
        bool envInverted = (params.vcfPolarity == 1);
        float lfoVCF = params.lfoToVCF * params.vcfLfoDepth;
        
        float activeCutoff = smoothedCutoff.getNextValue();
        signal = filter.processSample(signal, activeCutoff, params.resonance,
                                   envMod, envVal, envInverted,
                                   lfoVCF, voiceLfoValue,
                                   params.kybdTracking, currentFrequency,
                                   params.benderValue, params.benderToVCF,
                                   params.vcfSelfOscThreshold,
                                   params.vcfSaturation, params.vcfSelfOscInt,
                                   params.vcfWidth * mVcfWidthTrim, mVcfFrqTrim, calibrationPtr);
        
        juce::dsp::util::snapToZero(signal);
        
        // Update VCA CV slew (runs at audio rate)
        float vcaRaw = (params.vcaMode == 1) ? (isGateOn ? 1.0f : 0.0f) : envVal;
        float vcaSlewCoeff = 1.0f - std::exp(-1.0f / (params.vcaSlewMs * 0.001f * (float)sr));
        mVcaSlew += vcaSlewCoeff * (vcaRaw - mVcaSlew);

        // VCA Mode branching: exponential mapping of slewed CV
        float vcaLevelNorm = std::max(smoothedVCALevel.getNextValue(), 0.0001f); 
        float mappedVca = getVCAMappedGain(mVcaSlew, static_cast<int>(params.vcaCurveType), calibrationPtr);
        float vcaGain = vcaLevelNorm * std::max(mappedVca, 0.0001f);
        
        float velScale = std::pow(1.0f - params.velocitySens + (params.velocitySens * velocity), params.vcaVelSensScale);
        vcaGain *= velScale * params.vcaMasterGain * mVcaGainScale;
        
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

void Voice::setVoiceIndex(int i) {
    voiceIndex = i;
    initVoiceVariance();
}

void Voice::initVoiceVariance() {
    uint32_t seed = static_cast<uint32_t>(voiceIndex) * 2654435761u + 0x46756E6Bu;
    auto rng = [&seed]() -> float {
        seed = seed * 196314165u + 907633515u;
        return static_cast<float>(seed) / static_cast<float>(0xFFFFFFFF) * 2.0f - 1.0f;
    };
    rng(); // skip uFrq
    rng(); // skip uWidth
    rng(); // skip uGain
    mStaticOffsetUnit = rng() + rng() + rng();
    for (int i = 0; i < 3; ++i) {
        mWalkPhase[i] = (rng() + 1.0f) * juce::MathConstants<float>::pi;
    }
}
