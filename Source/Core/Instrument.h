#pragma once

#include <JuceHeader.h>
#include "EngineConfig.h"

namespace Omega {
namespace Core {

class CalibrationSettings;
class ServiceModeManager;
class PresetManager;
class ParameterManager;
struct EngineConfig;

/**
 * Instrument - Abstract interface for a sound engine.
 * Decouples the UI/Bridge layer (OMEGA Frame) from the specific PluginProcessor.
 */
class Instrument {
public:
    virtual ~Instrument() = default;

    // --- Identifiers ---
    virtual juce::String getInstrumentName() const = 0;
    virtual const juce::String getProgramName(int index) = 0;
    virtual void changeProgramName(int index, const juce::String& newName) = 0;
    virtual juce::String getCurrentTuningName() const = 0;

    // --- Core Architecture Accessors ---
    virtual juce::AudioProcessor& getAudioProcessor() = 0;
    virtual juce::AudioProcessorValueTreeState& getAPVTS() = 0;
    virtual ParameterManager& getParameterManager() = 0;
    virtual PresetManager* getPresetManager() = 0;
    virtual CalibrationSettings& getCalibrationSettings() = 0;
    virtual ServiceModeManager& getServiceModeManager() = 0;
    
    // --- OMEGA Refined Config ---
    virtual const EngineConfig& getCurrentConfig() const = 0;

    // --- Control Interface (Host to Engine) ---
    virtual void loadPreset(int index) = 0;
    virtual int getCurrentProgram() = 0;
    virtual void setCurrentProgram(int index) = 0;
    virtual void triggerPanic() = 0;
    virtual void triggerLFO() = 0;
    virtual void undo() = 0;
    virtual void redo() = 0;
    virtual void toggleMidiOut() = 0;
    virtual void requestPatchDump() = 0;
    virtual void updateParamsFromAPVTS() = 0;
    virtual void sendManualMode() = 0;
    virtual void triggerTestProgram(int index) = 0;
    virtual void enterTestMode(bool enter) = 0;
    virtual bool isTestModeActive() const = 0;
    virtual int getActiveABSlot() const = 0;
    virtual void switchABSlot(int slot) = 0;
    virtual void copyCurrentToAlternateSlot() = 0;
    virtual void setParamsDirty() = 0;
    
    // --- Messaging / Feedback ---
    virtual void notifyUIOfStateChange() = 0;
    virtual void sendParamUpdateToUI() = 0;
    virtual int getWipCount() const = 0;
    virtual void pianoNoteOn(int note, float velocity) = 0;
    virtual void pianoNoteOff(int note) = 0;
    virtual juce::MidiMessage getCurrentSysExData() = 0;
};

} // namespace Core
} // namespace Omega


