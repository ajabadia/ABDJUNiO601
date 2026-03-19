#include "Voice.h"
#include <cmath>
#include "../Core/SynthParams.h"
#include "../Core/JunoConstants.h"

using namespace JunoConstants;

Voice::Voice() {
    filter.setMode(juce::dsp::LadderFilterMode::LPF24);
    lastOutputLevel = 0.0f; // [Safety] Ensure initialized
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
    
    resCompFilter.prepare(spec);
    resCompFilter.reset();

    hpfShelfFilter.prepare(spec);
    hpfShelfFilter.reset(); 
    hpfShelfFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, 100.0f, 0.707f, 1.25f); // +2dB bump at 100Hz
    
    noiseColorFilter.prepare(spec);
    noiseColorFilter.reset();
    noiseColorFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, 4000.0f, 0.707f, 0.707f); 


    smoothedCutoff.reset(sr, 0.02);
    smoothedCutoff.setCurrentAndTargetValue(params.vcfFreq); // Avoid zero start
    smoothedResonance.reset(sr, 0.02);
    smoothedVCALevel.reset(sr, 0.02);
    smoothedVCALevel.setCurrentAndTargetValue(params.vcaLevel);
    
    thermalDrift = 0.0f;
    thermalTarget = 0.0f;
    thermalCounter = 0;

    tempBuffer.setSize(1, maxBlockSize);
}

