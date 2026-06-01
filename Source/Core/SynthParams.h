#pragma once

#include <JuceHeader.h>
#include "JunoConstants.h"

/**
 * SynthParams - Parameter definitions for JUNiO 601
 */
struct SynthParams {
    int dcoRange = 1;           // 0=16', 1=8', 2=4'
    bool sawOn = true;          
    bool pulseOn = false;       
    float pwmAmount = 0.0f;     
    int pwmMode = 0;            // 0=LFO, 1=Manual

    float subOscLevel = 0.0f;   
    float noiseLevel = 0.0f;    
    float lfoToDCO = 0.0f;      
    
    float vcfFreq = 1.0f;       
    float resonance = 0.0f;     
    float envAmount = 0.5f;     
    
    float attack = 0.01f;       
    float decay = 0.3f;         
    float sustain = 0.7f;       
    float release = 0.5f;       
    
    float lfoRate = 0.5f;       
    float lfoDelay = 0.0f;      
    
    bool chorus1 = false;       
    bool chorus2 = false;       
    
    int vcaMode = 0;           
    int chorusMode = 0;         
    int polyMode = 1;           
    
    float vcaLevel = 0.8f;      
    
    float benderValue = 0.0f;   
    float benderToDCO = 1.0f;   
    float benderToVCF = 0.0f;   
    float benderToLFO = 0.0f;   
    
    // [Fidelidad] Master & Performance
    float tune = 0.0f;
    bool portamentoOn = false;
    float portamentoTime = 0.2f;
    bool portamentoLegato = false;
    
    float thermalDrift = 0.0f; // [Senior Audit] Global drift
    float vcfLFOAmount = 0.0f;     
    float lfoToVCF = 0.0f;         
    float kybdTracking = 0.0f;     
    int vcfPolarity = 0;           
    int hpfFreq = 0;               
    
    // [Fidelidad] Global Preferences
    int midiChannel = 1;
    int benderRange = 2;        
    
    // [Modern] Modernization Features
    float unisonDetune = 0.5f;  // 0.0 to 1.0 (mapped to cents)
    float velocitySens = 0.5f;  
    float lcdBrightness = 0.8f; 
    int numVoices = 16;         
    bool sustainInverted = false;
    bool midiOut = false;       
    int midiFunction = 2; // [New] 0=I, 1=II, 2=III
    float unisonStereoWidth = 0.0f; // [New] Modern stereo spreading
    float aftertouchToVCF = 0.5f; // [New] Aftertouch -> VCF intensity
    float currentAftertouch = 0.0f; // [New] Current global aftertouch value
    bool lowCpuMode = false;        // [New] Reduce Hiss and use cheaper saturation
    int sustainMode = 0;           // [New] 0=Normal, 1=SOS, 2=Toggle
    
    // [Calibration] Dynamic values from CalibrationManager
    float adsrSlewMs = 1.5f;
    float adsrAttackFactor = 0.35f;
    float dcoMixerGain = 0.7f;
    
    // [Build 25/27] LFO Calibration
    float lfoMaxRate = 30.0f;
    float lfoMinRate = 0.1f;
    float lfoDelayMax = 3.0f;
    float lfoResolution = 7.5f; 

    // [Build 28] HPF Calibration — values from ngspice AC simulation + schematic
    float hpfFreq1          = 122.0f;  // Hardware: C=.022µF, R_eff=59.4K → 122 Hz (Juno-60)
    float hpfFreq2          = 236.0f;  // Hardware: C=.015µF, R_eff=44.9K → 236 Hz
    float hpfFreq3          = 754.0f;  // Hardware: C=.0047µF, R_eff=44.9K → 754 Hz
    float hpfShelfFreq      = 70.0f;   // Legacy (unused by BassBoostFilter, kept for UI compat)
    float hpfShelfGain      = 3.0f;    // Legacy shelf gain (unused by BassBoostFilter)
    float hpfQ              = 0.707f;  // Legacy Q (unused by 1-pole TPT, kept for UI compat)
    float hpfBassBoostGain  = 1.0f;   // Post-gain scale for Bass Boost (1.0 = hardware accurate)

