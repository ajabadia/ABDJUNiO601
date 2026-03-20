#include "PluginProcessor.h"
#include <JuceHeader.h>
#if !JUCE_HEADLESS_PLUGIN
 #include "PluginEditor.h"
 #include "../UI/WebView/WebViewEditor.h"
#endif
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
    DBG("SimpleJuno106AudioProcessor::Constructor Body START");

    // 1. [CRITICAL] Initialize Cached Pointers FIRST
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
    fmtThermalDrift = getParam("thermalDrift");
    fmtUnisonWidth = getParam("unisonWidth");
    fmtUnisonDetune = getParam("unisonDetune");
    fmtChorusMix = getParam("chorusMix");
    fmtVcfPolarity = getParam("vcfPolarity");
    fmtKybdTracking = getParam("kybdTracking");
    fmtEnvAmount = getParam("envAmount");
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
    fmtMasterVolume = getParam("masterVolume");
    fmtMidiOut = getParam("midiOut");
    fmtLfoTrig = getParam("lfoTrig");

    fmtSustainInverted = getParam("sustainInverted");
    fmtChorusHiss = getParam("chorusHiss");
    fmtMidiFunction = getParam("midiFunction");
    fmtAftertouchToVCF = getParam("aftertouchToVCF");
    fmtMidiChannel = getParam("midiChannel");
    fmtBenderRange = getParam("benderRange");
    fmtVelocitySens = getParam("velocitySens");
    fmtLcdBrightness = getParam("lcdBrightness");
    fmtNumVoices = getParam("numVoices");
    // fmtUnisonWidth = getParam("unisonWidth"); // This was already here, but the diff implies it's moved/re-added. Keeping it here for now.

    DBG("SimpleJuno106AudioProcessor::Parameters cached DONE");

    // 2. Initialize secondary components
    presetManager = std::make_unique<PresetManager>();
    DBG("SimpleJuno106AudioProcessor::PresetManager created DONE");

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
    DBG("SimpleJuno106AudioProcessor::Constructor Body END");
}

SimpleJuno106AudioProcessor::~SimpleJuno106AudioProcessor() {
    juce::Logger::setCurrentLogger (nullptr); 
}

const juce::String SimpleJuno106AudioProcessor::getName() const { return JucePlugin_Name; }
bool SimpleJuno106AudioProcessor::acceptsMidi() const { return true; }
bool SimpleJuno106AudioProcessor::producesMidi() const { return true; }
bool SimpleJuno106AudioProcessor::isMidiEffect() const { return false; }

int SimpleJuno106AudioProcessor::getNumPrograms() { return 1; }
int SimpleJuno106AudioProcessor::getCurrentProgram() { return 0; }
void SimpleJuno106AudioProcessor::setCurrentProgram (int index) { juce::ignoreUnused(index); }
const juce::String SimpleJuno106AudioProcessor::getProgramName (int index) { juce::ignoreUnused(index); return {}; }
void SimpleJuno106AudioProcessor::changeProgramName (int index, const juce::String& newName) { juce::ignoreUnused(index, newName); }

