#include "Voice.h"
#include <cmath>
#include "../Core/SynthParams.h"
#include "../Core/JunoConstants.h"

using namespace JunoConstants;

Voice::Voice() : ABD::VoiceBase() {
    // filter.setMode(juce::dsp::LadderFilterMode::LPF24); // Removed as per instruction
    lastOutputLevel = 0.0f; // [Safety] Ensure initialized
}

void Voice::onPrepare() {
    double sr = this->sr; // from VoiceBase
    int maxBlockSize = this->blockSz;
    sampleRate = sr;
    dco.prepare(sr, maxBlockSize);
    
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
    hpfShelfFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, 100.0f, 0.707f, 1.25f); // +2dB bump at 100Hz
    
    noiseColorFilter.prepare(spec);
    noiseColorFilter.reset();
    noiseColorFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, 4000.0f, 0.707f, 0.707f); 


    smoothedCutoff.reset(sr, 0.02);
    smoothedCutoff.setCurrentAndTargetValue(0.5f); // Safe default
    smoothedResonance.reset(sr, 0.02);
    smoothedVCALevel.reset(sr, 0.02);
    smoothedVCALevel.setCurrentAndTargetValue(0.5f);
    
    thermalDrift = 0.0f;
    thermalTarget = 0.0f;
    thermalCounter = 0;

    tempBuffer.setSize(1, maxBlockSize);
}

void Voice::onNoteOn(int midiNote, float vel) {
    // Current D601 specific logic for Unison/Legato might need a wrapper
    // For now, redirecting to the specific implementation
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
    
    // [Fidelity] Refined Unison Spread Logic
    // Centers the spread regardless of the number of active voices.
    if (params.polyMode == 3 && numVoicesInUnison > 1) {
        float spreadAmt = params.unisonDetune * kUnisonDetuneMaxSemitones; 
        float center = (numVoicesInUnison - 1) * 0.5f;
        float detuneSemitones = (voiceIndex - center) * spreadAmt;
        targetFrequency *= std::pow(2.0f, detuneSemitones / 12.0f);
    }

    // [Fidelidad] Correct Portamento Logic
    // If Portamento is ON:
    // - Always Glide (Poly 1 & 2) unless 'Solos' mode which requires Legato (not standard on Juno-106 but supported here via param)
    // - On Juno 106, if Portamento is ON, it always slides from the voice's last pitch.
    bool runGlide = params.portamentoOn;
    if (params.portamentoLegato) runGlide = runGlide && isLegato;
    
    bool shouldGlide = runGlide;
    
    adsr.setAttack(JunoConstants::curveMap(params.attack, Curves::kAttackMin, Curves::kAttackMax));
    adsr.setDecay(JunoConstants::curveMap(params.decay, Curves::kDecayMin, Curves::kDecayMax));
    adsr.setSustain(params.sustain);
    adsr.setRelease(JunoConstants::curveMap(params.release, Curves::kReleaseMin, Curves::kReleaseMax));
    adsr.setGateMode(params.vcaMode == 1);
    
    stealPending = false; // Reset steal flag on new note
    
    if (!isLegato) {
        // [Fidelidad] Fix "Ataques Sordos" (Dull Attacks)
        // We always reset the ADSR and DCO to ensure a punchy, consistent attack.
        // HOWEVER, we only reset the filter if the voice was truly IDLE (-1)
        // to avoid "choking" the audio energy and resonance during stealing.
        bool wasIdle = (currentNote == -1);
        
        adsr.reset(); 
        adsr.noteOn();
        dco.reset(); 
        
        if (wasIdle) {
            // [Fix] Reset the filter only when starting from silence 
            // to recover from any potential NaN/Inf explosions.
            filter.reset();
            hpFilter.reset();
            resCompFilter.reset();
            hpfShelfFilter.reset();
        }
    }
    
    if (!shouldGlide) currentFrequency = targetFrequency;
    dco.setFrequency(currentFrequency);
}

