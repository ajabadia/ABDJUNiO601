#include "PluginProcessor.h"
#include <JuceHeader.h>
#if !JUCE_HEADLESS_PLUGIN
 #include "PluginEditor.h"
#endif
#include "PresetManager.h"

//==============================================================================
SimpleJuno106AudioProcessor::SimpleJuno106AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
#else
    : AudioProcessor(JucePlugin_PreferredChannelConfigurations),
#endif
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    presetManager = std::make_unique<PresetManager>();
    midiLearnHandler.bind(16, "lfoRate");
    midiLearnHandler.bind(17, "lfoDelay");
    midiLearnHandler.bind(18, "lfoToDCO");
    midiLearnHandler.bind(19, "pwm");
    midiLearnHandler.bind(20, "subOsc");
    midiLearnHandler.bind(21, "noise");
    midiLearnHandler.bind(22, "hpfFreq");
    midiLearnHandler.bind(23, "vcfFreq");
    midiLearnHandler.bind(24, "resonance");
    midiLearnHandler.bind(25, "envAmount");
    midiLearnHandler.bind(26, "lfoToVCF");
    midiLearnHandler.bind(27, "kybdTracking");
    midiLearnHandler.bind(28, "attack");
    midiLearnHandler.bind(29, "decay");
    midiLearnHandler.bind(30, "sustain");
    midiLearnHandler.bind(31, "release");
    midiLearnHandler.bind(32, "vcaLevel");
    keyboardState.addListener(this);

    apvts.addParameterListener("lfoRate", this);
    apvts.addParameterListener("lfoDelay", this);
    apvts.addParameterListener("lfoToDCO", this);
    apvts.addParameterListener("pwm", this);
    apvts.addParameterListener("noise", this);
    apvts.addParameterListener("vcfFreq", this);
    apvts.addParameterListener("resonance", this);
    apvts.addParameterListener("envAmount", this);
    apvts.addParameterListener("lfoToVCF", this);
    apvts.addParameterListener("kybdTracking", this);
    apvts.addParameterListener("vcaLevel", this);
    apvts.addParameterListener("attack", this);
    apvts.addParameterListener("decay", this);
    apvts.addParameterListener("sustain", this);
    apvts.addParameterListener("release", this);
    apvts.addParameterListener("subOsc", this);
    apvts.addParameterListener("dcoRange", this);
    apvts.addParameterListener("pulseOn", this);
    apvts.addParameterListener("sawOn", this);
    apvts.addParameterListener("chorus1", this);
    apvts.addParameterListener("chorus2", this);
    apvts.addParameterListener("pwmMode", this);
    apvts.addParameterListener("vcfPolarity", this);
    apvts.addParameterListener("vcaMode", this);
    apvts.addParameterListener("hpfFreq", this);
    apvts.addParameterListener("hpfFreq", this);

    // [Optimization] Initialize Cached Pointers
    fmtDcoRange = apvts.getRawParameterValue("dcoRange");
    fmtSawOn = apvts.getRawParameterValue("sawOn");
    fmtPulseOn = apvts.getRawParameterValue("pulseOn");
    fmtPwm = apvts.getRawParameterValue("pwm");
    fmtPwmMode = apvts.getRawParameterValue("pwmMode");
    fmtSubOsc = apvts.getRawParameterValue("subOsc");
    fmtNoise = apvts.getRawParameterValue("noise");
    fmtLfoToDCO = apvts.getRawParameterValue("lfoToDCO");
    fmtHpfFreq = apvts.getRawParameterValue("hpfFreq");
    fmtVcfFreq = apvts.getRawParameterValue("vcfFreq");
    fmtResonance = apvts.getRawParameterValue("resonance");
    fmtEnvAmount = apvts.getRawParameterValue("envAmount");
    fmtVcfPolarity = apvts.getRawParameterValue("vcfPolarity");
    fmtKybdTracking = apvts.getRawParameterValue("kybdTracking");
    fmtLfoToVCF = apvts.getRawParameterValue("lfoToVCF");
    fmtVcaMode = apvts.getRawParameterValue("vcaMode");
    fmtVcaLevel = apvts.getRawParameterValue("vcaLevel");
    fmtAttack = apvts.getRawParameterValue("attack");
    fmtDecay = apvts.getRawParameterValue("decay");
    fmtSustain = apvts.getRawParameterValue("sustain");
    fmtRelease = apvts.getRawParameterValue("release");
    fmtLfoRate = apvts.getRawParameterValue("lfoRate");
    fmtLfoDelay = apvts.getRawParameterValue("lfoDelay");
    fmtChorus1 = apvts.getRawParameterValue("chorus1");
    fmtChorus2 = apvts.getRawParameterValue("chorus2");
    fmtPolyMode = apvts.getRawParameterValue("polyMode");
    fmtPortTime = apvts.getRawParameterValue("portamentoTime");
    fmtPortOn = apvts.getRawParameterValue("portamentoOn");
    fmtPortLegato = apvts.getRawParameterValue("portamentoLegato");
    fmtBender = apvts.getRawParameterValue("bender");
    fmtBenderDCO = apvts.getRawParameterValue("benderToDCO");
    fmtBenderVCF = apvts.getRawParameterValue("benderToVCF");
    fmtBenderLFO = apvts.getRawParameterValue("benderToLFO");
    fmtTune = apvts.getRawParameterValue("tune");
}

