#include <JuceHeader.h>
#include "CalibrationSettings.h"
#include "JunoConstants.h"
#include "../Synth/DacHzTable.h"
#include "../Synth/J106VCATable.h"
#include <fstream>
#include <iostream>
#include <sstream>

CalibrationSettings::CalibrationSettings()
{
    buildParameterList();
    load();
}

CalibrationSettings::~CalibrationSettings()
{
    save();
}

void CalibrationSettings::buildParameterList()
{
    // Simplified registration call
    auto reg = [this](std::string id, std::string label, std::string category, std::string unit, std::string tooltip, float def, float min, float max, float step, bool isInt = false) {
        Cal::CalibrationParam p;
        p.id = id; p.label = label; p.category = category; p.unit = unit; p.tooltip = tooltip;
        p.defaultValue = def; p.currentValue = def; p.minValue = min; p.maxValue = max; p.stepSize = step; p.isInteger = isInt;
        idToIndex[id] = (int)params.size();
        params.push_back(p);
    };

    // Helper for triple registration (creates _J6, _J60, and _J106 copies for model-specific tuning)
    auto regTriple = [this, &reg](std::string id, std::string label, std::string category, std::string unit, std::string tooltip, float def, float min, float max, float step, bool isInt = false) {
        reg(id, label, category, unit, tooltip, def, min, max, step, isInt);
        reg(id + "_J6", label + " (J6)", category, unit, tooltip + " [Juno-6 Model]", def, min, max, step, isInt);
        reg(id + "_J60", label + " (J60)", category, unit, tooltip + " [Juno-60 Model]", def, min, max, step, isInt);
        reg(id + "_J106", label + " (J106)", category, unit, tooltip + " [Juno-106 Model]", def, min, max, step, isInt);
    };

    // Helper for triple registration with custom default values for J6, J60, and J106
    auto regTripleWithDefaults = [this, &reg](std::string id, std::string label, std::string category, std::string unit, std::string tooltip, float defJ6, float defJ60, float defJ106, float min, float max, float step, bool isInt = false) {
        reg(id, label, category, unit, tooltip, defJ106, min, max, step, isInt); // Base defaults to J106
        reg(id + "_J6", label + " (J6)", category, unit, tooltip + " [Juno-6 Model]", defJ6, min, max, step, isInt);
        reg(id + "_J60", label + " (J60)", category, unit, tooltip + " [Juno-60 Model]", defJ60, min, max, step, isInt);
        reg(id + "_J106", label + " (J106)", category, unit, tooltip + " [Juno-106 Model]", defJ106, min, max, step, isInt);
    };

    // --- GENERAL PREFERENCES ---
    reg("calibrationProfile", "Profile", "GENERAL", "", "Sets the default calibration profile (0=Juno-6, 1=Juno-60, 2=Juno-106, 3=Super Six).", 2.0f, 0.0f, 3.0f, 1.0f, true);
    reg("skinType", "UI Skin Theme", "GENERAL", "", "Selects the UI theme (0 = Classic Blue, 1 = Juno-60 Classic, 2 = Juno-6 Analog, 3 = Juno-106 Classic, 4 = Juno-106S Dark, 5 = TR-808, 6 = DeepMind, 7 = Space Echo, 8 = ARP 2600).", 0.0f, 0.0f, 8.0f, 1.0f, true);
    reg("midiChannel", "Global MIDI Channel", "GENERAL", "", "Global MIDI input channel (1-16, 0=OMNI).", 1.0f, 0.0f, 16.0f, 1.0f, true);
    reg("numVoices", "Maximum Polyphony", "GENERAL", "voices", "Maximum number of simultaneous voices.", 16.0f, 1.0f, 16.0f, 1.0f, true);
    reg("benderRange", "Bender Pitch Range", "GENERAL", "semis", "Maximum pitch bend range in semitones.", 2.0f, 1.0f, 12.0f, 1.0f, true);
    reg("velocitySens", "Velocity Sensitivity", "GENERAL", "%", "Global multiplier for touch velocity influence.", 0.5f, 0.0f, 1.0f, 0.01f);
    reg("aftertouchToVCF", "Aftertouch to VCF", "GENERAL", "%", "Sensitivity of the filter cutoff to keyboard pressure.", 0.5f, 0.0f, 1.0f, 0.01f);
    reg("lcdBrightness", "LCD Brightness", "GENERAL", "%", "Intensity of the hardware-emulated LCD display backlight.", 0.8f, 0.0f, 1.0f, 0.1f);
    reg("sustainPedalInvert", "Invert Sustain Pedal", "GENERAL", "", "Inverts the polarity of the sustain pedal input.", 0.0f, 0.0f, 1.0f, 1.0f, true);
    reg("masterOutputGain", "Master Output Gain", "GENERAL", "dB", "Global gain trim applied to the final output stage.", 0.0f, -12.0f, 12.0f, 0.1f);
    reg("masterPitchCents", "Master Pitch Offset", "GENERAL", "cents", "Global pitch fine-tuning offset (applied to all oscillators).", 0.0f, -100.0f, 100.0f, 0.1f);
    reg("midiFunction", "MIDI SysEx Mode", "GENERAL", "", "Sets the depth of MIDI data transmission (I=Note, II=Patch, III=All/SysEx).", 1.0f, 0.0f, 2.0f, 1.0f, true);
    reg("unisonWidth", "Unison Stereo Width", "GENERAL", "%", "Stereo separation of voices during UNISON mode.", 1.0f, 0.0f, 1.0f, 0.01f);
    reg("unisonDetune", "Unison Detune Amt", "GENERAL", "%", "Pitch micro-detuning intensity in UNISON mode.", 0.35f, 0.0f, 1.0f, 0.01f);
    reg("sustainMode", "Sustain Pedal Mode", "GENERAL", "", "Behavior of the sustain pedal (0=Normal, 1=SOS, 2=Toggle).", 0.0f, 0.0f, 2.0f, 1.0f, true);
    reg("enableLogging", "System Logging", "GENERAL", "", "Enables/Disables diagnostic logging in the console (OFF by default).", 0.0f, 0.0f, 1.0f, 1.0f, true);

    // --- DCO ---
    regTriple("dcoMixerGain", "DCO Mixer Gain", "DCO", "", "Level of DCO output before VCF.", 0.70f, 0.1f, 1.5f, 0.05f);
    regTriple("subGainScale", "Sub-Osc Gain Scale", "DCO", "x", "Master gain multiplier for the sub-oscillator circuit.", 1.25f, 0.5f, 2.0f, 0.05f);
    regTriple("noiseGainScale", "Noise Gain Scale", "DCO", "x", "Master gain multiplier for the white noise generator.", 0.45f, 0.1f, 1.5f, 0.01f);
    regTriple("masterClockHz", "Oscillator Master Clock", "DCO", "Hz", "Frequency of the master high-speed clock used for DCO dividers (factory=8MHz).", 8000000.0f, 7000000.0f, 9000000.0f, 1.0f);
    regTriple("mixerSaturation", "DCO Mixer Clipping", "DCO", "", "Threshold for DCO mixing stage saturation clipping.", 0.6f, 0.1f, 4.0f, 0.05f);
    regTriple("noiseGain", "Noise Level Trim", "DCO", "", "Level of the white noise generator relative to DCOs.", 1.0f, 0.1f, 2.0f, 0.05f);
    regTriple("pwmCenterDuty", "PWM Center Duty", "DCO", "%", "Calibration of the 50% center point for pulse width.", 0.5f, 0.4f, 0.6f, 0.01f);
    regTriple("pwmMaxDuty", "PWM Maximum Duty", "DCO", "%", "Calibration of the maximum pulse width (factory=95%).", 0.95f, 0.9f, 0.99f, 0.01f);
    regTriple("pwmMinDuty", "PWM Minimum Duty", "DCO", "%", "Calibration of the minimum pulse width (factory=5%).", 0.05f, 0.01f, 0.1f, 0.01f);
    regTriple("pwmOffset", "PWM Tuning Offset", "DCO", "%", "Shifts the center point of the Pulse Width modulation.", 0.0f, -10.0f, 10.0f, 0.5f);
    regTripleWithDefaults("sawMixAmp",         "Saw Wave Mix Level",     "DCO","x","Mix amplitude for Sawtooth.",                  0.50f,  0.50f,  0.60f,  0.1f, 2.0f, 0.05f);
    regTripleWithDefaults("pulseMixAmp",       "Pulse Wave Mix Level",   "DCO","x","Mix amplitude for Pulse wave.",                0.50f,  0.50f,  0.50f,  0.1f, 2.0f, 0.05f);
    regTripleWithDefaults("subMixAmp",         "Sub-Osc Mix Level",      "DCO","x","Mix amplitude for Sub-oscillator.",           0.5942f, 0.5942f, 0.75f,  0.1f, 2.0f, 0.05f);
    regTripleWithDefaults("noiseMixAmp",       "Noise Mix Level",        "DCO","x","Mix amplitude for Noise gen.",                 1.00f,  1.00f,  1.20f,  0.1f, 3.0f, 0.05f);
    regTriple("audioTaperScale",   "Audio Taper Scale",      "DCO","","Exp scale for sub/noise taper: (exp(k*x)-1)/(exp(k)-1), factory k=3.", 3.0f, 1.0f, 6.0f, 0.1f);
    regTriple("dcoLfoShuntK",      "DCO-LFO Shunt Factor",   "DCO","","Taper denominator: k=Rpot/Rshunt (50K/10K=5, factory).", 5.0f, 1.0f, 10.0f, 0.5f);
    regTriple("dcoLfoMaxSemitones","DCO-LFO Max Depth",      "DCO","st","Max pitch mod depth at full slider (J106 factory: ±4 st).", 4.0f, 1.0f, 12.0f, 0.5f);
    regTriple("oscSwitchRampMs",   "Osc Switch Ramp Time",   "DCO","ms","Crossfade time for waveform on/off switching (factory: 1.45ms).", 1.45f, 0.1f, 10.0f, 0.1f);
    regTriple("dcoSawCurvature",   "DCO Sawtooth Curvature", "DCO","",  "Non-linear charge curve of the analog integrators (Factory 0.15).",  0.15f, 0.0f, 0.5f, 0.01f);
    
    // --- VCA ---
    regTriple("vcaMasterGain", "VCA Master Gain", "VCA", "", "Overall gain trim for each voice.", 1.0f, 0.1f, 3.0f, 0.05f);
    regTriple("vcaBleed", "VCA Bleed Level", "VCA", "dB", "Constant signal leakage from oscillators even when VCA is closed.", -85.0f, -100.0f, -60.0f, 0.5f);
    regTriple("vcaVelSensScale", "VCA Velocity Scale", "VCA", "", "Master multiplier for velocity sensitivity.", 1.0f, 0.0f, 2.0f, 0.1f);
    regTriple("vcaSagAmt", "VCA Power Sag", "VCA", "", "Sensitivity of global volume drop when multiple voices are active.", 0.025f, 0.0f, 0.1f, 0.005f);
    regTriple("vcaKillThreshold", "Voice Kill Threshold", "VCA", "", "Threshold below which a voice is considered silent and freed.", 0.004f, 0.001f, 0.02f, 0.001f);
    regTriple("vcaDcOffset", "DC Offset Correction", "VCA", "", "Per-voice DC offset compensation (simulates analog VCA imbalance).", 0.0f, -0.01f, 0.01f, 0.0001f);
    regTriple("vcaOffset", "VCA Bias Offset", "VCA", "", "Simulates per-voice hardware bias variance.", 0.0f, -0.05f, 0.05f, 0.005f);
    regTriple("vcaSlewMs", "VCA Analog Slew", "VCA", "ms", "Analog slew filter time constant for VCA CV (Original: 0.687ms).", 0.687f, 0.0f, 20.0f, 0.1f);
    regTripleWithDefaults("vcaCurveType", "VCA Curve Model", "VCA", "", "VCA transfer curve: 0=Juno-106 HW (Boaris), 1=Juno-6/60 Shockley, 2=Linear.", 1.0f, 1.0f, 0.0f, 0.0f, 2.0f, 1.0f, true);

    // --- ADSR CALIBRATION ---
    regTriple("adsrSlewMs", "ADSR Output Smoothing", "ADSR", "ms", "Slew time to prevent digital step clicks.", 1.5f, 0.1f, 10.0f, 0.1f);
    regTriple("adsrAttackFactor", "ADSR Attack Factor", "ADSR", "", "Curvature of the attack stage.", 0.35f, 0.1f, 1.0f, 0.01f);
    regTriple("adsrMcuRate", "Env MCU Speed", "ADSR", "ms", "Internal refresh rate of the envelope microprocessor (original 106 clock was ~4.23ms).", 4.2335f, 0.5f, 10.0f, 0.0001f);
    regTriple("adsrDacSteps", "Env DAC Resolution", "ADSR", "steps", "Quantization of the envelope DAC (approximates the discrete R-2R ladder steps).", 1024.0f, 16.0f, 16384.0f, 1.0f, true);
    regTriple("adsrOvershoot", "Attack Overshoot", "ADSR", "V", "Voltage spike at the end of the attack phase due to capacitor charging inertia.", 1.08f, 1.0f, 1.25f, 0.01f);
    regTriple("adsrCurveExponent", "ADSR Curve Exponent", "ADSR", "", "Non-linear scaling of ADSR sliders (factory/2.2 is recommended).", 2.2f, 1.0f, 4.0f, 0.1f);
    regTriple("adsrVariance", "ADSR Time Variance", "AGING", "%", "Static timing variance of the envelope components between voices.", 0.05f, 0.0f, 0.15f, 0.01f);
    
    // --- CHORUS ---
    regTriple("chorusMix", "Chorus Dry/Wet Mix", "CHORUS", "%", "Balance between dry signal and the analog BBD chorus output.", 1.0f, 0.0f, 1.0f, 0.01f);
    regTriple("chorusHiss", "Analog Hiss Level", "CHORUS", "dB", "Baseline analog noise level of the BBD delay lines.", -68.0f, -96.0f, -30.0f, 0.5f);
    regTriple("chorusDelayI", "Chorus I Base Delay", "CHORUS", "ms", "Base delay for Chorus I mode (Factory 3.2ms).", 3.2f, 1.0f, 10.0f, 0.1f);
    regTriple("chorusDelayII", "Chorus II Base Delay", "CHORUS", "ms", "Base delay for Chorus II mode (Factory 3.3ms).", 3.3f, 2.0f, 20.0f, 0.1f);
    regTriple("chorusGainDry", "Chorus Dry Gain (IC6)", "CHORUS", "x", "Dry signal gain multiplier at IC6 summing stage (Factory 0.863).", 0.863f, 0.5f, 1.5f, 0.001f);
    regTriple("chorusGainWet", "Chorus Wet Gain (IC6)", "CHORUS", "x", "Wet/BBD signal gain multiplier at IC6 summing stage (Factory 1.257).", 1.257f, 0.5f, 2.0f, 0.001f);
    regTriple("chorusLfoRate", "Chorus I LFO Frequency", "CHORUS", "Hz", "Internal hardware LFO rate for chorus modulation (Factory 0.513Hz).", 0.513f, 0.1f, 2.0f, 0.01f);
    regTriple("chorusLfoRateII", "Chorus II LFO Frequency", "CHORUS", "Hz", "Second internal LFO rate for chorus modulation (Factory 0.78Hz).", 0.78f, 0.1f, 2.0f, 0.01f);
    regTriple("chorusBothRate", "Chorus I+II Frequency", "CHORUS", "Hz", "Accelerated LFO rate when both I and II are pressed (Factory 7.7Hz).", 7.7f, 1.0f, 15.0f, 0.1f);
    regTriple("chorusModDepth", "Chorus Mod Depth", "CHORUS", "ms", "Maximum LFO sweep width in milliseconds.", 1.5f, 0.1f, 5.0f, 0.1f);
    regTriple("chorusSatBoost", "Chorus Saturation", "CHORUS", "", "Analog warmth boost and subtle clipping (1.0 = clean).", 1.2f, 0.5f, 2.0f, 0.05f);
    regTriple("chorusFilterCutoff", "Chorus Filter Cutoff", "CHORUS", "Hz", "Post-BBD reconstruction filter frequency range.", 8000.0f, 2000.0f, 15000.0f, 100.0f);
    regTriple("chorusHissColor", "Hiss Filter Color", "CHORUS", "", "Spectral character (Pink/White/Dark) of the BBD noise floor.", 0.4f, 0.05f, 1.0f, 0.05f);
    regTriple("chorusHissLvl", "Chorus Hiss Level (Cal)", "CHORUS", "dB", "Direct BBD noise floor trim.", -68.0f, -96.0f, -30.0f, 0.5f);
    regTriple("bbdHissColoration", "BBD Hiss Coloration", "CHORUS", "", "Tone character of the BBD noise floor (higher = brighter).", 0.4f, 0.05f, 1.0f, 0.05f);
    regTriple("bbdClockTrim", "BBD Clock Trim", "CHORUS", "", "Clock speed imbalance between Left and Right BBD lines.", 0.015f, 0.0f, 0.1f, 0.001f);

    // --- LFO ---
    regTriple("lfoMaxRate", "LFO Max Frequency", "LFO", "Hz", "Max frequency at slider 10.", 30.0f, 1.0f, 100.0f, 0.5f);
    regTriple("lfoMinRate", "LFO Min Frequency", "LFO", "Hz", "Min frequency at slider 0.", 0.1f, 0.01f, 5.0f, 0.05f);
    regTriple("lfoDelayMax", "LFO Max DelayTime", "LFO", "s", "Max delay time at slider 10.", 3.0f, 0.1f, 10.0f, 0.1f);
    regTriple("lfoResolution", "LFO DAC Steps (7.5=4bit)", "LFO", "", "Resolution of LFO (higher = cleaner).", 7.5f, 1.0f, 128.0f, 0.5f);
    regTriple("lfoTickRateMs",     "LFO MCU Tick Period",    "LFO","ms","Firmware main-loop period driving LFO steps (calibrated: 4.2335ms).", 4.2335f, 1.0f, 10.0f, 0.001f);
    regTriple("lfoAccumMax",       "LFO Accumulator Max",    "LFO","","LFO 16-bit accumulator ceiling (firmware: 0x1FFF = 8191).", 8191.0f, 4095.0f, 16383.0f, 1.0f, true);
    regTriple("lfoHoldoffThresh",  "LFO Holdoff Threshold",  "LFO","","Accumulator value at which Holdoff ends (firmware: 0x4000 = 16384).", 16384.0f, 4096.0f, 32767.0f, 1.0f, true);

    // --- VCF ---
    regTriple("vcfMinHz", "VCF Min Frequency", "VCF", "Hz", "Lowest possible cutoff frequency.", 10.0f, 5.0f, 100.0f, 1.0f);
    regTriple("vcfMaxHz", "VCF Max Frequency", "VCF", "Hz", "Highest possible cutoff frequency.", 18000.0f, 5000.0f, 22000.0f, 100.0f);
    regTriple("vcfSelfOscThreshold", "VCF Self-Osc Point", "VCF", "", "Resonance level where self-oscillation begins.", 0.95f, 0.85f, 1.0f, 0.01f);
    regTriple("vcfSaturation", "VCF OTA Saturation", "VCF", "", "Amount of non-linear drive in the filter stages.", 1.2f, 0.1f, 4.0f, 0.05f);
    regTriple("vcfResoComp", "VCF Resonance Comp", "VCF", "", "Base gain compensation level when resonance is high (Hardware stabilized).", 0.35f, 0.0f, 1.5f, 0.05f);
    regTriple("vcfResoCompBoost", "Reso Comp Global Boost", "VCF", "", "Additional multiplier for resonance compensation (useful for Organ patches).", 1.5f, 1.0f, 3.0f, 0.1f);
    regTriple("vcfLfoDepth", "LFO Filter Depth", "VCF", "", "Sensitivity of the VCF cutoff to the master LFO modulation.", 0.3f, 0.05f, 1.0f, 0.05f);
    regTriple("vcfEnvRange", "Env Filter Range", "VCF", "", "Maximum sweep range of the envelope generator on the VCF cutoff.", 2.0f, 0.5f, 4.0f, 0.1f);
    regTriple("vcfSelfOscInt", "Self-Osc Intensity", "VCF", "", "Intensity of the self-oscillating feedback loop at maximum resonance.", 0.5f, 0.1f, 2.0f, 0.05f);
    regTriple("vcfTrackCenter", "VCF Tracking Center", "VCF", "Hz", "Pitch pivot point for keyboard tracking (determines where tracking is exactly 1:1).", 440.0f, 100.0f, 1000.0f, 10.0f);
    regTriple("vcfResoSpread", "VCF Resonance Spread", "VCF", "%", "Variance in filter resonance response between voices (simulates 80017A chip tolerances).", 0.05f, 0.0f, 0.2f, 0.01f);
    regTriple("vcfWidth", "VCF Tracking Width", "VCF", "", "V/oct scaling accuracy (simulates VCF Width trim pot).", 1.0f, 0.8f, 1.2f, 0.01f);
    regTriple("vcfSlewMs", "VCF Analog Slew", "VCF", "ms", "Analog slew filter time constant for VCF CV (Original: 0.522ms).", 0.522f, 0.0f, 20.0f, 0.1f);
    regTriple("vcfResPolK", "VCF Res Polynomial K", "VCF", "", "Scale factor for ResK_J106 polynomial curve fitting (Factory 1.24).", 1.24f, 0.5f, 2.0f, 0.01f);
    regTriple("vcfFbScale", "VCF BA662 Feedback Scale", "VCF", "", "Feedback scale factor for BA662 OTA differential pair emulation (Factory 4.20).", 4.20f, 1.0f, 10.0f, 0.05f);
    regTriple("vcfResKModel", "VCF ResK Model Curve", "VCF", "", "Resonance feedback curve model: 0=Juno-60 (IR3109), 1=Juno-106 (80017A).", 1.0f, 0.0f, 1.0f, 1.0f, true);

    // --- HPF ---
    regTripleWithDefaults("hpfFreq1",         "HPF Pos 1 Frequency",     "HPF", "Hz",  "HPF cutoff at pos 1 (CD4051B selects C=.022µF). Hardware: 122 Hz.",     122.0f, 122.0f, 0.0f,    0.0f,  500.0f,  1.0f);
    regTripleWithDefaults("hpfFreq2",         "HPF Pos 2 Frequency",     "HPF", "Hz",  "HPF cutoff at pos 2 (C=.015µF, R_eff=44.9K). Hardware: 236 Hz.",     269.0f, 269.0f, 236.0f,   50.0f,  1000.0f, 1.0f);
    regTripleWithDefaults("hpfFreq3",         "HPF Pos 3 Frequency",     "HPF", "Hz",  "HPF cutoff at pos 3 (C=.0047µF, R_eff=44.9K). Hardware: 754 Hz.",   571.0f, 571.0f, 754.0f,   300.0f, 2500.0f, 5.0f);
    regTriple("hpfBassBoostGain", "Bass Boost Output Scale",  "HPF", "x",   "Post-gain scale for Bass Boost (pos 0). 1.0 = hardware (+10.5 dB DC, ~59 Hz shelf). Increase for more bass.", 1.0f, 0.1f, 3.0f, 0.05f);
    regTriple("hpfShelfFreq",     "HPF Pos 0 Shelf Freq",    "HPF", "Hz",  "[Legacy] Not used by BassBoostFilter. Kept for UI compatibility.",   70.0f,  20.0f,  300.0f,  1.0f);
    regTriple("hpfShelfGain",     "HPF Pos 0 Shelf Gain",    "HPF", "dB",  "[Legacy] Not used by BassBoostFilter. Kept for UI compatibility.",    3.0f,   0.0f,   12.0f,   0.1f);
    regTriple("hpfQ",             "HPF Filter Q",             "HPF", "",    "[Legacy] Not used by 1-pole TPT. Kept for UI compatibility.",        0.707f, 0.1f,   2.0f,    0.01f);

    // --- THERMAL DRIFT ---
    regTriple("thermalIntensity", "Thermal Intensity Scale", "THERMAL", "x", "Global scale for thermal pitch drift instability.", 1.5f, 0.1f, 3.0f, 0.1f);
    regTriple("thermalDrift", "Global Thermal Intensity", "THERMAL", "%", "Global scope of the pitch instability due to component heat.", 100.0f, 0.0f, 200.0f, 1.0f);
    regTriple("thermalInertia", "Thermal Inertia", "THERMAL", "samples", "Samples between drift updates (higher = slower drift).", 1024.0f, 64.0f, 8192.0f, 64.0f, true);
    regTriple("thermalMigration", "Thermal Migration Rate", "THERMAL", "", "Speed of the random pitch wandering over time.", 0.0005f, 0.0001f, 0.01f, 0.0001f);

    // --- ANALOG AGING & FIDELITY ---
    regTriple("vcaCrosstalk", "Voice Crosstalk", "AGING", "", "Amount of signal leakage between internal voice circuits.", 0.007f, 0.0f, 0.05f, 0.001f);
    regTriple("masterNoise", "Global Noise Floor", "AGING", "dB", "Analog output stage background noise floor.", -80.0f, -100.0f, -40.0f, 0.5f);
    regTriple("stereoBleed", "Stereo Cross-bleeding", "AGING", "", "Analog leakage between Left and Right output channels.", 0.03f, 0.0f, 0.15f, 0.005f);
    regTriple("voiceVariance", "Voice Pitch Variance", "AGING", "cents", "Static pitch difference between voices (pre-thermal).", 2.0f, 0.0f, 10.0f, 0.1f);
    regTriple("unisonSpread", "Unison Spread Scale", "AGING", "", "Master multiplier for the Unison detune amount.", 1.0f, 0.1f, 2.0f, 0.1f);
    regTriple("dcoGlobalDrift", "Master Clock Drift", "AGING", "cents", "Shared pitch instability across all voices (Master Clock variations).", 0.5f, 0.0f, 5.0f, 0.1f);
    regTriple("dcoVoiceDrift", "Voice Drift Amount", "AGING", "cents", "Individual voice pitch wandering due to thermal noise in DCO dividers.", 0.3f, 0.0f, 3.0f, 0.1f);
    regTriple("dcoDriftComplexity", "DCO Drift Complexity", "AGING", "", "Depth of fractal random walk for thermal pitch wandering.", 0.5f, 0.0f, 1.0f, 0.05f);
    regTriple("vcaRippleDepth", "VCA Ripple Depth", "AGING", "", "Depth of the envelope-induced power supply ripple noise (VCA crosstalk).", 0.0005f, 0.0f, 0.005f, 0.0001f);
    regTriple("lfoDelayCurve", "LFO Onset Curve", "AGING", "", "Attack curvature for the LFO onset (how fast it reaches full depth).", 5.0f, 1.0f, 10.0f, 0.1f);
    regTriple("dcoDriftRate", "Master Drift Rate", "AGING", "Hz", "Speed of the pitch wandering (random walk frequency).", 0.008f, 0.001f, 0.05f, 0.001f);
    regTriple("dcoLfoPitchDepth", "DCO Vibrato Depth", "DCO", "", "Sensitivity of the DCO pitch to the master LFO (vibrato depth).", 0.4f, 0.1f, 1.0f, 0.05f);
    regTriple("pwmOffThreshold", "PWM Cut-off Threshold", "DCO", "", "Point where pulse width becomes silent (Factory 0.05).", 0.05f, 0.0f, 0.15f, 0.01f);
    regTriple("pwmSlewRateManual", "PWM Manual Slew", "DCO", "", "Mechanical lag simulation for manual PWM.", 0.05f, 0.005f, 0.2f, 0.005f);
    regTriple("pwmSlewRateLFO", "PWM LFO Slew", "DCO", "", "Smoothing for LFO-driven PWM.", 0.1f, 0.01f, 0.5f, 0.01f);
    
    // --- ADDITIONAL REGISTRATIONS ---
    regTriple("noiseFloorMul", "Analog Noise Floor Multiplier", "AGING", "x", "User-adjustable analog broadband noise floor multiplier.", 1.0f, 0.0f, 5.0f, 0.1f);
    regTriple("mainsRippleMul", "Mains Ripple Multiplier", "AGING", "x", "User-adjustable mains ripple amplitude multiplier.", 1.0f, 0.0f, 5.0f, 0.1f);
    regTriple("voiceVcfFrqSpread", "VCF Cutoff Voice Spread", "AGING", "counts", "VCF Cutoff frequency fixed trim offset spread (J106: ±10 DAC counts).", 10.0f, 0.0f, 100.0f, 1.0f);
    regTriple("voiceVcfWidthSpread", "VCF Width Voice Spread", "AGING", "cents/oct", "VCF tracking width fixed trim offset spread (J106: ±10 cents/oct).", 10.0f, 0.0f, 100.0f, 1.0f);
    regTriple("voiceVcaGainSpread", "VCA Gain Voice Spread", "AGING", "%", "VCA gain fixed voice tolerance spread (J106: ±2.4% gain).", 0.024f, 0.0f, 0.1f, 0.001f);
    regTriple("driftWalkIntensity", "Drift Walk Intensity", "THERMAL", "cents", "Dynamic random drift walk max cents (J106: ±3 cents).", 3.0f, 0.0f, 10.0f, 0.1f);

    // --- SYSTEM ---
    regTriple("a4Reference", "A4 Reference Pitch", "SYSTEM", "Hz", "Master tuning reference frequency (Standard=440Hz).", 440.0f, 400.0f, 480.0f, 1.0f);
    regTriple("oversampling", "Internal Oversampling", "SYSTEM", "x", "Audio engine oversampling rate (1x=Normal, 2x=Hi-Fi, 4x=Extreme).", 1.0f, 1.0f, 4.0f, 1.0f, true);
    regTriple("sliderHysteresis", "Slider Hysteresis", "SYSTEM", "", "Simulates mechanical wear and friction in the analog sliders.", 0.01f, 0.0f, 0.1f, 0.005f);
    regTriple("paramSlewRate", "Parameter Slew Rate", "SYSTEM", "", "Latency of response when moving sliders (hardware lag simulation).", 0.95f, 0.5f, 1.0f, 0.01f);
    regTriple("staggeredUpdateMaxMs", "Staggered Update Max Delay", "SYSTEM", "ms", "Maximum micro-delay across all voices (staggered CV update).", 2.0f, 0.0f, 4.0f, 0.1f);
    regTriple("arpClockRelaxation", "Arp Clock Relaxation", "SYSTEM", "", "Schmitt-trigger clock hysteresis relaxation coefficient.", 0.5f, 0.0f, 1.0f, 0.01f);
}

