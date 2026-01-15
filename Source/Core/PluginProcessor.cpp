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
      apvts(*this, &undoManager, "Parameters", createParameterLayout())
{
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
    keyboardState.addListener(this);

    keyboardState.addListener(this);

    // [Optimization] Initialize Cached Pointers
    auto getParam = [&](const juce::String& id) {
        auto* p = apvts.getRawParameterValue(id);
        if (p == nullptr) { DBG("!!! CRITICAL ERROR: Parameter NOT FOUND: " + id); }
        jassert(p != nullptr);
        return p;
    };

    fmtDcoRange = getParam("dcoRange");
    fmtSawOn = getParam("sawOn");
    fmtPulseOn = getParam("pulseOn");
    fmtPwm = getParam("pwm");
    fmtPwmMode = getParam("pwmMode");
    fmtSubOsc = getParam("subOsc");
    fmtNoise = getParam("noise");
    fmtLfoToDCO = getParam("lfoToDCO");
    fmtHpfFreq = getParam("hpfFreq");
    fmtVcfFreq = getParam("vcfFreq");
    fmtResonance = getParam("resonance");
    fmtEnvAmount = getParam("envAmount");
    fmtVcfPolarity = getParam("vcfPolarity");
    fmtKybdTracking = getParam("kybdTracking");
    fmtLfoToVCF = getParam("lfoToVCF");
    fmtVcaMode = getParam("vcaMode");
    fmtVcaLevel = getParam("vcaLevel");
    fmtAttack = getParam("attack");
    fmtDecay = getParam("decay");
    fmtSustain = getParam("sustain");
    fmtRelease = getParam("release");
    fmtLfoRate = getParam("lfoRate");
    fmtLfoDelay = getParam("lfoDelay");
    fmtChorus1 = getParam("chorus1");
    fmtChorus2 = getParam("chorus2");
    fmtPolyMode = getParam("polyMode");
    fmtPortTime = getParam("portamentoTime");
    fmtPortOn = getParam("portamentoOn");
    fmtPortLegato = getParam("portamentoLegato");
    fmtBender = getParam("bender");
    fmtBenderDCO = getParam("benderToDCO");
    fmtBenderVCF = getParam("benderToVCF");
    fmtBenderLFO = getParam("benderToLFO");
    fmtTune = getParam("tune");
    fmtMasterVol = getParam("masterVolume");
    fmtMidiOut = getParam("midiOut");
    DBG("SimpleJuno106AudioProcessor::Constructor END");
}

