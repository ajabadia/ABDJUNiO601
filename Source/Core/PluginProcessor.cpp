#include "ABDSimpleJuno106AudioProcessor.h"
#include <JuceHeader.h>
#include <memory>
#include "JunoModelConfig.h"
#if !JUCE_HEADLESS_PLUGIN
 #include "PluginEditor.h"
 #include "../UI/WebView/WebViewEditor.h"
#endif
#include "PresetManager.h"
#include "CalibrationSettings.h"
#include "ServiceModeManager.h"
#include "JunoProtocol.h"
#include "JunoEngine.h"

using namespace JunoConstants;

ServiceModeManager& ABDSimpleJuno106AudioProcessor::getServiceModeManager() { return *serviceModeManager; }

//==============================================================================
ABDSimpleJuno106AudioProcessor::ABDSimpleJuno106AudioProcessor()
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
      apvts(*this, &undoManager, "Parameters", createParameterLayout())
{
    DBG("ABDSimpleJuno106AudioProcessor::Constructor Body START");

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
    fmtMemoryProtect = getCachedParam("memoryProtect");
    fmtMidiChannel = getCachedParam("midiChannel");
    fmtBenderRange = getCachedParam("benderRange");
    fmtVelocitySens = getCachedParam("velocitySens");
    fmtLcdBrightness = getCachedParam("lcdBrightness");
    fmtNumVoices = getCachedParam("numVoices");
    // fmtUnisonWidth = getCachedParam("unisonWidth"); 

    // Model Routing / Selección de Modelos
    fmtModelDCO = getCachedParam("modelDCO");
    fmtModelHPF = getCachedParam("modelHPF");
    fmtModelVCF = getCachedParam("modelVCF");
    fmtModelADSR = getCachedParam("modelADSR");
    fmtModelChorus = getCachedParam("modelChorus");
    fmtModelArp = getCachedParam("modelArp");
    fmtModelPoly = getCachedParam("modelPoly");
    fmtModelPorta = getCachedParam("modelPorta");
    fmtModelUnison = getCachedParam("modelUnison");

    // Arpeggiator Settings
    fmtArpEnabled = getCachedParam("arpEnabled");
    fmtArpMode = getCachedParam("arpMode");
    fmtArpRange = getCachedParam("arpRange");
    fmtArpRate = getCachedParam("arpRate");
    fmtArpSync = getCachedParam("arpSync");
    fmtArpDivision = getCachedParam("arpDivision");

    // Tape Echo / Delay Settings (Super Six only)
    fmtDelayEnabled = getCachedParam("delayEnabled");
    fmtDelaySetting = getCachedParam("delaySetting");
    fmtDelayRepeatRate = getCachedParam("delayRepeatRate");
    fmtDelayIntensity = getCachedParam("delayIntensity");
    fmtDelayBass = getCachedParam("delayBass");
    fmtDelayTreble = getCachedParam("delayTreble");
    fmtDelayReverbVol = getCachedParam("delayReverbVol");
    fmtDelayEchoVol = getCachedParam("delayEchoVol");
    fmtDelayEchoCancel = getCachedParam("delayEchoCancel");
    fmtDelaySyncEnabled = getCachedParam("delaySyncEnabled");
    fmtDelaySyncDivision = getCachedParam("delaySyncDivision");
    fmtDelayReverbType = getCachedParam("delayReverbType");
    fmtDelayWowFlutter = getCachedParam("delayWowFlutter");
    fmtDelayReverbDecay = getCachedParam("delayReverbDecay");
    fmtDelayEchoIsolator = getCachedParam("delayEchoIsolator");

    DBG("ABDSimpleJuno106AudioProcessor::Parameters cached DONE");
    loadUserSettings();

    // 2. Initialize secondary components
    presetManager = std::make_unique<PresetManager>();
    calibrationSettings = std::make_unique<CalibrationSettings>();
    serviceModeManager = std::make_unique<ServiceModeManager>(*this);
    DBG("ABDSimpleJuno106AudioProcessor::Components created DONE");

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
    midiLearnHandler.bind(80, "delayEchoCancel"); // CC80 = footswitch Echo Cancel (RE-201 style)
    
    keyboardState.addListener(this);

    // 3. Register calibration change callback
    calibrationSettings->setOnChangeCallback([this](std::string, float) {
        paramsAreDirty.store(true);
    });

    DBG("ABDSimpleJuno106AudioProcessor::Constructor Body END");
    
    // [Build 13] Ensure initial parameters are reflected in DSP and UI
    updateParamsFromAPVTS();
    requestPatchDump();

    // [Build 103] Recording Setup
    formatManager.registerBasicFormats();
    backgroundThread.startThread();
    engine = std::make_unique<JunoEngine>();
}