void CalibrationSettings::registerParam(Cal::CalibrationParam p)
{
    p.currentValue = p.defaultValue;
    idToIndex[p.id] = (int)params.size();
    params.push_back(p);
}

float CalibrationSettings::getValue(const std::string& id) const
{
    auto it = idToIndex.find(id);
    if (it != idToIndex.end())
        return params[it->second].currentValue;
    return 0.0f;
}

float CalibrationSettings::getValueForModel(const std::string& id, int model) const
{
    std::string suffix = "";
    if (model == 0) suffix = "_J6";
    else if (model == 1) suffix = "_J60";
    else if (model == 2) suffix = "_J106";

    std::string specificId = id + suffix;
    auto it = idToIndex.find(specificId);
    if (it != idToIndex.end())
        return params[it->second].currentValue;

    return getValue(id);
}

void CalibrationSettings::setValue(const std::string& id, float value, bool notify)
{
    auto it = idToIndex.find(id);
    if (it != idToIndex.end())
    {
        if (id == "calibrationProfile")
        {
            float prevValue = params[it->second].currentValue;
            int prevProfile = (int)std::round(prevValue);
            int newProfile = (int)std::round(value);
            
            params[it->second].currentValue = value;
            
            if (newProfile == 0 || newProfile == 1 || newProfile == 2)
            {
                // Force a hard reset for non-hybrid profiles
                for (auto& p : params)
                {
                    if (p.id == "calibrationProfile") continue;
                    
                    float targetDefault = p.defaultValue;
                    std::string j6Id = p.id + "_J6";
                    std::string j60Id = p.id + "_J60";
                    std::string j106Id = p.id + "_J106";
                    
                    if (idToIndex.find(j6Id) != idToIndex.end())
                    {
                        if (newProfile == 0) targetDefault = params[idToIndex[j6Id]].defaultValue;
                        else if (newProfile == 1) targetDefault = params[idToIndex[j60Id]].defaultValue;
                        else targetDefault = params[idToIndex[j106Id]].defaultValue;
                    }
                    
                    p.currentValue = targetDefault;
                    if (notify && onChangeCallback) onChangeCallback(p.id, p.currentValue);
                }
                if (notify) save();
                return;
            }
            
            // Soft update if switching to Super Six (3)
            for (auto& p : params)
            {
                if (p.id == "calibrationProfile") continue;
                
                float prevDefault = p.defaultValue;
                std::string j6Id = p.id + "_J6";
                std::string j60Id = p.id + "_J60";
                std::string j106Id = p.id + "_J106";
                
                if (idToIndex.find(j6Id) != idToIndex.end())
                {
                    if (prevProfile == 0) prevDefault = params[idToIndex[j6Id]].defaultValue;
                    else if (prevProfile == 1) prevDefault = params[idToIndex[j60Id]].defaultValue;
                    else prevDefault = params[idToIndex[j106Id]].defaultValue;
                }
                
                float newDefault = p.defaultValue;
                if (idToIndex.find(j6Id) != idToIndex.end())
                {
                    if (newProfile == 0) newDefault = params[idToIndex[j6Id]].defaultValue;
                    else if (newProfile == 1) newDefault = params[idToIndex[j60Id]].defaultValue;
                    else newDefault = params[idToIndex[j106Id]].defaultValue;
                }
                
                // If it was at the previous default (not changed by hand), update to new default
                if (p.currentValue == prevDefault)
                {
                    p.currentValue = newDefault;
                    if (notify && onChangeCallback) onChangeCallback(p.id, p.currentValue);
                }
            }
            if (notify) save();
            return;
        }

        params[it->second].currentValue = value;
        if (notify)
        {
            if (onChangeCallback) onChangeCallback(id, value);
            save(); 
        }
    }
}

