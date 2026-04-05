#define ABD_PROCESSOR_HAS_TELEMETRY 1
#ifndef ABDSIMPLEJUNO106AUDIOPROCESSOR_H_INCLUDED
#define ABDSIMPLEJUNO106AUDIOPROCESSOR_H_INCLUDED

#include <JuceHeader.h>
#include <memory>
#include <atomic>
#include <cmath>
#include <algorithm>
#include "../Synth/JunoVoice.h"
#include "JunoVoiceManager.h"
#include "JunoSysEx.h"
#include "MidiLearnHandler.h"
#include "JunoSysExEngine.h"
#include "PerformanceState.h"
#include "TuningManager.h"
#include "CalibrationSettings.h"
#include "ServiceModeManager.h"
#include "../Synth/ChorusBBD.h"
class PresetManager;

class ABDSimpleJuno106AudioProcessor : public juce::AudioProcessor,
                                     public juce::MidiKeyboardState::Listener {
public:
    ABDSimpleJuno106AudioProcessor();
    ~ABDSimpleJuno106AudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

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
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
    
    // [0006.txt] State management
    void migrateStateIfNeeded(juce::ValueTree& state, int formatVersion, const juce::String& pluginVersion);
    void notifyUIOfStateChange();
    
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    class PresetManager* getPresetManager();
    const ABD::JunoVoiceManager& getVoiceManager() const { return voiceManager; }
    ABD::JunoVoiceManager& getVoiceManagerNC() { return voiceManager; } 
    ABD::PerformanceState& getPerformanceState() { return performanceState; }
    
    juce::MidiKeyboardState keyboardState;

    void loadPreset(int index);
    void loadLibraryPreset(int libIdx, int presetIdx);
    void updateParamsFromAPVTS();
    
    void applyPerformanceModulations(SynthParams& p);
    void sendPatchDump();
    void sendManualMode(); 
    void triggerPanic();
    void setSustainPolarity(bool inverted) { sustainInverted = inverted; }
    void triggerLFO();
    void loadTuningFile();
    void resetTuning();
    juce::String getCurrentTuningName() const { return currentTuningName; }

    SynthParams getMirrorParameters(); // [Fidelidad] Block-consistent mirror

    float getChorusLfoPhase(int mode) const { 
        return (mode == 1) ? chorusLfoPhaseI : chorusLfoPhaseII; 
    }

    bool isTestMode = false;
    void triggerTestProgram(int bankIndex);
    void enterTestMode(bool enter);

    void handleNoteOn(juce::MidiKeyboardState*, int /*channel*/, int midiNoteNumber, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int /*channel*/, int midiNoteNumber, float velocity) override;

    juce::AudioProcessorEditor* editor = nullptr;

    juce::MidiMessage getCurrentSysExData();
    
    std::atomic<bool> paramsAreDirty { true };
    std::atomic<bool> patchDumpRequested { false };
    void requestPatchDump() { patchDumpRequested.store(true); }

    void randomizeSound();
    void undo() { undoManager.undo(); }
    void redo() { undoManager.redo(); }
    void toggleMidiOut() { 
        if (auto* p = apvts.getParameter("midiOut")) 
            p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.0f : 1.0f);
    }

    ServiceModeManager& getServiceModeManager();
    CalibrationSettings& getCalibrationSettings() { return *calibrationSettings; }

    // [Telemetry]
    std::atomic<bool> midiTrafficFlag { false };
    bool popMidiTrafficFlag() { return midiTrafficFlag.exchange(false); }

    MidiLearnHandler& getMidiLearnHandler() { return midiLearnHandler; }

    // [Advanced Browser] A/B Compare & WIP
    void switchABSlot(int slot);
    void copyCurrentToAlternateSlot();
    void updateMetadata(const SynthParams& newParams);
    int getWipCount() const;
    int getActiveABSlot() const { return activeSlot; }
    void sendParamUpdateToUI();

