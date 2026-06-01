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
#include "../Synth/ChorusBBD.h"
#include "../Synth/JunoArpeggiator.h"
#include "JunoTests.h"

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
    arpeggiator = std::make_unique<JunoArpeggiator>();
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

    voiceManager.prepare(sr, samplesPerBlock);
    if (arpeggiator != nullptr) {
        arpeggiator->SetSampleRate((float)sr);
        arpeggiator->Reset();
    }
    voiceManager.setTuningTable(tuningManager.getTuningTable());
    juce::dsp::ProcessSpec spec { sr, (juce::uint32)samplesPerBlock, 2 };
    chorus.prepare(sr, samplesPerBlock);
    
    // [Fideltiy Fix] Safe DC Blocker Initialization
    dcBlocker.state = new juce::dsp::IIR::Coefficients<float>(1.0f, -1.0f, 1.0f, -0.995f);
    dcBlocker.prepare(spec);
    dcBlocker.reset();
    
    chorusNoiseBuffer.setSize(2, samplesPerBlock);

    lfoBuffer.assign(samplesPerBlock + 128, 0.0f); 
    
    smoothedSagGain.reset(sr, 0.02);
    smoothedSagGain.setCurrentAndTargetValue(1.0f);

    masterLFO.prepare(sr, samplesPerBlock);
    masterLFO.setCalibrationSettings(calibrationSettings.get());
    wasAnyNoteHeld = false;

    // Initialize dry noise & mains ripple
    dryNoise.Init((float)sr);
    dryNoise.mPinkEnabled = true;
    dryNoise.SetHighShelf(500.0f, -16.0f, (float)sr);
    dryRipple.SetMainsHz(60.0f, (float)sr);
    dryRipple.SetAmplitudes(1.8e-5f, 8.9e-6f, 6.3e-6f);

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
    const int numSamples = buffer.getNumSamples();
    // Update Service mode (TuningManager is static)
    if (serviceModeManager) serviceModeManager->update(getSampleRate(), buffer.getNumSamples());
    
    // [Telemetry] Signal MIDI activity to UI
    if (!midiMessages.isEmpty()) midiTrafficFlag.store(true);

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
                modWheelValue.store(message.getControllerValue() / 127.0f, std::memory_order_relaxed);
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

        if (message.isNoteOn()) {
            if (arpeggiator != nullptr && currentParams.arpEnabled) {
                arpeggiator->NoteOn(message.getNoteNumber());
            } else {
                voiceManager.noteOn(message.getChannel(), message.getNoteNumber(), message.getVelocity());
            }
        }
        else if (message.isNoteOff()) {
            if (arpeggiator != nullptr && currentParams.arpEnabled) {
                arpeggiator->NoteOff(message.getNoteNumber());
            } else {
                performanceState.handleNoteOff(message.getNoteNumber(), voiceManager);
            }
        }
    }
    performanceState.flushSustain(voiceManager);

    if (arpeggiator != nullptr) {
        arpeggiator->mEnabled = currentParams.arpEnabled && (currentParams.modelArp != 2);
        arpeggiator->mMode = currentParams.arpMode;
        arpeggiator->mRange = currentParams.arpRange;
        arpeggiator->mRate = JunoArpeggiator::arpRate(currentParams.arpRate);
        
        arpeggiator->mSyncToHost = currentParams.arpSync;
        if (currentParams.arpSync) {
            if (auto* playHead = getPlayHead()) {
                juce::AudioPlayHead::CurrentPositionInfo posInfo;
                if (playHead->getCurrentPosition(posInfo)) {
                    arpeggiator->mHostPlaying = posInfo.isPlaying;
                    arpeggiator->mHostBPM = posInfo.bpm;
                    arpeggiator->mHostBeatPos = posInfo.ppqPosition;
                    arpeggiator->mDivision = currentParams.arpDivision;
                }
            } else {
                arpeggiator->mSyncToHost = false;
            }
        }
        
        arpeggiator->Process(numSamples,
            [this](int note, int /*sampleOffset*/) {
                voiceManager.noteOn(0, note, 0.8f);
            },
            [this](int note, int /*sampleOffset*/) {
                voiceManager.noteOff(0, note, 0.0f);
            }
        );
    }

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
    
    if (++thermalCounter > currentParams.thermalInertia) {
        thermalCounter = 0;
        thermalTarget = (chorusNoiseGen.nextFloat() * 2.0f - 1.0f) * currentParams.thermalIntensity;
    }
    globalDriftAudible += (thermalTarget - globalDriftAudible) * currentParams.thermalMigration;
    currentParams.thermalDrift = globalDriftAudible;

    voiceManager.updateParams(currentParams);
    if (needsVoiceReset.exchange(false)) {
        voiceManager.forceUpdate();
    }

    bool resolvedPortamentoOn = currentParams.portamentoOn;
    voiceManager.setPortamentoEnabled(resolvedPortamentoOn);
    voiceManager.setPortamentoTime(currentParams.portamentoTime);
    voiceManager.setPortamentoLegato(currentParams.portamentoLegato);
    voiceManager.setBenderAmount(currentParams.benderValue); // Removed globalDriftAudible (applied in updatePitch)

    renderAudio(buffer, numSamples);
    applyChorus(buffer, numSamples);
    processMasterEffects(buffer, numSamples);

    // [Build 103] Recording Capture (Post-Process)
    {
        const juce::ScopedLock sl(writerLock);
        if (threadedWriter != nullptr) {
            threadedWriter->write(buffer.getArrayOfReadPointers(), numSamples);
        }
    }
}