void Voice::onNoteOff(float /*vel*/) {
    noteOff();
}

// [Audit Fix] Portamento Reset on Patch Change/Stop
void Voice::onReset() {
    filter.reset();
    forceStop();
}

void Voice::forceStop() { 
    adsr.reset(); 
    currentNote = -1; 
    note_ = -1; // and the base class member
    isActive_ = false;
    lastOutputLevel = 0.0f; 
    currentFrequency = targetFrequency; // Reset portamento history
}

/* Original noteOff implementation preserved */
void Voice::noteOff() {
    isGateOn = false;
    adsr.noteOff();
    stealPending = false;
    // Note: isActive_ will be set to false in renderNextBlock when ADSR is idle
}

void Voice::prepareForStealing() {
    // Force a very fast release (3ms) to avoid clicks before re-triggering
    adsr.setRelease(0.003f);
    adsr.noteOff();
    stealPending = true;
}

void Voice::updateParams(const SynthParams& p) {
    params = p;
    smoothedCutoff.setTargetValue(params.vcfFreq);
    smoothedResonance.setTargetValue(params.resonance);
    
    // [VCA Audit] VCA level is now a master gain control for the voice
    smoothedVCALevel.setTargetValue(params.vcaLevel);
    
    dco.setRange(static_cast<JunoDCO::Range>(p.dcoRange));
    dco.setSawLevel(p.sawOn ? 1.0f : 0.0f); 
    dco.setPulseLevel(p.pulseOn ? 1.0f : 0.0f); 
    dco.setSubLevel(p.subOscLevel);
    dco.setNoiseLevel(p.noiseLevel);
    dco.setPWM(p.pwmAmount);
    dco.setPWMMode(static_cast<JunoDCO::PWMMode>(p.pwmMode));
    dco.setLFODepth(p.lfoToDCO);
    dco.setDrift(p.thermalDrift);
    
    // [Fidelidad] STRICT VCA MODE CONVENTION: 
    // vcaMode = 0 -> ENV (Controlled by ADSR)
    // vcaMode = 1 -> GATE (On/Off)
    
    // We pass the raw mode to the logic later.
    // BUT we also need to tell ADSR how to behave if it handles gate internally.
    // 'setGateMode(true)' forces ADSR to output square gate.
    
    adsr.setAttack(JunoConstants::curveMap(p.attack, Curves::kAttackMin, Curves::kAttackMax));
    adsr.setDecay(JunoConstants::curveMap(p.decay, Curves::kDecayMin, Curves::kDecayMax));
    adsr.setSustain(p.sustain);
    adsr.setRelease(JunoConstants::curveMap(p.release, Curves::kReleaseMin, Curves::kReleaseMax));
    adsr.setGateMode(p.vcaMode == 1);
    adsr.setSlewMs(p.adsrSlewMs);
    
    dco.setMixerGain(p.dcoMixerGain);
    dco.setSubAmpScale(p.subAmpScale);
    
    updateHPF();
}

void Voice::updateHPF() {
    switch (params.hpfFreq) {
        case 0: // Position 0: [Fidelity] Bass Boost (+3dB @ 70Hz shelving)
            hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, HPF::kShelfFreq, 0.707f, std::pow(10.0f, HPF::kShelfGainDb / 20.0f));
            break;
        case 1: // Position 1: Bypass (All-pass)
            hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeAllPass(sampleRate, 1000.0f);
            break;
        case 2: // Position 2: 225Hz
            hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, HPF::kFreq2, 0.707f);
            break;
        case 3: // Position 3: 700Hz
            hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, HPF::kFreq3, 0.707f);
            break;
        default:
            hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeAllPass(sampleRate, 1000.0f);
            break;
    }
}

