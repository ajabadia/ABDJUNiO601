#pragma once

#include <JuceHeader.h>
#include <map>
#include <vector>

namespace Cal {

/**
 * Individual Calibration Parameter
 */
struct CalibrationParam
{
    juce::String id;
    juce::String label;
    juce::String unit;
    juce::String tooltip;

    float defaultValue;
    float currentValue;
    float minValue;
    float maxValue;
    float stepSize;
    bool isInteger = false;

    bool isModified() const noexcept {
        return std::abs(currentValue - defaultValue) > (stepSize * 0.5f);
    }
};

} // namespace Cal

class CalibrationManager : public juce::ChangeBroadcaster
{
public:
    CalibrationManager();
    ~CalibrationManager() override;

    float getValue(const juce::String& id) const;
    void setValue(const juce::String& id, float value, bool notify = true);
    
    Cal::CalibrationParam* getParam(const juce::String& id);
    const std::vector<Cal::CalibrationParam>& getAllParams() const { return params; }

    void load();
    void save();
    void resetToDefaults();

    // External file support
    juce::Result loadFromFile(const juce::File& file);
    juce::Result saveToFile(const juce::File& file);

    // Callback for real-time DSP updates
    using OnChangeCallback = std::function<void(const juce::String& id, float newValue)>;
    void setOnChangeCallback(OnChangeCallback cb) { onChangeCallback = cb; }

private:
    std::vector<Cal::CalibrationParam> params;
    std::map<juce::String, int> idToIndex;
    OnChangeCallback onChangeCallback;

    void buildParameterList();
    void registerParam(Cal::CalibrationParam p);
    juce::File getConfigFile() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CalibrationManager)
};