    // [Build 29] VCF Calibration
    float vcfMinHz = 8.0f;
    float vcfMaxHz = 16000.0f;
    float vcfSelfOscThreshold = 0.95f; // [Renamed from vcfSelfOscPoint for consistency]
    float vcfResoComp = 0.5f;
    float vcfSaturation = 1.0f;
    float vcfResPolK = 1.24f;
    float vcfFbScale = 4.20f;

    // [Build 29] VCA & Mixer Calibration
    float vcaMasterGain = 1.0f;
    float vcaVelSensScale = 1.0f;
    float mixerSaturation = 1.15f;

    // [Build 29] Chorus Calibration
    float chorusDelayI = 3.2f;
    float chorusDelayII = 3.3f;
    float chorusGainDry = 0.863f;
    float chorusGainWet = 1.257f;
    float chorusModDepth = 1.5f;
    float chorusSatBoost = 1.2f;
    float chorusFilterCutoff = 8000.0f;
    float vcaSagAmt = 0.025f;
    float vcaCrosstalk = 0.007f;
    float masterNoise = -80.0f;
    float stereoBleed = 0.03f;
    float noiseGain = 1.0f;
    float pwmOffset = 0.0f;
    float voiceVariance = 2.0f;
    float unisonSpread = 1.0f;
    
    // [Advanced Fidelity]
    float dcoGlobalDrift = 0.5f;
    float dcoVoiceDrift = 0.3f;
    float vcfLfoDepth = 0.3f;
    float vcfEnvRange = 2.0f;
    float vcfSelfOscInt = 0.5f;
    float vcfTrackCenter = 440.0f;
    float adsrMcuRate = 3.0f;
    float adsrDacSteps = 1024.0f;
    float adsrOvershoot = 1.08f;
    float vcaRippleDepth = 0.0005f;
    float chorusHissColor = 0.4f;
    float lfoDelayCurve = 5.0f;
    float dcoDriftRate = 0.008f;
    float dcoLfoPitchDepth = 0.4f;
    float chorusHiss = 1.0f;        // [Moved here for full sync/persistence]
    float chorusMix = 1.0f;         // [Moved here for full sync/persistence]
    float chorusHissLvl = 1.0f;      // [New] Calibration level
    float chorusBothRate = 7.7f;     // [Audit Fix] Accelerated BBD LFO speed
    float adsrCurveExponent = 2.2f;  // [New] Non-linear ADSR slider scaling
    float vcfResoCompBoost = 1.5f;   // [New] Additional multiplier for resonance compensation

    // [Thermal Expansion]
    float thermalInertia = 1024.0f;
    float thermalMigration = 0.0005f;
    float masterOutputGain = 1.0f;
    float masterVolume = 1.0f;      // [New] SNAPSHOT-consistent volume
    float masterPitchCents = 0.0f;
    float masterClockHz = 8000000.0f;
    float a4Reference = 440.0f;     // [New] Master tuning ref
    int oversampling = 1;          // [New] 1x, 2x, 4x
    float sliderHysteresis = 0.01f; // [New] Pot wear simulation
    float paramSlewRate = 0.95f;    // [New] Hardware lag simulation
    float vcaKillThreshold = 0.004f; // [New] Silence threshold
    float vcaDcOffset = 0.0f;       // [New] Per-voice imbalance
    float chorusLfoRate = 0.513f;   // [New] BBD LFO speed
    float vcfResoSpread = 0.05f;    // [New] 80017A chip tolerance (resonance)
    float pwmCenterDuty = 0.5f;     // [New] PWM calibration
    float pwmMaxDuty = 0.95f;
    float pwmMinDuty = 0.05f;
    float vcfWidth = 1.0f;          // [New] V/oct scaling
    float vcfSlewMs = 0.522f;       // [New] Analog slew filter time constant for VCF CV
    float vcaSlewMs = 0.687f;       // [New] Analog slew filter time constant for VCA CV
    float staggeredUpdateMaxMs = 2.0f; // [New] Maximum micro-delay across all voices (staggered CV update)
    float dcoDriftComplexity = 0.5f; // [New] Fractal noise depth
    float vcaOffset = 0.0f;         // [New] Per-voice hardware bias
    float vcaBleed = -85.0f;        // [New] Constant leakage from DCO/Noise
    float chorusLfoRateII = 0.78f;  // [New] BBD LFO speed (Mode II)
    float subGainScale = 1.25f;     // [New] Master sub-oscillator multiplier
    float noiseGainScale = 0.45f;   // [New] Master noise generator multiplier
    float thermalIntensity = 1.5f;  // [New] Global scale for thermal drift
    float pwmOffThreshold = 0.05f;  // [New] PWM cut-off
    float pwmSlewRateManual = 0.05f;
    float pwmSlewRateLFO = 0.1f;
    float vcaCurveType = 0.0f;      // [New] VCA Curve Type: 0=Juno-106 HW, 1=Juno-6/60 Shockley, 2=Linear
    float adsrVariance = 0.05f;     // [New] ADSR Voice-to-voice variance
    