void ABDSimpleJuno106AudioProcessor::renderAudio(juce::AudioBuffer<float>& buffer, int numSamples) {
    using namespace JunoConstants;
    const double sr = getSampleRate();

    bool lfoTrigRequested = fmtLfoTrig->load() > 0.5f;
    bool anyHeld = voiceManager.isAnyNoteHeld();
    
    masterLFO.setDepth(1.0f);
    masterLFO.setDelay(currentParams.lfoDelay * currentParams.lfoDelayMax);
    masterLFO.updateGateState(anyHeld, lfoTrigRequested);
    
    if (lfoTrigRequested) {
        fmtLfoTrig->store(0.0f);
    }
    
    // Ticking the digital LFO at tick rate
    // mcuRateFactor is in ms (default 4.2335ms)
    float tickRateMs = 4.2335f;
    if (calibrationSettings != nullptr) {
        tickRateMs = calibrationSettings->getValue("lfoTickRateMs");
    }
    double tickRateHz = 1000.0 / (double)tickRateMs;
    double samplesPerTick = sr / tickRateHz;
    
    static double lfoTimeAccumulator = 0.0;
    
    for (int i = 0; i < numSamples; ++i) {
        lfoTimeAccumulator += 1.0;
        if (lfoTimeAccumulator >= samplesPerTick) {
            lfoTimeAccumulator -= samplesPerTick;
            masterLFO.tick106();
        }
        
        float lfoVal = masterLFO.process(currentParams.lfoRate);
        
        // Apply LFO Resolution stepping if configured
        float res = currentParams.lfoResolution;
        if (res > 1.0f) {
            lfoVal = std::round(lfoVal * res) / res;
        }
        
        lfoBuffer[i] = lfoVal;
    }

    voiceManager.renderNextBlock(buffer, 0, numSamples, lfoBuffer);
}

void ABDSimpleJuno106AudioProcessor::applyChorus (juce::AudioBuffer<float>& buffer, int /*numSamples*/)
{
    // [Build 29] Chorus Cycle Diagnostic Override
    int activeMode = (currentParams.chorusCycleMode >= 0) ? currentParams.chorusCycleMode : currentParams.chorusMode;
    ChorusBBD::Mode mode = static_cast<ChorusBBD::Mode>(activeMode);
    
    chorus.setMode(mode);
    chorus.setHissLevel(currentParams.chorusHissLvl);
    chorus.setHissMultiplier(currentParams.chorusHiss); // Add preference scaling
    
    if (mode == ChorusBBD::Mode::Off)
        return;

    // [Build 29] Dynamic Calibration Overrides
    chorus.setCalibrationParams(currentParams.chorusDelayI, 
                                currentParams.chorusDelayII, 
                                currentParams.chorusGainDry,
                                currentParams.chorusGainWet,
                                currentParams.chorusModDepth, 
                                currentParams.chorusSatBoost, 
                                currentParams.chorusFilterCutoff,
                                currentParams.chorusBothRate);

    // Map hardware-authentic rates (I=0.47Hz, II=0.78Hz)
    float rate = (mode == ChorusBBD::Mode::ChorusII) ? currentParams.chorusLfoRateII : currentParams.chorusLfoRate;
    if (mode == ChorusBBD::Mode::ChorusBoth) rate = currentParams.chorusBothRate; 
    
    chorus.setRate(rate);
    chorus.setDepth(1.0f); 
    chorus.setMix(currentParams.chorusMix);
    
    chorus.process(buffer);
}

