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
}

SimpleJuno106AudioProcessor::~SimpleJuno106AudioProcessor() {}

const juce::String SimpleJuno106AudioProcessor::getName() const { return JucePlugin_Name; }
bool SimpleJuno106AudioProcessor::acceptsMidi() const { return true; }
bool SimpleJuno106AudioProcessor::producesMidi() const { return true; }
bool SimpleJuno106AudioProcessor::isMidiEffect() const { return false; }
double SimpleJuno106AudioProcessor::getTailLengthSeconds() const { return 0.0; }
int SimpleJuno106AudioProcessor::getNumPrograms() { return 1; }
int SimpleJuno106AudioProcessor::getCurrentProgram() { return 0; }
void SimpleJuno106AudioProcessor::setCurrentProgram (int index) { juce::ignoreUnused(index); }
const juce::String SimpleJuno106AudioProcessor::getProgramName (int index) { juce::ignoreUnused(index); return {}; }
void SimpleJuno106AudioProcessor::changeProgramName (int index, const juce::String& newName) { juce::ignoreUnused(index, newName); }

void SimpleJuno106AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    voiceManager.prepare(sampleRate, samplesPerBlock);
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 2;
    chorus.prepare(spec);
    chorus.reset();
    dcBlocker.prepare(spec);
    *dcBlocker.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 20.0f);
    masterLfoPhase = 0.0f;
}

void SimpleJuno106AudioProcessor::releaseResources() {}

bool SimpleJuno106AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void SimpleJuno106AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    keyboardState.processNextMidiBuffer (midiMessages, 0, buffer.getNumSamples(), true);
    juce::MidiBuffer midiOutEvents;

    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();
        if (message.isSysEx()) {
            sysExEngine.handleIncomingSysEx(message, currentParams);
            continue;
        }
        if (message.isController()) {
            const auto cn = message.getControllerNumber();
            const auto cv = message.getControllerValue();
            if (cn == 1) {
                if (auto* p = apvts.getParameter("benderToLFO")) p->setValueNotifyingHost(cv / 127.0f);
            }
            else if (cn == 64) performanceState.handleSustain(cv);
            else midiLearnHandler.handleIncomingCC(cn, cv, apvts);
            continue;
        }
        if (message.isPitchWheel()) {
            const auto val = (float)message.getPitchWheelValue();
            float norm = (val / 8192.0f) - 1.0f;
            if (auto* p = apvts.getParameter("bender")) p->setValueNotifyingHost(p->convertTo0to1(norm));
            continue;
        }
        if (message.isNoteOn()) voiceManager.noteOn(message.getChannel(), message.getNoteNumber(), message.getVelocity());
        else if (message.isNoteOff()) performanceState.handleNoteOff(message.getNoteNumber(), voiceManager);
    }
    performanceState.flushSustain(voiceManager);
    updateParamsFromAPVTS();
    applyPerformanceModulations(currentParams);
    voiceManager.setBenderAmount(currentParams.benderValue);
    voiceManager.setPortamentoEnabled(currentParams.portamentoOn);
    voiceManager.setPortamentoTime(currentParams.portamentoTime);
    voiceManager.setPortamentoLegato(currentParams.portamentoLegato);
    voiceManager.updateParams(currentParams);

    // [reimplement.md] Global LFO Phase calculation
    float ratio = JunoTimeCurves::kLfoMaxHz / JunoTimeCurves::kLfoMinHz;
    float lfoRateHz = JunoTimeCurves::kLfoMinHz * std::pow(ratio, currentParams.lfoRate);
    float phaseIncrement = (lfoRateHz / (float)getSampleRate());

    // Single LFO value for the block start (optimization) or increment per sample
    masterLfoPhase += phaseIncrement * buffer.getNumSamples();
    if (masterLfoPhase >= 1.0f) masterLfoPhase = std::fmod(masterLfoPhase, 1.0f);
    float lfoVal = std::sin(masterLfoPhase * 2.0f * juce::MathConstants<float>::pi);

    buffer.clear();
    voiceManager.renderNextBlock(buffer, 0, buffer.getNumSamples(), lfoVal);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    bool c1 = apvts.getRawParameterValue("chorus1")->load() > 0.5f;
    bool c2 = apvts.getRawParameterValue("chorus2")->load() > 0.5f;
    if (c1 || c2) {
         if (c1 && !c2) { 
            chorus.setRate(JunoChorusConstants::kRateI); 
            chorus.setDepth(JunoChorusConstants::kDepthI); 
            chorus.setMix(0.5f); 
            chorus.setCentreDelay(JunoChorusConstants::kDelayI);
         } else if (!c1 && c2) { 
            chorus.setRate(JunoChorusConstants::kRateII); 
            chorus.setDepth(JunoChorusConstants::kDepthII); 
            chorus.setMix(0.5f); 
            chorus.setCentreDelay(JunoChorusConstants::kDelayII);
         } else { 
            chorus.setRate(JunoChorusConstants::kRateIII); 
            chorus.setDepth(JunoChorusConstants::kDepthIII); 
            chorus.setMix(0.5f); 
            chorus.setCentreDelay(JunoChorusConstants::kDelayIII);
         }
         auto* l = buffer.getWritePointer(0);
         auto* r = buffer.getWritePointer(1); 
         for (int i = 0; i < buffer.getNumSamples(); ++i) {
             float noise = (chorusNoiseGen.nextFloat() * 2.0f - 1.0f) * JunoChorusConstants::kNoiseLevel;
             l[i] += noise;
             if (r) r[i] += noise;
         }
         chorus.process(context);
    }
    dcBlocker.process(context);
    if (midiOutEnabled) {
        midiMessages.addEvents(midiOutEvents, 0, buffer.getNumSamples(), 0);
        midiMessages.addEvents(midiOutBuffer, 0, buffer.getNumSamples(), 0);
        midiOutBuffer.clear();
    }
}