void CalibrationSettings::hardResetToProfile(int profile)
{
    setValue("calibrationProfile", (float)profile, false);
    
    for (auto& p : params)
    {
        if (p.id == "calibrationProfile") continue;
        
        float targetDefault = p.defaultValue;
        std::string j6Id = p.id + "_J6";
        std::string j60Id = p.id + "_J60";
        std::string j106Id = p.id + "_J106";
        
        if (idToIndex.find(j6Id) != idToIndex.end())
        {
            if (profile == 0) targetDefault = params[idToIndex[j6Id]].defaultValue;
            else if (profile == 1) targetDefault = params[idToIndex[j60Id]].defaultValue;
            else targetDefault = params[idToIndex[j106Id]].defaultValue;
        }
        
        p.currentValue = targetDefault;
        if (onChangeCallback) onChangeCallback(p.id, p.currentValue);
    }
    save();
}

Cal::CalibrationParam* CalibrationSettings::getParam(const std::string& id)
{
    auto it = idToIndex.find(id);
    if (it != idToIndex.end())
        return &params[it->second];
    return nullptr;
}

std::string CalibrationSettings::getConfigFile() const
{
    juce::File f = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile("ABDJUNiO601")
                .getChildFile("calibration.json");
    if (!f.getParentDirectory().exists()) f.getParentDirectory().createDirectory();
    return f.getFullPathName().toStdString();
}