void ABDSimpleJuno106AudioProcessor::processMasterEffects(juce::AudioBuffer<float>& buffer, int numSamples) {
    float masterVol = currentParams.masterVolume * currentParams.masterOutputGain;
    float envSum = voiceManager.getTotalEnvelopeLevel();
    float finalSag = 1.0f - (envSum * currentParams.vcaSagAmt);
    finalSag = juce::jlimit(0.7f, 1.0f, finalSag);
    smoothedSagGain.setTargetValue(finalSag);
    
    buffer.applyGain(smoothedSagGain.getNextValue() * masterVol);

    // [Fidelity] Master Noise Floor (Pink floor noise) and Mains Ripple
    float dbScale = std::pow(10.0f, (currentParams.masterNoise + 80.0f) / 20.0f);
    float dryNoiseVol = 0.0015f * currentParams.noiseFloorMul * dbScale;
    float rippleVol = currentParams.mainsRippleMul;

    for (int i = 0; i < numSamples; ++i) {
        float n = dryNoise.Process() * dryNoiseVol;
        n += dryRipple.Process() * rippleVol;
        
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            buffer.getWritePointer(ch)[i] += n;
        }
    }

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
        float bleed = currentParams.stereoBleed;
        for (int i = 0; i < numSamples; ++i) {
            float vL = l[i], vR = r[i];
            l[i] = vL + vR * bleed;
            r[i] = vR + vL * bleed;
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

void ABDSimpleJuno106AudioProcessor::enterTestMode(bool enter) { isTestMode = enter; }
#include "TestPrograms.h"
void ABDSimpleJuno106AudioProcessor::triggerTestProgram(int bankIndex) {
    #if JUCE_DEBUG
    if (bankIndex == 99) {
        if (presetManager) JunoTests::runJunoPatchRoundtripTest(*presetManager);
        return;
    }
    if (bankIndex == 98) {
        JunoTests::runSysExPatchDumpRoundtripTest();
        return;
    }
    if (bankIndex == 97) {
        if (presetManager) JunoTests::runPresetJsonRoundtripTest(*presetManager);
        return;
    }
    #endif

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

void ABDSimpleJuno106AudioProcessor::handleNoteOn(juce::MidiKeyboardState*, int /*channel*/, int midiNoteNumber, float velocity) { voiceManager.noteOn(0, midiNoteNumber, velocity); }
void ABDSimpleJuno106AudioProcessor::handleNoteOff(juce::MidiKeyboardState*, int /*channel*/, int midiNoteNumber, float /*velocity*/) { performanceState.handleNoteOff(midiNoteNumber, voiceManager); }

SynthParams ABDSimpleJuno106AudioProcessor::getMirrorParameters() {
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

    // [New] SNAPSHOT-consistent master volume
    p.masterVolume = (fmtMasterVolume != nullptr) ? fmtMasterVolume->load() : 1.0f;

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
    p.memoryProtect = fmtMemoryProtect->load() > 0.5f;

    // Model Routing / Selección de Modelos
    p.modelDCO   = (int)std::lround(fmtModelDCO->load());
    p.modelHPF   = (int)std::lround(fmtModelHPF->load());
    p.modelVCF   = (int)std::lround(fmtModelVCF->load());
    p.modelADSR  = (int)std::lround(fmtModelADSR->load());
    p.modelChorus = (int)std::lround(fmtModelChorus->load());
    p.modelArp   = (int)std::lround(fmtModelArp->load());
    p.modelPoly  = (int)std::lround(fmtModelPoly->load());
    p.modelPorta = (int)std::lround(fmtModelPorta->load());
    p.modelUnison = (int)std::lround(fmtModelUnison->load());

    // Arpeggiator Settings
    p.arpEnabled = fmtArpEnabled->load() > 0.5f;
    p.arpMode    = (int)std::lround(fmtArpMode->load());
    p.arpRange   = (int)std::lround(fmtArpRange->load());
    p.arpRate    = fmtArpRate->load();
    p.arpSync    = fmtArpSync->load() > 0.5f;
    p.arpDivision = (int)std::lround(fmtArpDivision->load());
    
    // --- Inject Calibration Overrides ---
    p.dcoMixerGain = calibrationSettings->getValueForModel("dcoMixerGain", p.modelDCO);
    p.subGainScale = calibrationSettings->getValueForModel("subGainScale", p.modelDCO);
    p.noiseGainScale = calibrationSettings->getValueForModel("noiseGainScale", p.modelDCO);
    p.mixerSaturation = calibrationSettings->getValueForModel("mixerSaturation", p.modelDCO);

    p.thermalIntensity = calibrationSettings->getValue("thermalIntensity");
    p.thermalInertia = calibrationSettings->getValue("thermalInertia");
    p.thermalMigration = calibrationSettings->getValue("thermalMigration");
    p.vcaSagAmt = calibrationSettings->getValueForModel("vcaSagAmt", p.modelPoly);
    p.vcaCrosstalk = calibrationSettings->getValueForModel("vcaCrosstalk", p.modelPoly);
    p.masterNoise = calibrationSettings->getValue("masterNoise");
    p.stereoBleed = calibrationSettings->getValue("stereoBleed");
    p.sliderHysteresis = calibrationSettings->getValue("sliderHysteresis");
    p.paramSlewRate = calibrationSettings->getValue("paramSlewRate");
    p.staggeredUpdateMaxMs = calibrationSettings->getValue("staggeredUpdateMaxMs");
    
    // Load Voice Variables
    p.voiceVariance = calibrationSettings->getValueForModel("voiceVariance", p.modelPoly);
    p.unisonSpread = calibrationSettings->getValueForModel("unisonSpread", p.modelPoly);
    p.dcoDriftComplexity = calibrationSettings->getValueForModel("dcoDriftComplexity", p.modelDCO);
    p.pwmCenterDuty = calibrationSettings->getValueForModel("pwmCenterDuty", p.modelDCO);
    p.pwmMaxDuty = calibrationSettings->getValueForModel("pwmMaxDuty", p.modelDCO);
    p.pwmMinDuty = calibrationSettings->getValueForModel("pwmMinDuty", p.modelDCO);
    p.pwmOffThreshold = calibrationSettings->getValueForModel("pwmOffThreshold", p.modelDCO);
    p.pwmSlewRateManual = calibrationSettings->getValueForModel("pwmSlewRateManual", p.modelDCO);
    p.pwmSlewRateLFO = calibrationSettings->getValueForModel("pwmSlewRateLFO", p.modelDCO);
    p.dcoVoiceDrift = calibrationSettings->getValueForModel("dcoVoiceDrift", p.modelDCO);
    p.dcoGlobalDrift = calibrationSettings->getValueForModel("dcoGlobalDrift", p.modelDCO);

    // [Build 25/29] Filter Calibration
    p.vcfMinHz = calibrationSettings->getValueForModel("vcfMinHz", p.modelVCF);
    p.vcfMaxHz = calibrationSettings->getValueForModel("vcfMaxHz", p.modelVCF);
    p.vcfSelfOscThreshold = calibrationSettings->getValueForModel("vcfSelfOscThreshold", p.modelVCF);
    p.vcfSaturation = calibrationSettings->getValueForModel("vcfSaturation", p.modelVCF);
    p.vcfResoComp = calibrationSettings->getValueForModel("vcfResoComp", p.modelVCF);
    p.vcfResoCompBoost = calibrationSettings->getValueForModel("vcfResoCompBoost", p.modelVCF);
    p.vcfLfoDepth = calibrationSettings->getValueForModel("vcfLfoDepth", p.modelVCF);
    p.vcfEnvRange = calibrationSettings->getValueForModel("vcfEnvRange", p.modelVCF);
    p.vcfSelfOscInt = calibrationSettings->getValueForModel("vcfSelfOscInt", p.modelVCF);
    p.vcfTrackCenter = calibrationSettings->getValueForModel("vcfTrackCenter", p.modelVCF);
    p.vcfResoSpread = calibrationSettings->getValueForModel("vcfResoSpread", p.modelVCF);
    p.vcfWidth = calibrationSettings->getValueForModel("vcfWidth", p.modelVCF);
    p.vcfSlewMs = calibrationSettings->getValueForModel("vcfSlewMs", p.modelVCF);

    // [Build 28] HPF Calibration
    p.hpfFreq1         = calibrationSettings->getValueForModel("hpfFreq1", p.modelHPF);
    p.hpfFreq2         = calibrationSettings->getValueForModel("hpfFreq2", p.modelHPF);
    p.hpfFreq3         = calibrationSettings->getValueForModel("hpfFreq3", p.modelHPF);
    p.hpfShelfFreq     = calibrationSettings->getValueForModel("hpfShelfFreq", p.modelHPF);
    p.hpfShelfGain     = calibrationSettings->getValueForModel("hpfShelfGain", p.modelHPF);
    p.hpfQ             = calibrationSettings->getValueForModel("hpfQ", p.modelHPF);
    p.hpfBassBoostGain = calibrationSettings->getValueForModel("hpfBassBoostGain", p.modelHPF);

    // [Build 29] VCA Calibration
    p.vcaMasterGain = juce::jmax(0.01f, calibrationSettings->getValueForModel("vcaMasterGain", p.modelPoly));
    p.vcaVelSensScale = calibrationSettings->getValueForModel("vcaVelSensScale", p.modelPoly);
    p.vcaKillThreshold = calibrationSettings->getValueForModel("vcaKillThreshold", p.modelPoly);
    p.vcaBleed = calibrationSettings->getValueForModel("vcaBleed", p.modelPoly);
    p.vcaDcOffset = calibrationSettings->getValueForModel("vcaDcOffset", p.modelPoly);
    p.vcaOffset = calibrationSettings->getValueForModel("vcaOffset", p.modelPoly);
    p.vcaSlewMs = calibrationSettings->getValueForModel("vcaSlewMs", p.modelPoly);

    // [Fidelity Sync] Master & Global Offsets
    p.masterOutputGain = std::pow(10.0f, calibrationSettings->getValue("masterOutputGain") / 20.0f);
    p.masterPitchCents = calibrationSettings->getValue("masterPitchCents");

    // [Build 29] Envelope Calibration (Full Sync)
    p.adsrSlewMs = calibrationSettings->getValueForModel("adsrSlewMs", p.modelADSR);
    p.adsrAttackFactor = calibrationSettings->getValueForModel("adsrAttackFactor", p.modelADSR);
    p.adsrCurveExponent = calibrationSettings->getValueForModel("adsrCurveExponent", p.modelADSR);
    p.adsrMcuRate = calibrationSettings->getValueForModel("adsrMcuRate", p.modelADSR);
    p.adsrDacSteps = calibrationSettings->getValueForModel("adsrDacSteps", p.modelADSR);
    p.adsrOvershoot = calibrationSettings->getValueForModel("adsrOvershoot", p.modelADSR);
    p.adsrVariance = calibrationSettings->getValueForModel("adsrVariance", p.modelADSR);
    p.vcaCurveType = calibrationSettings->getValueForModel("vcaCurveType", p.modelPoly);

    // [Build 29] Chorus Calibration
    p.chorusHissLvl = calibrationSettings->getValueForModel("chorusHissLvl", p.modelChorus);
    p.chorusDelayI = calibrationSettings->getValueForModel("chorusDelayI", p.modelChorus);
    p.chorusDelayII = calibrationSettings->getValueForModel("chorusDelayII", p.modelChorus);
    p.chorusModDepth = calibrationSettings->getValueForModel("chorusModDepth", p.modelChorus);
    p.chorusSatBoost = calibrationSettings->getValueForModel("chorusSatBoost", p.modelChorus);
    p.chorusFilterCutoff = calibrationSettings->getValueForModel("chorusFilterCutoff", p.modelChorus);
    p.chorusLfoRate = calibrationSettings->getValueForModel("chorusLfoRate", p.modelChorus);
    p.chorusLfoRateII = calibrationSettings->getValueForModel("chorusLfoRateII", p.modelChorus);
    p.chorusBothRate = calibrationSettings->getValueForModel("chorusBothRate", p.modelChorus);

    // [BUG FIX] Resolve chorusMode from panel buttons (chorus1, chorus2).
    if (p.chorus1 && p.chorus2)
        p.chorusMode = 3;
    else if (p.chorus2)
        p.chorusMode = 2;
    else if (p.chorus1)
        p.chorusMode = 1;
    else
        p.chorusMode = 0;

    // [Build 25] LFO Calibration
    p.lfoMaxRate = calibrationSettings->getValue("lfoMaxRate");
    p.lfoMinRate = calibrationSettings->getValue("lfoMinRate");
    p.lfoDelayMax = calibrationSettings->getValue("lfoDelayMax");
    p.lfoResolution = calibrationSettings->getValue("lfoResolution");
    
    // New DCO Mix Levels and Switch Ramp ms
    p.sawMixAmp = calibrationSettings->getValueForModel("sawMixAmp", p.modelDCO);
    p.pulseMixAmp = calibrationSettings->getValueForModel("pulseMixAmp", p.modelDCO);
    p.subMixAmp = calibrationSettings->getValueForModel("subMixAmp", p.modelDCO);
    p.noiseMixAmp = calibrationSettings->getValueForModel("noiseMixAmp", p.modelDCO);
    p.audioTaperScale = calibrationSettings->getValueForModel("audioTaperScale", p.modelDCO);
    p.dcoLfoShuntK = calibrationSettings->getValueForModel("dcoLfoShuntK", p.modelDCO);
    p.dcoLfoMaxSemitones = calibrationSettings->getValueForModel("dcoLfoMaxSemitones", p.modelDCO);
    p.oscSwitchRampMs = calibrationSettings->getValueForModel("oscSwitchRampMs", p.modelDCO);
    
    // New LFO Calibration Params
    p.lfoTickRateMs = calibrationSettings->getValue("lfoTickRateMs");
    p.lfoAccumMax = calibrationSettings->getValue("lfoAccumMax");
    p.lfoHoldoffThresh = calibrationSettings->getValue("lfoHoldoffThresh");

    // [New Phase 3 Calibration]
    p.noiseFloorMul = calibrationSettings->getValue("noiseFloorMul");
    p.mainsRippleMul = calibrationSettings->getValue("mainsRippleMul");
    p.voiceVcfFrqSpread = calibrationSettings->getValue("voiceVcfFrqSpread");
    p.voiceVcfWidthSpread = calibrationSettings->getValue("voiceVcfWidthSpread");
    p.voiceVcaGainSpread = calibrationSettings->getValue("voiceVcaGainSpread");
    p.driftWalkIntensity = calibrationSettings->getValueForModel("driftWalkIntensity", p.modelPoly);
    p.oversampling = (int)std::lround(calibrationSettings->getValue("oversampling"));

    // [Build 29] Diagnostic Cycle States
    p.hpfCyclePos = serviceModeManager->getHpfCyclePos();
    p.chorusCycleMode = serviceModeManager->getChorusCycleMode();

    // Copy metadata from current state
    p.patchName     = currentParams.patchName;
    p.author        = currentParams.author;
    p.category      = currentParams.category;
    p.tags          = currentParams.tags;
    p.notes         = currentParams.notes;
    p.creationDate  = currentParams.creationDate;
    p.isFavorite    = currentParams.isFavorite;

    return p;
}

void ABDSimpleJuno106AudioProcessor::updateParamsFromAPVTS() {
    currentParams = getMirrorParameters();
    currentParams.currentAftertouch = currentAftertouch.load();
    lastParams = currentParams;
}

void ABDSimpleJuno106AudioProcessor::applyPerformanceModulations(SynthParams& p) {
    float mw = modWheelValue.load(std::memory_order_relaxed);
    p.lfoToDCO = juce::jlimit<float>(0.0f, 1.0f, p.lfoToDCO + mw * p.benderToLFO);
    p.vcfLFOAmount = p.lfoToVCF;
    p.envAmount = juce::jlimit<float>(0.0f, 1.0f, p.envAmount + currentAftertouch.load() * p.aftertouchToVCF);

    // 1. Portamento Rate 3-segment calculation
    float portTimeSlider = p.portamentoTime;
    float coeff = 0.0f;
    if (portTimeSlider > 0.0f) {
        float i = portTimeSlider * 127.f;
        if (i <= 25.f) {
            coeff = 255.f - 8.f * (i - 1.f);
        } else if (i <= 47.f) {
            coeff = 63.f - 2.f * (i - 25.f);
        } else {
            coeff = std::round(18.f * std::pow(0.9625f, i - 48.f));
        }
    }
    // Rate in semitones/second
    p.portamentoRateST = coeff * (1000.f / p.lfoTickRateMs) / 256.f;

    // 2. DCO LFO Depth Taper with shunt: t = t / (1 + k*t - k*t^2)
    float lfoToDcoRaw = p.lfoToDCO;
    float shuntK = p.dcoLfoShuntK;
    float lfoTaper = lfoToDcoRaw / (1.0f + shuntK * lfoToDcoRaw - shuntK * lfoToDcoRaw * lfoToDcoRaw);
    
    // Scale to max semitones
    p.dcoLfoPitchDepth = lfoTaper * p.dcoLfoMaxSemitones;
}

void ABDSimpleJuno106AudioProcessor::sendSysEx(const juce::MidiMessage& msg) {
    if (currentParams.midiOut) {
        midiOutBuffer.addEvent(msg, 0);
        lastSentSysExMessage = msg;
    }
    lastSysExMessage = msg; // Update for UI display

    // Notify WebUI for real-time stream display
    if (editor != nullptr) {
        if (auto* wv = dynamic_cast<WebViewEditor*>(editor)) {
            wv->dispatchToJS("sysexLog", juce::String::toHexString(msg.getRawData(), msg.getRawDataSize()));
        }
    }
}

void ABDSimpleJuno106AudioProcessor::sendPatchDump() { sendSysEx(sysExEngine.makePatchDump(currentParams.midiChannel - 1, currentParams)); }
void ABDSimpleJuno106AudioProcessor::sendManualMode() { 
    sendSysEx(JunoSysEx::createManualMode(currentParams.midiChannel - 1)); 
    
    // [Manual Mode Logic] Force immediate snapshot from APVTS to DSP
    updateParamsFromAPVTS();
    paramsAreDirty.store(true);
    patchDumpRequested.store(true); // Broadcast current physical state to MIDI out
    needsVoiceReset.store(true);
}

void ABDSimpleJuno106AudioProcessor::triggerPanic() {
    panicRequested.store(true);
    midiOutBuffer.addEvent(juce::MidiMessage::allNotesOff(1), 0);
}

void ABDSimpleJuno106AudioProcessor::triggerLFO() {
    if (fmtLfoTrig) fmtLfoTrig->store(1.0f);
}
void ABDSimpleJuno106AudioProcessor::applyPresetState(const juce::ValueTree& vt) {
    if (!vt.isValid()) return;

    // 1. Process all children recursively
    // This handles nested structures regardless of tag names ("Parameters", "Preset", etc.)
    for (int i = 0; i < vt.getNumChildren(); ++i) {
        applyPresetState(vt.getChild(i));
    }

    // 2. Iterate properties of the current node
    for (int i = 0; i < vt.getNumProperties(); ++i) {
        auto propName = vt.getPropertyName(i).toString();
        
        // [Fidelity] Systematic Calibration Hardening
        // We skip all parameters that belong to the "Hardware/Calibration" layer.
        // This ensures the user's calibration (Drift, Noise levels, Master Gain) 
        // survives preset loading.
        static const juce::StringArray calibrationParams = {
            "midiChannel", "numVoices", "benderRange", "velocitySens", "aftertouchToVCF", 
            "lcdBrightness", "sustainPedalInvert", "masterOutputGain", "masterPitchCents", 
            "midiFunction", "unisonWidth", "unisonDetune", "sustainMode", "enableLogging",
            "dcoMixerGain", "subGainScale", "noiseGainScale", "masterClockHz", "mixerSaturation", "dcoSawCurvature", 
            "noiseGain", "pwmCenterDuty", "pwmMaxDuty", "pwmMinDuty", "pwmOffset",
            "vcaMasterGain", "vcaBleed", "vcaVelSensScale", "vcaSagAmt", "vcaKillThreshold", "vcaDcOffset", "vcaOffset",
            "adsrSlewMs", "adsrAttackFactor", "adsrMcuRate", "adsrDacSteps", "adsrOvershoot", "adsrCurveExponent",
            "chorusMix", "chorusHiss", "chorusDelayI", "chorusDelayII", "chorusGainDry", "chorusGainWet", "chorusLfoRate", "chorusLfoRateII", "chorusBothRate", "chorusModDepth", "chorusSatBoost", "chorusFilterCutoff", "chorusHissColor",
            "lfoMaxRate", "lfoMinRate", "lfoDelayMax", "lfoResolution",
            "vcfMinHz", "vcfMaxHz", "vcfSelfOscThreshold", "vcfSaturation", "vcfResoComp", "vcfResoCompBoost", "vcfResPolK", "vcfFbScale", "vcfLfoDepth", "vcfEnvRange", "vcfSelfOscInt", "vcfTrackCenter", "vcfResoSpread", "vcfWidth",
            "hpfFreq1", "hpfFreq2", "hpfFreq3", "hpfBassBoostGain", "hpfShelfFreq", "hpfShelfGain", "hpfQ",
            "thermalIntensity", "thermalDrift", "thermalInertia", "thermalMigration",
            "vcaCrosstalk", "masterNoise", "stereoBleed", "voiceVariance", "unisonSpread", 
            "dcoGlobalDrift", "dcoVoiceDrift", "dcoDriftComplexity", "vcaRippleDepth", 
            "lfoDelayCurve", "dcoDriftRate", "dcoLfoPitchDepth", "pwmOffThreshold", 
            "pwmSlewRateManual", "pwmSlewRateLFO",
            "a4Reference", "oversampling", "sliderHysteresis", "paramSlewRate", "masterVolume",
            "name", "author", "category", "tags", "notes", "favorite", "date", 
            "originGroup", "originBank", "originPatch", "version", "currentBank", 
            "currentPreset", "activeABSlot"
        };
        
        if (calibrationParams.contains(propName)) continue;

        if (auto* p = apvts.getParameter(propName)) {
            float val = (float)(double)vt.getProperty(propName);
            auto range = p->getNormalisableRange();

            // [Safety] Adaptive Normalization
            // Only divide if val is significantly outside 0-1 range
            // and the parameter is a continuous slider (range.end > 3.1).
            // Discrete switches (0-1) and HPF (0-3) should never be normalized as floats.
            if (range.end > 3.1f && val > 1.001f) {
                val /= range.end;
            } else if (propName == "tune" && (val < -1.0f || val > 1.0f)) {
                val = range.convertTo0to1(val);
            }
            
            // [Audit] Log suspicious values that might cause silence
            if (val < 0.001f && (propName == "vcfFreq" || propName == "vcaLevel")) {
                juce::Logger::writeToLog("[JUNiO] Ingestion Warning: " + propName + " is virtually zero for patch data.");
            }
            
            p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, val));
        }
    }
}