ABDSimpleJuno106AudioProcessor::~ABDSimpleJuno106AudioProcessor() {
    stopRecording();
    backgroundThread.stopThread(500);
    juce::Logger::setCurrentLogger (nullptr); 
}

const juce::String ABDSimpleJuno106AudioProcessor::getName() const { return getJunoModelName(); }
bool ABDSimpleJuno106AudioProcessor::acceptsMidi() const { return true; }
bool ABDSimpleJuno106AudioProcessor::producesMidi() const { return true; }
bool ABDSimpleJuno106AudioProcessor::isMidiEffect() const { return false; }

int ABDSimpleJuno106AudioProcessor::getNumPrograms() { return 128; } // Standard GM bank size for mapping
int ABDSimpleJuno106AudioProcessor::getCurrentProgram() { 
    return presetManager ? presetManager->getCurrentPresetIndex() : 0; 
}
void ABDSimpleJuno106AudioProcessor::setCurrentProgram (int index) { loadPreset(index); }
const juce::String ABDSimpleJuno106AudioProcessor::getProgramName (int index) { 
    if (presetManager) {
        if (auto* p = presetManager->getPreset(index)) return p->name;
    }
    return "Initial Patch";
}
void ABDSimpleJuno106AudioProcessor::changeProgramName (int index, const juce::String& newName) { juce::ignoreUnused(index, newName); }

void ABDSimpleJuno106AudioProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    DBG("ABDSimpleJuno106AudioProcessor::prepareToPlay START (sr=" + juce::String(sr) + ")");
    setLatencySamples(0);

    engine->prepare(sr, samplesPerBlock);
    engine->getVoiceManager().setTuningTable(tuningManager.getTuningTable());
    engine->setLfoCalibration(calibrationSettings.get());

    if (calibrationSettings != nullptr) {
        JunoEngine::TapeEchoCal cal;
        cal.delayInputLevel = calibrationSettings->getValue("delayInputLevel");
        cal.delayWetDry = calibrationSettings->getValue("delayWetDry");
        cal.delayWowRate = calibrationSettings->getValue("delayWowRate");
        cal.delayFlutterRate = calibrationSettings->getValue("delayFlutterRate");
        cal.delayTapeScrapeRate = calibrationSettings->getValue("delayTapeScrapeRate");
        cal.delayWowAmp = calibrationSettings->getValue("delayWowAmp");
        cal.delayFlutterAmp = calibrationSettings->getValue("delayFlutterAmp");
        cal.delayTapeScrapeAmp = calibrationSettings->getValue("delayTapeScrapeAmp");
        cal.delayWowFlutterScale = calibrationSettings->getValue("delayWowFlutterScale");
        cal.delaySaturationInputGain = calibrationSettings->getValue("delaySaturationInputGain");
        cal.delayHead2Ratio = calibrationSettings->getValue("delayHead2Ratio");
        cal.delayHead3Ratio = calibrationSettings->getValue("delayHead3Ratio");
        cal.delayBassFreq = calibrationSettings->getValue("delayBassFreq");
        cal.delayTrebleFreq = calibrationSettings->getValue("delayTrebleFreq");
        cal.delayFeedbackLpfBase = calibrationSettings->getValue("delayFeedbackLpfBase");
        cal.delayFeedbackLpfRange = calibrationSettings->getValue("delayFeedbackLpfRange");
        cal.delaySpringGain = calibrationSettings->getValue("delaySpringGain");
        cal.delaySpringReflectionScale = calibrationSettings->getValue("delaySpringReflectionScale");
        cal.delaySchroederLpf = calibrationSettings->getValue("delaySchroederLpf");
        cal.delaySchroederGain = calibrationSettings->getValue("delaySchroederGain");
        cal.delaySchroederSatDrive = calibrationSettings->getValue("delaySchroederSatDrive");
        engine->setTapeEchoCalibration(cal);
    }

    DBG("ABDSimpleJuno106AudioProcessor::prepareToPlay END");
}

void ABDSimpleJuno106AudioProcessor::releaseResources() {}

bool ABDSimpleJuno106AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    return true;
}

