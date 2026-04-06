#pragma once

#include <JuceHeader.h>
#include "SynthParams.h"
#include "CalibrationSettings.h"
#include "ServiceModeManager.h"
#include <atomic>

namespace Omega {
namespace Core {

/**
 * ParameterManager - Centralizes all parameter synchronization logic.
 * Decouples the AudioProcessor (Host layer) from the Synthesis Engine (Core layer).
 */
class ParameterManager {
public:
    ParameterManager() {
        resetPointers();
    }

    /**
     * Cache pointers from APVTS for high-performance audio thread access.
     */
    void updatePointers(juce::AudioProcessorValueTreeState& apvts) {
        fmtDcoRange     = apvts.getRawParameterValue("dcoRange");
        fmtSawOn        = apvts.getRawParameterValue("sawOn");
        fmtPulseOn      = apvts.getRawParameterValue("pulseOn");
        fmtPwm          = apvts.getRawParameterValue("pwm");
        fmtPwmMode      = apvts.getRawParameterValue("pwmMode");
        fmtSubOsc       = apvts.getRawParameterValue("subOsc");
        fmtNoise        = apvts.getRawParameterValue("noise");
        fmtLfoToDCO     = apvts.getRawParameterValue("lfoToDCO");
        fmtHpfFreq      = apvts.getRawParameterValue("hpfFreq");
        fmtVcfFreq      = apvts.getRawParameterValue("vcfFreq");
        fmtResonance    = apvts.getRawParameterValue("resonance");
        fmtThermalDrift = apvts.getRawParameterValue("thermalDrift");
        fmtUnisonWidth  = apvts.getRawParameterValue("unisonWidth");
        fmtUnisonDetune = apvts.getRawParameterValue("unisonDetune");
        fmtChorusMix    = apvts.getRawParameterValue("chorusMix");
        fmtVcfPolarity  = apvts.getRawParameterValue("vcfPolarity");
        fmtKybdTracking = apvts.getRawParameterValue("kybdTracking");
        fmtLfoToVCF     = apvts.getRawParameterValue("lfoToVCF");
        fmtVcaMode      = apvts.getRawParameterValue("vcaMode");
        fmtVcaLevel     = apvts.getRawParameterValue("vcaLevel");
        fmtAttack       = apvts.getRawParameterValue("attack");
        fmtDecay        = apvts.getRawParameterValue("decay");
        fmtSustain      = apvts.getRawParameterValue("sustain");
        fmtRelease      = apvts.getRawParameterValue("release");
        fmtLfoRate      = apvts.getRawParameterValue("lfoRate");
        fmtLfoDelay     = apvts.getRawParameterValue("lfoDelay");
        fmtChorus1      = apvts.getRawParameterValue("chorus1");
        fmtChorus2      = apvts.getRawParameterValue("chorus2");
        fmtPolyMode     = apvts.getRawParameterValue("polyMode");
        fmtPortTime     = apvts.getRawParameterValue("portamentoTime");
        fmtPortOn       = apvts.getRawParameterValue("portamentoOn");
        fmtPortLegato   = apvts.getRawParameterValue("portamentoLegato");
        fmtBender       = apvts.getRawParameterValue("bender");
        fmtBenderDCO    = apvts.getRawParameterValue("benderToDCO");
        fmtBenderVCF    = apvts.getRawParameterValue("benderToVCF");
        fmtBenderLFO    = apvts.getRawParameterValue("benderToLFO");
        fmtTune         = apvts.getRawParameterValue("tune");
        fmtMasterVolume = apvts.getRawParameterValue("masterVolume");
        fmtMidiOut      = apvts.getRawParameterValue("midiOut");
        fmtLfoTrig      = apvts.getRawParameterValue("lfoTrig");
        fmtAftertouchToVCF = apvts.getRawParameterValue("aftertouchToVCF");
        fmtEnvAmount    = apvts.getRawParameterValue("envAmount");

        // Preferences
        fmtMidiChannel    = apvts.getRawParameterValue("midiChannel");
        fmtBenderRange    = apvts.getRawParameterValue("benderRange");
        fmtVelocitySens   = apvts.getRawParameterValue("velocitySens");
        fmtLcdBrightness  = apvts.getRawParameterValue("lcdBrightness");
        fmtNumVoices      = apvts.getRawParameterValue("numVoices");
        fmtSustainInverted = apvts.getRawParameterValue("sustainInverted");
        fmtChorusHiss     = apvts.getRawParameterValue("chorusHiss");
        fmtMidiFunction   = apvts.getRawParameterValue("midiFunction");
        fmtLowCpuMode     = apvts.getRawParameterValue("lowCpuMode");
        fmtMemoryProtect  = apvts.getRawParameterValue("memoryProtect");
    }