void SimpleJuno106AudioProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    DBG("SimpleJuno106AudioProcessor::prepareToPlay START (sr=" + juce::String(sr) + ")");
    setLatencySamples(0);

    voiceManager.prepare(sr, samplesPerBlock);
    voiceManager.setTuningTable(tuningManager.getTuningTable());
    juce::dsp::ProcessSpec spec { sr, (juce::uint32)samplesPerBlock, 2 };
    chorus.prepare(spec);
    chorus.reset();
    chorus2.prepare(spec); 
    chorus2.reset();
    
    dcBlocker.prepare(spec); 
    *dcBlocker.state = *new juce::dsp::IIR::Coefficients<float>(1.0f, -1.0f, 1.0f, -0.995f);
    
    chorusPreEmphasisFilter.prepare(spec);
    chorusDeEmphasisFilter.prepare(spec);
    chorusNoiseFilter.prepare(spec);
    chorusNoiseHPF.prepare(spec);
    
    *chorusPreEmphasisFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, 8000.0f, 0.707f, 1.5f);
    *chorusDeEmphasisFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, 12000.0f, 0.707f);
    *chorusNoiseFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, 8000.0f, 0.707f);
    *chorusNoiseHPF.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, 2500.0f, 0.707f);

    chorusNoiseBuffer.setSize(2, samplesPerBlock);

    lfoBuffer.assign(samplesPerBlock + 128, 0.0f); 
    
    smoothedSagGain.reset(sr, 0.02);
    smoothedSagGain.setCurrentAndTargetValue(1.0f);

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
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    if (panicRequested.exchange(false)) {
        voiceManager.resetAllVoices();
        chorus.reset();
        chorus2.reset();
        performanceState.noteOffFifo.reset(); 
        performanceState.noteOffBuffer.fill(0); 
    }
    
    if (paramsAreDirty.exchange(false) || patchDumpRequested.load()) {
        currentParams = getMirrorParameters();
    }

    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i) 
        buffer.clear (i, 0, numSamples);

    keyboardState.processNextMidiBuffer (midiMessages, 0, numSamples, true);

    for (const auto metadata : midiMessages) {
        const auto message = metadata.getMessage();
        
        if (message.isSysEx()) {
            if (currentParams.midiFunction >= 2)
                sysExEngine.handleIncomingSysEx(message, currentParams);
            lastSysExMessage = message; // [Fix] Update display on incoming too
            continue;
        }
        
        if (message.isProgramChange()) {
            if (currentParams.midiFunction >= 1)
                loadPreset(message.getProgramChangeNumber());
            continue;
        }

        if (message.isController()) {
            if (message.getControllerNumber() == 1) { 
                if (auto* p = apvts.getParameter("benderToLFO")) p->setValueNotifyingHost(message.getControllerValue() / 127.0f); 
            }
            else if (message.getControllerNumber() == 64) {
                 int val = message.getControllerValue();
                 if (currentParams.sustainInverted) val = 127 - val;
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
        if (message.isChannelPressure()) {
            currentAftertouch.store(message.getChannelPressureValue() / 127.0f);
            continue;
        }
        if (message.isNoteOn()) voiceManager.noteOn(message.getChannel(), message.getNoteNumber(), message.getVelocity());
        else if (message.isNoteOff()) performanceState.handleNoteOff(message.getNoteNumber(), voiceManager);
    }
    performanceState.flushSustain(voiceManager);

    if (currentParams.midiOut) {
        if (patchDumpRequested.exchange(false)) {
            sendPatchDump();
            // [Fix] Sync lastParams immediately to prevent redundant individual ParamChange messages 
            // from flooding the bridge in the same block for values already covered by the dump.
            lastParams = currentParams; 
        }
        
        // [Audit Fix] Raised limit to 10 to prevent dropping modulation updates (ENV/LFO)
        int msgsSent = 0;
        const int kMaxMsgsPerBlock = 10; 
        auto checkSendLocal = [&](float curr, float& last, int id) {
            if (msgsSent >= kMaxMsgsPerBlock) return;
            if (std::abs(curr - last) > 0.001f) {
                sendSysEx(JunoSysEx::createParamChange(currentParams.midiChannel - 1, id, (int)(curr * 127.0f)));
                last = curr;
                msgsSent++;
            }
        };

        checkSendLocal(currentParams.lfoRate, lastParams.lfoRate, JunoSysEx::LFO_RATE);
        checkSendLocal(currentParams.lfoDelay, lastParams.lfoDelay, JunoSysEx::LFO_DELAY);
        checkSendLocal(currentParams.lfoToDCO, lastParams.lfoToDCO, JunoSysEx::DCO_LFO);
        checkSendLocal(currentParams.pwmAmount, lastParams.pwmAmount, JunoSysEx::DCO_PWM);
        checkSendLocal(currentParams.noiseLevel, lastParams.noiseLevel, JunoSysEx::DCO_NOISE);
        checkSendLocal(currentParams.vcfFreq, lastParams.vcfFreq, JunoSysEx::VCF_FREQ);
        checkSendLocal(currentParams.resonance, lastParams.resonance, JunoSysEx::VCF_RES);
        checkSendLocal(currentParams.envAmount, lastParams.envAmount, JunoSysEx::VCF_ENV);
        checkSendLocal(currentParams.lfoToVCF, lastParams.lfoToVCF, JunoSysEx::VCF_LFO);
        checkSendLocal(currentParams.kybdTracking, lastParams.kybdTracking, JunoSysEx::VCF_KYBD);
        checkSendLocal(currentParams.vcaLevel, lastParams.vcaLevel, JunoSysEx::VCA_LEVEL);
        checkSendLocal(currentParams.attack, lastParams.attack, JunoSysEx::ENV_A);
        checkSendLocal(currentParams.decay, lastParams.decay, JunoSysEx::ENV_D);
        checkSendLocal(currentParams.sustain, lastParams.sustain, JunoSysEx::ENV_S);
        checkSendLocal(currentParams.release, lastParams.release, JunoSysEx::ENV_R);
        checkSendLocal(currentParams.subOscLevel, lastParams.subOscLevel, JunoSysEx::DCO_SUB);

        auto packSw1 = [](const SynthParams& p) -> int {
            int val = 0;
            if (p.chorus1 || p.chorus2) val |= (1 << 0);
            if (p.chorus2) val |= (1 << 1);
            int hwRange = juce::jlimit(0, 2, (int)p.dcoRange);
            val |= (hwRange & 0x03) << 3;
            if (p.pulseOn) val |= (1 << 5);
            if (p.sawOn)   val |= (1 << 6);
            return val;
        };
        int s1cur = packSw1(currentParams);
        int s1last = packSw1(lastParams);
        if (s1cur != s1last) {
            sendSysEx(JunoSysEx::createParamChange(currentParams.midiChannel - 1, JunoSysEx::SWITCHES_1, s1cur));
        }

        auto packSw2 = [](const SynthParams& p) -> int {
            int val = 0;
            if (p.pwmMode == 1)     val |= (1 << 0);
            if (p.vcaMode == 1)     val |= (1 << 1);
            if (p.vcfPolarity == 1) val |= (1 << 2);
            int hwHpf = 3 - juce::jlimit(0, 3, (int)p.hpfFreq);
            val |= (hwHpf & 0x03) << 3;
            return val;
        };
        int s2cur = packSw2(currentParams);
        int s2last = packSw2(lastParams);
        if (s2cur != s2last) {
            sendSysEx(JunoSysEx::createParamChange(currentParams.midiChannel - 1, JunoSysEx::SWITCHES_2, s2cur));
        }

        // [Fix] Actually merge the generated SysEx into host buffer!
        midiMessages.addEvents(midiOutBuffer, 0, numSamples, 0);
        midiOutBuffer.clear();
    }
    lastParams = currentParams;

    applyPerformanceModulations(currentParams);
    
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

    renderAudio(buffer, numSamples);
    applyChorus(buffer, numSamples);
    processMasterEffects(buffer, numSamples);
}

void SimpleJuno106AudioProcessor::renderAudio(juce::AudioBuffer<float>& buffer, int numSamples) {
    using namespace JunoConstants;
    const double sr = getSampleRate();

    float ratio = JunoTimeCurves::kLfoMaxHz / JunoTimeCurves::kLfoMinHz;
    float lfoRateHz = JunoTimeCurves::kLfoMinHz * std::pow(ratio, (float)currentParams.lfoRate);
    float lfoDelaySeconds = currentParams.lfoDelay * JunoTimeCurves::kLfoDelayMax;
    float delayIncrement = (lfoDelaySeconds > 0.001f) ? (1.0f / (lfoDelaySeconds * (float)sr)) : 1.0f;
    
    bool lfoTrigRequested = fmtLfoTrig->load() > 0.5f;
    if (lfoTrigRequested) {
        masterLfoPhase = 0.0f;
        masterLfoDelayEnvelope = 1.0f; 
        fmtLfoTrig->store(0.0f); 
    }

    bool anyHeld = voiceManager.isAnyNoteHeld();
    if (anyHeld && !wasAnyNoteHeld) masterLfoDelayEnvelope = 0.0f;
    wasAnyNoteHeld = anyHeld;
    
    for (int i = 0; i < numSamples; ++i) {
        masterLfoPhase += (lfoRateHz / (float)sr);
        if (masterLfoPhase >= 1.0f) masterLfoPhase -= 1.0f;
        
        if (anyHeld) {
            masterLfoDelayEnvelope += delayIncrement;
            if (masterLfoDelayEnvelope > 1.0f) masterLfoDelayEnvelope = 1.0f;
        } else {
            masterLfoDelayEnvelope = 0.0f;
        }

        float lfoTri = 1.0f - 4.0f * std::abs(masterLfoPhase - 0.5f); 
        float lfoTriStepped = std::floor(lfoTri * 15.99f) / 15.0f; 
        lfoBuffer[i] = lfoTriStepped * masterLfoDelayEnvelope;
    }

    voiceManager.renderNextBlock(buffer, 0, numSamples, lfoBuffer);
}

void SimpleJuno106AudioProcessor::applyChorus (juce::AudioBuffer<float>& buffer, int numSamples)
{
    auto mix = fmtChorusMix->load();
    if (mix <= 0.001f) return; // Dry only

    int mode = 0;
    if (fmtChorus1->load() > 0.5f) mode = 1;
    else if (fmtChorus2->load() > 0.5f) mode = 2;
    
    if (mode == 0) return;

    // Create a copy of the dry signal if mix < 1.0
    juce::AudioBuffer<float> dryBuffer;
    if (mix < 0.999f) {
        dryBuffer.makeCopyOf(buffer);
    }

    // The original chorus processing logic is now integrated into the new applyChorus
    // function, which handles both chorus1 and chorus2 modes and mixing.

    const double sr = getSampleRate();
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    chorusPreEmphasisFilter.process(context);
    
    juce::AudioBuffer<float> wetBuffer(2, numSamples);
    wetBuffer.clear();
    int targetMode = (currentParams.chorus1 && currentParams.chorus2) ? 3 : (currentParams.chorus1 ? 1 : 2);
    
    float rateI = Chorus::kRateI;
    float rateII = Chorus::kRateII;
    if (targetMode == 3) {
        rateI *= 1.2f; 
        rateII *= 1.2f;
    }
    
    float phIncI = rateI / (float)sr;
    float phIncII = rateII / (float)sr;
    
    for (int i = 0; i < numSamples; ++i) {
        chorusNoiseBuffer.setSample(0, i, chorusNoiseGen.nextFloat() * 2.0f - 1.0f);
        chorusNoiseBuffer.setSample(1, i, chorusNoiseGen.nextFloat() * 2.0f - 1.0f);
    }
    juce::dsp::AudioBlock<float> noiseBlock(chorusNoiseBuffer);
    juce::dsp::ProcessContextReplacing<float> noiseContext(noiseBlock);
    chorusNoiseHPF.process(noiseContext);
    chorusNoiseFilter.process(noiseContext);
    
    float hissScale = currentParams.chorusHiss;
    float noiseLevel = (targetMode == 2) ? 0.00251f : 0.00126f;
    if (targetMode == 3) noiseLevel = 0.00178f; 
    noiseLevel *= hissScale;
    
    for (int i = 0; i < numSamples; ++i) {
        float dry = buffer.getSample(0, i);
        chorusLfoPhaseI = std::fmod(chorusLfoPhaseI + phIncI, 1.0f);
        chorusLfoPhaseII = std::fmod(chorusLfoPhaseII + phIncII, 1.0f);
        
        float lfoI = 2.0f * std::abs(2.0f * (chorusLfoPhaseI - 0.5f)) - 1.0f;
        float lfoII = 2.0f * std::abs(2.0f * (chorusLfoPhaseII - 0.5f)) - 1.0f;
        
        float w1 = 0.0f, w2 = 0.0f;
        if (targetMode == 1 || targetMode == 3) 
            w1 = chorus.processSample(dry, Chorus::kDelayBaseMs + (lfoI * Chorus::kModDepthMs));
        if (targetMode == 2 || targetMode == 3) 
            w2 = chorus2.processSample(dry, Chorus::kDelayBaseMs + (lfoII * Chorus::kModDepthMs));
        
        float wetMix = (targetMode == 3) ? (w1 + w2) * 0.707f : (targetMode == 1 ? w1 : w2);
        
        float hissL = chorusNoiseBuffer.getSample(0, i) * noiseLevel;
        float hissR = chorusNoiseBuffer.getSample(1, i) * noiseLevel;
        
        wetBuffer.setSample(0, i, wetMix + hissL);
        wetBuffer.setSample(1, i, -wetMix + hissR);
    }

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.addFrom(ch, 0, wetBuffer, ch, 0, numSamples, mix); // Apply mix here
        
    chorusDeEmphasisFilter.process(context);

    // Apply mix for the dry signal if mix < 1.0
    if (mix < 0.999f) {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            auto* out = buffer.getWritePointer(ch);
            auto* dry = dryBuffer.getReadPointer(ch);
            for (int s = 0; s < numSamples; ++s) {
                out[s] = (dry[s] * (1.0f - mix)) + out[s]; // out[s] already contains the wet signal mixed by 'mix'
            }
        }
    }
}

