#pragma once
#include "CalibrationParam.h"
#include <map>
#include <vector>
#include <functional>
#include <string>
#include <array>

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

    // DAC Table CSV Support
    bool importDacTableCsv(const std::string& path);
    bool exportDacTableCsv(const std::string& path);
    float getDacHz(int index) const;

    std::array<float, 4096> customDacTable;
    bool hasCustomDacTable = false;

    // VCA Table CSV Support
    bool importVcaTableCsv(const std::string& path);
    bool exportVcaTableCsv(const std::string& path);
    float getVcaGain(int index) const;

    std::array<float, 256> customVcaTable;
    bool hasCustomVcaTable = false;

    // LFO Speed Table CSV (128 entries — ROM uPD7811G 0C60_lfoSpeedTbl)
    bool importLfoSpeedTableCsv(const std::string& path);
    bool exportLfoSpeedTableCsv(const std::string& path);
    uint16_t getLfoSpeedCoeff(int index) const;   // index 0..127

    std::array<uint16_t, 128> customLfoSpeedTable;
    bool hasCustomLfoSpeedTable = false;

    // LFO Ramp Table CSV (8 entries — ROM 0B30_LfoDelayRampTbl)
    bool importLfoRampTableCsv(const std::string& path);
    bool exportLfoRampTableCsv(const std::string& path);
    uint16_t getLfoRampIncrement(int index) const; // index 0..7

    std::array<uint16_t, 8> customLfoRampTable;
    bool hasCustomLfoRampTable = false;

    // Sub-Osc Level Curve CSV (11 points — dcoSubLevel_j106, hardware-measured)
    bool importSubLevelTableCsv(const std::string& path);
    bool exportSubLevelTableCsv(const std::string& path);
    float getSubLevel(int index) const;           // index 0..10

    std::array<float, 11> customSubLevelTable;
    bool hasCustomSubLevelTable = false;

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