void SimpleJuno106AudioProcessor::enterTestMode(bool enter) { isTestMode = enter; }
#include "TestPrograms.h"
void SimpleJuno106AudioProcessor::triggerTestProgram(int bankIndex) {
    if (!isTestMode || bankIndex < 0 || bankIndex >= 8) return;
    const auto prog = getTestProgram(bankIndex);
    auto setVal = [&](juce::String id, float val) { if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(val); };
    auto setInt = [&](juce::String id, int val) {
         if (auto* p = apvts.getParameter(id)) {
              float norm = p->getNormalisableRange().convertTo0to1(static_cast<float>(val));
              p->setValueNotifyingHost(norm);
         }
    };
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

void SimpleJuno106AudioProcessor::handleNoteOn(juce::MidiKeyboardState*, int channel, int midiNoteNumber, float velocity) { voiceManager.noteOn(channel, midiNoteNumber, velocity); }
void SimpleJuno106AudioProcessor::handleNoteOff(juce::MidiKeyboardState*, int channel, int midiNoteNumber, float velocity) { performanceState.handleNoteOff(midiNoteNumber, voiceManager); }

void SimpleJuno106AudioProcessor::updateParamsFromAPVTS() {
    auto getVal = [this](juce::String id) { return apvts.getRawParameterValue(id)->load(); };
    auto getBool = [this](juce::String id) { return apvts.getRawParameterValue(id)->load() > 0.5f; };
    auto getInt = [this](juce::String id) { return static_cast<int>(apvts.getRawParameterValue(id)->load()); };
    currentParams.dcoRange = getInt("dcoRange"); currentParams.sawOn = getBool("sawOn"); currentParams.pulseOn = getBool("pulseOn");
    currentParams.pwmAmount = getVal("pwm"); currentParams.pwmMode = getInt("pwmMode"); currentParams.subOscLevel = getVal("subOsc");
    currentParams.noiseLevel = getVal("noise"); currentParams.lfoToDCO = getVal("lfoToDCO"); currentParams.hpfFreq = getInt("hpfFreq");
    currentParams.vcfFreq = getVal("vcfFreq"); currentParams.resonance = getVal("resonance"); currentParams.envAmount = getVal("envAmount");
    currentParams.lfoToVCF = getVal("lfoToVCF"); currentParams.kybdTracking = getVal("kybdTracking"); currentParams.vcfPolarity = getInt("vcfPolarity");
    currentParams.vcaMode = getInt("vcaMode"); currentParams.vcaLevel = getVal("vcaLevel"); currentParams.attack = getVal("attack");
    currentParams.decay = getVal("decay"); currentParams.sustain = getVal("sustain"); currentParams.release = getVal("release");
    currentParams.lfoRate = getVal("lfoRate"); currentParams.lfoDelay = getVal("lfoDelay");
    currentParams.chorus1 = getBool("chorus1"); currentParams.chorus2 = getBool("chorus2");
    currentParams.polyMode = getInt("polyMode"); voiceManager.setPolyMode(currentParams.polyMode);
    currentParams.portamentoTime = getVal("portamentoTime"); currentParams.portamentoOn = getBool("portamentoOn");
    currentParams.benderValue = getVal("bender"); currentParams.benderToDCO = getVal("benderToDCO"); currentParams.benderToVCF = getVal("benderToVCF");
    currentParams.tune = getVal("tune"); midiOutEnabled = getBool("midiOut"); lastParams = currentParams;
}

void SimpleJuno106AudioProcessor::applyPerformanceModulations(SynthParams& p) {
    float modWheel = p.benderToLFO;
    p.lfoToDCO = juce::jlimit(0.0f, 1.0f, p.lfoToDCO + modWheel);
    p.vcfLFOAmount = juce::jlimit(0.0f, 1.0f, p.lfoToVCF + modWheel);
}

void SimpleJuno106AudioProcessor::sendPatchDump() { if (midiOutEnabled) midiOutBuffer.addEvent(sysExEngine.makePatchDump(midiChannel - 1, currentParams), 0); }

void SimpleJuno106AudioProcessor::loadPreset(int index) {
    if (presetManager) {
        presetManager->setCurrentPreset(index);
        voiceManager.resetAllVoices();
        auto state = presetManager->getCurrentPresetState();
        if (state.isValid()) {
            for (int i = 0; i < state.getNumProperties(); ++i) {
                auto propName = state.getPropertyName(i).toString();
                if (auto* p = apvts.getParameter(propName)) {
                    if (state.getProperty(propName).isDouble() || state.getProperty(propName).isInt()) {
                         float val = static_cast<float>(state.getProperty(propName));
                         p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(val));
                    }
                }
            }
            updateParamsFromAPVTS(); 
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
    std::unique_ptr<juce::XmlElement> xml = std::make_unique<juce::XmlElement>("JUNiO601");
    std::unique_ptr<juce::XmlElement> paramsXml(state.createXml());
    if (paramsXml) xml->addChildElement(paramsXml.release());
    auto midiXml = midiLearnHandler.saveState().createXml();
    if (midiXml) xml->addChildElement(midiXml.release());
    copyXmlToBinary(*xml, destData);
}

void SimpleJuno106AudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr) {
        if (xmlState->hasTagName("JUNiO601") || xmlState->hasTagName("Juno106Plugin")) {
            if (auto* paramsXml = xmlState->getChildByName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*paramsXml));
            if (auto* midiXml = xmlState->getChildByName("MIDI_MAPPINGS")) midiLearnHandler.loadState(juce::ValueTree::fromXml(*midiXml));
        }
        else if (xmlState->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}
