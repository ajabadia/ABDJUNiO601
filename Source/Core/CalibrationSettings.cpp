#include <JuceHeader.h>
#include "CalibrationSettings.h"
#include "JunoConstants.h"
#include <fstream>
#include <iostream>

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

    // --- GENERAL PREFERENCES ---
    reg("midiChannel", "Global MIDI Channel", "GENERAL", "", "Global MIDI input channel (1-16, 0=OMNI).", 0.0f, 0.0f, 16.0f, 1.0f, true);
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
    reg("dcoMixerGain", "DCO Mixer Gain", "DCO", "", "Level of DCO output before VCF.", 0.70f, 0.1f, 1.5f, 0.05f);
    reg("masterClockHz", "Oscillator Master Clock", "DCO", "Hz", "Frequency of the master high-speed clock used for DCO dividers (factory=8MHz).", 8000000.0f, 7000000.0f, 9000000.0f, 1.0f);
    reg("mixerSaturation", "DCO Mixer Clipping", "DCO", "", "Threshold for DCO mixing stage saturation clipping.", 0.6f, 0.1f, 4.0f, 0.05f);
    reg("subAmpScale", "Sub-Oscillator Gain", "DCO", "", "Level of the square sub-oscillator.", 0.707f, 0.1f, 1.5f, 0.05f);
    reg("noiseGain", "Noise Level Trim", "DCO", "", "Level of the white noise generator relative to DCOs.", 1.0f, 0.1f, 2.0f, 0.05f);
    reg("pwmCenterDuty", "PWM Center Duty", "DCO", "%", "Calibration of the 50% center point for pulse width.", 0.5f, 0.4f, 0.6f, 0.01f);
    reg("pwmMaxDuty", "PWM Maximum Duty", "DCO", "%", "Calibration of the maximum pulse width (factory=95%).", 0.95f, 0.9f, 0.99f, 0.01f);
    reg("pwmMinDuty", "PWM Minimum Duty", "DCO", "%", "Calibration of the minimum pulse width (factory=5%).", 0.05f, 0.01f, 0.1f, 0.01f);
    reg("pwmOffset", "PWM Tuning Offset", "DCO", "%", "Shifts the center point of the Pulse Width modulation.", 0.0f, -10.0f, 10.0f, 0.5f);
    
    // --- VCA ---
    reg("vcaMasterGain", "VCA Master Gain", "VCA", "", "Overall gain trim for each voice.", 1.0f, 0.1f, 2.0f, 0.05f);
    reg("vcaVelSensScale", "VCA Velocity Scale", "VCA", "", "Master multiplier for velocity sensitivity.", 1.0f, 0.0f, 2.0f, 0.1f);
    reg("vcaSagAmt", "VCA Power Sag", "VCA", "", "Sensitivity of global volume drop when multiple voices are active.", 0.025f, 0.0f, 0.1f, 0.005f);
    reg("vcaKillThreshold", "Voice Kill Threshold", "VCA", "", "Threshold below which a voice is considered silent and freed.", 0.004f, 0.001f, 0.02f, 0.001f);
    reg("vcaDcOffset", "DC Offset Correction", "VCA", "", "Per-voice DC offset compensation (simulates analog VCA imbalance).", 0.0f, -0.01f, 0.01f, 0.0001f);
    reg("vcaOffset", "VCA Bias Offset", "VCA", "", "Simulates per-voice hardware bias variance.", 0.0f, -0.05f, 0.05f, 0.005f);

    // --- ADSR CALIBRATION ---
    reg("adsrSlewMs", "ADSR Output Smoothing", "ADSR", "ms", "Slew time to prevent digital step clicks.", 1.5f, 0.1f, 10.0f, 0.1f);
    reg("adsrAttackFactor", "ADSR Attack Factor", "ADSR", "", "Curvature of the attack stage.", 0.35f, 0.1f, 1.0f, 0.01f);
    reg("adsrMcuRate", "Env MCU Speed", "ADSR", "ms", "Internal refresh rate of the envelope microprocessor (original 8031 was ~3ms).", 3.0f, 0.5f, 10.0f, 0.1f);
    reg("adsrDacSteps", "Env DAC Resolution", "ADSR", "steps", "Quantization of the envelope DAC (approximates the discrete R-2R ladder steps).", 1024.0f, 16.0f, 16384.0f, 1.0f, true);
    reg("adsrOvershoot", "Attack Overshoot", "ADSR", "V", "Voltage spike at the end of the attack phase due to capacitor charging inertia.", 1.08f, 1.0f, 1.25f, 0.01f);
    
    // --- CHORUS ---
    reg("chorusMix", "Chorus Dry/Wet Mix", "CHORUS", "%", "Balance between dry signal and the analog BBD chorus output.", 1.0f, 0.0f, 1.0f, 0.01f);
    reg("chorusHiss", "Analog Hiss Level", "CHORUS", "dB", "Baseline analog noise level of the BBD delay lines.", -52.0f, -96.0f, -30.0f, 0.5f);
    reg("chorusDelayI", "Chorus I Base Delay", "CHORUS", "ms", "Base delay for Chorus I mode (Factory 3.2ms).", 3.2f, 1.0f, 10.0f, 0.1f);
    reg("chorusDelayII", "Chorus II Base Delay", "CHORUS", "ms", "Base delay for Chorus II mode (Factory 6.4ms).", 6.4f, 2.0f, 20.0f, 0.1f);
    reg("chorusLfoRate", "Chorus LFO Frequency", "CHORUS", "Hz", "Internal hardware LFO rate for chorus modulation (Factory 0.513Hz).", 0.513f, 0.1f, 2.0f, 0.01f);
    reg("chorusModDepth", "Chorus Mod Depth", "CHORUS", "ms", "Maximum LFO sweep width in milliseconds.", 1.5f, 0.1f, 5.0f, 0.1f);
    reg("chorusSatBoost", "Chorus Saturation", "CHORUS", "", "Analog warmth boost and subtle clipping (1.0 = clean).", 1.2f, 0.5f, 2.0f, 0.05f);
    reg("chorusFilterCutoff", "Chorus Filter Cutoff", "CHORUS", "Hz", "Post-BBD reconstruction filter frequency range.", 8000.0f, 2000.0f, 15000.0f, 100.0f);
    reg("chorusHissColor", "Hiss Filter Color", "CHORUS", "", "Spectral character (Pink/White/Dark) of the BBD noise floor.", 0.4f, 0.05f, 1.0f, 0.05f);

    // --- LFO ---
    reg("lfoMaxRate", "LFO Max Frequency", "LFO", "Hz", "Max frequency at slider 10.", 30.0f, 1.0f, 100.0f, 0.5f);
    reg("lfoMinRate", "LFO Min Frequency", "LFO", "Hz", "Min frequency at slider 0.", 0.1f, 0.01f, 5.0f, 0.05f);
    reg("lfoDelayMax", "LFO Max DelayTime", "LFO", "s", "Max delay time at slider 10.", 3.0f, 0.1f, 10.0f, 0.1f);
    reg("lfoResolution", "LFO DAC Steps (7.5=4bit)", "LFO", "", "Resolution of LFO (higher = cleaner).", 7.5f, 1.0f, 128.0f, 0.5f);

    // --- VCF ---
    reg("vcfMinHz", "VCF Min Frequency", "VCF", "Hz", "Lowest possible cutoff frequency.", 18.0f, 5.0f, 100.0f, 1.0f);
    reg("vcfMaxHz", "VCF Max Frequency", "VCF", "Hz", "Highest possible cutoff frequency.", 18000.0f, 5000.0f, 22000.0f, 100.0f);
    reg("vcfSelfOscThreshold", "VCF Self-Osc Point", "VCF", "", "Resonance level where self-oscillation begins.", 0.95f, 0.85f, 1.0f, 0.01f);
    reg("vcfSaturation", "VCF OTA Saturation", "VCF", "", "Amount of non-linear drive in the filter stages.", 1.0f, 0.1f, 4.0f, 0.05f);
    reg("vcfResoComp", "VCF Resonance Comp", "VCF", "", "Gain compensation when resonance is high.", 0.5f, 0.0f, 1.5f, 0.05f);
    reg("vcfLfoDepth", "LFO Filter Depth", "VCF", "", "Sensitivity of the VCF cutoff to the master LFO modulation.", 0.3f, 0.05f, 1.0f, 0.05f);
    reg("vcfEnvRange", "Env Filter Range", "VCF", "", "Maximum sweep range of the envelope generator on the VCF cutoff.", 2.0f, 0.5f, 4.0f, 0.1f);
    reg("vcfSelfOscInt", "Self-Osc Intensity", "VCF", "", "Intensity of the self-oscillating feedback loop at maximum resonance.", 0.5f, 0.1f, 2.0f, 0.05f);
    reg("vcfTrackCenter", "VCF Tracking Center", "VCF", "Hz", "Pitch pivot point for keyboard tracking (determines where tracking is exactly 1:1).", 440.0f, 100.0f, 1000.0f, 10.0f);
    reg("vcfResoSpread", "VCF Resonance Spread", "VCF", "%", "Variance in filter resonance response between voices (simulates 80017A chip tolerances).", 0.05f, 0.0f, 0.2f, 0.01f);
    reg("vcfWidth", "VCF Tracking Width", "VCF", "", "V/oct scaling accuracy (simulates VCF Width trim pot).", 1.0f, 0.8f, 1.2f, 0.01f);

    // --- HPF ---
    reg("hpfFreq2", "HPF Pos 2 Frequency", "HPF", "Hz", "Frequency at high-pass position 2.", 225.0f, 50.0f, 1000.0f, 1.0f);
    reg("hpfFreq3", "HPF Pos 3 Frequency", "HPF", "Hz", "Frequency at high-pass position 3.", 700.0f, 300.0f, 2500.0f, 5.0f);
    reg("hpfShelfFreq", "HPF Pos 0 Shelf Freq", "HPF", "Hz", "Low shelf frequency for bass boost (pos 0).", 70.0f, 20.0f, 300.0f, 1.0f);
    reg("hpfShelfGain", "HPF Pos 0 Shelf Gain", "HPF", "dB", "Low shelf gain for bass boost (pos 0).", 3.0f, 0.0f, 12.0f, 0.1f);
    reg("hpfQ", "HPF Filter Q", "HPF", "", "Resonance/bandwidth of the HPF circuit.", 0.707f, 0.1f, 2.0f, 0.01f);

    // --- THERMAL DRIFT ---
    reg("thermalDrift", "Global Thermal Intensity", "THERMAL", "%", "Global scale of the pitch instability due to component heat.", 100.0f, 0.0f, 200.0f, 1.0f);
    reg("thermalInertia", "Thermal Inertia", "THERMAL", "samples", "Samples between drift updates (higher = slower drift).", 1024.0f, 64.0f, 8192.0f, 64.0f, true);
    reg("thermalMigration", "Thermal Migration Rate", "THERMAL", "", "Speed of the random pitch wandering over time.", 0.0005f, 0.0001f, 0.01f, 0.0001f);

    // --- ANALOG AGING & FIDELITY ---
    reg("vcaCrosstalk", "Voice Crosstalk", "AGING", "", "Amount of signal leakage between internal voice circuits.", 0.007f, 0.0f, 0.05f, 0.001f);
    reg("masterNoise", "Global Noise Floor", "AGING", "dB", "Analog output stage background noise floor.", -80.0f, -100.0f, -40.0f, 0.5f);
    reg("stereoBleed", "Stereo Cross-bleeding", "AGING", "", "Analog leakage between Left and Right output channels.", 0.03f, 0.0f, 0.15f, 0.005f);
    reg("voiceVariance", "Voice Pitch Variance", "AGING", "cents", "Static pitch difference between voices (pre-thermal).", 2.0f, 0.0f, 10.0f, 0.1f);
    reg("unisonSpread", "Unison Spread Scale", "AGING", "", "Master multiplier for the Unison detune amount.", 1.0f, 0.1f, 2.0f, 0.1f);
    reg("dcoGlobalDrift", "Master Clock Drift", "AGING", "cents", "Shared pitch instability across all voices (Master Clock variations).", 0.5f, 0.0f, 5.0f, 0.1f);
    reg("dcoVoiceDrift", "Voice Drift Amount", "AGING", "cents", "Individual voice pitch wandering due to thermal noise in DCO dividers.", 0.3f, 0.0f, 3.0f, 0.1f);
    reg("dcoDriftComplexity", "DCO Drift Complexity", "AGING", "", "Depth of fractal random walk for thermal pitch wandering.", 0.5f, 0.0f, 1.0f, 0.05f);
    reg("vcaRippleDepth", "VCA Ripple Depth", "AGING", "", "Depth of the envelope-induced power supply ripple noise (VCA crosstalk).", 0.0005f, 0.0f, 0.005f, 0.0001f);
    reg("lfoDelayCurve", "LFO Onset Curve", "AGING", "", "Attack curvature for the LFO onset (how fast it reaches full depth).", 5.0f, 1.0f, 10.0f, 0.1f);
    reg("dcoDriftRate", "Master Drift Rate", "AGING", "Hz", "Speed of the pitch wandering (random walk frequency).", 0.008f, 0.001f, 0.05f, 0.001f);
    reg("dcoLfoPitchDepth", "DCO Vibrato Depth", "DCO", "", "Sensitivity of the DCO pitch to the master LFO (vibrato depth).", 0.4f, 0.1f, 1.0f, 0.05f);
    
    // --- SYSTEM ---
    reg("a4Reference", "A4 Reference Pitch", "SYSTEM", "Hz", "Master tuning reference frequency (Standard=440Hz).", 440.0f, 400.0f, 480.0f, 1.0f);
    reg("oversampling", "Internal Oversampling", "SYSTEM", "x", "Audio engine oversampling rate (1x=Normal, 2x=Hi-Fi, 4x=Extreme).", 1.0f, 1.0f, 4.0f, 1.0f, true);
    reg("sliderHysteresis", "Slider Hysteresis", "SYSTEM", "", "Simulates mechanical wear and friction in the analog sliders.", 0.01f, 0.0f, 0.1f, 0.005f);
    reg("paramSlewRate", "Parameter Slew Rate", "SYSTEM", "", "Latency of response when moving sliders (hardware lag simulation).", 0.95f, 0.5f, 1.0f, 0.01f);
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

void CalibrationSettings::setValue(const std::string& id, float value, bool notify)
{
    auto it = idToIndex.find(id);
    if (it != idToIndex.end())
    {
        params[it->second].currentValue = value;
        if (notify)
        {
            if (onChangeCallback) onChangeCallback(id, value);
            save(); 
        }
    }
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
    }
}

void CalibrationSettings::save()
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    for (auto& p : params)
        obj->setProperty(juce::Identifier(p.id), p.currentValue);
    
    juce::File f(getConfigFile());
    f.replaceWithText(juce::JSON::toString(juce::var(obj.get())));
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
    if (auto* obj = json.getDynamicObject())
    {
        for (auto& p : params)
        {
            if (obj->hasProperty(juce::Identifier(p.id)))
                setValue(p.id, (float)obj->getProperty(juce::Identifier(p.id)), true);
        }
        save();
        return true;
    }
    return false;
}

bool CalibrationSettings::saveToPath(const std::string& path)
{
    juce::File file(path);
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    for (const auto& p : params)
        obj->setProperty(juce::Identifier(p.id), p.currentValue);
    
    return file.replaceWithText(juce::JSON::toString(juce::var(obj.get())));
}
