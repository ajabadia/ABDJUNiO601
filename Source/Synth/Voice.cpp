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
    adsr.setAttack(juce::jmap(params.attack, 0.0f, 1.0f, JunoTimeCurves::kAttackMin, JunoTimeCurves::kAttackMax));
    adsr.setDecay(juce::jmap(params.decay, 0.0f, 1.0f, JunoTimeCurves::kDecayMin, JunoTimeCurves::kDecayMax));
    adsr.setSustain(params.sustain);
    adsr.setRelease(juce::jmap(params.release, 0.0f, 1.0f, JunoTimeCurves::kReleaseMin, JunoTimeCurves::kReleaseMax));
    
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
    
    if (p.vcaMode == 1) { // GATE MODE
        adsr.setGateMode(true);
        // We don't need to override timings because setGateMode(true) ignores them in ADSR::getNextSample
    } else { // ENV MODE
        adsr.setGateMode(false);
        // Set authentic times
        adsr.setAttack(juce::jmap(p.attack, 0.0f, 1.0f, JunoTimeCurves::kAttackMin, JunoTimeCurves::kAttackMax));
        adsr.setDecay(juce::jmap(p.decay, 0.0f, 1.0f, JunoTimeCurves::kDecayMin, JunoTimeCurves::kDecayMax));
        adsr.setSustain(p.sustain);
        adsr.setRelease(juce::jmap(p.release, 0.0f, 1.0f, JunoTimeCurves::kReleaseMin, JunoTimeCurves::kReleaseMax));
    }
    
    updateHPF();
}

