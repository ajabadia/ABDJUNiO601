#include "PluginProcessor.h"
#include <JuceHeader.h>
#include <memory>
#if !JUCE_HEADLESS_PLUGIN
 #include "PluginEditor.h"
 #include "../UI/WebView/WebViewEditor.h"
#endif
#include "PresetManager.h"
#include "../Synth/ChorusBBD.h"
#include "JunoConstants.h"

using namespace JunoConstants;

//==============================================================================
SimpleJuno106AudioProcessor::SimpleJuno106AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : juce::AudioProcessor (juce::AudioProcessor::BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
#else
    : juce::AudioProcessor(JucePlugin_PreferredChannelConfigurations),
#endif
      apvts(*this, &undoManager, "Parameters", createParameterLayout()),
      calibrationManager()
{
    serviceModeManager = std::make_unique<ServiceModeManager>(*this);
    DBG("SimpleJuno106AudioProcessor::Constructor Body START");

    // 1. [CRITICAL] Initialize Cached Pointers FIRST
    auto getCachedParam = [&](const juce::String& id) {
        auto* p = apvts.getRawParameterValue(id);
        if (p == nullptr) { DBG("!!! CRITICAL ERROR: Parameter NOT FOUND: " + id); }
        jassert(p != nullptr);
        return p;
    };

    fmtDcoRange = getCachedParam("dcoRange");
    fmtSawOn = getCachedParam("sawOn");
    fmtPulseOn = getCachedParam("pulseOn");
    fmtPwm = getCachedParam("pwm");
    fmtPwmMode = getCachedParam("pwmMode");
    fmtSubOsc = getCachedParam("subOsc");
    fmtNoise = getCachedParam("noise");
    fmtLfoToDCO = getCachedParam("lfoToDCO");
    fmtHpfFreq = getCachedParam("hpfFreq");
    fmtVcfFreq = getCachedParam("vcfFreq");
    fmtResonance = getCachedParam("resonance");
    fmtThermalDrift = getCachedParam("thermalDrift");
    fmtUnisonWidth = getCachedParam("unisonWidth");
    fmtUnisonDetune = getCachedParam("unisonDetune");
    fmtChorusMix = getCachedParam("chorusMix");
    fmtVcfPolarity = getCachedParam("vcfPolarity");
    fmtKybdTracking = getCachedParam("kybdTracking");
    fmtEnvAmount = getCachedParam("envAmount");
    fmtLfoToVCF = getCachedParam("lfoToVCF");
    fmtVcaMode = getCachedParam("vcaMode");
    fmtVcaLevel = getCachedParam("vcaLevel");
    fmtAttack = getCachedParam("attack");
    fmtDecay = getCachedParam("decay");
    fmtSustain = getCachedParam("sustain");
    fmtRelease = getCachedParam("release");
    fmtLfoRate = getCachedParam("lfoRate");
    fmtLfoDelay = getCachedParam("lfoDelay");
    fmtChorus1 = getCachedParam("chorus1");
    fmtChorus2 = getCachedParam("chorus2");
    fmtPolyMode = getCachedParam("polyMode");
    fmtPortTime = getCachedParam("portamentoTime");
    fmtPortOn = getCachedParam("portamentoOn");
    fmtPortLegato = getCachedParam("portamentoLegato");
    fmtBender = getCachedParam("bender");
    fmtBenderDCO = getCachedParam("benderToDCO");
    fmtBenderVCF = getCachedParam("benderToVCF");
    fmtBenderLFO = getCachedParam("benderToLFO");
    fmtTune = getCachedParam("tune");
    fmtMasterVolume = getCachedParam("masterVolume");
    fmtMidiOut = getCachedParam("midiOut");
    fmtLfoTrig = getCachedParam("lfoTrig");

    fmtSustainInverted = getCachedParam("sustainInverted");
    fmtChorusHiss = getCachedParam("chorusHiss");
    fmtMidiFunction = getCachedParam("midiFunction");
    fmtAftertouchToVCF = getCachedParam("aftertouchToVCF");
    fmtLowCpuMode = getCachedParam("lowCpuMode");
    fmtMidiChannel = getCachedParam("midiChannel");
    fmtBenderRange = getCachedParam("benderRange");
    fmtVelocitySens = getCachedParam("velocitySens");
    fmtLcdBrightness = getCachedParam("lcdBrightness");
    fmtNumVoices = getCachedParam("numVoices");
    // fmtUnisonWidth = getCachedParam("unisonWidth"); 

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

    // 3. Register calibration change callback
    calibrationManager.setOnChangeCallback([this](const juce::String&, float) {
        paramsAreDirty.store(true);
    });

    DBG("SimpleJuno106AudioProcessor::Constructor Body END");
    
    // [Build 13] Ensure initial parameters are reflected in DSP and UI
    updateParamsFromAPVTS();
    requestPatchDump();
}

SimpleJuno106AudioProcessor::~SimpleJuno106AudioProcessor() {
    juce::Logger::setCurrentLogger (nullptr); 
}

const juce::String SimpleJuno106AudioProcessor::getName() const { return JucePlugin_Name; }
bool SimpleJuno106AudioProcessor::acceptsMidi() const { return true; }
bool SimpleJuno106AudioProcessor::producesMidi() const { return true; }
bool SimpleJuno106AudioProcessor::isMidiEffect() const { return false; }

int SimpleJuno106AudioProcessor::getNumPrograms() { return 128; } // Standard GM bank size for mapping
int SimpleJuno106AudioProcessor::getCurrentProgram() { 
    return presetManager ? presetManager->getCurrentPresetIndex() : 0; 
}
void SimpleJuno106AudioProcessor::setCurrentProgram (int index) { loadPreset(index); }
const juce::String SimpleJuno106AudioProcessor::getProgramName (int index) { 
    if (presetManager) {
        if (auto* p = presetManager->getPreset(index)) return p->name;
    }
    return "Initial Patch";
}
void SimpleJuno106AudioProcessor::changeProgramName (int index, const juce::String& newName) { juce::ignoreUnused(index, newName); }

void SimpleJuno106AudioProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    DBG("SimpleJuno106AudioProcessor::prepareToPlay START (sr=" + juce::String(sr) + ")");
    setLatencySamples(0);

    voiceManager.prepare(sr, samplesPerBlock);
    voiceManager.setTuningTable(tuningManager.getTuningTable());
    juce::dsp::ProcessSpec spec { sr, (juce::uint32)samplesPerBlock, 2 };
    chorus.prepare(sr, samplesPerBlock);
    
    dcBlocker.prepare(spec); 
    *dcBlocker.state = *new juce::dsp::IIR::Coefficients<float>(1.0f, -1.0f, 1.0f, -0.995f);
    
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
    // Update Service mode (TuningManager is static)
    if (serviceModeManager) serviceModeManager->update(getSampleRate(), buffer.getNumSamples());
    
    // Check for parameter changes
    if (panicRequested.exchange(false)) {
        voiceManager.resetAllVoices();
        chorus.reset();
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
            // [Fix] SysEx Echo Protection: Ignore if it's the exact same message we just sent
                if (lastSentSysExMessage.getRawDataSize() > 0 && 
                    message.getRawDataSize() == lastSentSysExMessage.getRawDataSize() &&
                    memcmp((const void*)message.getRawData(), (const void*)lastSentSysExMessage.getRawData(), (size_t)message.getRawDataSize()) == 0) 
            {
                continue; 
            }

            if (currentParams.midiFunction >= 2)
                sysExEngine.handleIncomingSysEx(message, currentParams);
            lastSysExMessage = message; 
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
        if (message.isController()) {
            if (message.getControllerNumber() == 64) { // Sustain
                int val = message.getControllerValue();
                if (currentParams.sustainInverted) val = 127 - val;
                performanceState.handleSustain(val >= 64);
                continue; // Handled
            }
        }

        if (message.isNoteOn()) voiceManager.noteOn(message.getChannel(), message.getNoteNumber(), message.getVelocity());
        else if (message.isNoteOff()) performanceState.handleNoteOff(message.getNoteNumber(), voiceManager);
    }
    performanceState.flushSustain(voiceManager);

    if (patchDumpRequested.exchange(false)) {
        sendPatchDump();
        // Sync lastParams immediately to prevent redundant individual messages
        lastParams = currentParams; 
    } else {
        // SysEx Flood Protection: count changes
        int changeCount = 0;
        auto countChange = [&](float curr, float last) {
            if (std::abs(curr - last) > 0.001f) changeCount++;
        };

        countChange(currentParams.lfoRate, lastParams.lfoRate);
        countChange(currentParams.lfoDelay, lastParams.lfoDelay);
        countChange(currentParams.lfoToDCO, lastParams.lfoToDCO);
        countChange(currentParams.pwmAmount, lastParams.pwmAmount);
        countChange(currentParams.noiseLevel, lastParams.noiseLevel);
        countChange(currentParams.vcfFreq, lastParams.vcfFreq);
        countChange(currentParams.resonance, lastParams.resonance);
        countChange(currentParams.envAmount, lastParams.envAmount);
        countChange(currentParams.lfoToVCF, lastParams.lfoToVCF);
        countChange(currentParams.kybdTracking, lastParams.kybdTracking);
        countChange(currentParams.vcaLevel, lastParams.vcaLevel);
        countChange(currentParams.attack, lastParams.attack);
        countChange(currentParams.decay, lastParams.decay);
        countChange(currentParams.sustain, lastParams.sustain);
        countChange(currentParams.release, lastParams.release);
        countChange(currentParams.subOscLevel, lastParams.subOscLevel);

        const int kParamChangeThreshold = 8; // If more than 8 params change, send a full dump
        
        if (changeCount >= kParamChangeThreshold) {
            sendPatchDump();
            lastParams = currentParams;
        } else {
            // Send individual messages (existing logic)
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

            // Handle switches (Sw1/Sw2) as before...
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
                int hwHpf = juce::jlimit(0, 3, (int)p.hpfFreq);
                val |= (hwHpf & 0x03) << 3;
                return val;
            };
            int s2cur = packSw2(currentParams);
            int s2last = packSw2(lastParams);
            if (s2cur != s2last) {
                sendSysEx(JunoSysEx::createParamChange(currentParams.midiChannel - 1, JunoSysEx::SWITCHES_2, s2cur));
            }
        }
    }

    if (currentParams.midiOut) {
        // Merge the generated SysEx into host buffer
        midiMessages.addEvents(midiOutBuffer, 0, numSamples, 0);
        midiOutBuffer.clear();
    }
    lastParams = currentParams;

    applyPerformanceModulations(currentParams);
    
    // [Service Mode] Apply VCF Sweep if active
    if (serviceModeManager && serviceModeManager->isVcfSweepActive()) {
        currentParams.vcfFreq = serviceModeManager->getVcfSweepCutoff();
    }
    
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
    voiceManager.setBenderAmount(currentParams.benderValue); // Removed globalDriftAudible (applied in updatePitch)

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
        // [Fidelity] Center-biased 4-bit stepping (to match 16-level resolution without DC bias)
        float lfoTriStepped = std::round(lfoTri * 7.5f) / 7.5f; 
        lfoBuffer[i] = lfoTriStepped * masterLfoDelayEnvelope;
    }

    voiceManager.renderNextBlock(buffer, 0, numSamples, lfoBuffer);
}