    /**
     * Get a block-consistent mirror of all parameters.
     * Injects calibration and service mode data.
     */
    SynthParams getCurrentParams(CalibrationSettings* calibration = nullptr, 
                                ServiceModeManager* service = nullptr) const {
        SynthParams p;
        if (!fmtDcoRange) return p;

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
        p.thermalDrift = fmtThermalDrift->load();
        p.unisonStereoWidth = fmtUnisonWidth->load();
        p.unisonDetune = fmtUnisonDetune->load();
        p.chorusMix = fmtChorusMix->load();
        p.vcfPolarity = (int)std::lround(fmtVcfPolarity->load());
        p.kybdTracking = fmtKybdTracking->load();
        p.lfoToVCF = fmtLfoToVCF->load();
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
        p.masterOutputGain = fmtMasterVolume->load();
        p.midiOut = fmtMidiOut->load() > 0.5f;
        p.aftertouchToVCF = fmtAftertouchToVCF->load();
        p.envAmount = fmtEnvAmount->load();

        // Preferences
        p.midiChannel = (int)std::lround(fmtMidiChannel->load());
        p.benderRange = (int)std::lround(fmtBenderRange->load());
        p.velocitySens = fmtVelocitySens->load();
        p.lcdBrightness = fmtLcdBrightness->load();
        p.numVoices = (int)std::lround(fmtNumVoices->load());
        p.sustainInverted = fmtSustainInverted->load() > 0.5f;
        p.chorusHiss = fmtChorusHiss->load();
        p.midiFunction = (int)std::lround(fmtMidiFunction->load());
        p.lowCpuMode = fmtLowCpuMode->load() > 0.5f;
        p.memoryProtect = fmtMemoryProtect->load() > 0.5f;

        // --- Inject Calibration Overrides ---
        if (calibration) {
            p.dcoMixerGain = calibration->getValue("dcoMixerGain");
            p.subAmpScale = calibration->getValue("subAmpScale");
            p.mixerSaturation = calibration->getValue("mixerSaturation");
            p.vcaSagAmt = calibration->getValue("vcaSagAmt");
            p.vcaCrosstalk = calibration->getValue("vcaCrosstalk");
            p.masterNoise = calibration->getValue("masterNoise");
            p.stereoBleed = calibration->getValue("stereoBleed");
            p.noiseGain = calibration->getValue("noiseGain");
            p.pwmOffset = calibration->getValue("pwmOffset") / 100.0f;
            p.voiceVariance = calibration->getValue("voiceVariance");
            p.unisonSpread = calibration->getValue("unisonSpread");
            p.dcoDriftComplexity = calibration->getValue("dcoDriftComplexity");
            p.vcfMinHz = calibration->getValue("vcfMinHz");
            p.vcfMaxHz = calibration->getValue("vcfMaxHz");
            p.vcfSelfOscThreshold = calibration->getValue("vcfSelfOscThreshold");
            p.vcfResoComp = calibration->getValue("vcfResoComp");
            p.vcfSaturation = calibration->getValue("vcfSaturation");
            p.vcfWidth = calibration->getValue("vcfWidth");
            p.hpfFreq2 = calibration->getValue("hpfFreq2");
            p.hpfFreq3 = calibration->getValue("hpfFreq3");
            p.hpfShelfFreq = calibration->getValue("hpfShelfFreq");
            p.hpfShelfGain = calibration->getValue("hpfShelfGain");
            p.hpfQ = calibration->getValue("hpfQ");
            p.vcaMasterGain = calibration->getValue("vcaMasterGain");
            p.vcaVelSensScale = calibration->getValue("vcaVelSensScale");
            p.vcaOffset = calibration->getValue("vcaOffset");
            p.adsrSlewMs = calibration->getValue("adsrSlewMs");
            p.adsrAttackFactor = calibration->getValue("adsrAttackFactor");
            p.chorusHissLvl = calibration->getValue("chorusHissLvl");
            p.chorusDelayI = calibration->getValue("chorusDelayI");
            p.chorusDelayII = calibration->getValue("chorusDelayII");
            p.chorusModDepth = calibration->getValue("chorusModDepth");
            p.chorusSatBoost = calibration->getValue("chorusSatBoost");
            p.chorusFilterCutoff = calibration->getValue("chorusFilterCutoff");
            p.lfoMaxRate = calibration->getValue("lfoMaxRate");
            p.lfoMinRate = calibration->getValue("lfoMinRate");
            p.lfoDelayMax = calibration->getValue("lfoDelayMax");
            p.lfoResolution = calibration->getValue("lfoResolution");
        }

        // --- Inject Service Mode Overrides ---
        if (service) {
            p.hpfCyclePos = service->getHpfCyclePos();
            p.chorusCycleMode = service->getChorusCycleMode();
        }

        return p;
    }