void SimpleJuno106AudioProcessor::processMasterEffects(juce::AudioBuffer<float>& buffer, int numSamples) {
    float envSum = voiceManager.getTotalEnvelopeLevel();
    float rawSag = 1.0f - (envSum * 0.025f);
    rawSag = juce::jlimit(0.8f, 1.0f, rawSag);
    smoothedSagGain.setTargetValue(rawSag);
    
    float masterVol = fmtMasterVolume->load();
    buffer.applyGain(smoothedSagGain.getNextValue() * masterVol);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        float* d = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i) {
            d[i] = std::tanh(d[i] * 1.1f); 
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

    if (powerOnDelaySamples < (int)(0.080f * getSampleRate())) {
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
    p.dcoRange = (int)std::lround(fmtDcoRange->load()); 
    p.sawOn = fmtSawOn->load() > 0.5f; 
    p.pulseOn = fmtPulseOn->load() > 0.5f;
    p.pwmAmount = fmtPwm->load(); 
    p.pwmMode = (int)std::lround(fmtPwmMode->load()); 
    p.subOscLevel = fmtSubOsc->load();
    p.noiseLevel = fmtNoise->load(); 
    p.lfoToDCO = fmtLfoToDCO->load(); 
    p.hpfFreq = (int)std::lround(fmtHpfFreq->load());
    p.vcfFreq = fmtVcfFreq->load(); 
    p.resonance = fmtResonance->load(); 
    p.envAmount = fmtEnvAmount->load();
    p.lfoToVCF = fmtLfoToVCF->load();
    p.thermalDrift = fmtThermalDrift->load();
    p.unisonStereoWidth = fmtUnisonWidth->load();
    p.unisonDetune = fmtUnisonDetune->load();
    p.chorusMix = fmtChorusMix->load();
    p.kybdTracking = fmtKybdTracking->load(); 
    p.vcfPolarity = (int)std::lround(fmtVcfPolarity->load()); 
    p.vcaMode = (int)std::lround(fmtVcaMode->load()); 
    p.vcaLevel = fmtVcaLevel->load(); 
    p.attack = fmtAttack->load();
    p.decay = fmtDecay->load(); 
    p.sustain = fmtSustain->load(); 
    p.release = fmtRelease->load();
    p.lfoRate = fmtLfoRate->load(); 
    p.lfoDelay = fmtLfoDelay->load();
    p.chorus1 = fmtChorus1->load() > 0.5f; 
    p.chorus2 = fmtChorus2->load() > 0.5f;
    p.polyMode = (int)std::lround(fmtPolyMode->load()); 
    p.portamentoTime = fmtPortTime->load();
    p.portamentoOn = fmtPortOn->load() > 0.5f; 
    p.portamentoLegato = fmtPortLegato->load() > 0.5f;
    p.benderValue = fmtBender->load(); 
    p.benderToDCO = fmtBenderDCO->load();
    p.benderToVCF = fmtBenderVCF->load(); 
    p.benderToLFO = fmtBenderLFO->load();
    p.tune = fmtTune->load(); 
    p.midiOut = fmtMidiOut->load() > 0.5f;

    p.midiChannel = (int)std::lround(fmtMidiChannel->load());
    p.benderRange = (int)std::lround(fmtBenderRange->load());
    p.velocitySens = fmtVelocitySens->load();
    p.lcdBrightness = fmtLcdBrightness->load();
    p.numVoices = (int)std::lround(fmtNumVoices->load());
    p.sustainInverted = fmtSustainInverted->load() > 0.5f;
    p.chorusHiss = fmtChorusHiss->load();
    p.midiFunction = (int)std::lround(fmtMidiFunction->load());
    p.aftertouchToVCF = fmtAftertouchToVCF->load();
    
    return p;
}

void SimpleJuno106AudioProcessor::updateParamsFromAPVTS() {
    currentParams = getMirrorParameters();
    currentParams.currentAftertouch = currentAftertouch.load();
    lastParams = currentParams;
}

void SimpleJuno106AudioProcessor::applyPerformanceModulations(SynthParams& p) {
    float modWheel = p.benderToLFO;
    p.lfoToDCO = juce::jlimit<float>(0.0f, 1.0f, p.lfoToDCO + modWheel * 0.3f);
    p.vcfLFOAmount = juce::jlimit<float>(0.0f, 1.0f, p.lfoToVCF + modWheel);
    p.envAmount = juce::jlimit<float>(0.0f, 1.0f, p.envAmount + currentAftertouch.load() * p.aftertouchToVCF);
}

void SimpleJuno106AudioProcessor::sendPatchDump() { sendSysEx(sysExEngine.makePatchDump(currentParams.midiChannel - 1, currentParams)); }
void SimpleJuno106AudioProcessor::sendManualMode() { sendSysEx(JunoSysEx::createManualMode(currentParams.midiChannel - 1)); }

void SimpleJuno106AudioProcessor::triggerPanic() {
    panicRequested.store(true);
    midiOutBuffer.addEvent(juce::MidiMessage::allNotesOff(1), 0);
}

void SimpleJuno106AudioProcessor::triggerLFO() {
    if (fmtLfoTrig) fmtLfoTrig->store(1.0f);
}

void SimpleJuno106AudioProcessor::loadPreset(int index) {
    if (presetManager) {
        presetManager->setCurrentPreset(index);
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
            voiceManager.updateParams(currentParams);
            voiceManager.forceUpdate(); 
            paramsAreDirty.store(true); 
            needsVoiceReset.store(true);
            patchDumpRequested.store(true);
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
    params.push_back (makeParam ("thermalDrift", "Thermal Drift", 0.0f, 1.0f, 0.0f));
    params.push_back (makeParam ("unisonWidth", "Unison Width", 0.0f, 1.0f, 0.0f));
    params.push_back (makeParam ("unisonDetune", "Unison Detune", 0.0f, 1.0f, 0.15f));
    params.push_back (makeParam ("chorusMix", "Chorus Mix", 0.0f, 1.0f, 1.0f));
    params.push_back (makeIntParam ("vcfPolarity", "VCF Polarity", 0, 1, 0));
    params.push_back (makeParam ("kybdTracking", "VCF Kybd Track", 0.0f, 1.0f, 0.0f));
    params.push_back(makeParam("envAmount", "VCF Env Amount", 0.0f, 1.0f, 0.5f));
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
    
    params.push_back(makeIntParam("midiChannel", "MIDI Channel", 1, 16, 1));
    params.push_back(makeIntParam("benderRange", "Bender Range", 1, 12, 2));
    params.push_back(makeParam("velocitySens", "Velocity Sens", 0.0f, 1.0f, 0.5f));
    params.push_back(makeParam("lcdBrightness", "LCD Brightness", 0.0f, 1.0f, 0.8f));
    params.push_back(makeIntParam("numVoices", "Voice Limit", 1, 16, 16));
    params.push_back(makeBool("sustainInverted", "Sustain Inverted", false));
    params.push_back(makeParam("chorusHiss", "Chorus Hiss", 0.0f, 2.0f, 1.0f));
    params.push_back(makeIntParam("midiFunction", "MIDI Function", 0, 2, 2));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("aftertouchToVCF", "Aftertouch -> VCF", 0.0f, 1.0f, 0.5f));
    
    return { params.begin(), params.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SimpleJuno106AudioProcessor(); }
bool SimpleJuno106AudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* SimpleJuno106AudioProcessor::createEditor() { return new WebViewEditor (*this); }

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
void SimpleJuno106AudioProcessor::loadTuningFile() {
    DBG("SimpleJuno106AudioProcessor::loadTuningFile CALLED");
    auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser = std::make_unique<juce::FileChooser>("Select Scala Tuning File...",
                                                      juce::File::getSpecialLocation(juce::File::userHomeDirectory),
                                                      "*.scl");

    fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc) {
        auto file = fc.getResult();
        if (file.existsAsFile()) {
            if (tuningManager.parseSCL(file)) {
                voiceManager.setTuningTable(tuningManager.getTuningTable());
                currentTuningName = file.getFileName();
                DBG("Custom Tuning Loaded: " << currentTuningName);
            }
        }
    });
}

void SimpleJuno106AudioProcessor::resetTuning() {
    DBG("SimpleJuno106AudioProcessor::resetTuning CALLED");
    tuningManager.resetToStandard();
    voiceManager.setTuningTable(tuningManager.getTuningTable());
    currentTuningName = "Standard Tuning";
    DBG("Standard Tuning Restored");
}

juce::MidiMessage SimpleJuno106AudioProcessor::getCurrentSysExData() {
    return lastSysExMessage;
}
