#include "PluginProcessor.h"
#include <JuceHeader.h>
#include "PluginEditor.h"
#include "../UI/WebView/WebViewEditor.h"
#include "PresetManager.h"
#include "JunoConstants.h"

using namespace JunoConstants;

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
      apvts(*this, &undoManager, "Parameters", createParameterLayout())
{
    // Initialize Local Logger in user documents (portable, no hardcoded paths)
    auto logDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                      .getChildFile("JUNiO601").getChildFile("logs");
    logDir.createDirectory();
    auto logFile = logDir.getChildFile ("plugin_log.txt");
    
    auto* logger = new juce::FileLogger (logFile, "JUNiO 601 Log Started\n====================");
    juce::Logger::setCurrentLogger (logger);
    
    juce::Logger::writeToLog ("Log file location: " + logFile.getFullPathName());

#if JUCE_HEADLESS_PLUGIN
    DBG("SimpleJuno106AudioProcessor::Constructor START (HEADLESS=1)");
#else
    DBG("SimpleJuno106AudioProcessor::Constructor START (HEADLESS=0)");
#endif
    presetManager = std::make_unique<PresetManager>();
    DBG("SimpleJuno106AudioProcessor::PresetManager created");
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
    // keyboardState.addListener(this); // [Safety] Handled via processNextMidiBuffer for stability

    // [Optimization] Initialize Cached Pointers
    auto getParam = [&](const juce::String& id) {
        auto* p = apvts.getRawParameterValue(id);
        if (p == nullptr) { DBG("!!! CRITICAL ERROR: Parameter NOT FOUND: " + id); }
        jassert(p != nullptr);
        return p;
    };

    dcoRangeParam = getParam("dcoRange");
    sawOnParam = getParam("sawOn");
    pulseOnParam = getParam("pulseOn");
    pwmParam = getParam("pwm");
    pwmModeParam = getParam("pwmMode");
    subOscParam = getParam("subOsc");
    noiseParam = getParam("noise");
    lfoToDcoParam = getParam("lfoToDCO");
    hpfFreqParam = getParam("hpfFreq");
    vcfFreqParam = getParam("vcfFreq");
    resonanceParam = getParam("resonance");
    envAmountParam = getParam("envAmount");
    vcfPolarityParam = getParam("vcfPolarity");
    kybdTrackingParam = getParam("kybdTracking");
    lfoToVcfParam = getParam("lfoToVCF");
    vcaModeParam = getParam("vcaMode");
    vcaLevelParam = getParam("vcaLevel");
    attackParam = getParam("attack");
    decayParam = getParam("decay");
    sustainParam = getParam("sustain");
    releaseParam = getParam("release");
    lfoRateParam = getParam("lfoRate");
    lfoDelayParam = getParam("lfoDelay");
    chorus1Param = getParam("chorus1");
    chorus2Param = getParam("chorus2");
    polyModeParam = getParam("polyMode");
    portTimeParam = getParam("portamentoTime");
    portOnParam = getParam("portamentoOn");
    portLegatoParam = getParam("portamentoLegato");
    benderParam = getParam("bender");
    benderDcoParam = getParam("benderToDCO");
    benderVcfParam = getParam("benderToVCF");
    benderLfoParam = getParam("benderToLFO");
    tuneParam = getParam("tune");
    masterVolParam = getParam("masterVolume");
    midiOutParam = getParam("midiOut");
    lfoTrigParam = getParam("lfoTrig");
    DBG("SimpleJuno106AudioProcessor::Constructor END");
}

SimpleJuno106AudioProcessor::~SimpleJuno106AudioProcessor() {
    juce::Logger::setCurrentLogger (nullptr); // releases the FileLogger
}

