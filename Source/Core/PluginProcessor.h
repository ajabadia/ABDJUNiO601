#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <cmath>
#include <algorithm>
#include "../Synth/Voice.h"
#include "JunoVoiceManager.h"
#include "JunoSysEx.h"
#include "MidiLearnHandler.h"
#include "JunoSysExEngine.h"
#include "PerformanceState.h"
#include "JunoBBD.h" // [Correct Placement]

class PresetManager;

class SimpleJuno106AudioProcessor : public juce::AudioProcessor,
                                     public juce::MidiKeyboardState::Listener {
public:
    SimpleJuno106AudioProcessor();
    ~SimpleJuno106AudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // midiOutEnabled removed, using SynthParams
    int midiChannel = 1; 
    juce::MidiBuffer midiOutBuffer;
    MidiLearnHandler midiLearnHandler;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    
    // [Fidelidad] TailTime: Release max 12s + Chorus 25ms => ~14s safety margin
    double getTailLengthSeconds() const override { return 14.0; }

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    // double getTailLengthSeconds() const override; // [Removed duplicate]
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
    
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    class PresetManager* getPresetManager();
    const JunoVoiceManager& getVoiceManager() const { return voiceManager; }
    JunoVoiceManager& getVoiceManagerNC() { return voiceManager; } 
    MidiLearnHandler& getMidiLearnHandler() { return midiLearnHandler; }
    
    juce::MidiKeyboardState keyboardState;

    void loadPreset(int index);
    void updateParamsFromAPVTS();
    
    void applyPerformanceModulations(SynthParams& p);
    void sendPatchDump();
    void sendManualMode(); 
    void triggerPanic();
    void setSustainPolarity(bool inverted) { sustainInverted = inverted; }

    SynthParams getMirrorParameters(); // [Fidelidad] Block-consistent mirror

    float getChorusLfoPhase(int mode) const { 
        return (mode == 1) ? chorusLfoPhaseI : chorusLfoPhaseII; 
    }

    bool isTestMode = false;
    void triggerTestProgram(int bankIndex);
    void enterTestMode(bool enter);
    void triggerLFO();

    void handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;

    juce::AudioProcessorEditor* editor = nullptr;

    juce::MidiMessage getCurrentSysExData();

    void undo() { undoManager.undo(); }
    void redo() { undoManager.redo(); }
    void toggleMidiOut() { 
        if (auto* p = apvts.getParameter("midiOut")) 
            p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.0f : 1.0f);
    }

private:
    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    JunoVoiceManager voiceManager;
    SynthParams currentParams;
    SynthParams lastParams;
    std::atomic<bool> needsVoiceReset { false };


    std::unique_ptr<class PresetManager> presetManager;
    
    JunoSysExEngine sysExEngine;
    PerformanceState performanceState;
    bool sustainInverted = false;
    
    // [Fidelidad] Store last SysEx for Display
    juce::MidiMessage lastSysExMessage;
    // Helper to send and store
    void sendSysEx(const juce::MidiMessage& msg) {
        if (currentParams.midiOut) midiOutBuffer.addEvent(msg, 0);
        lastSysExMessage = msg;
    }

    // [Fidelidad] Authentic MN3009 BBD Emulation
    JunoDSP::JunoBBD chorus; 
    JunoDSP::JunoBBD chorus2; // Mode II / Dual line
    juce::Random chorusNoiseGen; 
    
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> dcBlocker; 
    
    // [VCA/Chorus Audit] Filters for authentic chorus emulation
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> chorusPreEmphasisFilter;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> chorusDeEmphasisFilter;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> chorusNoiseFilter;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> chorusNoiseHPF;
    juce::AudioBuffer<float> chorusNoiseBuffer;

    float masterLfoPhase = 0.0f;
    float masterLfoDelayEnvelope = 0.0f;
    
    // [Fidelidad] Chorus LFOs para Leakage y LED
    float chorusLfoPhaseI = 0.0f;
    float chorusLfoPhaseII = 0.0f;
    
    float currentPowerSag = 0.0f;
    float chorusFade = 0.0f;

    void handleMidiEvents(juce::MidiBuffer& midiMessages);
    void updateParamsAndModulations();
    void renderAudio(juce::AudioBuffer<float>& buffer, int numSamples);
    void applyChorus(juce::AudioBuffer<float>& buffer, int numSamples);
    void processMasterEffects(juce::AudioBuffer<float>& buffer, int numSamples);

    int powerOnDelaySamples = 0;
    bool wasAnyNoteHeld = false;
    
    double warmUpTime = 0.0;
    float globalDriftAudible = 0.0f;
    int thermalCounter = 0;
    float thermalTarget = 0.0f;

    std::vector<float> lfoBuffer;

    // [Optimization] Cached Parameter Pointers (Audio Thread Safe)
    std::atomic<float>* dcoRangeParam = nullptr;
    std::atomic<float>* sawOnParam = nullptr;
    std::atomic<float>* pulseOnParam = nullptr;
    std::atomic<float>* pwmParam = nullptr;
    std::atomic<float>* pwmModeParam = nullptr;
    std::atomic<float>* subOscParam = nullptr;
    std::atomic<float>* noiseParam = nullptr;
    std::atomic<float>* lfoToDcoParam = nullptr;
    std::atomic<float>* hpfFreqParam = nullptr;
    std::atomic<float>* vcfFreqParam = nullptr;
    std::atomic<float>* resonanceParam = nullptr;
    std::atomic<float>* envAmountParam = nullptr;
    std::atomic<float>* vcfPolarityParam = nullptr;
    std::atomic<float>* kybdTrackingParam = nullptr;
    std::atomic<float>* lfoToVcfParam = nullptr;
    std::atomic<float>* vcaModeParam = nullptr;
    std::atomic<float>* vcaLevelParam = nullptr;
    std::atomic<float>* attackParam = nullptr;
    std::atomic<float>* decayParam = nullptr;
    std::atomic<float>* sustainParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    std::atomic<float>* lfoRateParam = nullptr;
    std::atomic<float>* lfoDelayParam = nullptr;
    std::atomic<float>* chorus1Param = nullptr;
    std::atomic<float>* chorus2Param = nullptr;
    std::atomic<float>* polyModeParam = nullptr;
    std::atomic<float>* portTimeParam = nullptr;
    std::atomic<float>* portOnParam = nullptr;
    std::atomic<float>* portLegatoParam = nullptr;
    std::atomic<float>* benderParam = nullptr;
    std::atomic<float>* benderDcoParam = nullptr;
    std::atomic<float>* benderVcfParam = nullptr;
    std::atomic<float>* benderLfoParam = nullptr;
    std::atomic<float>* tuneParam = nullptr;
    std::atomic<float>* masterVolParam = nullptr;
    std::atomic<float>* midiOutParam = nullptr;
    std::atomic<float>* lfoTrigParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleJuno106AudioProcessor)
};