SimpleJuno106AudioProcessor::~SimpleJuno106AudioProcessor() {
    apvts.removeParameterListener("lfoRate", this);
    apvts.removeParameterListener("lfoDelay", this);
    apvts.removeParameterListener("lfoToDCO", this);
    apvts.removeParameterListener("pwm", this);
    apvts.removeParameterListener("noise", this);
    apvts.removeParameterListener("vcfFreq", this);
    apvts.removeParameterListener("resonance", this);
    apvts.removeParameterListener("envAmount", this);
    apvts.removeParameterListener("lfoToVCF", this);
    apvts.removeParameterListener("kybdTracking", this);
    apvts.removeParameterListener("vcaLevel", this);
    apvts.removeParameterListener("attack", this);
    apvts.removeParameterListener("decay", this);
    apvts.removeParameterListener("sustain", this);
    apvts.removeParameterListener("release", this);
    apvts.removeParameterListener("subOsc", this);
    apvts.removeParameterListener("dcoRange", this);
    apvts.removeParameterListener("pulseOn", this);
    apvts.removeParameterListener("sawOn", this);
    apvts.removeParameterListener("chorus1", this);
    apvts.removeParameterListener("chorus2", this);
    apvts.removeParameterListener("pwmMode", this);
    apvts.removeParameterListener("vcfPolarity", this);
    apvts.removeParameterListener("vcaMode", this);
    apvts.removeParameterListener("hpfFreq", this);
}

const juce::String SimpleJuno106AudioProcessor::getName() const { return JucePlugin_Name; }
bool SimpleJuno106AudioProcessor::acceptsMidi() const { return true; }
bool SimpleJuno106AudioProcessor::producesMidi() const { return true; }
bool SimpleJuno106AudioProcessor::isMidiEffect() const { return false; }
// double SimpleJuno106AudioProcessor::getTailLengthSeconds() const { return 12.0; } // [Fidelidad] Máximo release 12s + chorus BBD delay
int SimpleJuno106AudioProcessor::getNumPrograms() { return 1; }
int SimpleJuno106AudioProcessor::getCurrentProgram() { return 0; }
void SimpleJuno106AudioProcessor::setCurrentProgram (int index) { juce::ignoreUnused(index); }
const juce::String SimpleJuno106AudioProcessor::getProgramName (int index) { juce::ignoreUnused(index); return {}; }
void SimpleJuno106AudioProcessor::changeProgramName (int index, const juce::String& newName) { juce::ignoreUnused(index, newName); }

void SimpleJuno106AudioProcessor::parameterChanged(const juce::String& parameterID, float newValue) {
    lastParamsChangeCounter++;
    if (editor != nullptr) {
        if (auto* param = apvts.getParameter(parameterID)) {
             lastChangedParamName = param->name;
             if (dynamic_cast<juce::AudioParameterBool*>(param)) lastChangedParamValue = newValue > 0.5f ? "ON" : "OFF";
             else if (dynamic_cast<juce::AudioParameterInt*>(param)) lastChangedParamValue = juce::String((int)apvts.getRawParameterValue(parameterID)->load());
             else lastChangedParamValue = juce::String(newValue, 2);
             
             if (auto* e = dynamic_cast<juce::AsyncUpdater*>(editor)) e->triggerAsyncUpdate();
        }
    }

    if (!midiOutEnabled) return;
    // [Senior Audit] SysEx generation moved to processBlock for thread safety.
    // parameterChanged can be called from UI (Message) or Host (Audio) threads.
    // Writing to unsynchronized midiOutBuffer caused race conditions.
}

void SimpleJuno106AudioProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    voiceManager.prepare(sr, samplesPerBlock);
    juce::dsp::ProcessSpec spec { sr, (juce::uint32)samplesPerBlock, 2 };
    chorus.prepare(spec);
    chorus.prepare(spec);
    chorus.reset();
    
    // [Safety] Pre-allocate LFO buffer
    lfoBuffer.resize(samplesPerBlock + 128); // Standard safety margin

    dcBlocker.prepare(spec); 
    *dcBlocker.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, 20.0f);
    
    chorusPreEmphasisFilter.prepare(spec);
    chorusDeEmphasisFilter.prepare(spec);
    chorusNoiseFilter.prepare(spec);
    
    *chorusPreEmphasisFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, 8000.0f, 0.707f, 1.5f);
    *chorusDeEmphasisFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, 12000.0f, 0.707f);
    *chorusNoiseFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, 8000.0f, 0.707f);

    chorusNoiseBuffer.setSize(2, samplesPerBlock);

    masterLfoPhase = 0.0f; 
    masterLfoDelayEnvelope = 0.0f; 
    wasAnyNoteHeld = false;
    lfoBuffer.resize(samplesPerBlock);
}

