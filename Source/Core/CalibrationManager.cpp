#include "CalibrationManager.h"
#include "../Core/JunoConstants.h"

CalibrationManager::CalibrationManager()
{
    buildParameterList();
    load();
}

CalibrationManager::~CalibrationManager()
{
    save();
}

void CalibrationManager::buildParameterList()
{
    // --- MIXER / DCO ---
    registerParam({ "dcoMixerGain", "DCO Mixer Gain", "", "Global pre-VCF gain scaling.", 0.70f, 0.70f, 0.1f, 1.5f, 0.01f });
    registerParam({ "subAmpScale", "Sub-Osc Amp Scale", "", "Mixing ratio for the sub-oscillator.", 0.75f, 0.75f, 0.1f, 1.5f, 0.01f });
    
    // --- VCF ---
    registerParam({ "vcfSelfOscPoint", "VCF Self-Osc Threshold", "", "Point where the VCF begins to oscillate.", 0.92f, 0.92f, 0.7f, 1.0f, 0.01f });
    registerParam({ "vcfSaturation", "VCF Stage Saturation", "", "Scaling for the Padé saturation stages.", 1.0f, 1.0f, 0.1f, 2.0f, 0.01f });
    
    // --- ADSR ---
    registerParam({ "adsrSlewMs", "ADSR Output Smoothing", "ms", "Slew time to prevent digital step clicks.", 1.5f, 1.5f, 0.1f, 10.0f, 0.1f });
    registerParam({ "adsrAttackFactor", "ADSR Attack Factor", "", "Curvature of the attack stage.", 0.35f, 0.35f, 0.1f, 1.0f, 0.01f });
    
    // --- CHORUS ---
    registerParam({ "chorusHissLvl", "Chorus Hiss Level", "dB", "Baseline analog noise level.", -52.0f, -52.0f, -96.0f, -40.0f, 0.5f });
}

void CalibrationManager::registerParam(Cal::CalibrationParam p)
{
    p.currentValue = p.defaultValue;
    idToIndex[p.id] = (int)params.size();
    params.push_back(p);
}

float CalibrationManager::getValue(const juce::String& id) const
{
    auto it = idToIndex.find(id);
    if (it != idToIndex.end())
        return params[it->second].currentValue;
    return 0.0f;
}

void CalibrationManager::setValue(const juce::String& id, float value, bool notify)
{
    auto it = idToIndex.find(id);
    if (it != idToIndex.end())
    {
        params[it->second].currentValue = value;
        if (notify)
        {
            if (onChangeCallback) onChangeCallback(id, value);
            sendChangeMessage();
        }
    }
}

Cal::CalibrationParam* CalibrationManager::getParam(const juce::String& id)
{
    auto it = idToIndex.find(id);
    if (it != idToIndex.end())
        return &params[it->second];
    return nullptr;
}

juce::File CalibrationManager::getConfigFile() const
{
    auto f = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile("ABDJUNiO601")
                .getChildFile("calibration.json");
    if (!f.getParentDirectory().exists()) f.getParentDirectory().createDirectory();
    return f;
}

void CalibrationManager::load()
{
    auto f = getConfigFile();
    if (!f.existsAsFile()) return;

    auto json = juce::JSON::parse(f);
    if (auto* obj = json.getDynamicObject())
    {
        for (auto& p : params)
        {
            if (obj->hasProperty(p.id))
                p.currentValue = (float)obj->getProperty(p.id);
        }
    }
}

void CalibrationManager::save()
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    for (auto& p : params)
        obj->setProperty(p.id, p.currentValue);
    
    auto f = getConfigFile();
    f.replaceWithText(juce::JSON::toString(juce::var(obj.get())));
}

void CalibrationManager::resetToDefaults()
{
    for (auto& p : params)
    {
        p.currentValue = p.defaultValue;
        if (onChangeCallback) onChangeCallback(p.id, p.currentValue);
    }
    sendChangeMessage();
    save();
}

juce::Result CalibrationManager::loadFromFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return juce::Result::fail("File does not exist.");

    auto json = juce::JSON::parse(file);
    if (auto* obj = json.getDynamicObject())
    {
        for (auto& p : params)
        {
            if (obj->hasProperty(p.id))
                setValue(p.id, (float)obj->getProperty(p.id), true);
        }
        save();
        return juce::Result::ok();
    }
    return juce::Result::fail("Failed to parse calibration file.");
}

juce::Result CalibrationManager::saveToFile(const juce::File& file)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    for (const auto& p : params)
        obj->setProperty(p.id, p.currentValue);
    
    auto result = file.replaceWithText(juce::JSON::toString(juce::var(obj.get())));
    return result ? juce::Result::ok() : juce::Result::fail("Could not write calibration file.");
}
