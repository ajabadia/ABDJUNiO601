#include "BridgeImport.h"
#include "../../Core/ABDSimpleJuno106AudioProcessor.h"
#include "../../Core/JunoTapeDecoder.h"
#include "../../Core/PresetManager.h"

namespace BridgeImport {

void confirmImportFile(ABDSimpleJuno106AudioProcessor& audioProcessor,
                       juce::File& pendingImportFile,
                       juce::String& pendingImportFormat,
                       const juce::Array<juce::var>& args,
                       const std::function<void(const juce::String&, const juce::var&)>& dispatchToJS,
                       const std::function<void()>& sendPresetListUpdate,
                       const std::function<void()>& requestPatchDump,
                       juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    juce::ignoreUnused(args);

    if (pendingImportFile.existsAsFile()) {
        bool success = false;
        juce::String message;

        if (pendingImportFormat == "json") {
            juce::String content = pendingImportFile.loadFileAsString();
            juce::var json;
            auto parseResult = juce::JSON::parse(content, json);
            if (parseResult.wasOk()) {
                auto* dynObj = json.getDynamicObject();
                if (dynObj != nullptr) {
                    auto patchesVar = dynObj->getProperty("patches");
                    auto* patchesArr = patchesVar.getArray();
                    int totalPatches = patchesArr != nullptr ? patchesArr->size() : 0;
                    int banksNeeded = (totalPatches + 63) / 64;
                    if (banksNeeded == 0) banksNeeded = 1;
                    auto emptyIndices = audioProcessor.getPresetManager()->findEmptyBankIndices(banksNeeded);
                    if ((int)emptyIndices.size() >= banksNeeded) {
                        auto libName = dynObj->getProperty("name").toString();
                        if (patchesArr != nullptr) {
                            auto* pm = audioProcessor.getPresetManager();
                            int targetIdx = emptyIndices[0];
                            auto& lib = pm->getLibrary(targetIdx);
                            for (int i = 0; i < totalPatches && i < 64; ++i) {
                                auto& pVar = (*patchesArr)[i];
                                if (auto* pObj = pVar.getDynamicObject()) {
                                    ABD::Preset preset;
                                    preset.name = pObj->getProperty("name").toString();
                                    preset.author = pObj->getProperty("author").toString();
                                    preset.category = pObj->getProperty("category").toString();
                                    preset.tags = pObj->getProperty("tags").toString();
                                    preset.notes = pObj->getProperty("notes").toString();
                                    preset.isFavorite = (bool)pObj->getProperty("favorite");
                                    auto dataVar = pObj->getProperty("data");
                                    if (dataVar.isObject()) {
                                        juce::ValueTree paramsVT("Parameters");
                                        if (auto* dObj = dataVar.getDynamicObject()) {
                                            for (auto& prop : dObj->getProperties())
                                                paramsVT.setProperty(prop.name, prop.value, nullptr);
                                        }
                                        preset.state = paramsVT;
                                    } else {
                                        preset.state = pm->bytesToState(nullptr, 0);
                                    }
                                    preset.originGroup = targetIdx;
                                    preset.originBank = (i / 8) + 1;
                                    preset.originPatch = (i % 8) + 1;
                                    lib.patches[i] = preset;
                                }
                            }
                            for (int i = totalPatches; i < 64; ++i) {
                                ABD::Preset init;
                                init.name = "INIT PATCH";
                                init.state = pm->bytesToState(nullptr, 0);
                                lib.patches[i] = init;
                            }
                            lib.name = juce::String::charToString((juce_wchar)('A' + targetIdx)) + " - " + libName;
                            pm->saveBrowserData();
                            success = true;
                            message = "Imported " + juce::String(totalPatches) + " patches into bank " + juce::String::charToString((juce_wchar)('A' + targetIdx));
                        }
                    } else {
                        message = "Error: Not enough empty banks available.";
                    }
                } else {
                    auto res = audioProcessor.getPresetManager()->importPresetsFromFile(pendingImportFile);
                    success = res.success;
                    message = res.message;
                }
            } else {
                message = "Invalid JSON format.";
            }
        } else {
            auto res = audioProcessor.getPresetManager()->importPresetsFromFile(pendingImportFile);
            success = res.success;
            message = res.message;
        }

        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty("success", success);
        obj->setProperty("message", message);
        dispatchToJS("onImportResult", juce::var(obj.get()));

        pendingImportFile = juce::File();
        pendingImportFormat = "";
        audioProcessor.requestPatchDump();
    } else {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty("success", false);
        obj->setProperty("message", "No pending import file.");
        completion(juce::var(obj.get()));
        return;
    }
    completion(juce::var::undefined());
}

void confirmTapeImport(ABDSimpleJuno106AudioProcessor& audioProcessor,
                       juce::File& pendingTapeFile,
                       JunoTapeDecoder::SmartDecodeResult& pendingSmartResult,
                       int& selectedDecoderIndex,
                       const juce::Array<juce::var>& args,
                       const std::function<void(const juce::String&, const juce::var&)>& dispatchToJS,
                       juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    int decoderIdx = -1;
    if (args.size() >= 1) decoderIdx = (int)args[0];

    if (pendingTapeFile.existsAsFile() && !pendingSmartResult.decoderResults.empty()) {
        int idx = (decoderIdx >= 0 && decoderIdx < (int)pendingSmartResult.decoderResults.size())
            ? decoderIdx : pendingSmartResult.winnerIndex;
        if (idx < 0) idx = 0;

        auto& entry = pendingSmartResult.decoderResults[idx];
        bool success = !entry.validated.empty();
        juce::String message;

        if (success) {
            auto& data = entry.validated;
            int baud = pendingSmartResult.metrics.detectedBaudRate;
            JunoTapeDecoder::DecodeResult decodeRes;
            decodeRes.data = data;
            decodeRes.detectedBaudRate = baud;
            decodeRes.success = true;
            auto importRes = audioProcessor.getPresetManager()->loadTapeFromData(pendingTapeFile, decodeRes);
            success = importRes.success;
            message = importRes.message;
        } else {
            message = "No patches available in selected decoder result.";
        }

        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty("success", success);
        obj->setProperty("message", message);
        dispatchToJS("onImportResult", juce::var(obj.get()));

        pendingTapeFile = juce::File();
        pendingSmartResult = JunoTapeDecoder::SmartDecodeResult();
    } else {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty("success", false);
        obj->setProperty("message", "No pending smart decode result. Please select a tape file first.");
        completion(juce::var(obj.get()));
        return;
    }
    completion(juce::var::undefined());
}

} // namespace BridgeImport