void SimpleJuno106AudioProcessor::releaseResources() {}

bool SimpleJuno106AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    return true;
}

void SimpleJuno106AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i) buffer.clear (i, 0, buffer.getNumSamples());

    keyboardState.processNextMidiBuffer (midiMessages, 0, buffer.getNumSamples(), true);

    for (const auto metadata : midiMessages) {
        const auto message = metadata.getMessage();
        if (message.isSysEx()) { sysExEngine.handleIncomingSysEx(message, currentParams); continue; }
        if (message.isController()) {
            if (message.getControllerNumber() == 1) { if (auto* p = apvts.getParameter("benderToLFO")) p->setValueNotifyingHost(message.getControllerValue() / 127.0f); }
            else if (message.getControllerNumber() == 64) {
                 int val = message.getControllerValue();
                 if (sustainInverted) val = 127 - val;
                 performanceState.handleSustain(val);
            }
            else midiLearnHandler.handleIncomingCC(message.getControllerNumber(), message.getControllerValue(), apvts);
            continue;
        }
        if (message.isPitchWheel()) {
            if (auto* p = apvts.getParameter("bender")) p->setValueNotifyingHost(p->convertTo0to1(((float)message.getPitchWheelValue() / 8192.0f) - 1.0f));
            continue;
        }
        if (message.isNoteOn()) voiceManager.noteOn(message.getChannel(), message.getNoteNumber(), message.getVelocity());
        else if (message.isNoteOff()) performanceState.handleNoteOff(message.getNoteNumber(), voiceManager);
    }
    performanceState.flushSustain(voiceManager);
    
    // [Fidelidad] Block-consistent parameter atomic-style copy
    // [Fidelidad] Block-consistent parameter atomic-style copy
    currentParams = getMirrorParameters();
    
    // [Senior Audit] Thread-Safe SysEx Generation
    if (midiOutEnabled) {
         // Helper to send change if different
         auto checkSend = [&](float curr, float last, int id) {
             if (std::abs(curr - last) > 0.001f) {
                 midiOutBuffer.addEvent(JunoSysEx::createParamChange(midiChannel - 1, id, (int)(curr * 127.0f)), 0);
             }
         };
         
         checkSend(currentParams.lfoRate, lastParams.lfoRate, JunoSysEx::LFO_RATE);
         checkSend(currentParams.lfoDelay, lastParams.lfoDelay, JunoSysEx::LFO_DELAY);
         checkSend(currentParams.lfoToDCO, lastParams.lfoToDCO, JunoSysEx::DCO_LFO);
         checkSend(currentParams.pwmAmount, lastParams.pwmAmount, JunoSysEx::DCO_PWM);
         checkSend(currentParams.noiseLevel, lastParams.noiseLevel, JunoSysEx::DCO_NOISE);
         checkSend(currentParams.vcfFreq, lastParams.vcfFreq, JunoSysEx::VCF_FREQ);
         checkSend(currentParams.resonance, lastParams.resonance, JunoSysEx::VCF_RES);
         checkSend(currentParams.envAmount, lastParams.envAmount, JunoSysEx::VCF_ENV);
         checkSend(currentParams.lfoToVCF, lastParams.lfoToVCF, JunoSysEx::VCF_LFO);
         checkSend(currentParams.kybdTracking, lastParams.kybdTracking, JunoSysEx::VCF_KYBD);
         checkSend(currentParams.vcaLevel, lastParams.vcaLevel, JunoSysEx::VCA_LEVEL);
         checkSend(currentParams.attack, lastParams.attack, JunoSysEx::ENV_A);
         checkSend(currentParams.decay, lastParams.decay, JunoSysEx::ENV_D);
         checkSend(currentParams.sustain, lastParams.sustain, JunoSysEx::ENV_S);
         checkSend(currentParams.release, lastParams.release, JunoSysEx::ENV_R);
         checkSend(currentParams.subOscLevel, lastParams.subOscLevel, JunoSysEx::DCO_SUB);
         
         // Switches 1
         auto packSw1 = [](const SynthParams& p) -> int {
             int val = 0;
             if (p.dcoRange == 0) val |= 1<<0; else if (p.dcoRange == 1) val |= 1<<1; else if (p.dcoRange == 2) val |= 1<<2;
             if (p.pulseOn) val |= 1<<3;
             if (p.sawOn) val |= 1<<4;
             // In hardware:
             // 00 = I+II (Off? No, I and II both off means OFF)
             // But let's look at logic:
             // Bit 5 (0x20): 0=Chorus On, 1=Chorus Off
             // Bit 6 (0x40): 0=II, 1=I (Only relevant if Bit 5 is 0)
             
             bool anyChorus = p.chorus1 || p.chorus2;
             bool isI = p.chorus1; // If I is true.
             
             // If I is true, Bit 6 should be 1.
             // If II is true (and I false), Bit 6 should be 0.
             // If both? I+II is typically a distinct mode, but here we treat parameter state.
             
             if (!anyChorus) val |= (1<<5); // Set Bit 5 high for OFF
             
             if (isI) val |= (1<<6); // Set Bit 6 high for I
             return val;
         };
         int s1cur = packSw1(currentParams);
         int s1last = packSw1(lastParams);
         if (s1cur != s1last) midiOutBuffer.addEvent(JunoSysEx::createParamChange(midiChannel - 1, JunoSysEx::SWITCHES_1, s1cur), 0);
         
         // Switches 2
         auto packSw2 = [](const SynthParams& p) -> int {
             int val = 0;
             if (p.pwmMode == 1) val |= 1<<0;
             if (p.vcaMode == 1) val |= 1<<1;
             if (p.vcfPolarity == 1) val |= 1<<2;
             int hpf = 3 - p.hpfFreq;
             val |= (hpf & 0x03) << 3;
             return val;
         };
         int s2cur = packSw2(currentParams);
         int s2last = packSw2(lastParams);
         if (s2cur != s2last) midiOutBuffer.addEvent(JunoSysEx::createParamChange(midiChannel - 1, JunoSysEx::SWITCHES_2, s2cur), 0);
    }
    
    lastParams = currentParams; // Update history for next block
    
    applyPerformanceModulations(currentParams);
    voiceManager.updateParams(currentParams);
    
    // [Fidelidad] Portamento and Voice Manager global updates
    voiceManager.setPortamentoEnabled(currentParams.portamentoOn);
    voiceManager.setPortamentoTime(currentParams.portamentoTime);
    voiceManager.setPortamentoLegato(currentParams.portamentoLegato);

    // [Fidelidad] POWER SUPPLY SAG (Pitch drops with voice attacks)
    float envSum = voiceManager.getTotalEnvelopeLevel();
    int activeCount = voiceManager.getActiveVoiceCount();
    float targetSag = 0.0001f * activeCount * envSum;
    currentPowerSag += (targetSag - currentPowerSag) * 0.05f; // Fast sag
    
    // [Senior Audit] GLOBAL THERMAL DRIFT (Shared DAC)
    // Simulates the single multiplexed DAC drifting over time/temperature
    if (++thermalCounter > 1024) {
        thermalCounter = 0;
        thermalTarget = (chorusNoiseGen.nextFloat() * 2.0f - 1.0f) * 1.5f; // +/- 1.5 cents range
        
        // [Senior Audit] STARTUP TUNING ANIMATION
        // Simulate oscillator warm-up instability in first 3 seconds
        if (warmUpTime < 3.0) {
            warmUpTime += 1024.0 / getSampleRate();
            thermalTarget = (chorusNoiseGen.nextFloat() * 2.0f - 1.0f) * 50.0f; // Massive drift +/- 50 cents
        }
    }
    globalDriftAudible += (thermalTarget - globalDriftAudible) * 0.0005f; // Slow thermal inertia
    currentParams.thermalDrift = globalDriftAudible;
    
    // [Fidelidad] CHORUS LFO LEAKAGE (Small FM drift)
    bool c1_main = apvts.getRawParameterValue("chorus1")->load() > 0.5f;
    bool c2_main = apvts.getRawParameterValue("chorus2")->load() > 0.5f;
    
    float sr_main = (float)getSampleRate();
    chorusLfoPhaseI += JunoChorusConstants::kRateI / sr_main;
    if (chorusLfoPhaseI >= 1.0f) chorusLfoPhaseI -= 1.0f;
    chorusLfoPhaseII += JunoChorusConstants::kRateII / sr_main;
    if (chorusLfoPhaseII >= 1.0f) chorusLfoPhaseII -= 1.0f;
    
    float chorusLeak = 0.0f;
    if (c1_main || c2_main) {
        float lfoVal = (c1_main && !c2_main) ? (2.0f * std::abs(2.0f * (chorusLfoPhaseI - 0.5f)) - 1.0f) :
                                               (2.0f * std::abs(2.0f * (chorusLfoPhaseII - 0.5f)) - 1.0f);
        chorusLeak = lfoVal * 0.0007f; // 0.7 cent FM (approx)
    }

    float finalBender = currentParams.benderValue - currentPowerSag + chorusLeak;
    
    // [Fidelidad] WARM-UP DRIFT (+0.7 cents over 3 mins)
    // Formula: 0.0007 * (1 - exp(-t/180))
    // We update this per block (approx 170 Hz)
    if (warmUpTime < 180.0) {
        warmUpTime += (double)buffer.getNumSamples() / getSampleRate();
        float driftFactor = 1.0f - (float)std::exp(-warmUpTime / 45.0f); // Faster curve for demo perceivability
        globalDriftAudible = 0.0007f * driftFactor;
    }
    finalBender += globalDriftAudible;

    voiceManager.setBenderAmount(finalBender);

    float ratio = JunoTimeCurves::kLfoMaxHz / JunoTimeCurves::kLfoMinHz;
    float lfoRateHz = JunoTimeCurves::kLfoMinHz * std::pow(ratio, currentParams.lfoRate);
    float phaseIncrement = (lfoRateHz / (float)getSampleRate());
    float lfoDelaySeconds = currentParams.lfoDelay * 5.0f;
    float delayIncrement = (lfoDelaySeconds > 0.001f) ? (1.0f / (lfoDelaySeconds * (float)getSampleRate())) : 1.0f;
    bool anyHeld = voiceManager.isAnyNoteHeld();
    if (anyHeld && !wasAnyNoteHeld) masterLfoDelayEnvelope = 0.0f;
    wasAnyNoteHeld = anyHeld;
    
    int numSamples = buffer.getNumSamples();
    // [Safety] lfoBuffer is pre-allocated in prepareToPlay.
    // If block size changes surprisingly, we resize, but this should be rare.
    if (lfoBuffer.size() < (size_t)numSamples) lfoBuffer.resize(numSamples);

    for (int i = 0; i < numSamples; ++i) {
        masterLfoPhase += phaseIncrement;
        if (masterLfoPhase >= 1.0f) masterLfoPhase -= 1.0f;
        
        if (anyHeld) {
            masterLfoDelayEnvelope += delayIncrement;
            if (masterLfoDelayEnvelope > 1.0f) masterLfoDelayEnvelope = 1.0f;
        } else {
            masterLfoDelayEnvelope = 0.0f;
        }

        float lfoTri = 2.0f * std::abs(2.0f * (masterLfoPhase - 0.5f)) - 1.0f;
        lfoBuffer[i] = lfoTri * masterLfoDelayEnvelope;
    }

    buffer.clear();
    voiceManager.renderNextBlock(buffer, 0, numSamples, lfoBuffer);

    // [Fidelidad] "ALIVE" NOISE FLOOR (-86dB white noise)
    // El hardware real tiene un ruido de fondo constante de ~‑86 dBFS.
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        auto* chData = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i) {
            // [Fidelidad] Noise Floor + DC Offset (0.8mV)
            // This prevents "digital black" and keeps the DAW from killing the tail prematurely.
            chData[i] += (chorusNoiseGen.nextFloat() * 2.0f - 1.0f) * 0.00005f + 0.0008f; 
        }
    }

    // [Fidelidad] POWER-ON POP (Easter Egg)
    // El 8253 arranca en 0 y todas las voces pegan un saw-up.
    // Hacemos un fade-in de 30ms en el primer noteOn.
    // [Fidelidad] POWER-ON POP (Easter Egg)
    double sr = getSampleRate(); // [Fix] Define sr
    if (powerOnDelaySamples < (int)(0.030f * sr)) {
        float g = (float)powerOnDelaySamples / (0.030f * (float)sr);
        buffer.applyGain(0, numSamples, g);
        powerOnDelaySamples += numSamples;
    }

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    // [Fix] Re-define c1/c2 locally as they were used but removed
    bool c1 = apvts.getRawParameterValue("chorus1")->load() > 0.5f;
    bool c2 = apvts.getRawParameterValue("chorus2")->load() > 0.5f;

    // [Fidelidad] CHORUS BUMP & SWITCH CLICK PROTECTION
    // Detect mode change and trigger fade-out/fade-in cycle
    int targetMode = 0;
    // [Senior Audit] No Fade - Authentic Relay Click
    // Remove fade logic which was causing reported lag
    
    // Logic for mode selection was redundant. simplified here:
    if (c1 && !c2) targetMode = 1;
    else if (!c1 && c2) targetMode = 2;
    else if (c1 && c2) targetMode = 3;
    
    // Relay behavior: Instant switch
    chorusFade = (targetMode > 0) ? 1.0f : 0.0f; 
    
    bool safeC1 = (targetMode == 1 || targetMode == 3);
    bool safeC2 = (targetMode == 2 || targetMode == 3);

    if (safeC1 || safeC2) {
         float noiseMultiplier = 1.0f;
         if (safeC1 && !safeC2) { chorus.setRate(JunoChorusConstants::kRateI); chorus.setDepth(JunoChorusConstants::kDepthI); chorus.setMix(0.5f); chorus.setCentreDelay(JunoChorusConstants::kDelayI); noiseMultiplier = 1.0f; }
         else if (!safeC1 && safeC2) { chorus.setRate(JunoChorusConstants::kRateII); chorus.setDepth(JunoChorusConstants::kDepthII); chorus.setMix(0.5f); chorus.setCentreDelay(JunoChorusConstants::kDelayII); noiseMultiplier = 1.5f; }
         else { chorus.setRate(JunoChorusConstants::kRateIII); chorus.setDepth(JunoChorusConstants::kDepthIII); chorus.setMix(0.5f); chorus.setCentreDelay(JunoChorusConstants::kDelayIII); noiseMultiplier = 1.2f; }
         
         // [Senior Audit] COMPANDER - COMPRESSOR (2:1)
         // Simple RMS-based compressor or fixed non-linear gain reduction
         // NE570 Compander: Input -> Compressed -> BBD -> Expander -> Output
         for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
             auto* data = buffer.getWritePointer(ch);
             for (int i = 0; i < buffer.getNumSamples(); ++i) {
                 // Simple 2:1 compression (Square root approx for signal > threshold)
                 // This boosts low levels and tames high levels before BBD noise floor
                 float s = data[i];
                 float sign = (s >= 0.0f) ? 1.0f : -1.0f;
                 float absS = std::abs(s);
                 // Soft knee compression
                 if (absS > 0.1f) data[i] = sign * (0.1f + (absS - 0.1f) * 0.5f); 
             }
         }

         chorusPreEmphasisFilter.process(context);

         // Generate and filter noise in separate buffer
         chorusNoiseBuffer.clear();
         auto* nl = chorusNoiseBuffer.getWritePointer(0);
         auto* nr = chorusNoiseBuffer.getNumChannels() > 1 ? chorusNoiseBuffer.getWritePointer(1) : nullptr;
         for (int i = 0; i < buffer.getNumSamples(); ++i) {
             float noise = (chorusNoiseGen.nextFloat() * 2.0f - 1.0f) * JunoChorusConstants::kNoiseLevel * noiseMultiplier;
             // [Senior Audit] CLOCK BLEED (High frequency constant tone ~12kHz leaches into audio)
             // MN3009 Clock frequency varies but audible artifacts around 10-12kHz exist in mode 1
             if (safeC1) noise += std::sin(i * 0.5f) * 0.00005f; // Slight whine
             nl[i] = noise;
             if (nr) nr[i] = noise;
         }
         juce::dsp::AudioBlock<float> noiseBlock(chorusNoiseBuffer);
         juce::dsp::AudioBlock<float> activeNoiseBlock = noiseBlock.getSubBlock(0, buffer.getNumSamples());
         juce::dsp::ProcessContextReplacing<float> noiseContext(activeNoiseBlock);
         chorusNoiseFilter.process(noiseContext);
         
         buffer.addFrom(0, 0, chorusNoiseBuffer, 0, 0, buffer.getNumSamples());
         if(buffer.getNumChannels() > 1 && chorusNoiseBuffer.getNumChannels() > 1) 
             buffer.addFrom(1, 0, chorusNoiseBuffer, 1, 0, buffer.getNumSamples());
         
         // [Senior Audit] THD (MN3009 Distortion)
         if (targetMode == 2 || targetMode == 3) { // More prominent in Mode II/III
             for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
                 auto* data = buffer.getWritePointer(ch);
                 for (int i = 0; i < buffer.getNumSamples(); ++i) {
                     float x = data[i];
                     if (std::abs(x) > 0.6f) data[i] = x - 0.008f * (x * x * x); // 0.8% THD approx
                 }
             }
         }

         chorus.process(context);
         
         chorusDeEmphasisFilter.process(context);

         // [Senior Audit] COMPANDER - EXPANDER (1:2)
         // Restores dynamic range and suppresses BBD noise
         for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
             auto* data = buffer.getWritePointer(ch);
             for (int i = 0; i < buffer.getNumSamples(); ++i) {
                 float s = data[i];
                 float sign = (s >= 0.0f) ? 1.0f : -1.0f;
                 float absS = std::abs(s);
                 // Expansion
                 if (absS > 0.1f) data[i] = sign * (0.1f + (absS - 0.1f) * 2.0f); 
             }
         }
         
         // [Senior Audit] CROSSTALK (-30dB)
         // Narrow stereo image
         if (buffer.getNumChannels() > 1) {
             auto* l = buffer.getWritePointer(0);
             auto* r = buffer.getWritePointer(1);
             for (int i = 0; i < buffer.getNumSamples(); ++i) {
                 float vL = l[i];
                 float vR = r[i];
                 l[i] = vL + vR * 0.03f; // ~30dB crosstalk
                 r[i] = vR + vL * 0.03f;
             }
         }
    }
    
    if (midiOutEnabled) { midiMessages.addEvents(midiOutBuffer, 0, buffer.getNumSamples(), 0); midiOutBuffer.clear(); }
}