    // [New Phase 3 Calibration]
    float noiseFloorMul = 1.0f;
    float mainsRippleMul = 1.0f;
    float voiceVcfFrqSpread = 10.0f;
    float voiceVcfWidthSpread = 10.0f;
    float voiceVcaGainSpread = 0.024f;
    float driftWalkIntensity = 3.0f;

    // DCO and LFO calibration parameters
    float sawMixAmp = 0.6f;
    float pulseMixAmp = 0.5f;
    float subMixAmp = 0.75f;
    float noiseMixAmp = 1.2f;
    float audioTaperScale = 3.0f;
    float dcoLfoShuntK = 5.0f;
    float dcoLfoMaxSemitones = 4.0f;
    float oscSwitchRampMs = 1.45f;
    float dcoSawCurvature = 0.15f;
    float lfoTickRateMs = 4.2335f;
    float lfoAccumMax = 8191.0f;
    float lfoHoldoffThresh = 16384.0f;
    float portamentoRateST = 0.0f;

    // --- Metadata ---
    juce::String patchName = "INITIAL PATCH";
    juce::String author = "ABD-IA";
    juce::String category = "Unknown";
    juce::String tags = "";
    juce::String notes = "";
    juce::String creationDate = "";
    bool isFavorite = false;
    bool memoryProtect = false; // [New] Hardware-accurate write protection

    // Diagnostic Cycle States (Read-only for UI)
    int hpfCyclePos = -1; // -1 = Off, 0-3 = Active position
    int chorusCycleMode = -1; // -1 = Off, 0=Off, 1=I, 2=II, 3=Both

    // ============================================================
    // Selección de Modelos / Ruteo Modular por Preset
    // 0 = J6, 1 = J60, 2 = J106
    // ============================================================
    int modelDCO   = 2;
    int modelHPF   = 2;
    int modelVCF   = 2;
    int modelADSR  = 2;
    int modelChorus = 2;
    int modelArp    = 0;
    int modelPoly   = 2;
    int modelPorta  = 2;
    int modelUnison = 2;

    // Configuración del Arpegiador
    bool arpEnabled = false;
    int arpMode = 0;       // 0=Up, 1=Up/Down, 2=Down
    int arpRange = 0;      // 0=1oct, 1=2oct, 2=3oct
    float arpRate = 0.5f;  // Rate slider (0..1)
    bool arpSync = false;
    int arpDivision = 6;   // 1/16 note (kDiv16)

#if defined(COMPILING_JUNO106)
  #define GET_MODEL_DCO(p)    2
  #define GET_MODEL_HPF(p)    2
  #define GET_MODEL_VCF(p)    2
  #define GET_MODEL_ADSR(p)   2
  #define GET_MODEL_CHORUS(p) 2
  #define GET_MODEL_ARP(p)    -1
  #define GET_MODEL_POLY(p)   2
  #define GET_MODEL_PORTA(p)  2
  #define GET_MODEL_UNISON(p) 2
#elif defined(COMPILING_JUNO60)
  #define GET_MODEL_DCO(p)    1
  #define GET_MODEL_HPF(p)    1
  #define GET_MODEL_VCF(p)    1
  #define GET_MODEL_ADSR(p)   1
  #define GET_MODEL_CHORUS(p) 1
  #define GET_MODEL_ARP(p)    1
  #define GET_MODEL_POLY(p)   1
  #define GET_MODEL_PORTA(p)  1
  #define GET_MODEL_UNISON(p) 1
#elif defined(COMPILING_JUNO6)
  #define GET_MODEL_DCO(p)    0
  #define GET_MODEL_HPF(p)    0
  #define GET_MODEL_VCF(p)    0
  #define GET_MODEL_ADSR(p)   0
  #define GET_MODEL_CHORUS(p) 0
  #define GET_MODEL_ARP(p)    0
  #define GET_MODEL_POLY(p)   0
  #define GET_MODEL_PORTA(p)  0
  #define GET_MODEL_UNISON(p) 0
#else
  // Híbrido / Conmutable en caliente (Super Juno SIX)
  #define GET_MODEL_DCO(p)    (p).modelDCO
  #define GET_MODEL_HPF(p)    (p).modelHPF
  #define GET_MODEL_VCF(p)    (p).modelVCF
  #define GET_MODEL_ADSR(p)   (p).modelADSR
  #define GET_MODEL_CHORUS(p) (p).modelChorus
  #define GET_MODEL_ARP(p)    (p).modelArp
  #define GET_MODEL_POLY(p)   (p).modelPoly
  #define GET_MODEL_PORTA(p)  (p).modelPorta
  #define GET_MODEL_UNISON(p) (p).modelUnison
#endif

