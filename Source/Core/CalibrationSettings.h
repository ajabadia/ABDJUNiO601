#pragma once
#include "CalibrationParam.h"
#include <map>
#include <vector>
#include <functional>
#include <string>

class CalibrationSettings 
{
public:
    CalibrationSettings();
    ~CalibrationSettings();

    float getValue(const std::string& id) const;
    void setValue(const std::string& id, float value, bool notify = true);
    
    Cal::CalibrationParam* getParam(const std::string& id);
    const std::vector<Cal::CalibrationParam>& getAllParams() const { return params; }

    void load();
    void save();
    void resetToDefaults();
    void resetParam(const std::string& id);
    void resetCategory(const std::string& category);

    // External file support
    bool loadFromPath(const std::string& path);
    bool saveToPath(const std::string& path);

    // Callback for real-time DSP updates
    using OnChangeCallback = std::function<void(std::string, float)>;
    void setOnChangeCallback(OnChangeCallback cb) { onChangeCallback = cb; }

private:
    std::vector<Cal::CalibrationParam> params;
    std::map<std::string, int> idToIndex;
    OnChangeCallback onChangeCallback;

    void buildParameterList();
    void registerParam(Cal::CalibrationParam p);
    std::string getConfigFile() const;
};
