#include "../../Source/Core/CalibrationSettings.h"
#include "../../Source/Synth/J106DACHzTable.h"
#include "../../Source/Synth/J106VCATable.h"

CalibrationSettings::CalibrationSettings() {}
CalibrationSettings::~CalibrationSettings() {}

float CalibrationSettings::getValue(const std::string& id) const
{
    return 0.0f;
}

float CalibrationSettings::getValueForModel(const std::string&, int) const
{
    return 0.0f;
}

void CalibrationSettings::setValue(const std::string&, float, bool) {}

Cal::CalibrationParam* CalibrationSettings::getParam(const std::string&)
{
    return nullptr;
}

void CalibrationSettings::load() {}
void CalibrationSettings::save() {}
void CalibrationSettings::resetToDefaults() {}
void CalibrationSettings::resetParam(const std::string&) {}
void CalibrationSettings::resetCategory(const std::string&) {}
void CalibrationSettings::hardResetToProfile(int) {}

bool CalibrationSettings::loadFromPath(const std::string&) { return false; }
bool CalibrationSettings::saveToPath(const std::string&) { return false; }

bool CalibrationSettings::importDacTableCsv(const std::string&) { return false; }
bool CalibrationSettings::exportDacTableCsv(const std::string&) { return false; }

float CalibrationSettings::getDacHz(int index) const
{
    if (hasCustomDacTable && index >= 0 && index < 4096)
        return customDacTable[index];
    if (index >= 0 && index < 4096)
        return static_cast<float>(kr106::kV4Hz[index]);
    return 0.0f;
}

bool CalibrationSettings::importVcaTableCsv(const std::string&) { return false; }
bool CalibrationSettings::exportVcaTableCsv(const std::string&) { return false; }

float CalibrationSettings::getVcaGain(int index) const
{
    if (hasCustomVcaTable && index >= 0 && index < 256)
        return customVcaTable[index];
    if (index >= 0 && index < 256)
        return kr106::kVCATable[static_cast<std::size_t>(index)];
    return 0.0f;
}

bool CalibrationSettings::importLfoSpeedTableCsv(const std::string&) { return false; }
bool CalibrationSettings::exportLfoSpeedTableCsv(const std::string&) { return false; }

uint16_t CalibrationSettings::getLfoSpeedCoeff(int index) const
{
    (void)index;
    return 0;
}

bool CalibrationSettings::importLfoRampTableCsv(const std::string&) { return false; }
bool CalibrationSettings::exportLfoRampTableCsv(const std::string&) { return false; }

uint16_t CalibrationSettings::getLfoRampIncrement(int index) const
{
    (void)index;
    return 0;
}

bool CalibrationSettings::importSubLevelTableCsv(const std::string&) { return false; }
bool CalibrationSettings::exportSubLevelTableCsv(const std::string&) { return false; }

float CalibrationSettings::getSubLevel(int index) const
{
    if (hasCustomSubLevelTable && index >= 0 && index < 11)
        return customSubLevelTable[index];
    if (index >= 0 && index < 11)
    {
        static const float defaultSubLevel[11] = {
            0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f
        };
        return defaultSubLevel[index];
    }
    return 0.0f;
}

void CalibrationSettings::setLibraryPath(const std::string&) {}