void Voice::forceUpdate() {
    updateParams(params); // ensure internal state is consistent
    smoothedCutoff.setCurrentAndTargetValue(params.vcfFreq);
    smoothedResonance.setCurrentAndTargetValue(params.resonance);
    smoothedVCALevel.setCurrentAndTargetValue(params.vcaLevel);
    
    // [Fix] Recover from NaN on patch change
    filter.reset();
    hpFilter.reset();
    resCompFilter.reset();
    hpfShelfFilter.reset();
}

void Voice::renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) {
    if (!lfoBufferPtr) return;
    Voice::renderNextBlock(buffer, startSample, numSamples, *lfoBufferPtr, currentNeighborCrosstalk, currentUnisonCount);
}

void Voice::renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, const std::vector<float>& lfoBuffer, float neighborCrosstalk, int numVoicesInUnison) {
    if (!(adsr.isActive() || lastOutputLevel > 0.0001f)) {
        isActive_ = false;
        return;
    }
    isActive_ = true;
    
    if (numSamples > tempBuffer.getNumSamples()) numSamples = tempBuffer.getNumSamples();
    
    float bendedFrequency = updatePitch(numSamples);
    dco.setFrequency(bendedFrequency);
    
    float* voiceData = tempBuffer.getWritePointer(0);
    renderVoiceCycles(voiceData, numSamples, lfoBuffer, neighborCrosstalk);
    processFinalOutput(buffer, startSample, numSamples, voiceData, numVoicesInUnison);
}

float Voice::updatePitch(int numSamples) {
    if (params.portamentoOn && std::abs(currentFrequency - targetFrequency) > 0.1f) {
        // [Fidelity] Exponential mapping for better sensitivity in short ranges
        float glideTime = 0.005f * std::pow(400.0f, params.portamentoTime);
        float glideCoeff = 1.0f - std::exp(-static_cast<float>(numSamples) /
                               (juce::jmax(0.001f, glideTime) * static_cast<float>(sampleRate)));
        currentFrequency += (targetFrequency - currentFrequency) * glideCoeff;
    } else {
        currentFrequency = targetFrequency;
    }
    
    float bendedFrequency = currentFrequency * std::pow(2.0f, params.tune / 1200.0f);
    if (params.benderValue != 0.0f && params.benderToDCO > 0.0f) {
        // [Fidelity] Bender Range optimization: scale by the user-selected range (e.g. 2, 7, 12 semitones)
        bendedFrequency *= std::pow(2.0f, params.benderValue * (params.benderToDCO * (float)params.benderRange / 12.0f));
    }

    // [Senior Audit] Global Thermal Drift (Shared DAC)
    // [Fidelity] Inverse Drift Scaling: Suppress stochastic drift as intentional detune increases
    float driftFactor = 1.0f - (params.polyMode == 3 ? params.unisonDetune : 0.0f);
    bendedFrequency *= std::pow(2.0f, (params.thermalDrift * 0.1f * driftFactor) / 12.0f);
    return bendedFrequency;
}