void CalibrationSettings::load()
{
    juce::File f(getConfigFile());
    if (!f.existsAsFile()) return;

    auto json = juce::JSON::parse(f);
    if (auto* obj = json.getDynamicObject())
    {
        for (auto& p : params)
        {
            if (obj->hasProperty(juce::Identifier(p.id)))
                p.currentValue = (float)obj->getProperty(juce::Identifier(p.id));
        }
        // Load string properties
        if (obj->hasProperty("_strings"))
        {
            auto* strObj = obj->getProperty("_strings").getDynamicObject();
            if (strObj != nullptr && strObj->hasProperty("libraryPath"))
                libraryPath = strObj->getProperty("libraryPath").toString().toStdString();
        }
    }
}

void CalibrationSettings::save()
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    for (auto& p : params)
        obj->setProperty(juce::Identifier(p.id), p.currentValue);
    
    // Save string properties
    juce::DynamicObject::Ptr strObj = new juce::DynamicObject();
    strObj->setProperty("libraryPath", juce::String(libraryPath));
    obj->setProperty("_strings", juce::var(strObj.get()));
    
    juce::File f(getConfigFile());
    f.replaceWithText(juce::JSON::toString(juce::var(obj.get())));
}

void CalibrationSettings::setLibraryPath(const std::string& path)
{
    libraryPath = path;
    save();
}