void ABDSimpleJuno106AudioProcessor::loadPreset(int index) {
    if (presetManager) {
        presetManager->setCurrentPreset(index);
        auto state = presetManager->getCurrentPresetState();
        if (state.isValid()) {
            applyPresetState(state);
            
            updateParamsFromAPVTS(); 
            voiceManager.updateParams(currentParams);
            voiceManager.forceUpdate(); 
            paramsAreDirty.store(true); 
            needsVoiceReset.store(true);
            patchDumpRequested.store(true);
        }
        sendParamUpdateToUI();
        notifyUIOfStateChange();
    }
}


void ABDSimpleJuno106AudioProcessor::randomizeSound() {
    if (presetManager) {
        presetManager->randomizeCurrentParameters(apvts);
        
        // Ensure engine is zero-latency updated
        updateParamsFromAPVTS();
        voiceManager.updateParams(currentParams);
        voiceManager.forceUpdate();
        
        paramsAreDirty.store(true);
        requestPatchDump(); // Force full SysEx/WebUI refresh
        notifyUIOfStateChange();
    }
}


void ABDSimpleJuno106AudioProcessor::loadLibraryPreset(int libIdx, int presetIdx) {
    if (presetManager) {
        // [Safety] Calculate absolute index for the 26-bank global space
        int absoluteIdx = (libIdx * 64) + presetIdx;
        
        presetManager->selectLibrary(libIdx);
        presetManager->setCurrentPreset(absoluteIdx);
        
        auto state = presetManager->getCurrentPresetState();
        if (state.isValid()) {
            applyPresetState(state);
            
            updateParamsFromAPVTS();
            voiceManager.updateParams(currentParams);
            voiceManager.forceUpdate();
            paramsAreDirty.store(true);
            needsVoiceReset.store(true);
            patchDumpRequested.store(true);
        }
        sendParamUpdateToUI();
        notifyUIOfStateChange();
    }
}