void Voice::renderVoiceCycles(float* voiceData, int numSamples, const std::vector<float>& lfoBuffer, float neighborCrosstalk) {
    for (int i = 0; i < numSamples; ++i) {
        float envVal = adsr.getNextSample(); // Juno Authentic ADSR
        float voiceLfo = lfoBuffer[i];
        
        // 1. DCO & Noise (Pre-VCF Mix)
        float dcoSample = dco.getNextSample(voiceLfo);
        
        // [Fidelity] Characterize Noise (White with gentle 2-sample smoothing for "Warm" feel)
        float whiteNoise = (noiseGen.nextFloat() * 2.0f - 1.0f);
        float smoothNoise = 0.5f * (whiteNoise + lastNoise);
        lastNoise = whiteNoise;
        
        float noiseSignal = smoothNoise * params.noiseLevel;
        float rippleNoise = (noiseGen.nextFloat() - 0.5f) * 0.0005f * envVal;
        
        // Soft-clipper (DCO Mixer saturation)
        if (std::abs(dcoSample) > kDcoMixerSaturationThreshold) {
             float x = dcoSample * 1.15f;
             dcoSample = x - (x * x * x) / 24.0f; 
        }
        
        float signal = dcoSample + neighborCrosstalk * kVoiceCrosstalkAmount + rippleNoise;
        
        // 2. HPF (Original position preserved)
        signal = hpFilter.processSample(signal);
        if (params.hpfFreq == 0) signal = hpfShelfFilter.processSample(signal);
        
        // 3. VCF (High-Fidelity TPT Ladder)
        float voiceHz = currentFrequency;
        
        // Modulations centered/scaled for OTA model
        float envMod = params.envAmount * (envVal - 0.5f) * 2.0f;
        if (params.vcfPolarity == 1) envMod = -envMod;
        
        float lfoVCF = params.lfoToVCF * voiceLfo * 0.3f;
        
        // Process VCF using the new high-fidelity model
        signal = filter.processSample(
            signal,
            params.vcfFreq,
            params.resonance,
            envMod,
            lfoVCF,
            params.kybdTracking,
            voiceHz,
            params.vcfSelfOscThreshold,
            params.vcfSaturation
        );
        
        // 3. (REMOVED REDUNDANT HPF PROCESSING HERE)
        
        // [Fidelity] Denormal protection: Flush tiny signals to zero
        juce::dsp::util::snapToZero(signal);
        // 4. VCA
        float rawVcaLev = smoothedVCALevel.getNextValue();
        
        // [Fidelity] Resonance-compensated VCA Gain
        // Moog-style/Juno ladders thin out at high resonance. We compensate slightly.
        float resComp = 1.0f + (params.resonance * params.resonance * 0.5f);
        
        // [Improvement] Velocity Sensitivity
        float velScale = 1.0f - params.velocitySens + (params.velocitySens * velocity);
        
        // [Fidelity Fix] Always use envVal (even in GATE mode) to leverage the 2ms anti-click slew
        float vcaGain = (params.vcaMode == 1) ? (rawVcaLev * envVal) : (envVal * rawVcaLev);
        vcaGain *= velScale;
        
        // Final Output
        voiceData[i] = signal * vcaGain * resComp * kVoiceOutputGain;
    }
}

void Voice::processFinalOutput(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, float* voiceData, int numVoicesInUnison) {
    float pan = 0.0f;
    if (numVoicesInUnison > 1) {
        // [Modern] Distribute voices across stereo field in Unison
        pan = (voiceIndex / (float)(numVoicesInUnison - 1)) * 2.0f - 1.0f;
    }
    
    float unisonWidth = params.unisonStereoWidth;
    float gainL = std::sqrt(0.5f * (1.0f - pan * unisonWidth));
    float gainR = std::sqrt(0.5f * (1.0f + pan * unisonWidth));

    float currentBlockMax = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        float sample = voiceData[i];
        if (std::isnan(sample) || std::isinf(sample)) sample = 0.0f;
        sample = std::tanh(sample * 1.0f);
        
        float absSample = std::abs(sample);
        if (absSample > currentBlockMax) currentBlockMax = absSample;

        buffer.addSample(0, startSample + i, sample * gainL);
        if (buffer.getNumChannels() > 1) buffer.addSample(1, startSample + i, sample * gainR);
    }
    
    lastOutputLevel = currentBlockMax;
    
    // [Fidelity] "Voice Kill" threshold (~0.4%)
    if (!adsr.isActive() && lastOutputLevel < kVoiceKillThreshold) {
         currentNote = -1;
         note_ = -1;
         isActive_ = false;
         isGateOn = false;
    }
}

void Voice::setBender(float v) { params.benderValue = v; }
void Voice::setPortamentoEnabled(bool b) { params.portamentoOn = b; }
void Voice::setPortamentoTime(float v) { params.portamentoTime = v; }
void Voice::setPortamentoLegato(bool b) { params.portamentoLegato = b; }
