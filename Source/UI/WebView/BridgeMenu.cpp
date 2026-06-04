#include "BridgeMenu.h"
#include "../../Core/ABDSimpleJuno106AudioProcessor.h"
#include "../../Core/PresetManager.h"
#include "../../Core/JunoTapeEncoder.h"
#include "../../Core/JunoTapeDecoder.h"
#include "../../Core/Importers/JunoSysexImporter.h"
#include "../../Core/Importers/JunoCsvImporter.h"

namespace BridgeMenu {

void menuAction(
    ABDSimpleJuno106AudioProcessor& audioProcessor,
    std::unique_ptr<juce::FileChooser>& fileChooser,
    juce::File& pendingImportFile,
    juce::String& pendingImportFormat,
    juce::File& pendingTapeFile,
    JunoTapeDecoder::SmartDecodeResult& pendingSmartResult,
    int& selectedDecoderIndex,
    const juce::Array<juce::var>& args,
    const std::function<void(const juce::String&, const juce::var&)>& dispatchToJS,
    const std::function<void()>& sendPresetListUpdate,
    const std::function<void()>& showAboutCallback,
    const std::function<void()>& showSettingsCallback,
    const std::function<void()>& showServiceModeCallback,
    juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    if (args.size() >= 1) {
        juce::String action = args[0].toString();
        if (action == "panic") audioProcessor.triggerPanic();
        else if (action == "undo") audioProcessor.undo();
        else if (action == "redo") audioProcessor.redo();
        else if (action == "getCurrentPresetName") {
            if (auto* pm = audioProcessor.getPresetManager()) {
                completion(juce::var(pm->getCurrentPreset().name));
                return;
            }
            completion(juce::var("New Preset"));
            return;
        }
        else if (action == "toggleMidiOut") audioProcessor.toggleMidiOut();
        else if (action == "handleManual") {
            audioProcessor.enterTestMode(false);
            audioProcessor.sendManualMode();
        }
        else if (action == "handleTest") {
            audioProcessor.enterTestMode(true);
        }
        else if (action == "handleTestProgram") {
            if (args.size() >= 2) audioProcessor.triggerTestProgram((int)args[1]);
        }
        else if (action == "handleRandomize") {
            audioProcessor.randomizeSound();
        }
        else if (action == "handleAbout") showAboutCallback();
        else if (action == "handleSettings") showSettingsCallback();
        else if (action == "handleServiceMode") showServiceModeCallback();
        else if (action == "exit") {
            if (auto* app = juce::JUCEApplication::getInstance())
                app->systemRequestedQuit();
        }
        else if (action == "handleLoad") {
            // Unified Load: single files route through Smart Import, multi-file direct import
            fileChooser = std::make_unique<juce::FileChooser>("Load patch or bank...",
                juce::File(audioProcessor.getPresetManager()->getLastPath()),
                "*.jno;*.syx;*.wav;*.bin;*.pjunoxl;*.csv;*.json");
            fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                [&audioProcessor, &pendingImportFile, &pendingImportFormat, &pendingTapeFile, &pendingSmartResult, &selectedDecoderIndex, dispatchToJS, sendPresetListUpdate](const juce::FileChooser& chooser) {
                    auto results = chooser.getResults();
                    if (results.size() > 0) {
                        // Single file → route through Smart Import
                        if (results.size() == 1) {
                            auto file = results[0];
                            juce::String ext = file.getFileExtension().toLowerCase();

                            if (ext == ".wav") {
                                // Route to Tape Smart Import
                                pendingTapeFile = juce::File();
                                pendingImportFile = juce::File();
                                dispatchToJS("onSmartImportProgress", "Cargando archivo WAV...");
                                auto progressFn = [dispatchToJS](const juce::String& msg) {
                                    dispatchToJS("onSmartImportProgress", msg);
                                };
                                auto smartResult = JunoTapeDecoder::smartDecode(file, progressFn);
                                juce::AudioFormatManager fmtMgr;
                                fmtMgr.registerBasicFormats();
                                std::unique_ptr<juce::AudioFormatReader> reader(fmtMgr.createReaderFor(file));
                                juce::DynamicObject::Ptr resultObj = new juce::DynamicObject();
                                resultObj->setProperty("fileName", file.getFileName());
                                resultObj->setProperty("filePath", file.getFullPathName());
                                resultObj->setProperty("success", smartResult.success);
                                resultObj->setProperty("autoSelected", smartResult.autoSelected);
                                resultObj->setProperty("winnerIndex", smartResult.winnerIndex);
                                if (reader != nullptr) {
                                    resultObj->setProperty("sampleRate", (int)reader->sampleRate);
                                    resultObj->setProperty("bitDepth", (int)reader->bitsPerSample);
                                    resultObj->setProperty("channels", (int)reader->numChannels);
                                    resultObj->setProperty("duration", (double)reader->lengthInSamples / reader->sampleRate);
                                    resultObj->setProperty("fileSize", (int64)file.getSize());
                                    juce::AudioBuffer<float> buf((int)reader->numChannels, (int)reader->lengthInSamples);
                                    reader->read(&buf, 0, (int)reader->lengthInSamples, 0, true, true);
                                    juce::Array<juce::var> waveform;
                                    const int kThumbPoints = 500;
                                    float* ch0 = buf.getWritePointer(0);
                                    int step = juce::jmax(1, (int)reader->lengthInSamples / kThumbPoints);
                                    for (int si = 0; si < (int)reader->lengthInSamples && waveform.size() < kThumbPoints; si += step)
                                        waveform.add((double)ch0[si]);
                                    resultObj->setProperty("waveform", waveform);
                                }
                                auto& metrics = smartResult.metrics;
                                resultObj->setProperty("snrDb", metrics.snrDb);
                                resultObj->setProperty("jitterPct", metrics.jitterPct);
                                resultObj->setProperty("durationS", metrics.durationS);
                                resultObj->setProperty("dropoutPct", metrics.dropoutPct);
                                resultObj->setProperty("qualityScore", metrics.qualityScore);
                                resultObj->setProperty("qualityLabel", metrics.qualityLabel);
                                resultObj->setProperty("detectedBaudRate", metrics.detectedBaudRate);
                                juce::Array<juce::var> decoderEntries;
                                for (auto& entry : smartResult.decoderResults) {
                                    juce::DynamicObject::Ptr eObj = new juce::DynamicObject();
                                    eObj->setProperty("label", entry.label);
                                    eObj->setProperty("patchCount", entry.patchCount);
                                    eObj->setProperty("rawBytes", entry.rawBytes);
                                    eObj->setProperty("elapsedS", entry.elapsedS);
                                    eObj->setProperty("rank", entry.rank);
                                    eObj->setProperty("duplicates", entry.duplicates);
                                    juce::Array<juce::var> patchesHex;
                                    int maxPatches = std::min(64, entry.patchCount);
                                    for (int pi = 0; pi < maxPatches; ++pi) {
                                        size_t off = (size_t)pi * 18;
                                        if (off + 18 <= entry.validated.size()) {
                                            juce::String hex;
                                            for (int b = 0; b < 18; ++b)
                                                hex += juce::String::formatted("%02X ", (int)entry.validated[off + b]);
                                            patchesHex.add(hex.trimEnd());
                                        }
                                    }
                                    eObj->setProperty("patchesHex", patchesHex);
                                    decoderEntries.add(juce::var(eObj.get()));
                                }
                                resultObj->setProperty("decoderResults", decoderEntries);
                                pendingTapeFile = file;
                                pendingSmartResult = smartResult;
                                selectedDecoderIndex = smartResult.winnerIndex >= 0 ? smartResult.winnerIndex : 0;
                                dispatchToJS("onSmartImportResult", juce::var(resultObj.get()));
                            } else if (ext == ".syx" || ext == ".mid") {
                                // Route to SysEx Smart Import
                                pendingImportFile = file;
                                pendingImportFormat = "sysex";
                                dispatchToJS("onSmartImportProgress", "Analyzing SysEx file: " + file.getFileName());
                                auto parseRes = ABD::JunoSysexImporter::loadFromFile(file);
                                juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                                obj->setProperty("format", "sysex");
                                obj->setProperty("fileName", file.getFileName());
                                obj->setProperty("success", parseRes.result.wasOk());
                                obj->setProperty("detailedFormat", "Roland Juno SysEx");
                                if (parseRes.result.wasOk()) {
                                    int totalPatches = 0;
                                    juce::Array<juce::var> presetNames;
                                    for (auto& lib : parseRes.libraries) {
                                        totalPatches += (int)lib.patches.size();
                                        for (auto& p : lib.patches)
                                            presetNames.add(p.name);
                                    }
                                    obj->setProperty("totalPatches", totalPatches);
                                    obj->setProperty("banksNeeded", (int)parseRes.libraries.size());
                                    obj->setProperty("isSinglePatch", totalPatches <= 1);
                                    obj->setProperty("presetNames", presetNames);
                                    juce::MemoryBlock mb;
                                    file.loadFileAsData(mb);
                                    if (mb.getSize() >= 10) {
                                        const uint8_t* data = (const uint8_t*)mb.getData();
                                        obj->setProperty("deviceId", (int)data[2]);
                                        obj->setProperty("functionCode", (int)data[4]);
                                        if (mb.getSize() > 5) {
                                            int csum = 0;
                                            for (size_t i = 5; i < mb.getSize() - 1; ++i) csum += data[i];
                                            obj->setProperty("checksumValid", (csum & 0x7F) == data[mb.getSize() - 2]);
                                        }
                                        juce::String hex;
                                        size_t previewLen = std::min(mb.getSize(), (size_t)64);
                                        for (size_t i = 0; i < previewLen; ++i)
                                            hex += juce::String::formatted("%02X ", (int)data[i]);
                                        obj->setProperty("hexPreview", hex.trimEnd());
                                    }
                                    dispatchToJS("onSmartImportResult", juce::var(obj.get()));
                                } else {
                                    obj->setProperty("totalPatches", 0);
                                    obj->setProperty("message", parseRes.result.getErrorMessage());
                                    dispatchToJS("onSmartImportResult", juce::var(obj.get()));
                                }
                            } else if (ext == ".csv") {
                                // Route to CSV Smart Import
                                pendingImportFile = file;
                                pendingImportFormat = "csv";
                                dispatchToJS("onSmartImportProgress", "Analyzing CSV file: " + file.getFileName());
                                auto parseRes = ABD::JunoCsvImporter::loadFromFile(file);
                                juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                                obj->setProperty("format", "csv");
                                obj->setProperty("fileName", file.getFileName());
                                obj->setProperty("success", parseRes.result.wasOk());
                                obj->setProperty("detailedFormat", "Juno CSV Presets");
                                if (parseRes.result.wasOk()) {
                                    int totalPatches = (int)parseRes.presets.size();
                                    juce::Array<juce::var> presetNames;
                                    for (auto& p : parseRes.presets)
                                        presetNames.add(p.name);
                                    obj->setProperty("totalPatches", totalPatches);
                                    obj->setProperty("banksNeeded", (totalPatches + 63) / 64);
                                    obj->setProperty("isSinglePatch", totalPatches <= 1);
                                    obj->setProperty("presetNames", presetNames);
                                    juce::StringArray lines;
                                    file.readLines(lines);
                                    if (lines.size() > 0) {
                                        juce::StringArray headers = juce::StringArray::fromTokens(lines[0], ",", "\"");
                                        obj->setProperty("columnCount", (int)headers.size());
                                        juce::Array<juce::var> colNames;
                                        for (auto& h : headers)
                                            colNames.add(h.trim());
                                        obj->setProperty("columnNames", colNames);
                                    }
                                    dispatchToJS("onSmartImportResult", juce::var(obj.get()));
                                } else {
                                    obj->setProperty("totalPatches", 0);
                                    obj->setProperty("message", parseRes.result.getErrorMessage());
                                    dispatchToJS("onSmartImportResult", juce::var(obj.get()));
                                }
                            } else if (ext == ".json" || ext == ".jno") {
                                // Route to JSON Smart Import
                                pendingImportFile = file;
                                pendingImportFormat = "json";
                                dispatchToJS("onSmartImportProgress", "Analyzing JSON file: " + file.getFileName());
                                juce::String content = file.loadFileAsString();
                                juce::var json;
                                auto parseResult = juce::JSON::parse(content, json);
                                juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                                obj->setProperty("format", "json");
                                obj->setProperty("fileName", file.getFileName());
                                obj->setProperty("success", parseResult.wasOk());
                                obj->setProperty("detailedFormat", "JSON Bank");
                                if (parseResult.wasOk()) {
                                    auto* dynObj = json.getDynamicObject();
                                    if (dynObj != nullptr) {
                                        auto name = dynObj->getProperty("name");
                                        if (name.toString().isNotEmpty())
                                            obj->setProperty("libraryName", name.toString());
                                        auto patchesVar = dynObj->getProperty("patches");
                                        auto* patchesArr = patchesVar.getArray();
                                        int totalPatches = patchesArr != nullptr ? patchesArr->size() : 0;
                                        obj->setProperty("totalPatches", totalPatches);
                                        obj->setProperty("banksNeeded", (totalPatches + 63) / 64);
                                        obj->setProperty("isSinglePatch", totalPatches <= 1);
                                        juce::Array<juce::var> presetNames;
                                        if (patchesArr != nullptr) {
                                            for (auto& pVar : *patchesArr) {
                                                if (auto* pObj = pVar.getDynamicObject())
                                                    presetNames.add(pObj->getProperty("name").toString());
                                                else
                                                    presetNames.add("Unnamed");
                                            }
                                        }
                                        obj->setProperty("presetNames", presetNames);
                                        auto cat = dynObj->getProperty("category");
                                        if (cat.toString().isNotEmpty())
                                            obj->setProperty("category", cat.toString());
                                    } else {
                                        obj->setProperty("totalPatches", 1);
                                        obj->setProperty("banksNeeded", 1);
                                        obj->setProperty("isSinglePatch", true);
                                        obj->setProperty("presetNames", juce::Array<juce::var>({file.getFileNameWithoutExtension()}));
                                    }
                                    dispatchToJS("onSmartImportResult", juce::var(obj.get()));
                                } else {
                                    obj->setProperty("totalPatches", 0);
                                    obj->setProperty("message", "Invalid JSON file.");
                                    dispatchToJS("onSmartImportResult", juce::var(obj.get()));
                                }
                            } else {
                                // Other formats (pjunoxl, bin, xml) — direct import
                                auto res = audioProcessor.getPresetManager()->importPresetsFromFile(file);
                                juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                                obj->setProperty("success", res.success);
                                obj->setProperty("message", res.message);
                                dispatchToJS("onImportResult", juce::var(obj.get()));
                                audioProcessor.requestPatchDump();
                            }
                        } else {
                            // Multiple files — direct import for efficiency
                            int successCount = 0;
                            int totalFiles = results.size();
                            juce::String lastError;
                            juce::String finalMessage;
                            for (int i = 0; i < totalFiles; ++i) {
                                auto res = audioProcessor.getPresetManager()->importPresetsFromFile(results[i], (i > 0));
                                if (res.success) {
                                    successCount++;
                                    if (totalFiles == 1) finalMessage = res.message;
                                } else {
                                    lastError = res.message;
                                }
                            }
                            if (totalFiles > 1) {
                                if (successCount == totalFiles)
                                    finalMessage = "Successfully imported " + juce::String(successCount) + " files.";
                                else if (successCount > 0)
                                    finalMessage = "Imported " + juce::String(successCount) + " of " + juce::String(totalFiles) + " files.";
                                else
                                    finalMessage = "Import failed: " + lastError;
                            } else if (successCount == 0) {
                                finalMessage = lastError;
                            }
                            juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                            obj->setProperty("success", successCount > 0);
                            obj->setProperty("message", finalMessage);
                            dispatchToJS("onImportResult", juce::var(obj.get()));
                            audioProcessor.requestPatchDump();
                        }
                    }
                });
        }
        else if (action == "handleImportSysex") {
            // Route through Smart Import modal
            fileChooser = std::make_unique<juce::FileChooser>("Import SysEx / Syx...",
                juce::File(audioProcessor.getPresetManager()->getLastPath()),
                "*.syx;*.mid");
            fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [&audioProcessor, &pendingImportFile, &pendingImportFormat, dispatchToJS](const juce::FileChooser& chooser) {
                    auto result = chooser.getResult();
                    if (result.existsAsFile()) {
                        pendingImportFile = result;
                        pendingImportFormat = "sysex";
                        dispatchToJS("onSmartImportProgress", "Analyzing SysEx file: " + result.getFileName());
                        auto parseRes = ABD::JunoSysexImporter::loadFromFile(result);
                        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                        obj->setProperty("format", "sysex");
                        obj->setProperty("fileName", result.getFileName());
                        obj->setProperty("success", parseRes.result.wasOk());
                        obj->setProperty("detailedFormat", "Roland Juno SysEx");
                        if (parseRes.result.wasOk()) {
                            int totalPatches = 0;
                            juce::Array<juce::var> presetNames;
                            for (auto& lib : parseRes.libraries) {
                                totalPatches += (int)lib.patches.size();
                                for (auto& p : lib.patches)
                                    presetNames.add(p.name);
                            }
                            obj->setProperty("totalPatches", totalPatches);
                            int banksNeeded = (int)parseRes.libraries.size();
                            obj->setProperty("banksNeeded", banksNeeded);
                            obj->setProperty("isSinglePatch", totalPatches <= 1);
                            obj->setProperty("presetNames", presetNames);
                            juce::MemoryBlock mb;
                            result.loadFileAsData(mb);
                            if (mb.getSize() >= 10) {
                                const uint8_t* data = (const uint8_t*)mb.getData();
                                obj->setProperty("deviceId", (int)data[2]);
                                obj->setProperty("functionCode", (int)data[4]);
                                if (mb.getSize() > 5) {
                                    int csum = 0;
                                    for (size_t i = 5; i < mb.getSize() - 1; ++i) csum += data[i];
                                    obj->setProperty("checksumValid", (csum & 0x7F) == data[mb.getSize() - 2]);
                                }
                                juce::String hex;
                                size_t previewLen = std::min(mb.getSize(), (size_t)64);
                                for (size_t i = 0; i < previewLen; ++i)
                                    hex += juce::String::formatted("%02X ", (int)data[i]);
                                obj->setProperty("hexPreview", hex.trimEnd());
                            }
                            dispatchToJS("onSmartImportResult", juce::var(obj.get()));
                        } else {
                            obj->setProperty("totalPatches", 0);
                            obj->setProperty("message", parseRes.result.getErrorMessage());
                            dispatchToJS("onSmartImportResult", juce::var(obj.get()));
                        }
                    }
                });
        }
        else if (action == "handleImportCsv") {
            fileChooser = std::make_unique<juce::FileChooser>("Import CSV...",
                juce::File(audioProcessor.getPresetManager()->getLastPath()),
                "*.csv");
            fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [&audioProcessor, &pendingImportFile, &pendingImportFormat, dispatchToJS](const juce::FileChooser& chooser) {
                    auto result = chooser.getResult();
                    if (result.existsAsFile()) {
                        pendingImportFile = result;
                        pendingImportFormat = "csv";
                        dispatchToJS("onSmartImportProgress", "Analyzing CSV file: " + result.getFileName());
                        auto parseRes = ABD::JunoCsvImporter::loadFromFile(result);
                        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                        obj->setProperty("format", "csv");
                        obj->setProperty("fileName", result.getFileName());
                        obj->setProperty("success", parseRes.result.wasOk());
                        obj->setProperty("detailedFormat", "Juno CSV Presets");
                        if (parseRes.result.wasOk()) {
                            int totalPatches = (int)parseRes.presets.size();
                            juce::Array<juce::var> presetNames;
                            for (auto& p : parseRes.presets)
                                presetNames.add(p.name);
                            obj->setProperty("totalPatches", totalPatches);
                            obj->setProperty("banksNeeded", (totalPatches + 63) / 64);
                            obj->setProperty("isSinglePatch", totalPatches <= 1);
                            obj->setProperty("presetNames", presetNames);
                            juce::StringArray lines;
                            result.readLines(lines);
                            if (lines.size() > 0) {
                                juce::StringArray headers = juce::StringArray::fromTokens(lines[0], ",", "\"");
                                obj->setProperty("columnCount", (int)headers.size());
                                juce::Array<juce::var> colNames;
                                for (auto& h : headers)
                                    colNames.add(h.trim());
                                obj->setProperty("columnNames", colNames);
                            } else {
                                obj->setProperty("columnCount", 0);
                                obj->setProperty("columnNames", juce::Array<juce::var>());
                            }
                            dispatchToJS("onSmartImportResult", juce::var(obj.get()));
                        } else {
                            obj->setProperty("totalPatches", 0);
                            obj->setProperty("message", parseRes.result.getErrorMessage());
                            dispatchToJS("onSmartImportResult", juce::var(obj.get()));
                        }
                    }
                });
        }
        else if (action == "handleImportJson") {
            fileChooser = std::make_unique<juce::FileChooser>("Import JSON / JNO...",
                juce::File(audioProcessor.getPresetManager()->getLastPath()),
                "*.json;*.jno");
            fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [&audioProcessor, &pendingImportFile, &pendingImportFormat, dispatchToJS](const juce::FileChooser& chooser) {
                    auto result = chooser.getResult();
                    if (result.existsAsFile()) {
                        pendingImportFile = result;
                        pendingImportFormat = "json";
                        dispatchToJS("onSmartImportProgress", "Analyzing JSON file: " + result.getFileName());
                        juce::String content = result.loadFileAsString();
                        juce::var json;
                        auto parseResult = juce::JSON::parse(content, json);
                        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                        obj->setProperty("format", "json");
                        obj->setProperty("fileName", result.getFileName());
                        obj->setProperty("success", parseResult.wasOk());
                        obj->setProperty("detailedFormat", "JSON Bank");
                        if (parseResult.wasOk()) {
                            auto* dynObj = json.getDynamicObject();
                            if (dynObj != nullptr) {
                                auto name = dynObj->getProperty("name");
                                if (name.toString().isNotEmpty())
                                    obj->setProperty("libraryName", name.toString());
                                auto patchesVar = dynObj->getProperty("patches");
                                auto* patchesArr = patchesVar.getArray();
                                int totalPatches = patchesArr != nullptr ? patchesArr->size() : 0;
                                obj->setProperty("totalPatches", totalPatches);
                                obj->setProperty("banksNeeded", (totalPatches + 63) / 64);
                                obj->setProperty("isSinglePatch", totalPatches <= 1);
                                juce::Array<juce::var> presetNames;
                                if (patchesArr != nullptr) {
                                    for (auto& pVar : *patchesArr) {
                                        if (auto* pObj = pVar.getDynamicObject())
                                            presetNames.add(pObj->getProperty("name").toString());
                                        else
                                            presetNames.add("Unnamed");
                                    }
                                }
                                obj->setProperty("presetNames", presetNames);
                                auto cat = dynObj->getProperty("category");
                                if (cat.toString().isNotEmpty())
                                    obj->setProperty("category", cat.toString());
                            } else {
                                obj->setProperty("totalPatches", 1);
                                obj->setProperty("banksNeeded", 1);
                                obj->setProperty("isSinglePatch", true);
                                obj->setProperty("presetNames", juce::Array<juce::var>({result.getFileNameWithoutExtension()}));
                            }
                            dispatchToJS("onSmartImportResult", juce::var(obj.get()));
                        } else {
                            obj->setProperty("totalPatches", 0);
                            obj->setProperty("message", "Invalid JSON file.");
                            dispatchToJS("onSmartImportResult", juce::var(obj.get()));
                        }
                    }
                });
        }
        else if (action == "handleSave" || action == "handleExportBank") {
            fileChooser = std::make_unique<juce::FileChooser>("Save patch...",
                juce::File(audioProcessor.getPresetManager()->getLastPath()),
                "*.jno;*.json");
            fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                [&audioProcessor](const juce::FileChooser& chooser) {
                    auto result = chooser.getResult();
                    if (result.exists()) { // allow overwrite
                        audioProcessor.getPresetManager()->exportCurrentPresetToJson(result);
                    } else if (result != juce::File()) {
                        audioProcessor.getPresetManager()->exportCurrentPresetToJson(result.withFileExtension(".jno"));
                    }
                });
        }
        else if (action == "handleLoadTape") {
            fileChooser = std::make_unique<juce::FileChooser>("Load Tape (.wav)...",
                juce::File(audioProcessor.getPresetManager()->getLastPath()),
                "*.wav");
            fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [&audioProcessor, &pendingTapeFile, &pendingSmartResult, &selectedDecoderIndex, dispatchToJS](const juce::FileChooser& chooser) {
                    if (chooser.getResults().size() > 0) {
                        auto file = chooser.getResults()[0];
                        dispatchToJS("onSmartImportProgress", "Cargando archivo WAV...");
                        auto progressFn = [dispatchToJS](const juce::String& msg) {
                            dispatchToJS("onSmartImportProgress", msg);
                        };
                        auto smartResult = JunoTapeDecoder::smartDecode(file, progressFn);
                        juce::AudioFormatManager fmtMgr;
                        fmtMgr.registerBasicFormats();
                        std::unique_ptr<juce::AudioFormatReader> reader(fmtMgr.createReaderFor(file));
                        juce::DynamicObject::Ptr resultObj = new juce::DynamicObject();
                        resultObj->setProperty("fileName", file.getFileName());
                        resultObj->setProperty("filePath", file.getFullPathName());
                        resultObj->setProperty("success", smartResult.success);
                        resultObj->setProperty("autoSelected", smartResult.autoSelected);
                        resultObj->setProperty("winnerIndex", smartResult.winnerIndex);
                        if (reader != nullptr) {
                            resultObj->setProperty("sampleRate", (int)reader->sampleRate);
                            resultObj->setProperty("bitDepth", (int)reader->bitsPerSample);
                            resultObj->setProperty("channels", (int)reader->numChannels);
                            resultObj->setProperty("duration", (double)reader->lengthInSamples / reader->sampleRate);
                            resultObj->setProperty("fileSize", (int64)file.getSize());
                            juce::AudioBuffer<float> buf((int)reader->numChannels, (int)reader->lengthInSamples);
                            reader->read(&buf, 0, (int)reader->lengthInSamples, 0, true, true);
                            juce::Array<juce::var> waveform;
                            const int kThumbPoints = 500;
                            float* ch0 = buf.getWritePointer(0);
                            int step = juce::jmax(1, (int)reader->lengthInSamples / kThumbPoints);
                            for (int si = 0; si < (int)reader->lengthInSamples && waveform.size() < kThumbPoints; si += step)
                                waveform.add((double)ch0[si]);
                            resultObj->setProperty("waveform", waveform);
                        }
                        auto& metrics = smartResult.metrics;
                        resultObj->setProperty("snrDb", metrics.snrDb);
                        resultObj->setProperty("jitterPct", metrics.jitterPct);
                        resultObj->setProperty("durationS", metrics.durationS);
                        resultObj->setProperty("dropoutPct", metrics.dropoutPct);
                        resultObj->setProperty("qualityScore", metrics.qualityScore);
                        resultObj->setProperty("qualityLabel", metrics.qualityLabel);
                        resultObj->setProperty("detectedBaudRate", metrics.detectedBaudRate);
                        juce::Array<juce::var> decoderEntries;
                        for (auto& entry : smartResult.decoderResults) {
                            juce::DynamicObject::Ptr eObj = new juce::DynamicObject();
                            eObj->setProperty("label", entry.label);
                            eObj->setProperty("patchCount", entry.patchCount);
                            eObj->setProperty("rawBytes", entry.rawBytes);
                            eObj->setProperty("elapsedS", entry.elapsedS);
                            eObj->setProperty("rank", entry.rank);
                            eObj->setProperty("duplicates", entry.duplicates);
                            juce::Array<juce::var> patchesHex;
                            int maxPatches = std::min(64, entry.patchCount);
                            for (int pi = 0; pi < maxPatches; ++pi) {
                                size_t off = (size_t)pi * 18;
                                if (off + 18 <= entry.validated.size()) {
                                    juce::String hex;
                                    for (int b = 0; b < 18; ++b)
                                        hex += juce::String::formatted("%02X ", (int)entry.validated[off + b]);
                                    patchesHex.add(hex.trimEnd());
                                }
                            }
                            eObj->setProperty("patchesHex", patchesHex);
                            decoderEntries.add(juce::var(eObj.get()));
                        }
                        resultObj->setProperty("decoderResults", decoderEntries);
                        pendingTapeFile = file;
                        pendingSmartResult = smartResult;
                        selectedDecoderIndex = smartResult.winnerIndex >= 0 ? smartResult.winnerIndex : 0;
                        dispatchToJS("onSmartImportResult", juce::var(resultObj.get()));
                    }
                });
        }
        else if (action == "handleSaveTape") {
            fileChooser = std::make_unique<juce::FileChooser>("Save Tape (.wav)...",
                juce::File(audioProcessor.getPresetManager()->getLastPath()),
                "*.wav");
            fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                [&audioProcessor](const juce::FileChooser& chooser) {
                    auto result = chooser.getResult();
                    if (result != juce::File()) {
                        auto file = result.withFileExtension(".wav");
                        auto* alert = new juce::AlertWindow("Save Tape", "Select the cassette tape format to export:", juce::MessageBoxIconType::QuestionIcon);
                        alert->addButton("Juno-106 (1200 Baud)", 106);
                        alert->addButton("Juno-60 (340 Baud)", 60);
                        alert->addButton("Cancel", 0);
                        alert->enterModalState(true, juce::ModalCallbackFunction::create([file, alert, &audioProcessor](int choice) {
                            if (choice == 106) {
                                audioProcessor.getPresetManager()->exportCurrentPresetToTape(file, JunoTapeEncoder::Juno106);
                            } else if (choice == 60) {
                                audioProcessor.getPresetManager()->exportCurrentPresetToTape(file, JunoTapeEncoder::Juno60);
                            }
                            delete alert;
                        }), true);
                    }
                });
        }
        else if (action == "bank-inc" || action == "handleBankInc") {
            if (auto* pm = audioProcessor.getPresetManager()) {
                pm->nextBank();
                audioProcessor.loadLibraryPreset(pm->getCurrentLibraryIndex(), pm->getCurrentPresetIndex());
            }
        }
        else if (action == "bank-dec" || action == "handleBankDec") {
            if (auto* pm = audioProcessor.getPresetManager()) {
                pm->prevBank();
                audioProcessor.loadLibraryPreset(pm->getCurrentLibraryIndex(), pm->getCurrentPresetIndex());
            }
        }
        else if (action == "patch-inc" || action == "handlePatchInc") {
            if (auto* pm = audioProcessor.getPresetManager()) {
                pm->nextPatch();
                audioProcessor.loadLibraryPreset(pm->getCurrentLibraryIndex(), pm->getCurrentPresetIndex());
            }
        }
        else if (action == "patch-dec" || action == "handlePatchDec") {
            if (auto* pm = audioProcessor.getPresetManager()) {
                pm->prevPatch();
                audioProcessor.loadLibraryPreset(pm->getCurrentLibraryIndex(), pm->getCurrentPresetIndex());
            }
        }
        else if (action == "handleSavePreset") {
            if (auto* pm = audioProcessor.getPresetManager()) {
                auto res = pm->saveCurrentPresetFromState(audioProcessor.getAPVTS());
                if (res.failed()) dispatchToJS("alert", res.getErrorMessage());
                else {
                    dispatchToJS("alert", "PRESET SAVED");
                    sendPresetListUpdate();
                }
            }
        }
        else if (action == "handleSavePresetAs") {
            if (auto* pm = audioProcessor.getPresetManager()) {
                juce::String newName;
                if (args.size() > 1) newName = args[1].toString();
                else newName = pm->getCurrentPreset().name + " Copy";

                auto res = pm->saveAsNewPresetFromState(audioProcessor.getAPVTS(), newName);
                if (res.failed()) dispatchToJS("alert", res.getErrorMessage());
                else {
                    dispatchToJS("alert", "PRESET SAVED AS " + newName);
                    sendPresetListUpdate();
                }
            }
        }
        else if (action == "handleLoadTuning") {
            fileChooser = std::make_unique<juce::FileChooser>("Load Scala Tuning (.scl)...",
                juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                "*.scl");
            fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [&audioProcessor, dispatchToJS](const juce::FileChooser& fc) {
                auto result = fc.getResult();
                if (result.existsAsFile()) {
                    bool ok = audioProcessor.loadScalaTuning(result);
                    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                    obj->setProperty("ok", ok);
                    obj->setProperty("name", result.getFileNameWithoutExtension());
                    dispatchToJS("onTuningLoaded", juce::var(obj.get()));
                }
            });
        }
        else if (action == "handleResetTuning") {
            audioProcessor.resetTuning();
            dispatchToJS("onTuningLoaded", juce::var("reset"));
        }
        else if (action == "showBrowser") {
            dispatchToJS("showModal", "browser");
        }
        else if (action == "setUserName") {
            if (args.size() >= 2) audioProcessor.setUserName(args[1].toString());
        }
        else if (action == "getUserName") {
            completion(audioProcessor.getUserName());
            return;
        }
        else if (action == "writeToInternalSlot") {
            if (args.size() >= 2) {
                int slot = (int)args[1];
                juce::String name = args.size() >= 3 ? args[2].toString() : "";
                juce::String author = args.size() >= 4 ? args[3].toString() : "";

                int group = slot / 64;
                int rem = slot % 64;
                int bank = (rem / 8) + 1;
                int patch = (rem % 8) + 1;
                if (auto* pm = audioProcessor.getPresetManager()) {
                    pm->writeToInternalSlot(group, bank, patch, audioProcessor.getAPVTS().copyState(), name, author);
                    sendPresetListUpdate();
                }
            }
        }
    }
    completion({});
}

} // namespace BridgeMenu