PresetManager* ABDSimpleJuno106AudioProcessor::getPresetManager() { return presetManager.get(); }

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
    params.push_back(makeIntParam("numVoices", "Voice Limit", 1, 16, 16));
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
        voiceManager.updateParams(currentParams);
        voiceManager.forceUpdate();
        notifyUIOfStateChange();
    }
}
void ABDSimpleJuno106AudioProcessor::loadTuningFile() {
    DBG("ABDSimpleJuno106AudioProcessor::loadTuningFile CALLED");
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
                juce::Logger::writeToLog("[JUNiO] Custom Tuning Loaded: " + currentTuningName);
            }
        }
    });
}

void ABDSimpleJuno106AudioProcessor::resetTuning() {
    DBG("ABDSimpleJuno106AudioProcessor::resetTuning CALLED");
    tuningManager.resetToStandard();
    voiceManager.setTuningTable(tuningManager.getTuningTable());
    currentTuningName = "Standard Tuning";
    DBG("Standard Tuning Restored");
}

bool ABDSimpleJuno106AudioProcessor::loadScalaTuning(const juce::File& file) {
    if (file.existsAsFile() && tuningManager.parseSCL(file)) {
        voiceManager.setTuningTable(tuningManager.getTuningTable());
        currentTuningName = file.getFileName();
        juce::Logger::writeToLog("[JUNiO] SCL loaded via WebView: " + currentTuningName);
        return true;
    }
    juce::Logger::writeToLog("[JUNiO] SCL parse failed: " + file.getFullPathName());
    return false;
}

