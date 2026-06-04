import sys
import os

# Find the project root
script_dir = os.path.dirname(os.path.abspath(__file__))
project_root = os.path.dirname(script_dir)
cpp_path = os.path.join(project_root, "Source", "UI", "WebView", "WebViewEditor.cpp")

with open(cpp_path, 'r', encoding='utf-8') as f:
    content = f.read()

old_func = '''        .withNativeFunction ("confirmImportFile", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            juce::ignoreUnused(args);
            
            if (pendingImportFile.existsAsFile()) {
                auto res = audioProcessor.getPresetManager()->importPresetsFromFile(pendingImportFile);
                
                juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                obj->setProperty("success", res.success);
                obj->setProperty("message", res.message);
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
        })'''

new_func = '''        .withNativeFunction ("confirmImportFile", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
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
                                auto libName = dynObj->getProperty("name", "JSON Import").toString();
                                if (patchesArr != nullptr) {
                                    auto* pm = audioProcessor.getPresetManager();
                                    int targetIdx = emptyIndices[0];
                                    auto& lib = pm->getLibrary(targetIdx);
                                    for (int i = 0; i < totalPatches && i < 64; ++i) {
                                        auto& pVar = (*patchesArr)[i];
                                        if (auto* pObj = pVar.getDynamicObject()) {
                                            ABD::Preset preset;
                                            preset.name = pObj->getProperty("name", "Unnamed").toString();
                                            preset.author = pObj->getProperty("author", "").toString();
                                            preset.category = pObj->getProperty("category", "User").toString();
                                            preset.tags = pObj->getProperty("tags", "").toString();
                                            preset.notes = pObj->getProperty("notes", "").toString();
                                            preset.isFavorite = (bool)pObj->getProperty("favorite", false);
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
        })'''

if old_func in content:
    content = content.replace(old_func, new_func, 1)
    with open(cpp_path, 'w', encoding='utf-8') as f:
        f.write(content)
    print("EDIT_OK: confirmImportFile updated with JSON support")
else:
    print("ERROR: Could not find old confirmImportFile function in file")
    # Debug: find the function
    idx = content.find("confirmImportFile")
    if idx >= 0:
        print(f"  Found at index {idx}")
        print(f"  Context: {content[idx-50:idx+200]}")
    sys.exit(1)
