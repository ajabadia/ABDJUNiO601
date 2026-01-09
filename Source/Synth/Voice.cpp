#include "Voice.h"
#include <cmath>
#include "../Core/SynthParams.h"

Voice::Voice() {
    filter.setMode(juce::dsp::LadderFilterMode::LPF24);
}

void Voice::prepare(double sr, int maxBlockSize) {
    sampleRate = sr;
    dco.prepare(sr, maxBlockSize);
    
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sr;
    spec.maximumBlockSize = static_cast<juce::uint32>(maxBlockSize);
    spec.numChannels = 1;
    
    adsr.setSampleRate(sr);
    filter.prepare(spec);
    filter.reset();
    
    hpFilter.prepare(spec);
    hpFilter.reset();
    updateHPF();
    
    bassBoostFilter.prepare(spec);
    bassBoostFilter.reset();

    smoothedCutoff.reset(sr, 0.02);
    smoothedResonance.reset(sr, 0.02);
    smoothedVCALevel.reset(sr, 0.02);
    
    tempBuffer.setSize(1, maxBlockSize);
}

void Voice::noteOn(int midiNote, float vel, bool isLegato) {
    currentNote = midiNote;
    velocity = vel;
    isGateOn = true;
    releaseCounter = 0; 
    lastOutputLevel = 1.0f;
    
    targetFrequency = 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
    bool shouldGlide = params.portamentoOn && isLegato;
    
    adsr.setSampleRate(sampleRate);
    adsr.setAttack(JunoTimeCurves::kAttackMin + (JunoTimeCurves::kAttackMax - JunoTimeCurves::kAttackMin) * std::pow(params.attack, 3.0f));
    adsr.setDecay(JunoTimeCurves::kDecayMin + (JunoTimeCurves::kDecayMax - JunoTimeCurves::kDecayMin) * std::pow(params.decay, 3.0f));
    adsr.setSustain(params.sustain);
    adsr.setRelease(JunoTimeCurves::kReleaseMin + (JunoTimeCurves::kReleaseMax - JunoTimeCurves::kReleaseMin) * std::pow(params.release, 3.0f));
    
    if (!isLegato) {
        adsr.noteOn();
        lastModOctaves = 0.0f;
        dco.reset(); 
    }
    
    if (!shouldGlide) currentFrequency = targetFrequency;
    dco.setFrequency(currentFrequency);
}

void Voice::noteOff() {
    isGateOn = false;
    releaseCounter = 0;
    adsr.noteOff();
}

void Voice::updateParams(const SynthParams& p) {
    params = p;
    smoothedCutoff.setTargetValue(params.vcfFreq);
    smoothedResonance.setTargetValue(params.resonance);

    if (params.vcaMode == 1) {
        smoothedVCALevel.setTargetValue(params.vcaLevel);
    } else {
        smoothedVCALevel.setTargetValue(1.0f); 
    }
    
    dco.setRange(static_cast<JunoDCO::Range>(p.dcoRange));
    dco.setSawLevel(p.sawOn ? 1.0f : 0.0f); 
    dco.setPulseLevel(p.pulseOn ? 1.0f : 0.0f); 
    dco.setSubLevel(p.subOscLevel);
    dco.setNoiseLevel(p.noiseLevel);
    dco.setPWM(p.pwmAmount);
    dco.setPWMMode(static_cast<JunoDCO::PWMMode>(p.pwmMode));
    dco.setLFODepth(p.lfoToDCO);
    dco.setDrift(p.drift);
    
    adsr.setGateMode(p.vcaMode == 1);
    
    if (p.vcaMode == 1) {
        adsr.setAttack(0.001f);
        adsr.setDecay(0.001f);
        adsr.setSustain(1.0f);
        adsr.setRelease(0.001f);
    } else {
        adsr.setAttack(JunoTimeCurves::kAttackMin + (JunoTimeCurves::kAttackMax - JunoTimeCurves::kAttackMin) * std::pow(p.attack, 3.0f));
        adsr.setDecay(JunoTimeCurves::kDecayMin + (JunoTimeCurves::kDecayMax - JunoTimeCurves::kDecayMin) * std::pow(p.decay, 3.0f));
        adsr.setSustain(p.sustain);
        adsr.setRelease(JunoTimeCurves::kReleaseMin + (JunoTimeCurves::kReleaseMax - JunoTimeCurves::kReleaseMin) * std::pow(p.release, 3.0f));
    }
    
    updateHPF();
}

void Voice::updateHPF() {
    switch (params.hpfFreq) {
        case 0: // Bass Boost (~+3dB at 80Hz)
            hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, 80.0f, 0.707f, 1.4f);
            break;
        case 1: // Approx 220Hz
            hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 220.0f, 0.707f);
            break;
        case 2: // Approx 400Hz
            hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 400.0f, 0.707f);
            break;
        case 3: // Approx 650Hz
            hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 650.0f, 0.707f);
            break;
    }
}