private:
    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    ABD::JunoVoiceManager voiceManager;
    SynthParams currentParams;
    SynthParams lastParams;
    
    // [Advanced Browser] A/B Slots
    SynthParams slotA, slotB;
    int activeSlot = 0; // 0=A, 1=B

    std::atomic<bool> needsVoiceReset { false };


    std::unique_ptr<class PresetManager> presetManager;
    TuningManager tuningManager;
    std::unique_ptr<CalibrationSettings> calibrationSettings;
    std::unique_ptr<ServiceModeManager> serviceModeManager;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::String currentTuningName { "Standard Tuning" };
    
    JunoSysExEngine sysExEngine;
    ABD::PerformanceState performanceState;
    bool sustainInverted = false;
    
    // Store last SysEx for Echo Protection and Display
    juce::MidiMessage lastSysExMessage;
    juce::MidiMessage lastSentSysExMessage;
    
    // Helper to send and store
    void sendSysEx(const juce::MidiMessage& msg) {
        if (currentParams.midiOut) {
            midiOutBuffer.addEvent(msg, 0);
            lastSentSysExMessage = msg;
        }
        lastSysExMessage = msg; // Update for UI display
    }

    // [Fidelidad] Authentic MN3009 BBD Emulation
    ChorusBBD chorus; 
    juce::Random chorusNoiseGen; 
    juce::Random masterNoiseGen; 
    
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> dcBlocker; 
    
    // [VCA/Chorus Audit] Dynamic noise buffer
    juce::AudioBuffer<float> chorusNoiseBuffer;

    float masterLfoPhase = 0.0f;
    float masterLfoDelayEnvelope = 0.0f;
    
    // [Fidelidad] Chorus LFOs para Leakage y LED
    float chorusLfoPhaseI = 0.0f;
    float chorusLfoPhaseII = 0.0f;
    
    std::atomic<bool> panicRequested { false };
    juce::LinearSmoothedValue<float> smoothedSagGain;
    
    float currentPowerSag = 0.0f;
    std::atomic<float> currentAftertouch { 0.0f };
    float chorusFade = 0.0f;
    int midiFunction = 2; // [New] 0=I, 1=II, 2=III
    float unisonStereoWidth = 0.0f; // [New] Modern stereo spreading

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
    std::atomic<float>* fmtDcoRange = nullptr;
    std::atomic<float>* fmtSawOn = nullptr;
    std::atomic<float>* fmtPulseOn = nullptr;
    std::atomic<float>* fmtPwm = nullptr;
    std::atomic<float>* fmtPwmMode = nullptr;
    std::atomic<float>* fmtSubOsc = nullptr;
    std::atomic<float>* fmtNoise = nullptr;
    std::atomic<float>* fmtLfoToDCO = nullptr;
    std::atomic<float>* fmtHpfFreq = nullptr;
    std::atomic<float>* fmtVcfFreq = nullptr;
    std::atomic<float>* fmtResonance = nullptr;
    std::atomic<float>* fmtThermalDrift = nullptr;
    std::atomic<float>* fmtUnisonWidth = nullptr;
    std::atomic<float>* fmtUnisonDetune = nullptr;
    std::atomic<float>* fmtChorusMix = nullptr;
    std::atomic<float>* fmtVcfPolarity = nullptr;
    std::atomic<float>* fmtKybdTracking = nullptr;
    std::atomic<float>* fmtLfoToVCF = nullptr;
    std::atomic<float>* fmtVcaMode = nullptr;
    std::atomic<float>* fmtVcaLevel = nullptr;
    std::atomic<float>* fmtAttack = nullptr;
    std::atomic<float>* fmtDecay = nullptr;
    std::atomic<float>* fmtSustain = nullptr;
    std::atomic<float>* fmtRelease = nullptr;
    std::atomic<float>* fmtLfoRate = nullptr;
    std::atomic<float>* fmtLfoDelay = nullptr;
    std::atomic<float>* fmtChorus1 = nullptr;
    std::atomic<float>* fmtChorus2 = nullptr;
    std::atomic<float>* fmtPolyMode = nullptr;
    std::atomic<float>* fmtPortTime = nullptr;
    std::atomic<float>* fmtPortOn = nullptr;
    std::atomic<float>* fmtPortLegato = nullptr;
    std::atomic<float>* fmtBender = nullptr;
    std::atomic<float>* fmtBenderDCO = nullptr;
    std::atomic<float>* fmtBenderVCF = nullptr;
    std::atomic<float>* fmtBenderLFO = nullptr;
    std::atomic<float>* fmtTune = nullptr;
    std::atomic<float>* fmtMasterVolume = nullptr;
    std::atomic<float>* fmtMidiOut = nullptr;
    std::atomic<float>* fmtLfoTrig = nullptr;
    std::atomic<float>* fmtAftertouchToVCF = nullptr;
    std::atomic<float>* fmtEnvAmount = nullptr;

    // [Fidelidad] Preference Pointers
    std::atomic<float>* fmtMidiChannel = nullptr;
    std::atomic<float>* fmtBenderRange = nullptr;
    std::atomic<float>* fmtVelocitySens = nullptr;
    std::atomic<float>* fmtLcdBrightness = nullptr;
    std::atomic<float>* fmtNumVoices = nullptr;
    std::atomic<float>* fmtSustainInverted = nullptr;
    std::atomic<float>* fmtChorusHiss = nullptr;
    std::atomic<float>* fmtMidiFunction = nullptr;
    std::atomic<float>* fmtLowCpuMode = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ABDSimpleJuno106AudioProcessor)
};
#endif