void CalibrationSettings::resetToDefaults()
{
    for (auto& p : params)
    {
        p.currentValue = p.defaultValue;
        if (onChangeCallback) onChangeCallback(p.id, p.currentValue);
    }
    save();
}

void CalibrationSettings::resetParam(const std::string& id)
{
    auto it = idToIndex.find(id);
    if (it != idToIndex.end())
    {
        params[it->second].currentValue = params[it->second].defaultValue;
        if (onChangeCallback) onChangeCallback(id, params[it->second].currentValue);
        save();
    }
}

void CalibrationSettings::resetCategory(const std::string& category)
{
    for (auto& p : params)
    {
        if (p.category == category)
        {
            p.currentValue = p.defaultValue;
            if (onChangeCallback) onChangeCallback(p.id, p.currentValue);
        }
    }
    save();
}

bool CalibrationSettings::loadFromPath(const std::string& path)
{
    juce::File file(path);
    if (!file.existsAsFile()) return false;

    auto json = juce::JSON::parse(file);
    if (auto* rootObj = json.getDynamicObject())
    {
        if (rootObj->hasProperty("__metadata__"))
        {
            if (auto* metaObj = rootObj->getProperty("__metadata__").getDynamicObject())
            {
                juce::String fileModel = metaObj->getProperty("model").toString();
                std::string currentModelName = "Super Juno SIX";
#if defined(COMPILING_JUNO106)
                currentModelName = "Juno-106";
#elif defined(COMPILING_JUNO60)
                currentModelName = "Juno-60";
#elif defined(COMPILING_JUNO6)
                currentModelName = "Juno-6";
#else
                int profile = (int)std::round(getValue("calibrationProfile"));
                if (profile == 0) currentModelName = "Juno-6";
                else if (profile == 1) currentModelName = "Juno-60";
                else currentModelName = "Juno-106";
#endif

#if defined(COMPILING_JUNO106) || defined(COMPILING_JUNO60) || defined(COMPILING_JUNO6)
                if (fileModel != juce::String (currentModelName))
                {
                    juce::Logger::writeToLog("[JUNiO] Calibration import rejected: Model mismatch. File is for " + fileModel + ", current model is " + juce::String (currentModelName));
                    return false;
                }
#else
                if (fileModel != juce::String (currentModelName))
                {
                    int targetProfile = 2; // default J106
                    if (fileModel == "Juno-6") targetProfile = 0;
                    else if (fileModel == "Juno-60") targetProfile = 1;
                    
                    setValue("calibrationProfile", (float)targetProfile, true);
                    juce::Logger::writeToLog("[JUNiO] Calibration import auto-switched profile to: " + fileModel);
                }
#endif
            }
        }

        for (auto& p : params)
        {
            if (rootObj->hasProperty(juce::Identifier(p.id)))
                setValue(p.id, (float)rootObj->getProperty(juce::Identifier(p.id)), true);
        }
        save();
        return true;
    }
    return false;
}

