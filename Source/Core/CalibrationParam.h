#pragma once
#include <string>

namespace Cal {

/**
 * Individual Calibration Parameter (Decoupled from JUCE for header stability)
 */
struct CalibrationParam
{
    std::string id;
    std::string label;
    std::string category; 
    std::string unit;
    std::string tooltip;

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
