#include "ABDSimpleJuno106AudioProcessor.h"
#include "PresetManager.h"
#include "../UI/WebView/WebViewEditor.h"

//==============================================================================
// Preset Loading
//==============================================================================
void ABDSimpleJuno106AudioProcessor::loadPreset(int index)
{
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

//==============================================================================
void ABDSimpleJuno106AudioProcessor::loadLibraryPreset(int libIdx, int presetIdx)
{
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

//==============================================================================
void ABDSimpleJuno106AudioProcessor::randomizeSound()
{
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

//==============================================================================
PresetManager* ABDSimpleJuno106AudioProcessor::getPresetManager() { return presetManager.get(); }

//==============================================================================
// A/B Slot Management
//==============================================================================
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

//==============================================================================
void ABDSimpleJuno106AudioProcessor::copyCurrentToAlternateSlot()
{
    if (activeSlot == 0) slotB = getMirrorParameters();
    else                 slotA = getMirrorParameters();
}

//==============================================================================
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