void SimpleJuno106AudioProcessor::applyChorus (juce::AudioBuffer<float>& buffer, int numSamples)
{
    juce::ignoreUnused(numSamples);
    
    // Set Chorus Mode from APVTS bits
    bool ch1 = fmtChorus1->load() > 0.5f;
    bool ch2 = fmtChorus2->load() > 0.5f;
    
    ChorusBBD::Mode mode = ChorusBBD::Mode::Off;
    if (ch1 && ch2) mode = ChorusBBD::Mode::ChorusBoth;
    else if (ch1) mode = ChorusBBD::Mode::ChorusI;
    else if (ch2) mode = ChorusBBD::Mode::ChorusII;
    
    chorus.setMode(mode);
    chorus.setHissLevel(currentParams.chorusHissLvl);
    
    if (mode == ChorusBBD::Mode::Off)
        return;

    // Map hardware-authentic rates if not overridden by custom parameters
    // In this implementation, we use the standard rates but allow APVTS depth/mix control
    float rate = ch2 ? JunoConstants::Chorus::kRateII : JunoConstants::Chorus::kRateI;
    if (mode == ChorusBBD::Mode::ChorusBoth) rate *= 1.2f; 
    
    chorus.setRate(rate);
    chorus.setDepth(1.0f); // Default to full hardware depth, controlled by mix
    chorus.setMix(fmtChorusMix->load());
    
    chorus.process(buffer);
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
        if (currentParams.lowCpuMode) {
            // Cubic saturation: x - x^3/3 (approx tanh)
            for (int i = 0; i < numSamples; ++i) {
                float x = juce::jlimit(-1.5f, 1.5f, d[i] * 1.1f);
                d[i] = x - (x * x * x) * 0.333333f;
            }
        } else {
            for (int i = 0; i < numSamples; ++i) {
                d[i] = std::tanh(d[i] * 1.1f); 
            }
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
    p.lowCpuMode = fmtLowCpuMode->load() > 0.5f;
    
    // --- Inject Calibration Overrides ---
    p.vcfSelfOscThreshold = calibrationManager.getValue("vcfSelfOscPoint");
    p.subAmpScale = calibrationManager.getValue("subAmpScale");
    p.dcoMixerGain = calibrationManager.getValue("dcoMixerGain");
    p.adsrSlewMs = calibrationManager.getValue("adsrSlewMs");
    p.vcfSaturation = calibrationManager.getValue("vcfSaturation");
    p.adsrAttackFactor = calibrationManager.getValue("adsrAttackFactor");
    p.chorusHissLvl = calibrationManager.getValue("chorusHissLvl");

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
void SimpleJuno106AudioProcessor::sendManualMode() { 
    sendSysEx(JunoSysEx::createManualMode(currentParams.midiChannel - 1)); 
    
    // [Manual Mode Logic] Force immediate snapshot from APVTS to DSP
    updateParamsFromAPVTS();
    paramsAreDirty.store(true);
    patchDumpRequested.store(true); // Broadcast current physical state to MIDI out
    needsVoiceReset.store(true);
}

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
    
    params.push_back(std::make_unique<juce::AudioParameterBool>("lowCpuMode", "Low CPU Mode", false));
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