    void triggerLFO() {
        if (fmtLfoTrig) fmtLfoTrig->store(1.0f);
    }

    std::atomic<float>* getLfoTrig() { return fmtLfoTrig; }

private:
    void resetPointers() {
        fmtDcoRange = fmtSawOn = fmtPulseOn = fmtPwm = fmtPwmMode = nullptr;
        fmtSubOsc = fmtNoise = fmtLfoToDCO = fmtHpfFreq = fmtVcfFreq = nullptr;
        fmtResonance = fmtThermalDrift = fmtUnisonWidth = fmtUnisonDetune = nullptr;
        fmtChorusMix = fmtVcfPolarity = fmtKybdTracking = fmtLfoToVCF = nullptr;
        fmtVcaMode = fmtVcaLevel = fmtAttack = fmtDecay = fmtSustain = nullptr;
        fmtRelease = fmtLfoRate = fmtLfoDelay = fmtChorus1 = fmtChorus2 = nullptr;
        fmtPolyMode = fmtPortTime = fmtPortOn = fmtPortLegato = nullptr;
        fmtBender = fmtBenderDCO = fmtBenderVCF = fmtBenderLFO = nullptr;
        fmtTune = fmtMasterVolume = fmtMidiOut = fmtLfoTrig = nullptr;
        fmtAftertouchToVCF = fmtEnvAmount = nullptr;
        
        fmtMidiChannel = fmtBenderRange = fmtVelocitySens = nullptr;
        fmtLcdBrightness = fmtNumVoices = fmtSustainInverted = nullptr;
        fmtChorusHiss = fmtMidiFunction = fmtLowCpuMode = nullptr;
        fmtMemoryProtect = nullptr;
    }

    // Cached Parameter Pointers (Audio Thread Safe)
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

    std::atomic<float>* fmtMidiChannel = nullptr;
    std::atomic<float>* fmtBenderRange = nullptr;
    std::atomic<float>* fmtVelocitySens = nullptr;
    std::atomic<float>* fmtLcdBrightness = nullptr;
    std::atomic<float>* fmtNumVoices = nullptr;
    std::atomic<float>* fmtSustainInverted = nullptr;
    std::atomic<float>* fmtChorusHiss = nullptr;
    std::atomic<float>* fmtMidiFunction = nullptr;
    std::atomic<float>* fmtLowCpuMode = nullptr;
    std::atomic<float>* fmtMemoryProtect = nullptr;
};

} // namespace Core
} // namespace Omega