void Voice::noteOn(int midiNote, float vel, bool isLegato) {
    currentNote = midiNote;
    velocity = vel;
    isGateOn = true;
    lastOutputLevel = 1.0f;
    
    targetFrequency = 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
    
    // [Fidelidad] UNISON DETUNE
    // Adds ±6 cents spread across voices for authentic stacking thickness
    if (params.polyMode == 3) {
        // Range 12 cents total (+/- 6 cents).
        // Spread constant per index deviation (range -2.5 to 2.5).
        // 0.024f * 2.5 = 0.06 semitones (6 cents).
        float spread = 0.024f; 
        targetFrequency *= std::pow(2.0f, ((voiceIndex - 2.5f) * spread) / 12.0f);
    }

    // [Fidelidad] Correct Portamento Logic
    // If Portamento is ON:
    // - Always Glide (Poly 1 & 2) unless 'Solos' mode which requires Legato (not standard on Juno-106 but supported here via param)
    // - On Juno 106, if Portamento is ON, it always slides from the voice's last pitch.
    bool runGlide = params.portamentoOn;
    if (params.portamentoLegato) runGlide = runGlide && isLegato;
    
    bool shouldGlide = runGlide;
    
    adsr.setSampleRate(sampleRate);
    // [ENV Audit] Set simplified, linear times. The ADSR class now handles the exponential curve.
    // [Fidelidad] Exponential slider mapping for ADSR times
    auto curveMap = [](float val, float minV, float maxV) {
        return minV * std::pow(maxV / minV, val);
    };
    adsr.setAttack(curveMap(params.attack, Curves::kAttackMin, Curves::kAttackMax));
    adsr.setDecay(curveMap(params.decay, Curves::kDecayMin, Curves::kDecayMax));
    adsr.setSustain(params.sustain);
    adsr.setRelease(curveMap(params.release, Curves::kReleaseMin, Curves::kReleaseMax));
    
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

// [Audit Fix] Portamento Reset on Patch Change/Stop
void Voice::forceStop() { 
    adsr.reset(); 
    currentNote = -1; 
    lastOutputLevel = 0.0f; 
    currentFrequency = targetFrequency; // Reset portamento history
}

/* Original noteOff implementation preserved */
void Voice::noteOff() {
    isGateOn = false;
    adsr.noteOff();
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
    
    auto curveMap = [](float val, float minV, float maxV) {
        return minV * std::pow(maxV / minV, val);
    };

    if (p.vcaMode == 1) { // GATE MODE
        adsr.setGateMode(true);
    } else { // ENV MODE
        adsr.setGateMode(false);
        adsr.setAttack(curveMap(p.attack, Curves::kAttackMin, Curves::kAttackMax));
        adsr.setDecay(curveMap(p.decay, Curves::kDecayMin, Curves::kDecayMax));
        adsr.setSustain(p.sustain);
        adsr.setRelease(curveMap(p.release, Curves::kReleaseMin, Curves::kReleaseMax));
    }
    
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

void Voice::renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, const std::vector<float>& lfoBuffer, float neighborCrosstalk) {
    if (!(adsr.isActive() || lastOutputLevel > 0.0001f)) return;
    
    if (numSamples > tempBuffer.getNumSamples()) numSamples = tempBuffer.getNumSamples();
    
    float bendedFrequency = updatePitch(numSamples);
    dco.setFrequency(bendedFrequency);
    
    float* voiceData = tempBuffer.getWritePointer(0);
    renderVoiceCycles(voiceData, numSamples, lfoBuffer, neighborCrosstalk);
    processFinalOutput(buffer, startSample, numSamples, voiceData);
}

float Voice::updatePitch(int numSamples) {
    if (params.portamentoOn && std::abs(currentFrequency - targetFrequency) > 0.1f) {
        float glideTime = params.portamentoTime * 5.0f;
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
    bendedFrequency *= std::pow(2.0f, (params.thermalDrift * 0.1f) / 12.0f);
    return bendedFrequency;
}

void Voice::renderVoiceCycles(float* voiceData, int numSamples, const std::vector<float>& lfoBuffer, float neighborCrosstalk) {
    float resParam = smoothedResonance.getNextValue();
    filter.setResonance(juce::jlimit(0.0f, 0.99f, resParam)); 
    filter.setDrive(1.0f + resParam * 1.2f); 

    for (int i = 0; i < numSamples; ++i) {
        float envVal = adsr.getNextSample();
        float voiceLfo = lfoBuffer[i];
        
        // 1. DCO
        float dcoSample = dco.getNextSample(voiceLfo);
        float rippleNoise = (noiseGen.nextFloat() - 0.5f) * 0.0005f * envVal;
        
        // Soft-clipper (DCO Mixer saturation)
        if (std::abs(dcoSample) > kDcoMixerSaturationThreshold) {
             float x = dcoSample * 1.15f;
             dcoSample = x - (x * x * x) / 24.0f; 
        }
        
        float signal = dcoSample + neighborCrosstalk * kVoiceCrosstalkAmount + rippleNoise;
        
        // 2. HPF
        signal = hpFilter.processSample(signal);
        if (params.hpfFreq == 0) signal = hpfShelfFilter.processSample(signal);
        
        // 3. VCF (Per-Sample Modulation for High Fidelity)
        float vcfParam = smoothedCutoff.getNextValue();
        // [Fidelity] Refined VCF Curve: Target 20kHz at max, but more "open" in mid-range (exponent 0.65)
        float baseCutoff = 10.0f * std::pow(2000.0f, std::pow(vcfParam, 0.65f));
        
        if (params.kybdTracking > 0.001f) {
             float semitones = static_cast<float>(currentNote) - 60.0f;
             baseCutoff *= std::pow(2.0f, (semitones * params.kybdTracking) / 12.0f);
        }
        
        // VCF Modulation Mapping (Approx 5 octaves)
        float envMod = (params.vcfPolarity == 1) ? -envVal : envVal;
        float finalModOct = (envMod * params.envAmount * 5.0f) + 
                            (voiceLfo * params.lfoToVCF * 4.0f) + 
                            (params.benderValue * params.benderToVCF * 2.0f);
                            
        // [Fidelity] Apply thermal drift to cutoff (approx +/- 20 cents)
        float targetCutoff = baseCutoff * std::pow(2.0f, finalModOct + (thermalDrift / 1200.0f));
        filter.setCutoffFrequencyHz(juce::jlimit(8.0f, static_cast<float>(sampleRate * 0.48), targetCutoff));
        
        // [Enrichment] Analog Saturation: Gentle drive to add harmonics
        filter.setResonance(smoothedResonance.getNextValue());
        filter.setDrive(1.35f + (params.resonance * 0.15f)); // Drive increases slightly with resonance
        
        // VCF Processing
        float* signalPtr = &signal;
        juce::dsp::AudioBlock<float> sampleBlock (&signalPtr, 1, 1);
        juce::dsp::ProcessContextReplacing<float> vcfContext (sampleBlock);
        filter.process (vcfContext);
        
        // 4. VCA
        float rawVcaLev = smoothedVCALevel.getNextValue();
        
        // [Fidelity] Resonance-compensated VCA Gain
        // Moog-style/Juno ladders thin out at high resonance. We compensate slightly.
        float resComp = 1.0f + (resParam * resParam * 0.5f);
        
        // [Improvement] Velocity Sensitivity
        float velScale = 1.0f - params.velocitySens + (params.velocitySens * velocity);
        
        float vcaGain = (params.vcaMode == 1) ? (rawVcaLev * (isGateOn ? 1.0f : 0.0f)) : (envVal * rawVcaLev);
        vcaGain *= velScale;
        
        // Final Output
        voiceData[i] = signal * vcaGain * resComp * kVoiceOutputGain;
    }
}

void Voice::processFinalOutput(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, float* voiceData) {
    float currentBlockMax = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        float sample = voiceData[i];
        if (std::isnan(sample) || std::isinf(sample)) sample = 0.0f;
        sample = std::tanh(sample * 1.0f);
        
        float absSample = std::abs(sample);
        if (absSample > currentBlockMax) currentBlockMax = absSample;

        buffer.addSample(0, startSample + i, sample);
        if (buffer.getNumChannels() > 1) buffer.addSample(1, startSample + i, sample);
    }
    
    lastOutputLevel = currentBlockMax;
    
    // [Fidelity] "Voice Kill" threshold (~0.4%)
    if (!adsr.isActive() && lastOutputLevel < kVoiceKillThreshold) {
         currentNote = -1;
         isGateOn = false;
    }
}

void Voice::setBender(float v) { params.benderValue = v; }
void Voice::setPortamentoEnabled(bool b) { params.portamentoOn = b; }
void Voice::setPortamentoTime(float v) { params.portamentoTime = v; }
void Voice::setPortamentoLegato(bool b) { params.portamentoLegato = b; }