SimpleJuno106AudioProcessor::~SimpleJuno106AudioProcessor() {
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
    
    *chorusPreEmphasisFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, 8000.0f, 0.707f, 1.5f);
    *chorusDeEmphasisFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, 12000.0f, 0.707f);
    *chorusNoiseFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, 8000.0f, 0.707f);

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
    const double sr = getSampleRate();

    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i) 
        buffer.clear (i, 0, numSamples);

    keyboardState.processNextMidiBuffer (midiMessages, 0, numSamples, true);

    // 1. MIDI Handling
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

    // 2. Parameter Mirroring & SysEx MIDI Out
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

        // Pack Switches 1/2 logic
        auto packSw1 = [](const SynthParams& p) -> int {
            int val = 0;
            if (p.dcoRange == 0) val |= 1<<0; else if (p.dcoRange == 1) val |= 1<<1; else if (p.dcoRange == 2) val |= 1<<2;
            if (p.pulseOn) val |= 1<<3;
            if (p.sawOn) val |= 1<<4;
            if (!(p.chorus1 || p.chorus2)) val |= (1<<5);
            if (p.chorus1) val |= (1<<6);
            return val;
        };
        int s1cur = packSw1(currentParams);
        int s1last = packSw1(lastParams);
        if (s1cur != s1last) {
            midiOutBuffer.addEvent(JunoSysEx::createParamChange(midiChannel - 1, JunoSysEx::SWITCHES_1, s1cur), 0);
        }

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
        if (s2cur != s2last) {
            midiOutBuffer.addEvent(JunoSysEx::createParamChange(midiChannel - 1, JunoSysEx::SWITCHES_2, s2cur), 0);
        }

        // Add events to host output
        int eventsSent = 0;
        for (const auto metadata : midiOutBuffer) {
            if (eventsSent >= 2) break;
            midiMessages.addEvent(metadata.getMessage(), metadata.samplePosition);
            eventsSent++;
        }
        midiOutBuffer.clear();
    }
    lastParams = currentParams;

    // 3. DSP Modulations & Voice Updates
    applyPerformanceModulations(currentParams);
    
    // [Global Thermal Drift]
    if (++thermalCounter > 1024) {
        thermalCounter = 0;
        thermalTarget = (chorusNoiseGen.nextFloat() * 2.0f - 1.0f) * 1.5f;
    }
    globalDriftAudible += (thermalTarget - globalDriftAudible) * 0.0005f;
    currentParams.thermalDrift = globalDriftAudible;

    voiceManager.updateParams(currentParams);
    voiceManager.setPortamentoEnabled(currentParams.portamentoOn);
    voiceManager.setPortamentoTime(currentParams.portamentoTime);
    voiceManager.setPortamentoLegato(currentParams.portamentoLegato);
    voiceManager.setBenderAmount(currentParams.benderValue + globalDriftAudible);

    // 4. LFO Generation (Master)
    float ratio = JunoTimeCurves::kLfoMaxHz / JunoTimeCurves::kLfoMinHz;
    float lfoRateHz = JunoTimeCurves::kLfoMinHz * std::pow(ratio, (float)currentParams.lfoRate);
    float lfoDelaySeconds = currentParams.lfoDelay * 5.0f;
    float delayIncrement = (lfoDelaySeconds > 0.001f) ? (1.0f / (lfoDelaySeconds * (float)sr)) : 1.0f;
    
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

    // 5. Voice Rendering
    voiceManager.renderNextBlock(buffer, 0, numSamples, lfoBuffer);

    // 6. Global PSU Sag
    float envSum = voiceManager.getTotalEnvelopeLevel();
    float sagGain = 1.0f - (envSum * 0.025f);
    if (sagGain < 0.8f) sagGain = 0.8f;
    float masterVol = fmtMasterVol->load();
    buffer.applyGain(sagGain * masterVol);

    // 7. Chorus Processing
    if (currentParams.chorus1 || currentParams.chorus2) {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        chorusPreEmphasisFilter.process(context);
        
        juce::AudioBuffer<float> wetBuffer(2, numSamples);
        wetBuffer.clear();
        
        int targetMode = (currentParams.chorus1 && currentParams.chorus2) ? 3 : (currentParams.chorus1 ? 1 : 2);
        float phIncI = JunoChorusConstants::kRateI / (float)sr;
        float phIncII = JunoChorusConstants::kRateII / (float)sr;
        
        // [Fidelidad] Generate filtered chorus hiss
        for (int i = 0; i < numSamples; ++i) {
            chorusNoiseBuffer.setSample(0, i, chorusNoiseGen.nextFloat() * 2.0f - 1.0f);
            chorusNoiseBuffer.setSample(1, i, chorusNoiseGen.nextFloat() * 2.0f - 1.0f);
        }
        juce::dsp::AudioBlock<float> noiseBlock(chorusNoiseBuffer);
        juce::dsp::ProcessContextReplacing<float> noiseContext(noiseBlock);
        chorusNoiseFilter.process(noiseContext);
        
        // Noise levels: Mode II is slightly noiser (~6dB more? Let's use 0.0004 for I, 0.0008 for II)
        float noiseLevel = (targetMode == 2) ? 0.0008f : 0.0004f;
        if (targetMode == 3) noiseLevel = 0.0006f; // Mode I+II

        for (int i = 0; i < numSamples; ++i) {
            float dry = buffer.getSample(0, i);
            chorusLfoPhaseI = std::fmod(chorusLfoPhaseI + phIncI, 1.0f);
            chorusLfoPhaseII = std::fmod(chorusLfoPhaseII + phIncII, 1.0f);
            
            float lfoI = 2.0f * std::abs(2.0f * (chorusLfoPhaseI - 0.5f)) - 1.0f;
            float lfoII = 2.0f * std::abs(2.0f * (chorusLfoPhaseII - 0.5f)) - 1.0f;
            
            float w1 = 0.0f, w2 = 0.0f;
            if (targetMode == 1 || targetMode == 3) 
                w1 = chorus.processSample(dry, JunoChorusConstants::kDelayI + (lfoI * JunoChorusConstants::kDepthI * 2.0f));
            if (targetMode == 2 || targetMode == 3) 
                w2 = chorus2.processSample(dry, JunoChorusConstants::kDelayII + (lfoII * JunoChorusConstants::kDepthII * 2.0f));
            
            float wetMix = (targetMode == 3) ? (w1 + w2) * 0.707f : (targetMode == 1 ? w1 : w2);
            
            // Add Hiss to wet signal
            float hissL = chorusNoiseBuffer.getSample(0, i) * noiseLevel;
            float hissR = chorusNoiseBuffer.getSample(1, i) * noiseLevel;
            
            wetBuffer.setSample(0, i, wetMix + hissL);
            wetBuffer.setSample(1, i, -wetMix + hissR);
        }
        
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.addFrom(ch, 0, wetBuffer, ch, 0, numSamples, 1.0f);
            
        chorusDeEmphasisFilter.process(context);

        // Compander & Crosstalk
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            float* d = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i) {
                float s = d[i];
                if (std::abs(s) > 0.1f) 
                    d[i] = ((s >= 0.0f) ? 1.0f : -1.0f) * (0.1f + (std::abs(s) - 0.1f) * 2.0f);
            }
        }
        if (buffer.getNumChannels() > 1) {
            float* l = buffer.getWritePointer(0);
            float* r = buffer.getWritePointer(1);
            for (int i = 0; i < numSamples; ++i) {
                float vL = l[i], vR = r[i];
                l[i] = vL + vR * 0.03f;
                r[i] = vR + vL * 0.03f;
            }
        }
    }

    // 8. Power-On Pop & DC Block
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
    p.midiOut = fmtMidiOut->load() > 0.5f;
    
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
            
            // [Fix] Explicitly update local params for non-audio purposes (SysEx, etc)
            // And force voiceManager update
            updateParamsFromAPVTS(); 
            voiceManager.updateParams(currentParams);
            voiceManager.forceUpdate(); // Essential for VCF smoothing reset
            
            voiceManager.forceUpdate(); // Essential for VCF smoothing reset
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
    DBG("SimpleJuno106AudioProcessor::createEditor() START");
    auto* e = new SimpleJuno106AudioProcessorEditor (*this); 
    DBG("SimpleJuno106AudioProcessor::createEditor() END");
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
