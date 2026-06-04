#include "BridgeService.h"
#include "../../Core/ABDSimpleJuno106AudioProcessor.h"
#include "../../Core/CalibrationSettings.h"
#include "../../Core/ServiceModeManager.h"

namespace BridgeService {

void serviceAction(
    ABDSimpleJuno106AudioProcessor& audioProcessor,
    std::unique_ptr<juce::FileChooser>& fileChooser,
    const juce::Array<juce::var>& args,
    const std::function<void(const juce::String&, const juce::var&)>& dispatchToJS,
    juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    if (args.size() >= 1) {
        auto obj = args[0].getDynamicObject();
        if (obj != nullptr) {
            juce::String action = obj->getProperty("action").toString();
            auto& smm = audioProcessor.getServiceModeManager();
            if (action == "testVoice") smm.setVoiceSolo((int)obj->getProperty("voice"));
            else if (action == "stopVoiceTest") smm.clearVoiceSolo();
            else if (action == "sweepVCF") smm.startVCFSweep();
            else if (action == "playTestScale") {
                if (smm.isTestScaleActive()) smm.stopAllTests();
                else smm.startTestScale();
            }
            else if (action == "resetToFactory") audioProcessor.getCalibrationSettings().resetToDefaults();
            else if (action == "hardResetToProfile") {
                int profile = (int)obj->getProperty("profile");
                audioProcessor.getCalibrationSettings().hardResetToProfile(profile);
            }
            else if (action == "resetParam") audioProcessor.getCalibrationSettings().resetParam(obj->getProperty("id").toString().toStdString());
            else if (action == "resetCategory") audioProcessor.getCalibrationSettings().resetCategory(obj->getProperty("category").toString().toStdString());
            else if (action == "exportCalibration") {
                fileChooser = std::make_unique<juce::FileChooser>("Export Calibration JSON",
                                                                  juce::File::getSpecialLocation(juce::File::userHomeDirectory),
                                                                  "*.json");
                fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                [&audioProcessor](const juce::FileChooser& fc) {
                    auto result = fc.getResult();
                    if (result.existsAsFile() || !result.exists()) {
                        audioProcessor.getCalibrationSettings().saveToPath(result.getFullPathName().toStdString());
                        juce::Logger::writeToLog("[JUNiO] Calibration Exported successfully to: " + result.getFullPathName());
                    }
                });
            }
            else if (action == "importCalibration") {
                fileChooser = std::make_unique<juce::FileChooser>("Import Calibration JSON",
                                                                  juce::File::getSpecialLocation(juce::File::userHomeDirectory),
                                                                  "*.json");
                fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [&audioProcessor](const juce::FileChooser& fc) {
                    auto result = fc.getResult();
                    if (result.existsAsFile()) {
                        audioProcessor.getCalibrationSettings().loadFromPath(result.getFullPathName().toStdString());
                        juce::Logger::writeToLog("[JUNiO] Calibration Imported successfully from: " + result.getFullPathName());
                    }
                });
            }
            else if (action == "hpfCycle") { smm.startHpfCycle(); }
            else if (action == "chorusCycle") { smm.startChorusCycle(); }
            else if (action == "autoTuneVCF") { smm.startAutoVcfTune(); }
            else if (action == "toggleRecord") { audioProcessor.toggleRecording(); }
            // DAC Hz Table CSV import/export
            else if (action == "importDacTable") {
                fileChooser = std::make_unique<juce::FileChooser>("Import DAC Hz Table (.csv)...",
                    juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.csv");
                fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [&audioProcessor, dispatchToJS](const juce::FileChooser& fc) {
                    auto result = fc.getResult();
                    if (result.existsAsFile()) {
                        bool ok = audioProcessor.getCalibrationSettings().importDacTableCsv(result.getFullPathName().toStdString());
                        dispatchToJS("onDacTableImport", juce::var(ok));
                    }
                });
            }
            else if (action == "exportDacTable") {
                fileChooser = std::make_unique<juce::FileChooser>("Export DAC Hz Table (.csv)...",
                    juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("J106DACHzTable.csv"), "*.csv");
                fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                [&audioProcessor](const juce::FileChooser& fc) {
                    auto result = fc.getResult();
                    if (result != juce::File()) {
                        audioProcessor.getCalibrationSettings().exportDacTableCsv(result.withFileExtension(".csv").getFullPathName().toStdString());
                    }
                });
            }
            else if (action == "importVcaTable") {
                fileChooser = std::make_unique<juce::FileChooser>("Import VCA Gain Table (.csv)...",
                    juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.csv");
                fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [&audioProcessor, dispatchToJS](const juce::FileChooser& fc) {
                    auto result = fc.getResult();
                    if (result.existsAsFile()) {
                        bool ok = audioProcessor.getCalibrationSettings().importVcaTableCsv(result.getFullPathName().toStdString());
                        dispatchToJS("onVcaTableImport", juce::var(ok));
                    }
                });
            }
            else if (action == "exportVcaTable") {
                fileChooser = std::make_unique<juce::FileChooser>("Export VCA Gain Table (.csv)...",
                    juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("J106VCAGainTable.csv"), "*.csv");
                fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                [&audioProcessor](const juce::FileChooser& fc) {
                    auto result = fc.getResult();
                    if (result != juce::File()) {
                        audioProcessor.getCalibrationSettings().exportVcaTableCsv(result.withFileExtension(".csv").getFullPathName().toStdString());
                    }
                });
            }
            else if (action == "importLfoSpeedTable") {
                fileChooser = std::make_unique<juce::FileChooser>("Import LFO Speed Table (.csv)...",
                    juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.csv");
                fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [&audioProcessor, dispatchToJS](const juce::FileChooser& fc) {
                    auto result = fc.getResult();
                    if (result.existsAsFile()) {
                        bool ok = audioProcessor.getCalibrationSettings().importLfoSpeedTableCsv(result.getFullPathName().toStdString());
                        dispatchToJS("onLfoSpeedTableImport", juce::var(ok));
                    }
                });
            }
            else if (action == "exportLfoSpeedTable") {
                fileChooser = std::make_unique<juce::FileChooser>("Export LFO Speed Table (.csv)...",
                    juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("J106LFOSpeedTable.csv"), "*.csv");
                fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                [&audioProcessor](const juce::FileChooser& fc) {
                    auto result = fc.getResult();
                    if (result != juce::File()) {
                        audioProcessor.getCalibrationSettings().exportLfoSpeedTableCsv(result.withFileExtension(".csv").getFullPathName().toStdString());
                    }
                });
            }
            else if (action == "importLfoRampTable") {
                fileChooser = std::make_unique<juce::FileChooser>("Import LFO Ramp Table (.csv)...",
                    juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.csv");
                fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [&audioProcessor, dispatchToJS](const juce::FileChooser& fc) {
                    auto result = fc.getResult();
                    if (result.existsAsFile()) {
                        bool ok = audioProcessor.getCalibrationSettings().importLfoRampTableCsv(result.getFullPathName().toStdString());
                        dispatchToJS("onLfoRampTableImport", juce::var(ok));
                    }
                });
            }
            else if (action == "exportLfoRampTable") {
                fileChooser = std::make_unique<juce::FileChooser>("Export LFO Ramp Table (.csv)...",
                    juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("J106LFORampTable.csv"), "*.csv");
                fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                [&audioProcessor](const juce::FileChooser& fc) {
                    auto result = fc.getResult();
                    if (result != juce::File()) {
                        audioProcessor.getCalibrationSettings().exportLfoRampTableCsv(result.withFileExtension(".csv").getFullPathName().toStdString());
                    }
                });
            }
            else if (action == "importSubLevelTable") {
                fileChooser = std::make_unique<juce::FileChooser>("Import Sub Level Table (.csv)...",
                    juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.csv");
                fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [&audioProcessor, dispatchToJS](const juce::FileChooser& fc) {
                    auto result = fc.getResult();
                    if (result.existsAsFile()) {
                        bool ok = audioProcessor.getCalibrationSettings().importSubLevelTableCsv(result.getFullPathName().toStdString());
                        dispatchToJS("onSubLevelTableImport", juce::var(ok));
                    }
                });
            }
            else if (action == "exportSubLevelTable") {
                fileChooser = std::make_unique<juce::FileChooser>("Export Sub Level Table (.csv)...",
                    juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("J106SubLevelTable.csv"), "*.csv");
                fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                [&audioProcessor](const juce::FileChooser& fc) {
                    auto result = fc.getResult();
                    if (result != juce::File()) {
                        audioProcessor.getCalibrationSettings().exportSubLevelTableCsv(result.withFileExtension(".csv").getFullPathName().toStdString());
                    }
                });
            }
        }
    }
    completion({});
}

} // namespace BridgeService