juce::MidiMessage ABDSimpleJuno106AudioProcessor::getCurrentSysExData() {
    return lastSysExMessage;
}

void ABDSimpleJuno106AudioProcessor::switchABSlot(int slot)
{
    if (slot == activeSlot) return;

    // Save current parameters to the snapshot of the soon-to-be-inactive slot
    if (activeSlot == 0) slotA = getMirrorParameters();
    else                 slotB = getMirrorParameters();

    activeSlot = slot;
    const auto& newParams = (activeSlot == 0) ? slotA : slotB;

    // Apply snapshot to APVTS (this triggers updateParamsFromAPVTS via listeners effectively)
    auto setParam = [&](juce::String id, float val) {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(val);
    };

    setParam("dcoRange", (float)newParams.dcoRange);
    setParam("sawOn", newParams.sawOn ? 1.0f : 0.0f);
    setParam("pulseOn", newParams.pulseOn ? 1.0f : 0.0f);
    setParam("pwm", newParams.pwmAmount);
    setParam("pwmMode", (float)newParams.pwmMode);
    setParam("subOsc", newParams.subOscLevel);
    setParam("noise", newParams.noiseLevel);
    setParam("lfoToDCO", newParams.lfoToDCO);
    setParam("hpfFreq", (float)newParams.hpfFreq);
    setParam("vcfFreq", newParams.vcfFreq);
    setParam("resonance", newParams.resonance);
    setParam("envAmount", newParams.envAmount);
    setParam("lfoToVCF", newParams.lfoToVCF);
    setParam("kybdTracking", newParams.kybdTracking);
    setParam("vcfPolarity", (float)newParams.vcfPolarity);
    setParam("vcaMode", (float)newParams.vcaMode);
    setParam("vcaLevel", newParams.vcaLevel);
    setParam("attack", newParams.attack);
    setParam("decay", newParams.decay);
    setParam("sustain", newParams.sustain);
    setParam("release", newParams.release);
    setParam("lfoRate", newParams.lfoRate);
    setParam("lfoDelay", newParams.lfoDelay);
    setParam("chorus1", newParams.chorus1 ? 1.0f : 0.0f);
    setParam("chorus2", newParams.chorus2 ? 1.0f : 0.0f);

    // Also update current metadata
    currentParams.patchName     = newParams.patchName;
    currentParams.author        = newParams.author;
    currentParams.category      = newParams.category;
    currentParams.tags          = newParams.tags;
    currentParams.notes         = newParams.notes;
    currentParams.creationDate  = newParams.creationDate;
    currentParams.isFavorite    = newParams.isFavorite;

    notifyUIOfStateChange();
}