void ABDSimpleJuno106AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0) return;
    const int numSamples = buffer.getNumSamples();
    // Update Service mode (TuningManager is static)
    if (serviceModeManager) serviceModeManager->update(getSampleRate(), buffer.getNumSamples());
    
    // [Telemetry] Signal MIDI activity to UI
    if (!midiMessages.isEmpty()) midiTrafficFlag.store(true);

    // Check for parameter changes
    if (panicRequested.exchange(false)) {
        engine->panic();
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
                modWheelValue.store(message.getControllerValue() / 127.0f, std::memory_order_relaxed);
            }
            else if (message.getControllerNumber() == 64) {
                 int val = message.getControllerValue();
                 if (currentParams.sustainInverted) val = 127 - val;
                 engine->getPerformanceState().handleSustain(val);
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
                engine->getPerformanceState().handleSustain(val >= 64);
                continue; // Handled
            }
        }

        if (message.isNoteOn()) {
            engine->noteOn(message.getChannel(), message.getNoteNumber(), message.getVelocity());
        }
        else if (message.isNoteOff()) {
            engine->noteOff(message.getChannel(), message.getNoteNumber(), message.getVelocity());
        }
    }
    engine->flushSustain();

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

            // [Canonical Protocol] Use centralized JunoProtocol for bit-packing
            int s1cur = JunoProtocol::encodeSW1(currentParams);
            int s1last = JunoProtocol::encodeSW1(lastParams);
            if (s1cur != s1last) {
                sendSysEx(JunoSysEx::createParamChange(currentParams.midiChannel - 1, JunoSysEx::SWITCHES_1, s1cur));
            }

            int s2cur = JunoProtocol::encodeSW2(currentParams);
            int s2last = JunoProtocol::encodeSW2(lastParams);
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
    
    // [Service Mode] Apply Diagnostic Overrides
    if (serviceModeManager) {
        if (serviceModeManager->isVcfSweepActive()) 
            currentParams.vcfFreq = serviceModeManager->getVcfSweepCutoff();
            
        currentParams.hpfCyclePos = serviceModeManager->getHpfCyclePos();
        currentParams.chorusCycleMode = serviceModeManager->getChorusCycleMode();
    }
    
    if (needsVoiceReset.exchange(false)) {
        engine->getVoiceManager().forceUpdate();
    }

    // [Tape echo calibration from settings]
    if (calibrationSettings != nullptr && isSuperSix()) {
        JunoEngine::TapeEchoCal cal;
        cal.delayInputLevel = calibrationSettings->getValue("delayInputLevel");
        cal.delayWetDry = calibrationSettings->getValue("delayWetDry");
        cal.delayWowRate = calibrationSettings->getValue("delayWowRate");
        cal.delayFlutterRate = calibrationSettings->getValue("delayFlutterRate");
        cal.delayTapeScrapeRate = calibrationSettings->getValue("delayTapeScrapeRate");
        cal.delayWowAmp = calibrationSettings->getValue("delayWowAmp");
        cal.delayFlutterAmp = calibrationSettings->getValue("delayFlutterAmp");
        cal.delayTapeScrapeAmp = calibrationSettings->getValue("delayTapeScrapeAmp");
        cal.delayWowFlutterScale = calibrationSettings->getValue("delayWowFlutterScale");
        cal.delaySaturationInputGain = calibrationSettings->getValue("delaySaturationInputGain");
        cal.delayHead2Ratio = calibrationSettings->getValue("delayHead2Ratio");
        cal.delayHead3Ratio = calibrationSettings->getValue("delayHead3Ratio");
        cal.delayBassFreq = calibrationSettings->getValue("delayBassFreq");
        cal.delayTrebleFreq = calibrationSettings->getValue("delayTrebleFreq");
        cal.delayFeedbackLpfBase = calibrationSettings->getValue("delayFeedbackLpfBase");
        cal.delayFeedbackLpfRange = calibrationSettings->getValue("delayFeedbackLpfRange");
        cal.delaySpringGain = calibrationSettings->getValue("delaySpringGain");
        cal.delaySpringReflectionScale = calibrationSettings->getValue("delaySpringReflectionScale");
        cal.delaySchroederLpf = calibrationSettings->getValue("delaySchroederLpf");
        cal.delaySchroederGain = calibrationSettings->getValue("delaySchroederGain");
        cal.delaySchroederSatDrive = calibrationSettings->getValue("delaySchroederSatDrive");
        engine->setTapeEchoCalibration(cal);
    }

    // [Host info for arp/delay sync]
    if (auto* playHead = getPlayHead()) {
        juce::AudioPlayHead::CurrentPositionInfo posInfo;
        if (playHead->getCurrentPosition(posInfo)) {
            engine->setHostInfo(posInfo.bpm, posInfo.isPlaying, posInfo.ppqPosition);
            delaySyncBPM_.store(posInfo.bpm);
        }
    }

    // [LFO trigger]
    bool lfoTrigRequested = fmtLfoTrig->load() > 0.5f;
    if (lfoTrigRequested) {
        fmtLfoTrig->store(0.0f);
    }

    // [Engine process — all DSP in one call]
    engine->process(currentParams, buffer, numSamples, lfoTrigRequested);

    // [Build 103] Recording Capture (Post-Process)
    {
        const juce::ScopedLock sl(writerLock);
        if (threadedWriter != nullptr) {
            threadedWriter->write(buffer.getArrayOfReadPointers(), numSamples);
        }
    }
}