const juce::String SimpleJuno106AudioProcessor::getName() const { return JucePlugin_Name; }
bool SimpleJuno106AudioProcessor::acceptsMidi() const { return true; }
bool SimpleJuno106AudioProcessor::producesMidi() const { return true; }
bool SimpleJuno106AudioProcessor::isMidiEffect() const { return false; }
// double SimpleJuno106AudioProcessor::getTailLengthSeconds() const { return 12.0; } // [Fidelidad] Máximo release 12s + chorus BBD delay
int SimpleJuno106AudioProcessor::getNumPrograms() { 
    static bool logged = false;
    if (!logged) { DBG("SimpleJuno106AudioProcessor::getNumPrograms() called"); logged = true; }
    return 1; 
}
int SimpleJuno106AudioProcessor::getCurrentProgram() { return 0; }
void SimpleJuno106AudioProcessor::setCurrentProgram (int index) { juce::ignoreUnused(index); }
const juce::String SimpleJuno106AudioProcessor::getProgramName (int index) { juce::ignoreUnused(index); return {}; }
void SimpleJuno106AudioProcessor::changeProgramName (int index, const juce::String& newName) { juce::ignoreUnused(index, newName); }

// [QA-SynthOps] Removed legacy audio-thread listener to avoid glitches



void SimpleJuno106AudioProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    DBG("SimpleJuno106AudioProcessor::prepareToPlay START (sr=" + juce::String(sr) + ")");
    // [QA-SynthOps] Commandment 4: Latency Reporting
    setLatencySamples(0);

    voiceManager.prepare(sr, samplesPerBlock);
    DBG("SimpleJuno106AudioProcessor::voiceManager prepared");
    juce::dsp::ProcessSpec spec { sr, (juce::uint32)samplesPerBlock, 2 };
    chorus.prepare(spec);
    chorus.reset();
    chorus2.prepare(spec); // [Fidelidad] Second BBD Line
    chorus2.reset();
    
    // [Safety] Pre-allocate LFO buffer
    lfoBuffer.resize(samplesPerBlock + 128); // Standard safety margin

    dcBlocker.prepare(spec); 
    *dcBlocker.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, 20.0f);
    
    chorusPreEmphasisFilter.prepare(spec);
    chorusDeEmphasisFilter.prepare(spec);
    chorusNoiseFilter.prepare(spec);
    chorusNoiseHPF.prepare(spec);
    
    *chorusPreEmphasisFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, 8000.0f, 0.707f, 1.5f);
    *chorusDeEmphasisFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, 12000.0f, 0.707f);
    
    // [Fidelity] Chorus Hiss EQ: HPF @ 2.5kHz (1-pole approx), LPF @ 11kHz (2-pole)
    *chorusNoiseFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, 11000.0f, 0.707f);
    *chorusNoiseHPF.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, 2500.0f, 0.707f);

    chorusNoiseBuffer.setSize(2, samplesPerBlock);

    masterLfoPhase = 0.0f; 
    masterLfoDelayEnvelope = 0.0f; 
    wasAnyNoteHeld = false;
    DBG("SimpleJuno106AudioProcessor::prepareToPlay END");
}

void SimpleJuno106AudioProcessor::releaseResources() {}

bool SimpleJuno106AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    return true;
}

void SimpleJuno106AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    static bool firstBlock = true;
    if (firstBlock) { DBG("SimpleJuno106AudioProcessor::processBlock FIRST CALL"); firstBlock = false; }

    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i) 
        buffer.clear (i, 0, numSamples);

    keyboardState.processNextMidiBuffer (midiMessages, 0, numSamples, true);

    handleMidiEvents(midiMessages);
    updateParamsAndModulations();
    renderAudio(buffer, numSamples);
    applyChorus(buffer, numSamples);
    processMasterEffects(buffer, numSamples);
}

void SimpleJuno106AudioProcessor::handleMidiEvents(juce::MidiBuffer& midiMessages) {
    for (const auto metadata : midiMessages) {
        const auto message = metadata.getMessage();
        if (message.isSysEx()) { sysExEngine.handleIncomingSysEx(message, currentParams); continue; }
        if (message.isController()) {
            if (message.getControllerNumber() == 1) { 
                if (auto* p = apvts.getParameter("benderToLFO")) p->setValueNotifyingHost(message.getControllerValue() / 127.0f); 
            }
            else if (message.getControllerNumber() == 64) {
                 int val = message.getControllerValue();
                 if (sustainInverted) val = 127 - val;
                 performanceState.handleSustain(val);
            }
            else midiLearnHandler.handleIncomingCC(message.getControllerNumber(), message.getControllerValue(), apvts);
            continue;
        }
        if (message.isPitchWheel()) {
            if (auto* p = apvts.getParameter("bender")) 
                p->setValueNotifyingHost(p->convertTo0to1(((float)message.getPitchWheelValue() / 8192.0f) - 1.0f));
            continue;
        }
        if (message.isNoteOn()) voiceManager.noteOn(message.getChannel(), message.getNoteNumber(), message.getVelocity());
        else if (message.isNoteOff()) performanceState.handleNoteOff(message.getNoteNumber(), voiceManager);
    }
    performanceState.flushSustain(voiceManager);
}