void SimpleJuno106AudioProcessor::enterTestMode(bool enter) { isTestMode = enter; }
#include "TestPrograms.h"
void SimpleJuno106AudioProcessor::triggerTestProgram(int bankIndex) {
    if (!isTestMode || bankIndex < 0 || bankIndex >= 8) return;
    const auto prog = getTestProgram(bankIndex);
    auto setVal = [&](juce::String id, float val) { if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(val); };
    auto setInt = [&](juce::String id, int val) { if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1((float)val)); };
    auto setBool = [&](juce::String id, bool val) { if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(val ? 1.0f : 0.0f); };
    setVal("lfoRate", prog.lfoRate); setVal("lfoDelay", prog.lfoDelay); setVal("lfoToDCO", prog.lfoToDCO);
    setInt("dcoRange", prog.dcoRange); setBool("sawOn", prog.sawOn); setBool("pulseOn", prog.pulseOn);
    setVal("pwm", prog.pwm); setInt("pwmMode", prog.pwmMode); setVal("subOsc", prog.subOsc); setVal("noise", prog.noise);
    setInt("hpfFreq", prog.hpfFreq); setVal("vcfFreq", prog.vcfFreq); setVal("resonance", prog.resonance);
    setVal("envAmount", prog.envAmount); setVal("lfoToVCF", prog.lfoToVCF); setVal("kybdTracking", prog.kybdTracking);
    setInt("vcfPolarity", prog.vcfPolarity); setInt("vcaMode", prog.vcaMode); setVal("vcaLevel", prog.vcaLevel);
    setVal("attack", prog.attack); setVal("decay", prog.decay); setVal("sustain", prog.sustain); setVal("release", prog.release);
    setBool("chorus1", prog.chorus1); setBool("chorus2", prog.chorus2);
}