void ABDSimpleJuno106AudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0) return;
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
    midiMessages.clear();
    if (engine) engine->panic();
}

void ABDSimpleJuno106AudioProcessor::handleNoteOn(juce::MidiKeyboardState*, int channel, int midiNoteNumber, float velocity) { engine->noteOn(channel, midiNoteNumber, velocity); }
void ABDSimpleJuno106AudioProcessor::handleNoteOff(juce::MidiKeyboardState*, int channel, int midiNoteNumber, float velocity) { engine->noteOff(channel, midiNoteNumber, velocity); }

juce::AudioProcessorValueTreeState::ParameterLayout ABDSimpleJuno106AudioProcessor::createParameterLayout() {
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
    params.push_back(makeBool("memoryProtect", "Memory Protect", false));
    params.push_back(makeParam("masterVolume", "Master Volume", 0.0f, 1.0f, 1.0f));
    params.push_back(makeBool("lfoTrig", "LFO Trigger", false));
    
    params.push_back(makeIntParam("midiChannel", "MIDI Channel", 1, 16, 1));
    params.push_back(makeIntParam("benderRange", "Bender Range", 1, 12, 2));
    params.push_back(makeParam("velocitySens", "Velocity Sens", 0.0f, 1.0f, 0.5f));
    params.push_back(makeParam("lcdBrightness", "LCD Brightness", 0.0f, 1.0f, 0.8f));
    params.push_back(makeIntParam("numVoices", "Voice Limit", 1, 16, 6));
    params.push_back(makeBool("sustainInverted", "Sustain Inverted", false));
    params.push_back(makeParam("chorusHiss", "Chorus Hiss", 0.0f, 2.0f, 1.0f));
    params.push_back(makeIntParam("midiFunction", "MIDI Function", 0, 2, 2));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("aftertouchToVCF", "Aftertouch -> VCF", 0.0f, 1.0f, 0.5f));
    
    params.push_back(std::make_unique<juce::AudioParameterBool>("lowCpuMode", "Low CPU Mode", false));

    // Model Routing / Selección de Modelos (0 = J6, 1 = J60, 2 = J106)
    params.push_back(makeIntParam("modelDCO", "Model DCO", 0, 2, 2));
    params.push_back(makeIntParam("modelHPF", "Model HPF", 0, 2, 2));
    params.push_back(makeIntParam("modelVCF", "Model VCF", 0, 2, 2));
    params.push_back(makeIntParam("modelADSR", "Model ADSR", 0, 2, 2));
    params.push_back(makeIntParam("modelChorus", "Model Chorus", 0, 2, 2));
    params.push_back(makeIntParam("modelArp", "Model Arp", 0, 2, 0));
    params.push_back(makeIntParam("modelPoly", "Model Poly", 0, 2, 2));
    params.push_back(makeIntParam("modelPorta", "Model Portamento", 0, 2, 2));
    params.push_back(makeIntParam("modelUnison", "Model Unison", 0, 2, 2));

    // Arpeggiator Settings
    params.push_back(makeBool("arpEnabled", "Arpeggiator Enable", false));
    params.push_back(makeIntParam("arpMode", "Arpeggiator Mode", 0, 2, 0));
    params.push_back(makeIntParam("arpRange", "Arpeggiator Range", 0, 2, 0));
    params.push_back(makeParam("arpRate", "Arpeggiator Rate", 0.0f, 1.0f, 0.5f));
    params.push_back(makeBool("arpSync", "Arpeggiator Sync", false));
    params.push_back(makeIntParam("arpDivision", "Arpeggiator Division", 0, 8, 6));

    // Tape Echo / Delay Settings (Super Six only)
    params.push_back(makeBool("delayEnabled", "Delay Enable", false));
    params.push_back(makeIntParam("delaySetting", "Delay Setting", 0, 11, 11)); // default=REV ONLY
    params.push_back(makeParam("delayRepeatRate", "Delay Repeat Rate", 0.0f, 1.0f, 0.5f));
    params.push_back(makeParam("delayIntensity", "Delay Intensity", 0.0f, 1.0f, 0.5f));
    params.push_back(makeParam("delayBass", "Delay Bass", 0.0f, 1.0f, 0.5f));
    params.push_back(makeParam("delayTreble", "Delay Treble", 0.0f, 1.0f, 0.5f));
    params.push_back(makeParam("delayReverbVol", "Delay Reverb Vol", 0.0f, 1.0f, 0.5f));
    params.push_back(makeParam("delayEchoVol", "Delay Echo Vol", 0.0f, 1.0f, 0.5f));
    params.push_back(makeBool("delayEchoCancel", "Delay Echo Cancel", false));
    params.push_back(makeBool("delaySyncEnabled", "Delay Sync", false));
    params.push_back(makeIntParam("delaySyncDivision", "Delay Sync Division", 0, 8, 2)); // default=1/4 note
    params.push_back(makeIntParam("delayReverbType", "Delay Reverb Type", 0, 2, 0));
    params.push_back(makeParam("delayWowFlutter", "Delay Wow Flutter", 0.0f, 1.0f, 0.5f));
    params.push_back(makeParam("delayReverbDecay", "Delay Reverb Decay", 0.0f, 1.0f, 0.5f));
    params.push_back(makeParam("delayEchoIsolator", "Delay Echo Isolator", 0.0f, 1.0f, 0.5f));

    return { params.begin(), params.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ABDSimpleJuno106AudioProcessor(); }