    bool isSamePatch(const SynthParams& other, bool includeMetadata = false) const {
        const float tol = 0.008f; // [Fix] Increased tolerance for 7-bit SysEx quantization (1/127 approx 0.0078)
        
        if (includeMetadata)
        {
            if (patchName != other.patchName || author != other.author || 
                category != other.category || tags != other.tags || 
                notes != other.notes || creationDate != other.creationDate || 
                isFavorite != other.isFavorite)
                return false;
        }

        return dcoRange == other.dcoRange &&
               sawOn == other.sawOn &&
               pulseOn == other.pulseOn &&
               std::abs(pwmAmount - other.pwmAmount) < tol &&
               pwmMode == other.pwmMode &&
               std::abs(subOscLevel - other.subOscLevel) < tol &&
               std::abs(noiseLevel - other.noiseLevel) < tol &&
               std::abs(lfoToDCO - other.lfoToDCO) < tol &&
               std::abs(vcfFreq - other.vcfFreq) < tol &&
               std::abs(resonance - other.resonance) < tol &&
               std::abs(envAmount - other.envAmount) < tol &&
               std::abs(attack - other.attack) < tol &&
               std::abs(decay - other.decay) < tol &&
               std::abs(sustain - other.sustain) < tol &&
               std::abs(release - other.release) < tol &&
               std::abs(lfoRate - other.lfoRate) < tol &&
               std::abs(lfoDelay - other.lfoDelay) < tol &&
               chorus1 == other.chorus1 &&
               chorus2 == other.chorus2 &&
               vcaMode == other.vcaMode &&
               std::abs(vcaLevel - other.vcaLevel) < tol &&
               std::abs(lfoToVCF - other.lfoToVCF) < tol &&
               std::abs(kybdTracking - other.kybdTracking) < tol &&
               vcfPolarity == other.vcfPolarity &&
               hpfFreq == other.hpfFreq;
    }
};

/**
 * [VCA/Chorus Audit] Authentic Juno-106 Chorus Constants (Service Manual Aligned)
 */
struct JunoChorusConstants {
    // [Fidelidad] Medidas de servicio (BDD capacitor 2.2 uF): 0.47Hz y 0.78Hz
    static constexpr float kRateI = 0.47f;
    static constexpr float kDepthI = 0.15f; 
    static constexpr float kDelayI = 14.5f;   

    static constexpr float kRateII = 0.78f;
    static constexpr float kDepthII = 0.35f;  
    static constexpr float kDelayII = 14.5f;  

    static constexpr float kRateIII = 0.9f;
    static constexpr float kDepthIII = 0.25f;
    static constexpr float kDelayIII = 12.0f;
    
    // [Fidelidad] El nivel de ruido de los BBD es constante e independiente del modo
    static constexpr float kNoiseLevel = 0.0006f; 
};

struct JunoTimeCurves {
    static constexpr float kAttackMin  = 0.0015f; // [Fidelity] 1.5ms
    static constexpr float kAttackMax  = 3.0f;   
    static constexpr float kDecayMin   = 0.0015f; 
    static constexpr float kDecayMax   = 12.0f;  
    static constexpr float kReleaseMin = 0.0015f; 
    static constexpr float kReleaseMax = 12.0f;  

    static constexpr float kLfoMinHz = 0.1f;
    static constexpr float kLfoMaxHz = 30.0f;
    static constexpr float kLfoDelayMax = 3.0f;
};
