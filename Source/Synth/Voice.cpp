#include "Voice.h"
#include <cmath>
#include "../Core/SynthParams.h"

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
    
    // [Fidelidad] KEYBOARD SCAN CLICK (2mV spike on noteOn)
    // El 8031 inyecta un pequeño ripple al scannear; este "thud" ayuda al ataque.
    lastModOctaves += 0.05f; 
    
    // [Fidelidad] Key Click Artifact (2mV DC + Burst)
    if (!isLegato) {
        lastOutputLevel += 0.002f;
        float clickNoise = (noiseGen.nextFloat() * 2.0f - 1.0f) * 0.006f; // Audio burst
        tempBuffer.addFrom(0, 0, &clickNoise, 1);
    } 
    
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
    adsr.setAttack(curveMap(params.attack, JunoTimeCurves::kAttackMin, JunoTimeCurves::kAttackMax));
    adsr.setDecay(curveMap(params.decay, JunoTimeCurves::kDecayMin, JunoTimeCurves::kDecayMax));
    adsr.setSustain(params.sustain);
    adsr.setRelease(curveMap(params.release, JunoTimeCurves::kReleaseMin, JunoTimeCurves::kReleaseMax));
    
    if (!isLegato) {
        // [Fidelidad] Fix "Ataques Sordos" (Dull Attacks)
        // We always reset the ADSR and DCO to ensure a punchy, consistent attack,
        // even if the voice is being stolen while still active.
        adsr.reset(); 
        adsr.noteOn();
        lastModOctaves = 0.0f;
        dco.reset(); 
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
        adsr.setAttack(curveMap(p.attack, JunoTimeCurves::kAttackMin, JunoTimeCurves::kAttackMax));
        adsr.setDecay(curveMap(p.decay, JunoTimeCurves::kDecayMin, JunoTimeCurves::kDecayMax));
        adsr.setSustain(p.sustain);
        adsr.setRelease(curveMap(p.release, JunoTimeCurves::kReleaseMin, JunoTimeCurves::kReleaseMax));
    }
    
    updateHPF();
}

void Voice::updateHPF() {
    switch (params.hpfFreq) {
        case 0: // [Hardware] Position 0: Bass Boost (AllPass + Shelf in process)
            hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeAllPass(sampleRate, 1000.0f);
            break;
        case 1: // [Hardware] Position 1: Flat
            hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeAllPass(sampleRate, 1000.0f);
            break;
        case 2: // [Hardware] Position 2: 225Hz
            hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 225.0f, 0.5f);
            break;
        case 3: // [Hardware] Position 3: 450Hz
            hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 450.0f, 0.5f);
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
}

void Voice::renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, const std::vector<float>& lfoBuffer, float neighborCrosstalk) {
    bool isActive = adsr.isActive() || lastOutputLevel > 0.0001f;
    if (!isActive) return;
    
    if (params.portamentoOn && std::abs(currentFrequency - targetFrequency) > 0.1f) {
        float glideTime = params.portamentoTime * 5.0f;
        float glideRate = 1.0f / (juce::jmax(0.001f, glideTime) * static_cast<float>(sampleRate));
        if (currentFrequency < targetFrequency) currentFrequency += (targetFrequency - currentFrequency) * glideRate;
        else currentFrequency -= (currentFrequency - targetFrequency) * glideRate;
    } else {
        currentFrequency = targetFrequency;
    }
    
    float bendedFrequency = currentFrequency;
    if (params.benderValue != 0.0f && params.benderToDCO > 0.0f) {
        bendedFrequency *= std::pow(2.0f, params.benderValue * (params.benderToDCO * 2.0f / 12.0f));
    }
    
    float masterTune = (params.tune - 0.5f); // -0.5 to 0.5 semitones
    bendedFrequency *= std::pow(2.0f, (params.thermalDrift * 0.1f + masterTune) / 12.0f);
    dco.setFrequency(bendedFrequency);
    
    float resParam = smoothedResonance.getNextValue();
    filter.setResonance(juce::jlimit(0.0f, 1.0f, resParam * 0.98f));
    filter.setDrive(1.0f + resParam * 0.4f);

    float vcaLevel = smoothedVCALevel.getNextValue();
    float* outL = buffer.getWritePointer(0, startSample);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1, startSample) : nullptr;

    float currentBlockMax = 0.0f;

    for (int i = 0; i < numSamples; ++i) {
        float envVal = adsr.getNextSample();
        float voiceLfo = lfoBuffer[i];
        
        // 1. DCO Generation
        float dcoSample = dco.getNextSample(voiceLfo);
        
        // 2. HPF Stage
        float hpfOut = hpFilter.processSample(dcoSample + neighborCrosstalk * 0.005f);
        if (params.hpfFreq == 0) hpfOut = hpfShelfFilter.processSample(hpfOut);
        
        // 3. VCF Stage (Per-sample modulation for analog fidelity)
        float vcfParam = smoothedCutoff.getNextValue();
        float baseCutoff = 10.0f * std::pow(20000.0f / 10.0f, vcfParam);
        
        if (params.kybdTracking > 0.001f) {
             float semitones = static_cast<float>(currentNote) - 60.0f;
             baseCutoff *= std::pow(2.0f, (semitones * params.kybdTracking) / 12.0f);
        }

        float envModScale = params.envAmount * 8.5f; // ~8.5 octaves max
        float envMod = (params.vcfPolarity == 1) ? -envVal * envModScale : envVal * envModScale;
        float lfoMod = voiceLfo * params.vcfLFOAmount * 2.5f;
        float benderMod = params.benderValue * params.benderToVCF * 1.5f;

        float finalCutoff = baseCutoff * std::pow(2.0f, envMod + lfoMod + benderMod);
        filter.setCutoffFrequencyHz(juce::jlimit(10.0f, static_cast<float>(sampleRate * 0.48), finalCutoff));

        float vcfOut = filter.processSample(0, hpfOut);

        // 4. VCA Stage (After VCF - Authentic Juno-106 Path)
        float vcaGain = (params.vcaMode == 1) ? (isGateOn ? 1.0f : 0.0f) : envVal;
        float output = vcfOut * vcaGain * vcaLevel * 1.8f;

        // 5. Soft Saturation
        output = std::tanh(output * 0.9f);

        if (std::abs(output) > currentBlockMax) currentBlockMax = std::abs(output);

        outL[i] += output;
        if (outR) outR[i] += output;
    }
    
    lastOutputLevel = currentBlockMax;
    smoothedResonance.skip(numSamples);
    smoothedVCALevel.skip(numSamples);
    
    if (!adsr.isActive() && lastOutputLevel < 0.002f) {
         currentNote = -1;
         isGateOn = false;
    }
}

void Voice::setBender(float v) { params.benderValue = v; }
void Voice::setPortamentoEnabled(bool b) { params.portamentoOn = b; }
void Voice::setPortamentoTime(float v) { params.portamentoTime = v; }
void Voice::setPortamentoLegato(bool b) { params.portamentoLegato = b; }