bool CalibrationSettings::saveToPath(const std::string& path)
{
    juce::File file(path);
    juce::DynamicObject::Ptr rootObj = new juce::DynamicObject();

    // Create metadata
    juce::DynamicObject::Ptr metaObj = new juce::DynamicObject();
    metaObj->setProperty("app", "ABDJUNiO601");

    std::string currentModelName = "Super Juno SIX";
#if defined(COMPILING_JUNO106)
    currentModelName = "Juno-106";
#elif defined(COMPILING_JUNO60)
    currentModelName = "Juno-60";
#elif defined(COMPILING_JUNO6)
    currentModelName = "Juno-6";
#else
    int profile = (int)std::round(getValue("calibrationProfile"));
    if (profile == 0) currentModelName = "Juno-6";
    else if (profile == 1) currentModelName = "Juno-60";
    else currentModelName = "Juno-106";
#endif
    metaObj->setProperty("model", juce::String (currentModelName));
    metaObj->setProperty("calibrationProfile", getValue("calibrationProfile"));

    rootObj->setProperty("__metadata__", juce::var(metaObj));

    for (const auto& p : params)
        rootObj->setProperty(juce::Identifier(p.id), p.currentValue);

    return file.replaceWithText(juce::JSON::toString(juce::var(rootObj.get())));
}

