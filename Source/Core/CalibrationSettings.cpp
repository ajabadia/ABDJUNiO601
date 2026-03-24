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

    // --- DCO ---
    reg("dcoMixerGain", "DCO Mixer Gain", "DCO", "", "Level of DCO output before VCF.", 0.70f, 0.1f, 1.5f, 0.05f);
    reg("mixerSaturation", "DCO Mixer Clipping", "DCO", "", "Threshold for DCO mixing stage saturation clipping.", 1.15f, 0.1f, 4.0f, 0.05f);
    reg("subAmpScale", "Sub-Oscillator Gain", "DCO", "", "Level of the square sub-oscillator.", 0.75f, 0.1f, 1.5f, 0.05f);
    reg("noiseGain", "Noise Level Trim", "DCO", "", "Level of the white noise generator relative to DCOs.", 1.0f, 0.1f, 2.0f, 0.05f);
    reg("pwmOffset", "PWM Center Offset", "DCO", "%", "Shifts the center point of the Pulse Width modulation.", 0.0f, -10.0f, 10.0f, 0.5f);
    
    // --- VCA ---
    reg("vcaMasterGain", "VCA Master Gain", "VCA", "", "Overall gain trim for each voice.", 1.0f, 0.1f, 2.0f, 0.05f);
    reg("vcaVelSensScale", "VCA Velocity Scale", "VCA", "", "Master multiplier for velocity sensitivity.", 1.0f, 0.0f, 2.0f, 0.1f);
    reg("vcaSagAmt", "VCA Power Sag", "VCA", "", "Sensitivity of global volume drop when multiple voices are active.", 0.025f, 0.0f, 0.1f, 0.005f);

    // --- ADSR CALIBRATION ---
    reg("adsrSlewMs", "ADSR Output Smoothing", "ADSR", "ms", "Slew time to prevent digital step clicks.", 1.5f, 0.1f, 10.0f, 0.1f);
    reg("adsrAttackFactor", "ADSR Attack Factor", "ADSR", "", "Curvature of the attack stage.", 0.35f, 0.1f, 1.0f, 0.01f);
    
    // --- CHORUS ---
    reg("chorusHissLvl", "Chorus Hiss Level", "CHORUS", "dB", "Baseline analog noise level.", -52.0f, -96.0f, -40.0f, 0.5f);
    reg("chorusDelayI", "Chorus I Base Delay", "CHORUS", "ms", "Base delay for Chorus I mode.", 3.2f, 1.0f, 10.0f, 0.1f);
    reg("chorusDelayII", "Chorus II Base Delay", "CHORUS", "ms", "Base delay for Chorus II mode.", 6.4f, 2.0f, 20.0f, 0.1f);
    reg("chorusModDepth", "Chorus Mod Depth", "CHORUS", "ms", "LFO sweep width in milliseconds.", 1.5f, 0.1f, 5.0f, 0.1f);
    reg("chorusSatBoost", "Chorus Saturation", "CHORUS", "", "Analog warmth boost (1.0 = clean).", 1.2f, 0.5f, 2.0f, 0.05f);
    reg("chorusFilterCutoff", "Chorus Filter Cutoff", "CHORUS", "Hz", "Post-BBD reconstruction filter freq.", 8000.0f, 2000.0f, 15000.0f, 100.0f);

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

    // --- HPF ---
    reg("hpfFreq2", "HPF Pos 2 Frequency", "HPF", "Hz", "Frequency at high-pass position 2.", 225.0f, 50.0f, 1000.0f, 1.0f);
    reg("hpfFreq3", "HPF Pos 3 Frequency", "HPF", "Hz", "Frequency at high-pass position 3.", 700.0f, 300.0f, 2500.0f, 5.0f);
    reg("hpfShelfFreq", "HPF Pos 0 Shelf Freq", "HPF", "Hz", "Low shelf frequency for bass boost (pos 0).", 70.0f, 20.0f, 300.0f, 1.0f);
    reg("hpfShelfGain", "HPF Pos 0 Shelf Gain", "HPF", "dB", "Low shelf gain for bass boost (pos 0).", 3.0f, 0.0f, 12.0f, 0.1f);
    reg("hpfQ", "HPF Filter Q", "HPF", "", "Resonance/bandwidth of the HPF circuit.", 0.707f, 0.1f, 2.0f, 0.01f);

    // --- ANALOG AGING & FIDELITY ---
    reg("thermalDrift", "Global Thermal Drift", "AGING", "", "Pitch instability due to component heat.", 0.0f, 0.0f, 10.0f, 0.1f);
    reg("vcaCrosstalk", "Voice Crosstalk", "AGING", "", "Amount of signal leakage between internal voice circuits.", 0.007f, 0.0f, 0.05f, 0.001f);
    reg("masterNoise", "Global Noise Floor", "AGING", "dB", "Analog output stage background noise floor.", -80.0f, -100.0f, -40.0f, 0.5f);
    reg("stereoBleed", "Stereo Cross-bleeding", "AGING", "", "Analog leakage between Left and Right output channels.", 0.03f, 0.0f, 0.15f, 0.005f);
    reg("voiceVariance", "Voice Pitch Variance", "AGING", "cents", "Static pitch difference between voices (pre-thermal).", 2.0f, 0.0f, 10.0f, 0.1f);
    reg("unisonSpread", "Unison Spread Scale", "AGING", "", "Master multiplier for the Unison detune amount.", 1.0f, 0.1f, 2.0f, 0.1f);
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