bool ABDSimpleJuno106AudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* ABDSimpleJuno106AudioProcessor::createEditor() { 
    editor = new WebViewEditor (*this); 
    return editor; 
}

void ABDSimpleJuno106AudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    
    // [0006.txt] Add Session node for non-automatable state
    juce::ValueTree session("Session");
    session.setProperty("version", 2, nullptr);
    session.setProperty("currentBank", (presetManager ? presetManager->getActiveLibraryIndex() : 0), nullptr);
    session.setProperty("currentPreset", (presetManager ? presetManager->getCurrentPresetIndex() : 0), nullptr);
    session.setProperty("activeABSlot", activeSlot, nullptr);
    
    // Current Metadata
    session.setProperty("patchName", currentParams.patchName, nullptr);
    session.setProperty("author",    currentParams.author, nullptr);
    session.setProperty("category",  currentParams.category, nullptr);
    session.setProperty("tags",      currentParams.tags, nullptr);
    session.setProperty("notes",     currentParams.notes, nullptr);
    session.setProperty("date",      currentParams.creationDate, nullptr);
    session.setProperty("favorite",  currentParams.isFavorite, nullptr);

    state.addChild(session, -1, nullptr);

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ABDSimpleJuno106AudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType())) {
        auto tree = juce::ValueTree::fromXml(*xmlState);
        apvts.replaceState(tree);
        
        // Restore Session State if available (v2)
        auto session = tree.getChildWithName("Session");
        if (session.isValid()) {
            activeSlot = session.getProperty("activeABSlot", 0);
            
            currentParams.patchName    = session.getProperty("patchName", "Initial Patch");
            currentParams.author       = session.getProperty("author", "");
            currentParams.category     = session.getProperty("category", "Uncategorized");
            currentParams.tags         = session.getProperty("tags", "");
            currentParams.notes        = session.getProperty("notes", "");
            currentParams.creationDate = session.getProperty("date", "");
            currentParams.isFavorite   = session.getProperty("favorite", false);
            
            int bank = session.getProperty("currentBank", 0);
            /*int preset =*/ session.getProperty("currentPreset", 0);
            if (presetManager) {
                presetManager->selectLibrary(bank);
                // We don't necessarily load the preset here to avoid overwriting session tweaks,
                // but we keep the index synced for the UI.
            }
        }

        updateParamsFromAPVTS();
        engine->getVoiceManager().updateParams(currentParams);
        engine->getVoiceManager().forceUpdate();
        notifyUIOfStateChange();
    }
}
juce::MidiMessage ABDSimpleJuno106AudioProcessor::getCurrentSysExData() {
    return lastSysExMessage;
}
