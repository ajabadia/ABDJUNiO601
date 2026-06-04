#include "ABDSimpleJuno106AudioProcessor.h"
#include "JunoProtocol.h"
#include <cmath>

//==============================================================================
// State Mirroring — reads all APVTS params + calibration settings
//==============================================================================
SynthParams ABDSimpleJuno106AudioProcessor::getMirrorParameters()
{
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

//==============================================================================
void ABDSimpleJuno106AudioProcessor::updateParamsFromAPVTS()
{
    currentParams = getMirrorParameters();
    currentParams.currentAftertouch = currentAftertouch.load();
    lastParams = currentParams;
}

//==============================================================================
void ABDSimpleJuno106AudioProcessor::applyPerformanceModulations(SynthParams& p)
{
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

//==============================================================================
// Preset State Application
//==============================================================================
void ABDSimpleJuno106AudioProcessor::applyPresetState(const juce::ValueTree& vt)
{
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