void ABDSimpleJuno106AudioProcessor::copyCurrentToAlternateSlot()
{
    if (activeSlot == 0) slotB = getMirrorParameters();
    else                 slotA = getMirrorParameters();
}

int ABDSimpleJuno106AudioProcessor::getWipCount() const
{
    if (presetManager)
    {
        int wipIdx = presetManager->getLibraryIndex("WIP");
        if (wipIdx >= 0)
        {
            const auto& lib = presetManager->getLibrary(wipIdx);
            return (int)lib.patches.size();
        }
    }
    return 0; 
}

void ABDSimpleJuno106AudioProcessor::updateMetadata(const SynthParams& newParams)
{
    currentParams.patchName    = newParams.patchName;
    currentParams.author       = newParams.author;
    currentParams.category     = newParams.category;
    currentParams.tags         = newParams.tags;
    currentParams.notes        = newParams.notes;
    currentParams.creationDate = newParams.creationDate;
    currentParams.isFavorite   = newParams.isFavorite;

    notifyUIOfStateChange();
}

void ABDSimpleJuno106AudioProcessor::notifyUIOfStateChange()
{
    // Notify the host that the state has changed
    updateHostDisplay();
    
    // Custom trigger for WebViewEditor if needed
    if (editor != nullptr) {
        if (auto* wv = dynamic_cast<WebViewEditor*>(editor)) {
            wv->sendPresetListUpdate();
            if (presetManager) {
                const auto& p = presetManager->getCurrentPreset();
                wv->sendBankPatchUpdate(p.originGroup, p.originBank, p.originPatch);
            }
        }
    }
}