void Voice::updateHPF() {
    switch (params.hpfFreq) {
        case 0: // [Fidelidad] Position 0: Neutral (shelf applied in process)
            hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeAllPass(sampleRate, 1000.0f);
            break;
        case 1: // Approx 220Hz
            hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 225.0f, 0.5f);
            break;
        case 2: // Approx 400Hz
            hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 450.0f, 0.5f);
            break;
        case 3: // Approx 650Hz
            hpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 750.0f, 0.5f);
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
    
    if (numSamples > tempBuffer.getNumSamples()) numSamples = tempBuffer.getNumSamples();
    
    if (params.portamentoOn && std::abs(currentFrequency - targetFrequency) > 0.1f) {
        float glideTime = params.portamentoTime * 5.0f;
        // [Safety] Avoid division by zero
        float glideRate = 1.0f / (juce::jmax(0.001f, glideTime) * static_cast<float>(sampleRate));
        if (currentFrequency < targetFrequency) currentFrequency += (targetFrequency - currentFrequency) * glideRate;
        else currentFrequency -= (currentFrequency - targetFrequency) * glideRate;
    } else {
        currentFrequency = targetFrequency;
    }
    
    float bendedFrequency = currentFrequency * std::pow(2.0f, params.tune / 1200.0f);
    if (params.benderValue != 0.0f && params.benderToDCO > 0.0f) {
        bendedFrequency *= std::pow(2.0f, params.benderValue * (params.benderToDCO * 2.0f / 12.0f));
    }

    // === THERMAL DRIFT SIMULATION ===
    // [Senior Audit] Global Thermal Drift (Shared DAC)
    // We now use the global drift value calculated in PluginProcessor, shared by all voices.
    // This authenticates the single multiplexed DAC behavior.
    
    float drift = params.thermalDrift; 
    
    // [Senior Audit] MASTER TUNE
    // params.tune is 0..1 => +/- 50 cents (approx) = +/- 0.5 semitones
    // 0.5 = 0 cents.
    float masterTune = (params.tune - 0.5f); // -0.5 to 0.5 semitones
    
    bendedFrequency *= std::pow(2.0f, (drift * 0.1f + masterTune) / 12.0f); // Subtle pitch drift + Master Tune

    dco.setFrequency(bendedFrequency);
    
    // [Fidelidad] Unified Sample Loop
    // Merging DCO, VCA, and VCF processing into a single loop ensures that 
    // the Envelope (ADSR) state is consistent for both Amplitude and Filter modulation.
    // Previously, split loops caused the Filter to read "stale" or end-of-block envelope values.

    juce::dsp::AudioBlock<float> block(tempBuffer);
    float* voiceData = tempBuffer.getWritePointer(0);
    float targetModOctaves = 0.0f; 
    float targetModCutoff = 0.0f; 

    // --- STAGE 1: DCO, VCA, HPF (Per-Sample) ---
    for (int i = 0; i < numSamples; ++i) {
        float envVal = adsr.getNextSample();
        float voiceLfo = lfoBuffer[i];
        float rawVcaLev = smoothedVCALevel.getNextValue();
        
        uint8_t vcaByte = (uint8_t)(rawVcaLev * 127.0f);
        float vcaLev = vcaByte / 127.0f;
        uint8_t env8bit = (uint8_t)(envVal * 255.0f);
        float steppedEnv = env8bit / 255.0f;

        if (steppedEnv > 0.7f) {
            float drive = 1.0f + (steppedEnv - 0.7f) * 0.15f;
            steppedEnv = std::tanh(steppedEnv * drive) / std::tanh(drive);
        }

        float finalGain = (params.vcaMode == 1) ? (vcaLev * (isGateOn ? 1.0f : 0.0f)) : (steppedEnv * vcaLev);

        float dcoSample = dco.getNextSample(voiceLfo);
        float rippleNoise = (noiseGen.nextFloat() - 0.5f) * 0.0005f * steppedEnv;

        if (std::abs(dcoSample) > 0.6f) {
             float x = dcoSample * 1.2f;
             dcoSample = x - (x * x * x) / 24.0f; 
        }

        float rawSignal = (dcoSample + neighborCrosstalk * 0.007f + rippleNoise) * finalGain;
        float processed = hpFilter.processSample(rawSignal);
        
        if (params.hpfFreq == 0) {
            processed = hpfShelfFilter.processSample(processed);
        }
        
        voiceData[i] = processed;
        
        // Capture last modulation values for block-rate VCF update
        if (i == numSamples - 1) {
            float vcfParam = smoothedCutoff.getNextValue();
            float baseCutoff = 5.0f * std::pow(20000.0f / 5.0f, vcfParam);
            if (params.kybdTracking > 0.001f) {
                 float semitones = static_cast<float>(currentNote) - 60.0f;
                 baseCutoff *= std::pow(2.0f, (semitones * params.kybdTracking) / 12.0f);
            }
            float baseEnvMod = envVal * params.envAmount * 4.0f;
            float envMod = (params.vcfPolarity == 1) ? baseEnvMod : -baseEnvMod;
            float lfoMod = voiceLfo * params.lfoToVCF * 2.0f;
            float benderMod = params.benderValue * params.benderToVCF * 3.0f;
            float masterTuneOct = (params.tune - 0.5f) * 100.0f / 1200.0f; 
            targetModOctaves = envMod + lfoMod + benderMod + masterTuneOct;
            lastModOctaves += (targetModOctaves - lastModOctaves) * 0.15f; 
            targetModCutoff = baseCutoff * std::pow(2.0f, lastModOctaves + (params.thermalDrift / 1200.0f));
        } else {
            smoothedCutoff.skip(1);
        }
    }

    // --- STAGE 2: VCF (Block Processing - HUGE SPEEDUP) ---
    float resParam = smoothedResonance.getNextValue();
    float finalCutoff = juce::jlimit(5.0f, static_cast<float>(sampleRate * 0.45), targetModCutoff);
    filter.setCutoffFrequencyHz(finalCutoff);
    float res = juce::jlimit(0.0f, 1.0f, resParam);
    filter.setResonance(res * 0.99f + (res > 0.98f ? 0.02f : 0.0f)); 
    filter.setDrive(1.0f + res * 0.5f); 

    juce::dsp::AudioBlock<float> voiceBlock = block.getSubBlock(0, (size_t)numSamples);
    juce::dsp::ProcessContextReplacing<float> vcfContext(voiceBlock);
    filter.process(vcfContext);

    // --- STAGE 3: Final Output (Per-Sample) ---
    float currentBlockMax = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        float sample = voiceData[i];
        if (std::isnan(sample) || std::isinf(sample)) sample = 0.0f;
        sample = std::tanh(sample * 1.0f);
        
        float absSample = std::abs(sample);
        if (absSample > currentBlockMax) currentBlockMax = absSample;

        buffer.addSample(0, startSample + i, sample * 0.95f);
        if (buffer.getNumChannels() > 1) buffer.addSample(1, startSample + i, sample);
    }
    
    lastOutputLevel = currentBlockMax;
    
    // [Fidelidad] Umbral de "Voice Kill" del Juno-106 (~0.4%)
    if (!adsr.isActive() && lastOutputLevel < 0.004f) {
         currentNote = -1;
         isGateOn = false;
    }
}

void Voice::setBender(float v) { params.benderValue = v; }
void Voice::setPortamentoEnabled(bool b) { params.portamentoOn = b; }
void Voice::setPortamentoTime(float v) { params.portamentoTime = v; }
void Voice::setPortamentoLegato(bool b) { params.portamentoLegato = b; }
