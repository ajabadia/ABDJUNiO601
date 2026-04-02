#include <JuceHeader.h>
#include "JunoVoice.h"
#include <cmath>
#include "../Core/SynthParams.h"
#include "../Core/JunoConstants.h"

using namespace JunoConstants;

Voice::Voice() : ABD::VoiceBase() {
    lastOutputLevel = 0.0f;
}

void Voice::onPrepare() {
    double sr = getSampleRate(); 
    int maxBlockSize = getBlockSize();
    sampleRate = sr;
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

    smoothedCutoff.reset(sr, 0.02);
    smoothedCutoff.setCurrentAndTargetValue(0.5f);
    smoothedResonance.reset(sr, 0.02);
    smoothedVCALevel.reset(sr, 0.02);
    smoothedVCALevel.setCurrentAndTargetValue(0.5f);
    
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
    
    adsr.setAttack(JunoConstants::curveMap(params.attack, Curves::kAttackMin, Curves::kAttackMax));
    adsr.setDecay(JunoConstants::curveMap(params.decay, Curves::kDecayMin, Curves::kDecayMax));
    adsr.setSustain(params.sustain);
    adsr.setRelease(JunoConstants::curveMap(params.release, Curves::kReleaseMin, Curves::kReleaseMax));
    adsr.setGateMode(params.vcaMode == 1);
    
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
}

void Voice::noteOff() {
    isGateOn = false;
    adsr.noteOff();
    stealPending = false;
}

void Voice::prepareForStealing() {
    adsr.setRelease(0.003f);
    adsr.noteOff();
    stealPending = true;
}

void Voice::updateParams(const SynthParams& p) {
    params = p;
    smoothedCutoff.setTargetValue(params.vcfFreq);
    smoothedResonance.setTargetValue(params.resonance);
    smoothedVCALevel.setTargetValue(params.vcaLevel);
    
    dco.setRange(static_cast<JunoDCO::Range>(p.dcoRange));
    dco.setSawLevel(p.sawOn ? 1.0f : 0.0f); 
    dco.setPulseLevel(p.pulseOn ? 1.0f : 0.0f); 
    dco.setSubLevel(p.subOscLevel);
    dco.setNoiseLevel(p.noiseLevel);
    dco.setPWM(p.pwmAmount);
    dco.setPWMMode(static_cast<JunoDCO::PWMMode>(p.pwmMode));
    dco.setLFODepth(p.lfoToDCO);
    dco.setMasterClock(p.masterClockHz);
    
    adsr.setAttack(JunoConstants::curveMap(p.attack, Curves::kAttackMin, Curves::kAttackMax));
    adsr.setDecay(JunoConstants::curveMap(p.decay, Curves::kDecayMin, Curves::kDecayMax));
    adsr.setSustain(p.sustain);
    adsr.setRelease(JunoConstants::curveMap(p.release, Curves::kReleaseMin, Curves::kReleaseMax));
    adsr.setGateMode(p.vcaMode == 1);
    adsr.setSlewMs(p.adsrSlewMs);
    
    dco.setMixerGain(p.dcoMixerGain);
    dco.setSubAmpScale(p.subAmpScale);
    dco.setPWMOffset(p.pwmOffset);
    dco.setNoiseGain(p.noiseGain);
    dco.setVoiceVariance(p.voiceVariance);
    dco.setGlobalDriftScale(p.dcoGlobalDrift);
    dco.setVoiceDriftScale(p.dcoVoiceDrift);
    dco.setDriftRate(p.dcoDriftRate);
    dco.setLfoPitchDepth(p.dcoLfoPitchDepth);
    
    voiceLFO.setDelayCurve(p.lfoDelayCurve);
    updateHPF();
}