void ABDSimpleJuno106AudioProcessor::sendParamUpdateToUI()
{
    if (editor != nullptr) {
        if (auto* wv = dynamic_cast<WebViewEditor*>(editor)) {
            juce::DynamicObject::Ptr state = new juce::DynamicObject();
            for (auto* param : getParameters()) {
                if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param)) {
                    state->setProperty(juce::Identifier(p->getParameterID()), (double)p->getValue());
                }
            }
            wv->dispatchToJS("parameterSetUpdate", juce::var(state.get()));
            juce::Logger::writeToLog("[JUNiO] Parameter Set Update dispatched to UI");
        }
    }
}
void ABDSimpleJuno106AudioProcessor::loadUserSettings() {
    auto file = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                    .getChildFile("ABD-IA")
                    .getChildFile("JUNiO-601")
                    .getChildFile("settings.xml");
    if (file.existsAsFile()) {
        auto xml = juce::XmlDocument::parse(file);
        if (xml != nullptr) {
            auto vt = juce::ValueTree::fromXml(*xml);
            if (vt.hasType("Settings")) {
                userName = vt.getProperty("userName", "ABD USER").toString();
            }
        }
    }
}

void ABDSimpleJuno106AudioProcessor::saveUserSettings() {
    auto file = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                    .getChildFile("ABD-IA")
                    .getChildFile("JUNiO-601")
                    .getChildFile("settings.xml");
    if (!file.getParentDirectory().exists()) file.getParentDirectory().createDirectory();

    juce::ValueTree vt("Settings");
    vt.setProperty("userName", userName, nullptr);
    auto xml = vt.createXml();
    if (xml != nullptr) xml->writeTo(file);
}

// [Build 103] Recording Implementation
void ABDSimpleJuno106AudioProcessor::toggleRecording() {
    if (isRecording()) stopRecording();
    else startRecording();
}

void ABDSimpleJuno106AudioProcessor::startRecording() {
    stopRecording();
    
    // 1. Create temporary file
    tempRecordingFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getNonexistentChildFile("junio_rec_tmp", ".wav");
    
    auto options = juce::AudioFormatWriterOptions()
                   .withSampleRate (getSampleRate())
                   .withNumChannels (2)
                   .withBitsPerSample (32);

    std::unique_ptr<juce::OutputStream> fileStream (tempRecordingFile.createOutputStream());
    if (fileStream != nullptr) {
        juce::WavAudioFormat wavFormat;
        if (auto writer = wavFormat.createWriterFor(fileStream, options)) {
            const juce::ScopedLock sl(writerLock);
            threadedWriter.reset (new juce::AudioFormatWriter::ThreadedWriter (writer.release(), backgroundThread, 32768));
            juce::Logger::writeToLog("[JUNiO] Recording started (32-bit float): " + tempRecordingFile.getFullPathName());
        }
    }
}

void ABDSimpleJuno106AudioProcessor::stopRecording() {
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writerToDestroy;
    {
        const juce::ScopedLock sl(writerLock);
        writerToDestroy.reset(threadedWriter.release());
    }
    
    if (writerToDestroy != nullptr) {
        // Wait for background thread to flush and destroy
        writerToDestroy.reset(); 
        juce::Logger::writeToLog("[JUNiO] Recording stopped. Finalizing file...");

        // Trigger File Chooser for official save
        juce::String timestamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
        juce::String defaultName = "junio601_" + timestamp + ".wav";
        
        fileChooser = std::make_unique<juce::FileChooser>("Save Synthesizer Recording",
                                                          juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(defaultName),
                                                          "*.wav");
        
        fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                                 [this](const juce::FileChooser& fc) {
            auto result = fc.getResult();
            if (result.getFullPathName().isNotEmpty()) {
                if (result.existsAsFile()) result.deleteFile();
                tempRecordingFile.moveFileTo(result);
                juce::Logger::writeToLog("[JUNiO] Recording saved to: " + result.getFullPathName());
            } else {
                tempRecordingFile.deleteFile();
                juce::Logger::writeToLog("[JUNiO] Recording discarded by user.");
            }
        });
    }
}

ABDSimpleJuno106AudioProcessor::SelfTestResult ABDSimpleJuno106AudioProcessor::runSelfTest()
{
    SelfTestResult result;
    result.hasRun = true;
    
    // Test 1: 128 Factory Patches Roundtrip
    if (presetManager) {
        JunoTests::runJunoPatchRoundtripTest(*presetManager, result.presetFailures, result.failedPresets);
    } else {
        result.presetFailures = 128; // Si no hay manager, falla todo
    }

    // Test 2: SysEx Dump Protocol
    JunoTests::runSysExPatchDumpRoundtripTest(result.sysExOk);

    // Test 3: ValueTree/JSON Serialization
    if (presetManager) {
        JunoTests::runPresetJsonRoundtripTest(*presetManager, result.jsonOk);
    } else {
        result.jsonOk = false;
    }

    // Overall Certification Logic
    result.ok = (result.presetFailures == 0) && result.sysExOk && result.jsonOk;
    
    lastSelfTestResult = result;
    
    juce::Logger::writeToLog("[JUNiO] Fidelity Self-Test completed. Certified: " + juce::String(result.ok ? "YES" : "NO"));
    return result;
}