void SimpleJuno106AudioProcessor::updateParamsAndModulations() {
    using namespace JunoConstants;
    
    currentParams = getMirrorParameters();
    
    // [Senior Audit] Thread-Safe SysEx Generation & Rate Limiting
    if (currentParams.midiOut) {
        int msgsSent = 0;
        const int kMaxMsgsPerBlock = 2;
        auto checkSend = [&](float curr, float& last, int id) {
            if (msgsSent >= kMaxMsgsPerBlock) return;
            if (std::abs(curr - last) > 0.001f) {
                midiOutBuffer.addEvent(JunoSysEx::createParamChange(midiChannel - 1, id, (int)(curr * 127.0f)), 0);
                last = curr;
                msgsSent++;
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

        // [Hardware Authenticity] Pack Switches 1/2 logic
        auto packSw1 = [](const SynthParams& p) -> int {
            int val = (p.dcoRange & 0x07);
            if (p.pulseOn) val |= (1 << 3);
            if (p.sawOn)   val |= (1 << 4);
            if (p.chorus1 || p.chorus2) {
                val |= (1 << 5);
                if (p.chorus2) val |= (1 << 6);
            }
            return val;
        };
        int s1cur = packSw1(currentParams);
        int s1last = packSw1(lastParams);
        if (s1cur != s1last) {
            midiOutBuffer.addEvent(JunoSysEx::createParamChange(midiChannel - 1, JunoSysEx::SWITCHES_1, s1cur), 0);
        }

        auto packSw2 = [](const SynthParams& p) -> int {
            int val = 0;
            if (p.pwmMode == 1)     val |= (1 << 0);
            if (p.vcaMode == 1)     val |= (1 << 1);
            if (p.vcfPolarity == 1) val |= (1 << 2);
            int hwHpf = 3 - juce::jlimit(0, 3, p.hpfFreq);
            val |= (hwHpf & 0x03) << 3;
            return val;
        };
        int s2cur = packSw2(currentParams);
        int s2last = packSw2(lastParams);
        if (s2cur != s2last) {
            midiOutBuffer.addEvent(JunoSysEx::createParamChange(midiChannel - 1, JunoSysEx::SWITCHES_2, s2cur), 0);
        }
    }
    lastParams = currentParams;

    applyPerformanceModulations(currentParams);
    
    // [Global Thermal Drift]
    if (++thermalCounter > kThermalInertia) {
        thermalCounter = 0;
        thermalTarget = (chorusNoiseGen.nextFloat() * 2.0f - 1.0f) * kGlobalThermalDriftScale;
    }
    globalDriftAudible += (thermalTarget - globalDriftAudible) * kThermalMigration;
    currentParams.thermalDrift = globalDriftAudible;

    voiceManager.updateParams(currentParams);
    if (needsVoiceReset.exchange(false)) {
        voiceManager.forceUpdate();
    }

    voiceManager.setPortamentoEnabled(currentParams.portamentoOn);
    voiceManager.setPortamentoTime(currentParams.portamentoTime);
    voiceManager.setPortamentoLegato(currentParams.portamentoLegato);
    voiceManager.setBenderAmount(currentParams.benderValue + globalDriftAudible);
}

void SimpleJuno106AudioProcessor::renderAudio(juce::AudioBuffer<float>& buffer, int numSamples) {
    using namespace JunoConstants;
    const double sr = getSampleRate();

    // 4. LFO Generation (Master)
    float ratio = Curves::kLfoMaxHz / Curves::kLfoMinHz;
    float lfoRateHz = Curves::kLfoMinHz * std::pow(ratio, (float)currentParams.lfoRate);
    float lfoDelaySeconds = currentParams.lfoDelay * 5.0f;
    float delayIncrement = (lfoDelaySeconds > 0.001f) ? (1.0f / (lfoDelaySeconds * (float)sr)) : 1.0f;
    
    // [Fidelity] LFO Manual Trigger
    bool lfoTrigRequested = lfoTrigParam->load() > 0.5f;
    if (lfoTrigRequested) {
        masterLfoPhase = 0.0f;
        masterLfoDelayEnvelope = 1.0f; // Force instant onset
        lfoTrigParam->store(0.0f); // Reset after action
    }

    bool anyHeld = voiceManager.isAnyNoteHeld();
    if (anyHeld && !wasAnyNoteHeld) masterLfoDelayEnvelope = 0.0f;
    wasAnyNoteHeld = anyHeld;
    
    if (lfoBuffer.size() < (size_t)numSamples) lfoBuffer.resize(numSamples + 128);

    for (int i = 0; i < numSamples; ++i) {
        masterLfoPhase += (lfoRateHz / (float)sr);
        if (masterLfoPhase >= 1.0f) masterLfoPhase -= 1.0f;
        
        if (anyHeld) {
            masterLfoDelayEnvelope += delayIncrement;
            if (masterLfoDelayEnvelope > 1.0f) masterLfoDelayEnvelope = 1.0f;
        } else {
            masterLfoDelayEnvelope = 0.0f;
        }

        float lfoTri = 2.0f * std::abs(2.0f * (masterLfoPhase - 0.5f)) - 1.0f;
        float lfoTriStepped = std::floor(lfoTri * 15.99f) / 15.0f; 
        lfoBuffer[i] = lfoTriStepped * masterLfoDelayEnvelope;
    }

    voiceManager.renderNextBlock(buffer, 0, numSamples, lfoBuffer);
}

void SimpleJuno106AudioProcessor::applyChorus(juce::AudioBuffer<float>& buffer, int numSamples) {
    if (!(currentParams.chorus1 || currentParams.chorus2)) return;

    const double sr = getSampleRate();
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    chorusPreEmphasisFilter.process(context);
    
    juce::AudioBuffer<float> wetBuffer(2, numSamples);
    wetBuffer.clear();
        int targetMode = (currentParams.chorus1 && currentParams.chorus2) ? 3 : (currentParams.chorus1 ? 1 : 2);
    
    // [Fidelity] Authentic Chorus Rates
    float rateI = Chorus::kRateI;
    float rateII = Chorus::kRateII;
    if (targetMode == 3) {
        rateI *= 1.2f; // [Fidelity] I+II is faster
        rateII *= 1.2f;
    }
    
    float phIncI = rateI / (float)sr;
    float phIncII = rateII / (float)sr;
    
    // [Fidelity] Generate filtered chorus hiss
    for (int i = 0; i < numSamples; ++i) {
        chorusNoiseBuffer.setSample(0, i, chorusNoiseGen.nextFloat() * 2.0f - 1.0f);
        chorusNoiseBuffer.setSample(1, i, chorusNoiseGen.nextFloat() * 2.0f - 1.0f);
    }
    juce::dsp::AudioBlock<float> noiseBlock(chorusNoiseBuffer);
    juce::dsp::ProcessContextReplacing<float> noiseContext(noiseBlock);
    chorusNoiseHPF.process(noiseContext);
    chorusNoiseFilter.process(noiseContext);
    
    // [Fidelity] Authentic Hiss Levels
    float baseNoiseLevel = std::pow(10.0f, Chorus::kHissLevelHalfDb / 20.0f);
    if (targetMode == 2) baseNoiseLevel = std::pow(10.0f, Chorus::kHissLevelFullDb / 20.0f);
    if (targetMode == 3) baseNoiseLevel *= 1.2f; // More intense
    
    for (int i = 0; i < numSamples; ++i) {
        float dry = buffer.getSample(0, i);
        chorusLfoPhaseI = std::fmod(chorusLfoPhaseI + phIncI, 1.0f);
        chorusLfoPhaseII = std::fmod(chorusLfoPhaseII + phIncII, 1.0f);
        
        float lfoI = 2.0f * std::abs(2.0f * (chorusLfoPhaseI - 0.5f)) - 1.0f;
        float lfoII = 2.0f * std::abs(2.0f * (chorusLfoPhaseII - 0.5f)) - 1.0f;
        
        float w1 = 0.0f, w2 = 0.0f;
        // [Fidelity] Authentic Delay Ranges (~15ms base ±4ms)
        if (targetMode == 1 || targetMode == 3) 
            w1 = chorus.processSample(dry, Chorus::kDelayBaseMs + (lfoI * Chorus::kModDepthMs));
        if (targetMode == 2 || targetMode == 3) 
            w2 = chorus2.processSample(dry, Chorus::kDelayBaseMs + (lfoII * Chorus::kModDepthMs));
        
        float wetMix = (targetMode == 3) ? (w1 + w2) * 0.707f : (targetMode == 1 ? w1 : w2);
        
        float hissL = chorusNoiseBuffer.getSample(0, i) * baseNoiseLevel;
        float hissR = chorusNoiseBuffer.getSample(1, i) * baseNoiseLevel;
        
        wetBuffer.setSample(0, i, wetMix + hissL);
        wetBuffer.setSample(1, i, -wetMix + hissR);
    }

    
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.addFrom(ch, 0, wetBuffer, ch, 0, numSamples, 1.0f);
        
    chorusDeEmphasisFilter.process(context);
}

void SimpleJuno106AudioProcessor::processMasterEffects(juce::AudioBuffer<float>& buffer, int numSamples) {
    // 6. Global PSU Sag
    float envSum = voiceManager.getTotalEnvelopeLevel();
    float sagGain = 1.0f - (envSum * 0.025f);
    if (sagGain < 0.8f) sagGain = 0.8f;
    float masterVol = masterVolParam->load();
    buffer.applyGain(sagGain * masterVol);

    // Simple Soft Saturation (Master Stage)
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        float* d = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i) {
            d[i] = std::tanh(d[i] * 1.1f); 
        }
    }

    // Stereo Crosstalk (Analog leakage)
    if (buffer.getNumChannels() > 1) {
        float* l = buffer.getWritePointer(0);
        float* r = buffer.getWritePointer(1);
        for (int i = 0; i < numSamples; ++i) {
            float vL = l[i], vR = r[i];
            l[i] = vL + vR * 0.03f;
            r[i] = vR + vL * 0.03f;
        }
    }

    // 8. Power-On Pop & DC Block
    const double sr = getSampleRate();
    if (powerOnDelaySamples < (int)(0.080f * sr)) {
        if (powerOnDelaySamples == 0) {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) buffer.addSample(ch, 0, 4.0f);
        }
        powerOnDelaySamples += numSamples;
    }

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    dcBlocker.process(context);
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
    
    p.dcoRange = (int)dcoRangeParam->load(); 
    p.sawOn = sawOnParam->load() > 0.5f; 
    p.pulseOn = pulseOnParam->load() > 0.5f;
    p.pwmAmount = pwmParam->load(); 
    p.pwmMode = (int)pwmModeParam->load(); 
    p.subOscLevel = subOscParam->load();
    p.noiseLevel = noiseParam->load(); 
    p.lfoToDCO = lfoToDcoParam->load(); 
    p.hpfFreq = (int)hpfFreqParam->load();
    p.vcfFreq = vcfFreqParam->load(); 
    p.resonance = resonanceParam->load(); 
    p.envAmount = envAmountParam->load();
    p.lfoToVCF = lfoToVcfParam->load(); 
    p.kybdTracking = kybdTrackingParam->load(); 
    p.vcfPolarity = (int)vcfPolarityParam->load();
    p.vcaMode = (int)vcaModeParam->load(); 
    p.vcaLevel = vcaLevelParam->load(); 
    p.attack = attackParam->load();
    p.decay = decayParam->load(); 
    p.sustain = sustainParam->load(); 
    p.release = releaseParam->load();
    p.lfoRate = lfoRateParam->load(); 
    p.lfoDelay = lfoDelayParam->load();
    p.chorus1 = chorus1Param->load() > 0.5f; 
    p.chorus2 = chorus2Param->load() > 0.5f;
    p.polyMode = (int)polyModeParam->load(); 
    p.portamentoTime = portTimeParam->load();
    p.portamentoOn = portOnParam->load() > 0.5f; 
    p.portamentoLegato = portLegatoParam->load() > 0.5f;
    p.benderValue = benderParam->load(); 
    p.benderToDCO = benderDcoParam->load();
    p.benderToVCF = benderVcfParam->load(); 
    p.benderToLFO = benderLfoParam->load();
    p.tune = tuneParam->load(); 
    p.midiChannel = midiChannel; 
    p.midiOut = midiOutParam->load() > 0.5f;
    
    return p;
}