void SimpleJuno106AudioProcessor::handleNoteOn(juce::MidiKeyboardState*, int /*channel*/, int midiNoteNumber, float velocity) { voiceManager.noteOn(0, midiNoteNumber, velocity); }
void SimpleJuno106AudioProcessor::handleNoteOff(juce::MidiKeyboardState*, int /*channel*/, int midiNoteNumber, float /*velocity*/) { performanceState.handleNoteOff(midiNoteNumber, voiceManager); }

SynthParams SimpleJuno106AudioProcessor::getMirrorParameters() {
    SynthParams p;
    // [Optimization] Fast pointer access (No string lookups)
    
    p.dcoRange = (int)fmtDcoRange->load(); 
    p.sawOn = fmtSawOn->load() > 0.5f; 
    p.pulseOn = fmtPulseOn->load() > 0.5f;
    p.pwmAmount = fmtPwm->load(); 
    p.pwmMode = (int)fmtPwmMode->load(); 
    p.subOscLevel = fmtSubOsc->load();
    p.noiseLevel = fmtNoise->load(); 
    p.lfoToDCO = fmtLfoToDCO->load(); 
    p.hpfFreq = (int)fmtHpfFreq->load();
    p.vcfFreq = fmtVcfFreq->load(); 
    p.resonance = fmtResonance->load(); 
    p.envAmount = fmtEnvAmount->load();
    p.lfoToVCF = fmtLfoToVCF->load(); 
    p.kybdTracking = fmtKybdTracking->load(); 
    p.vcfPolarity = (int)fmtVcfPolarity->load();
    p.vcaMode = (int)fmtVcaMode->load(); 
    p.vcaLevel = fmtVcaLevel->load(); 
    p.attack = fmtAttack->load();
    p.decay = fmtDecay->load(); 
    p.sustain = fmtSustain->load(); 
    p.release = fmtRelease->load();
    p.lfoRate = fmtLfoRate->load(); 
    p.lfoDelay = fmtLfoDelay->load();
    p.chorus1 = fmtChorus1->load() > 0.5f; 
    p.chorus2 = fmtChorus2->load() > 0.5f;
    p.polyMode = (int)fmtPolyMode->load(); 
    p.portamentoTime = fmtPortTime->load();
    p.portamentoOn = fmtPortOn->load() > 0.5f; 
    p.portamentoLegato = fmtPortLegato->load() > 0.5f;
    p.benderValue = fmtBender->load(); 
    p.benderToDCO = fmtBenderDCO->load();
    p.benderToVCF = fmtBenderVCF->load(); 
    p.benderToLFO = fmtBenderLFO->load();
    p.tune = fmtTune->load(); 
    p.midiChannel = midiChannel; 
    return p;
}