float CalibrationSettings::getDacHz(int index) const
{
    if (index < 0) index = 0;
    if (index > 4095) index = 4095;
    
    if (hasCustomDacTable)
        return customDacTable[(size_t)index];
    
    return static_cast<float>(getDacHzTable()[index]);
}

bool CalibrationSettings::importDacTableCsv(const std::string& path)
{
    juce::File file(path);
    if (!file.existsAsFile()) return false;
    
    std::ifstream in(file.getFullPathName().toStdString());
    if (!in.is_open()) return false;
    
    std::string line;
    int count = 0;
    
    // Skip header
    std::getline(in, line);
    
    while (std::getline(in, line) && count < 4096)
    {
        std::stringstream ss(line);
        std::string indexStr, hzStr;
        if (std::getline(ss, indexStr, ',') && std::getline(ss, hzStr, ','))
        {
            try {
                customDacTable[count] = std::stof(hzStr);
                count++;
            } catch (...) {
                return false;
            }
        }
    }
    
    if (count == 4096)
    {
        hasCustomDacTable = true;
        return true;
    }
    return false;
}

bool CalibrationSettings::exportDacTableCsv(const std::string& path)
{
    juce::File file(path);
    std::ofstream out(file.getFullPathName().toStdString());
    if (!out.is_open()) return false;
    
    auto* table = getDacHzTable();
    out << "DAC_Code,Frequency_Hz\n";
    for (int i = 0; i < 4096; ++i)
    {
        float val = hasCustomDacTable ? customDacTable[i] : static_cast<float>(table[i]);
        out << i << "," << val << "\n";
    }
    
    return true;
}

float CalibrationSettings::getVcaGain(int index) const
{
    if (index < 0) index = 0;
    if (index > 255) index = 255;
    
    if (hasCustomVcaTable)
        return customVcaTable[(size_t)index];
    
    return kr106::kVCATableHW[(size_t)index];
}

bool CalibrationSettings::importVcaTableCsv(const std::string& path)
{
    juce::File file(path);
    if (!file.existsAsFile()) return false;
    
    std::ifstream in(file.getFullPathName().toStdString());
    if (!in.is_open()) return false;
    
    std::string line;
    int count = 0;
    
    // Skip header
    std::getline(in, line);
    
    while (std::getline(in, line) && count < 256)
    {
        std::stringstream ss(line);
        std::string indexStr, gainStr;
        if (std::getline(ss, indexStr, ',') && std::getline(ss, gainStr, ','))
        {
            try {
                customVcaTable[count] = std::stof(gainStr);
                count++;
            } catch (...) {
                return false;
            }
        }
    }
    
    if (count == 256)
    {
        hasCustomVcaTable = true;
        return true;
    }
    return false;
}

bool CalibrationSettings::exportVcaTableCsv(const std::string& path)
{
    juce::File file(path);
    std::ofstream out(file.getFullPathName().toStdString());
    if (!out.is_open()) return false;
    
    out << "Index,Gain\n";
    for (int i = 0; i < 256; ++i)
    {
        float val = hasCustomVcaTable ? customVcaTable[i] : kr106::kVCATableHW[(size_t)i];
        out << i << "," << val << "\n";
    }
    
    return true;
}

// ROM uPD7811G 0C60_lfoSpeedTbl
static constexpr uint16_t kDefaultLfoSpeedTbl[128] = {
    0x0005, 0x000f, 0x0019, 0x0028, 0x0037, 0x0046, 0x0050, 0x005a, // idx 0-7
    0x0064, 0x006e, 0x0078, 0x0082, 0x008c, 0x0096, 0x00a0, 0x00aa, // idx 8-15
    0x00b4, 0x00be, 0x00c8, 0x00d2, 0x00dc, 0x00e6, 0x00f0, 0x00fa, // idx 16-23
    0x0104, 0x010e, 0x0118, 0x0122, 0x012c, 0x0136, 0x0140, 0x014a, // idx 24-31
    0x0154, 0x015e, 0x0168, 0x0172, 0x017c, 0x0186, 0x0190, 0x019a, // idx 32-39
    0x01a4, 0x01ae, 0x01b8, 0x01c2, 0x01cc, 0x01d6, 0x01e0, 0x01ea, // idx 40-47
    0x01f4, 0x01fe, 0x0208, 0x0212, 0x021c, 0x0226, 0x0230, 0x023a, // idx 48-55
    0x0244, 0x024e, 0x0258, 0x0262, 0x026c, 0x0276, 0x0280, 0x028a, // idx 56-63
    0x029a, 0x02aa, 0x02ba, 0x02ca, 0x02da, 0x02ea, 0x02fa, 0x030a, // idx 64-71
    0x031a, 0x032a, 0x033a, 0x034a, 0x035a, 0x036a, 0x037a, 0x038a, // idx 72-79
    0x039a, 0x03aa, 0x03ba, 0x03ca, 0x03da, 0x03ea, 0x03fa, 0x040a, // idx 80-87
    0x041a, 0x042a, 0x043a, 0x044a, 0x045a, 0x046a, 0x047a, 0x048a, // idx 88-95
    0x04be, 0x04f2, 0x0526, 0x055a, 0x058e, 0x05c2, 0x05f6, 0x062c, // idx 96-103
    0x0672, 0x06b8, 0x0708, 0x0758, 0x07a8, 0x07f8, 0x085c, 0x08c0, // idx 104-111
    0x0924, 0x0988, 0x09ec, 0x0a50, 0x0ab4, 0x0b18, 0x0b7c, 0x0be0, // idx 112-119
    0x0c58, 0x0cd0, 0x0d48, 0x0dde, 0x0e74, 0x0f0a, 0x0fa0, 0x1000  // idx 120-127
};