void SimpleJuno106AudioProcessor::updateParamsFromAPVTS() {
    currentParams = getMirrorParameters();
    lastParams = currentParams;
}


void SimpleJuno106AudioProcessor::applyPerformanceModulations(SynthParams& p) {
    float modWheel = p.benderToLFO;
    p.lfoToDCO = juce::jlimit(0.0f, 1.0f, p.lfoToDCO + modWheel * 0.3f);
    p.vcfLFOAmount = juce::jlimit(0.0f, 1.0f, p.lfoToVCF + modWheel);
}

void SimpleJuno106AudioProcessor::sendPatchDump() { sendSysEx(sysExEngine.makePatchDump(midiChannel - 1, currentParams)); }
void SimpleJuno106AudioProcessor::sendManualMode() { sendSysEx(JunoSysEx::createManualMode(midiChannel - 1)); }

juce::MidiMessage SimpleJuno106AudioProcessor::getCurrentSysExData() { 
    // [Fix] Live feedback: regenerate dump from current parameters cached from APVTS
    // This ensures the HEX display updates as the user moves controls.
    auto p = getMirrorParameters();
    return sysExEngine.makePatchDump(midiChannel - 1, p); 
}
void SimpleJuno106AudioProcessor::triggerPanic() {
    voiceManager.resetAllVoices(); // [Fidelidad] Deep Reset
    chorus.reset();
    chorus2.reset();
    performanceState.noteOffFifo.reset(); 
    performanceState.noteOffBuffer.fill(0); 
    midiOutBuffer.addEvent(juce::MidiMessage::allNotesOff(1), 0);
}