void SimpleJuno106AudioProcessor::updateParamsFromAPVTS() {
    currentParams = getMirrorParameters();
    midiOutEnabled = apvts.getRawParameterValue("midiOut")->load() > 0.5f;
    lastParams = currentParams;
}

juce::MidiMessage SimpleJuno106AudioProcessor::getCurrentSysExData() {
    return sysExEngine.makePatchDump(midiChannel - 1, currentParams);
}

void SimpleJuno106AudioProcessor::applyPerformanceModulations(SynthParams& p) {
    float modWheel = p.benderToLFO;
    p.lfoToDCO = juce::jlimit(0.0f, 1.0f, p.lfoToDCO + modWheel);
    p.vcfLFOAmount = juce::jlimit(0.0f, 1.0f, p.lfoToVCF + modWheel);
}

void SimpleJuno106AudioProcessor::sendPatchDump() { if (midiOutEnabled) midiOutBuffer.addEvent(sysExEngine.makePatchDump(midiChannel - 1, currentParams), 0); }
void SimpleJuno106AudioProcessor::sendManualMode() { if (midiOutEnabled) midiOutBuffer.addEvent(JunoSysEx::createManualMode(midiChannel - 1), 0); }
void SimpleJuno106AudioProcessor::triggerPanic() {
    for (int n = 0; n < 128; ++n) performanceState.handleNoteOff(n, voiceManager);
}