// ROM 0B30_LfoDelayRampTbl (idx = pot >> 4)
static constexpr uint16_t kDefaultLfoRampTbl[8] = {
    0xFFFF, 0x0419, 0x020C, 0x015E, 0x0100, 0x0100, 0x0100, 0x0100
};

// hardware-measured dcoSubLevel_j106
static constexpr float kDefaultSubLevelTbl[11] = {
    0.00154f, 0.01251f, 0.10875f, 0.22125f, 0.33576f, 0.45206f,
    0.55750f, 0.67178f, 0.78447f, 0.91132f, 1.00000f
};

uint16_t CalibrationSettings::getLfoSpeedCoeff(int index) const
{
    if (index < 0) index = 0;
    if (index > 127) index = 127;
    if (hasCustomLfoSpeedTable)
        return customLfoSpeedTable[(size_t)index];
    return kDefaultLfoSpeedTbl[index];
}

bool CalibrationSettings::importLfoSpeedTableCsv(const std::string& path)
{
    juce::File file(path);
    if (!file.existsAsFile()) return false;
    std::ifstream in(file.getFullPathName().toStdString());
    if (!in.is_open()) return false;
    std::string line;
    int count = 0;
    std::getline(in, line); // Skip header
    while (std::getline(in, line) && count < 128)
    {
        std::stringstream ss(line);
        std::string indexStr, coeffStr;
        if (std::getline(ss, indexStr, ',') && std::getline(ss, coeffStr, ','))
        {
            try {
                customLfoSpeedTable[count] = (uint16_t)std::stoul(coeffStr);
                count++;
            } catch (...) {
                return false;
            }
        }
    }
    if (count == 128)
    {
        hasCustomLfoSpeedTable = true;
        return true;
    }
    return false;
}

bool CalibrationSettings::exportLfoSpeedTableCsv(const std::string& path)
{
    juce::File file(path);
    std::ofstream out(file.getFullPathName().toStdString());
    if (!out.is_open()) return false;
    out << "Index,Coeff\n";
    for (int i = 0; i < 128; ++i)
    {
        uint16_t val = hasCustomLfoSpeedTable ? customLfoSpeedTable[i] : kDefaultLfoSpeedTbl[i];
        out << i << "," << val << "\n";
    }
    return true;
}

uint16_t CalibrationSettings::getLfoRampIncrement(int index) const
{
    if (index < 0) index = 0;
    if (index > 7) index = 7;
    if (hasCustomLfoRampTable)
        return customLfoRampTable[(size_t)index];
    return kDefaultLfoRampTbl[index];
}

bool CalibrationSettings::importLfoRampTableCsv(const std::string& path)
{
    juce::File file(path);
    if (!file.existsAsFile()) return false;
    std::ifstream in(file.getFullPathName().toStdString());
    if (!in.is_open()) return false;
    std::string line;
    int count = 0;
    std::getline(in, line);
    while (std::getline(in, line) && count < 8)
    {
        std::stringstream ss(line);
        std::string indexStr, incStr;
        if (std::getline(ss, indexStr, ',') && std::getline(ss, incStr, ','))
        {
            try {
                customLfoRampTable[count] = (uint16_t)std::stoul(incStr);
                count++;
            } catch (...) {
                return false;
            }
        }
    }
    if (count == 8)
    {
        hasCustomLfoRampTable = true;
        return true;
    }
    return false;
}

bool CalibrationSettings::exportLfoRampTableCsv(const std::string& path)
{
    juce::File file(path);
    std::ofstream out(file.getFullPathName().toStdString());
    if (!out.is_open()) return false;
    out << "Index,Increment\n";
    for (int i = 0; i < 8; ++i)
    {
        uint16_t val = hasCustomLfoRampTable ? customLfoRampTable[i] : kDefaultLfoRampTbl[i];
        out << i << "," << val << "\n";
    }
    return true;
}

float CalibrationSettings::getSubLevel(int index) const
{
    if (index < 0) index = 0;
    if (index > 10) index = 10;
    if (hasCustomSubLevelTable)
        return customSubLevelTable[(size_t)index];
    return kDefaultSubLevelTbl[index];
}

bool CalibrationSettings::importSubLevelTableCsv(const std::string& path)
{
    juce::File file(path);
    if (!file.existsAsFile()) return false;
    std::ifstream in(file.getFullPathName().toStdString());
    if (!in.is_open()) return false;
    std::string line;
    int count = 0;
    std::getline(in, line);
    while (std::getline(in, line) && count < 11)
    {
        std::stringstream ss(line);
        std::string indexStr, lvlStr;
        if (std::getline(ss, indexStr, ',') && std::getline(ss, lvlStr, ','))
        {
            try {
                customSubLevelTable[count] = std::stof(lvlStr);
                count++;
            } catch (...) {
                return false;
            }
        }
    }
    if (count == 11)
    {
        hasCustomSubLevelTable = true;
        return true;
    }
    return false;
}

bool CalibrationSettings::exportSubLevelTableCsv(const std::string& path)
{
    juce::File file(path);
    std::ofstream out(file.getFullPathName().toStdString());
    if (!out.is_open()) return false;
    out << "Index,Level\n";
    for (int i = 0; i < 11; ++i)
    {
        float val = hasCustomSubLevelTable ? customSubLevelTable[i] : kDefaultSubLevelTbl[i];
        out << i << "," << val << "\n";
    }
    return true;
}