void Voice::updateHPF(int position) {
    int activePos = (position >= 0) ? position : params.hpfFreq;
    switch (activePos) {
        case 0: hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, params.hpfShelfFreq, params.hpfQ, std::pow(10.0f, params.hpfShelfGain / 20.0f)); break;
        case 1: hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeAllPass(sampleRate, 1000.0f); break;
        case 2: hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, params.hpfFreq2, params.hpfQ); break;
        case 3: hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, params.hpfFreq3, params.hpfQ); break;
        default: hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeAllPass(sampleRate, 1000.0f); break;
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
        float glideTime = 0.005f * std::pow(400.0f, params.portamentoTime);
        float glideCoeff = 1.0f - std::exp(-static_cast<float>(numSamples) / (juce::jmax(0.001f, glideTime) * static_cast<float>(sampleRate)));
        currentFrequency += (targetFrequency - currentFrequency) * glideCoeff;
    } else {
        currentFrequency = targetFrequency;
    }
    
    float bendedFrequency = currentFrequency * std::pow(2.0f, (params.tune + params.masterPitchCents) / 1200.0f);
    if (params.a4Reference != 440.0f) bendedFrequency *= (params.a4Reference / 440.0f);

    if (params.benderValue != 0.0f && params.benderToDCO > 0.0f) {
        bendedFrequency *= std::pow(2.0f, params.benderValue * (params.benderToDCO * (float)params.benderRange / 12.0f));
    }

    float driftFactor = 1.0f - (params.polyMode == 3 ? params.unisonDetune : 0.0f);
    bendedFrequency *= std::pow(2.0f, (params.thermalDrift * 0.1f * driftFactor) / 12.0f);
    return bendedFrequency;
}

void Voice::renderVoiceCycles(float* voiceData, int numSamples, const std::vector<float>& lfoBuffer, float neighborCrosstalk) {
    for (int i = 0; i < numSamples; ++i) {
        float envVal = adsr.getNextSample();
        float voiceLfoValue = lfoBuffer[i];
        
        float dcoSample = dco.getNextSample(voiceLfoValue);
        float rippleNoise = (noiseGen.nextFloat() - 0.5f) * params.vcaRippleDepth * envVal;
        float signal = dcoSample + neighborCrosstalk * params.vcaCrosstalk + rippleNoise;
        
        int activeHpfPos = (params.hpfCyclePos >= 0) ? params.hpfCyclePos : params.hpfFreq;
        signal = hpFilter.processSample(signal); 
        if (activeHpfPos == 0) signal = hpfShelfFilter.processSample(signal);
        
        float envMod = params.envAmount * (envVal - 0.5f) * params.vcfEnvRange;
        if (params.vcfPolarity == 1) envMod = -envMod;
        float lfoVCF = params.lfoToVCF * voiceLfoValue * params.vcfLfoDepth;
        
        signal = filter.processSample(signal, params.vcfFreq, params.resonance, envMod, lfoVCF, params.kybdTracking,
                                   currentFrequency, params.vcfMinHz, params.vcfMaxHz, params.vcfSelfOscThreshold,
                                   params.vcfSaturation, params.vcfSelfOscInt, params.vcfTrackCenter,
                                   params.vcfWidth);
        
        juce::dsp::util::snapToZero(signal);
        
        float vcaGain = (params.vcaMode == 1) ? (smoothedVCALevel.getNextValue() * envVal) : (envVal * smoothedVCALevel.getNextValue());
        float velScale = std::pow(1.0f - params.velocitySens + (params.velocitySens * velocity), params.vcaVelSensScale);
        vcaGain *= velScale * params.vcaMasterGain;
        
        float resComp = 1.0f + (params.resonance * params.resonance * params.vcfResoComp);
        voiceData[i] = (signal + params.vcaDcOffset + params.vcaOffset) * vcaGain * resComp * kVoiceOutputGain;
    }
}

void Voice::processFinalOutput(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, float* voiceData, int numVoicesInUnison) {
    float pan = (numVoicesInUnison > 1) ? ((voiceIndex / (float)(numVoicesInUnison - 1)) * 2.0f - 1.0f) : 0.0f;
    float gainL = std::sqrt(0.5f * (1.0f - pan * params.unisonStereoWidth));
    float gainR = std::sqrt(0.5f * (1.0f + pan * params.unisonStereoWidth));

    float currentBlockMax = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        float sample = voiceData[i];
        if (std::isnan(sample) || std::isinf(sample)) sample = 0.0f;
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