void SimpleJuno106AudioProcessor::loadPreset(int index) {
    if (presetManager) {
        presetManager->setCurrentPreset(index);
        // [Fidelidad] No resetear voces - permitir que las colas de release sigan con los nuevos parámetros
        // voiceManager.resetAllVoices(); 
        auto state = presetManager->getCurrentPresetState();
        if (state.isValid()) {
            for (int i = 0; i < state.getNumProperties(); ++i) {
                auto propName = state.getPropertyName(i).toString();
                if (auto* p = apvts.getParameter(propName)) {
                    auto prop = state.getProperty(propName);
                    if (prop.isDouble() || prop.isInt() || prop.isBool()) {
                         float val = static_cast<float>(prop);
                         p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(val));
                    }
                }
            }
            updateParamsFromAPVTS(); 
            // [Fix] Removed direct voiceManager.updateParams/forceUpdate from UI thread
            // processBlock will handle the distribution consistently.
            lastParamsChangeCounter++;
        }
    }
}

PresetManager* SimpleJuno106AudioProcessor::getPresetManager() { return presetManager.get(); }

juce::AudioProcessorValueTreeState::ParameterLayout SimpleJuno106AudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    auto makeParam = [](juce::String id, juce::String name, float min, float max, float def) { return std::make_unique<juce::AudioParameterFloat>(id, name, min, max, def); };
    auto makeIntParam = [](juce::String id, juce::String name, int min, int max, int def) { return std::make_unique<juce::AudioParameterInt>(id, name, min, max, def); };
    auto makeBool = [](juce::String id, juce::String name, bool def) { return std::make_unique<juce::AudioParameterBool>(id, name, def); };
    params.push_back(makeIntParam("dcoRange", "DCO Range", 0, 2, 1));
    params.push_back(makeBool("sawOn", "DCO Saw", true));
    params.push_back(makeBool("pulseOn", "DCO Pulse", false));
    params.push_back(makeParam("pwm", "PWM Level", 0.0f, 1.0f, 0.0f));
    params.push_back(makeIntParam("pwmMode", "PWM Mode", 0, 1, 0));
    params.push_back(makeParam("subOsc", "Sub Osc Level", 0.0f, 1.0f, 0.0f));
    params.push_back(makeParam("noise", "Noise Level", 0.0f, 1.0f, 0.0f));
    params.push_back(makeParam("lfoToDCO", "LFO to DCO", 0.0f, 1.0f, 0.0f));
    params.push_back(makeIntParam("hpfFreq", "HPF Freq", 0, 3, 0));
    params.push_back(makeParam("vcfFreq", "VCF Freq", 0.0f, 1.0f, 1.0f));
    params.push_back(makeParam("resonance", "Resonance", 0.0f, 1.0f, 0.0f));
    params.push_back(makeParam("envAmount", "Env Amount", 0.0f, 1.0f, 0.0f));
    params.push_back(makeIntParam("vcfPolarity", "VCF Polarity", 0, 1, 0));
    params.push_back(makeParam("kybdTracking", "VCF Kykd Track", 0.0f, 1.0f, 0.0f));
    params.push_back(makeParam("lfoToVCF", "LFO to VCF", 0.0f, 1.0f, 0.0f));
    params.push_back(makeIntParam("vcaMode", "VCA Mode", 0, 1, 0));
    params.push_back(makeParam("vcaLevel", "VCA Level", 0.0f, 1.0f, 1.0f));
    params.push_back(makeParam("attack", "Attack", 0.0f, 1.0f, 0.0f));
    params.push_back(makeParam("decay", "Decay", 0.0f, 1.0f, 0.0f));
    params.push_back(makeParam("sustain", "Sustain", 0.0f, 1.0f, 1.0f));
    params.push_back(makeParam("release", "Release", 0.0f, 1.0f, 0.0f));
    params.push_back(makeParam("lfoRate", "LFO Rate", 0.0f, 1.0f, 0.5f));
    params.push_back(makeParam("lfoDelay", "LFO Delay", 0.0f, 1.0f, 0.0f));
    params.push_back(makeBool("chorus1", "Chorus I", false));
    params.push_back(makeBool("chorus2", "Chorus II", false));
    params.push_back(makeIntParam("polyMode", "Poly Mode", 1, 3, 1));
    params.push_back(makeParam("portamentoTime", "Portamento Time", 0.0f, 1.0f, 0.0f));
    params.push_back(makeBool("portamentoOn", "Portamento On", false));
    params.push_back(makeBool("portamentoLegato", "Portamento Legato", false));
    params.push_back(makeParam("bender", "Bender", -1.0f, 1.0f, 0.0f));
    params.push_back(makeParam("benderToDCO", "Bender to DCO", 0.0f, 1.0f, 0.0f));
    params.push_back(makeParam("benderToVCF", "Bender to VCF", 0.0f, 1.0f, 0.0f));
    params.push_back(makeParam("benderToLFO", "Bender to LFO", 0.0f, 1.0f, 0.0f));
    params.push_back(makeParam("tune", "Master Tune", -50.0f, 50.0f, 0.0f));
    params.push_back(makeBool("midiOut", "MIDI Out Enabled", false));
    return { params.begin(), params.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SimpleJuno106AudioProcessor(); }
bool SimpleJuno106AudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* SimpleJuno106AudioProcessor::createEditor() { return new SimpleJuno106AudioProcessorEditor (*this); }

void SimpleJuno106AudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}
void SimpleJuno106AudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr) if (xmlState->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        updateParamsFromAPVTS();
        voiceManager.updateParams(currentParams);
        voiceManager.forceUpdate();
    }
}