void SimpleJuno106AudioProcessor::triggerLFO() {
    if (lfoTrigParam) lfoTrigParam->store(1.0f);
}

void SimpleJuno106AudioProcessor::loadPreset(int index) {
    if (presetManager) {
        presetManager->setCurrentPreset(index);
        
        auto state = presetManager->getCurrentPresetState();
        if (state.isValid()) {
            // Suspend processing to avoid partial updates? No, just atomic updates.
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
            // [Fix] Thread-safe Preset Update via needsVoiceReset flag
            // Defer manual voiceManager alterations to the processBlock to prevent denormal crashes
            needsVoiceReset.store(true);
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
    params.push_back(makeParam("masterVolume", "Master Volume", 0.0f, 1.0f, 1.0f));
    params.push_back(makeBool("lfoTrig", "LFO Trigger", false));
    return { params.begin(), params.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { 
    DBG(">>> createPluginFilter() called <<<");
    return new SimpleJuno106AudioProcessor(); 
}
bool SimpleJuno106AudioProcessor::hasEditor() const { 
    DBG("SimpleJuno106AudioProcessor::hasEditor() query");
    return true; 
}
juce::AudioProcessorEditor* SimpleJuno106AudioProcessor::createEditor() { 
    juce::Logger::writeToLog("SimpleJuno106AudioProcessor::createEditor() START");
    
#if JUNO_USE_NATIVE_UI
    juce::Logger::writeToLog("Creating NATIVE Editor");
    auto* e = new SimpleJuno106AudioProcessorEditor (*this);
#else
    juce::Logger::writeToLog("Creating SPECTACULAR WEB Editor");
    auto* e = new WebViewEditor (*this);
#endif

    juce::Logger::writeToLog("SimpleJuno106AudioProcessor::createEditor() END");
    return e;
}

void SimpleJuno106AudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}
void SimpleJuno106AudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    DBG("SimpleJuno106AudioProcessor::setStateInformation START (" + juce::String(sizeInBytes) + " bytes)");
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr) if (xmlState->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        updateParamsFromAPVTS();
        voiceManager.updateParams(currentParams);
        voiceManager.forceUpdate();
    }
    DBG("SimpleJuno106AudioProcessor::setStateInformation END");
}