void Voice::renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, const std::vector<float>& lfoBuffer) {
    bool isActive = adsr.isActive() || lastOutputLevel > 0.0001f;
    if (!isActive) return;
    
    if (numSamples > tempBuffer.getNumSamples()) numSamples = tempBuffer.getNumSamples();
    
    if (params.portamentoOn && std::abs(currentFrequency - targetFrequency) > 0.1f) {
        float glideTime = params.portamentoTime * 5.0f;
        float glideRate = 1.0f / (glideTime * static_cast<float>(sampleRate));
        if (currentFrequency < targetFrequency) currentFrequency += (targetFrequency - currentFrequency) * glideRate;
        else currentFrequency -= (currentFrequency - targetFrequency) * glideRate;
    } else {
        currentFrequency = targetFrequency;
    }
    
    float bendedFrequency = currentFrequency * std::pow(2.0f, params.tune / 1200.0f);
    if (params.benderValue != 0.0f && params.benderToDCO > 0.0f) {
        bendedFrequency *= std::pow(2.0f, params.benderValue * (params.benderToDCO * 2.0f / 12.0f));
    }
    dco.setFrequency(bendedFrequency);
    
    juce::AudioBuffer<float> voiceBuffer(tempBuffer.getArrayOfWritePointers(), 1, 0, numSamples);
    voiceBuffer.clear();
    float* voiceData = voiceBuffer.getWritePointer(0);
    
    for (int i = 0; i < numSamples; ++i) {
        float voiceLfo = lfoBuffer[i];
        float envVal = adsr.getNextSample(); 
        float vcaSmooth = smoothedVCALevel.getNextValue();
        
        float dcoSample = dco.getNextSample(voiceLfo);
        voiceData[i] = dcoSample * envVal * velocity * vcaSmooth;
    }

    juce::dsp::AudioBlock<float> block(voiceBuffer);
    juce::dsp::ProcessContextReplacing<float> hpContext(block);
    hpFilter.process(hpContext);

    float resParam = smoothedResonance.getNextValue();
    float bassBoostGain = 1.0f + resParam * 0.4f;
    bassBoostFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, 120.0f, 0.707f, bassBoostGain);
    juce::dsp::ProcessContextReplacing<float> bassBoostContext(block);
    bassBoostFilter.process(bassBoostContext);

    float targetModOctaves = 0.0f;
    for (int i = 0; i < numSamples; i += 8) {
        int currentBatchSize = std::min(8, numSamples - i);
        float vcfParam = smoothedCutoff.getNextValue();
        
        float baseCutoff = 10.0f * std::pow(12000.0f / 10.0f, vcfParam);
        
        float kybdTrackValue = 0.0f;
        if (params.kybdTracking > 0.4f && params.kybdTracking < 0.6f) kybdTrackValue = 0.5f;
        else if (params.kybdTracking >= 0.9f) kybdTrackValue = 1.0f;

        if (kybdTrackValue > 0.0f) {
            float semitones = static_cast<float>(currentNote) - 60.0f;
            baseCutoff *= std::pow(2.0f, (semitones * kybdTrackValue) / 12.0f);
        }

        float envVal = adsr.getCurrentValue();
        float voiceLfo = lfoBuffer[i];
        
        float envMod = envVal * params.envAmount * 4.0f;
        if (params.vcfPolarity == 1) envMod = -envMod;
        float lfoMod = voiceLfo * params.lfoToVCF * 2.0f;
        float benderMod = params.benderValue * params.benderToVCF * 3.0f;
        
        targetModOctaves = envMod + lfoMod + benderMod;
        lastModOctaves += (targetModOctaves - lastModOctaves) * 0.4f; 
        
        float modulatedCutoff = baseCutoff * std::pow(2.0f, lastModOctaves);
        modulatedCutoff = juce::jlimit(5.0f, static_cast<float>(sampleRate * 0.45), modulatedCutoff);
        
        filter.setCutoffFrequencyHz(modulatedCutoff);
        filter.setResonance(juce::jmin(1.05f, resParam * 1.05f));
        filter.setDrive(1.3f);
        
        juce::dsp::AudioBlock<float> subBlock = block.getSubBlock(i, currentBatchSize);
        juce::dsp::ProcessContextReplacing<float> subContext(subBlock);
        filter.process(subContext);
    }

    float blockMax = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        float sample = voiceData[i];
        if (std::isnan(sample) || std::isinf(sample)) sample = 0.0f;
        float absS = std::abs(sample);
        if (absS > blockMax) blockMax = absS;
        sample *= 0.75f; 
        sample = std::tanh(sample);
        buffer.addSample(0, startSample + i, sample);
        if (buffer.getNumChannels() > 1) buffer.addSample(1, startSample + i, sample);
    }
    lastOutputLevel = blockMax;
    
    if (adsr.isActive()) {
        if (!isGateOn) {
            releaseCounter += numSamples;
            int timeoutSamples = (kReleaseTimeoutMs * static_cast<int>(sampleRate)) / 1000;
            if (releaseCounter > timeoutSamples) {
                adsr.reset(); 
                releaseCounter = 0;
            }
        } else {
            releaseCounter = 0;
        }
    } else {
        releaseCounter = 0;
        if (lastOutputLevel < 0.0001f) {
             currentNote = -1;
             isGateOn = false;
        }
    }
}

void Voice::setBender(float v) { params.benderValue = v; }
void Voice::setPortamentoEnabled(bool b) { params.portamentoOn = b; }
void Voice::setPortamentoTime(float v) { params.portamentoTime = v; }
void Voice::setPortamentoLegato(bool b) { params.portamentoLegato = b; }
