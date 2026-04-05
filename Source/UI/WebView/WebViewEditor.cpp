#include <JuceHeader.h>
#include "WebViewEditor.h"
#include "../../Core/ABDSimpleJuno106AudioProcessor.h"
#include "../../Core/CalibrationSettings.h"
#include "../../Core/ServiceModeManager.h"
#include "../../Core/PresetManager.h"
#include "../../Core/BuildVersion.h"
#include <optional>



WebViewEditor::WebViewEditor (ABDSimpleJuno106AudioProcessor& p)
    : juce::AudioProcessorEditor (&p), audioProcessor (p)
{
    auto options = juce::WebBrowserComponent::Options{}
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
        .withInitialisationData ("buildVersion", juce::String(JUNO_BUILD_VERSION))
        .withInitialisationData ("buildTimestamp", juce::String(JUNO_BUILD_TIMESTAMP))
        .withNativeIntegrationEnabled (true)
        .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2()
            .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ABD_JUNiO_601_WebView2")))
        .withResourceProvider ([this] (const juce::String& url) -> std::optional<juce::WebBrowserComponent::Resource> {
            juce::String path = url;
            if (path == "/" || path.isEmpty()) path = "/index.html";
            if (path.startsWith("/")) path = path.substring(1);
            
            juce::File webUiDir = juce::File("d:\\desarrollos\\ABDJUNiO601\\Source\\UI\\WebUI");
            if (!webUiDir.exists()) {
                juce::File exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
                webUiDir = exeDir.getChildFile("WebUI");
            }

            juce::File file = webUiDir.getChildFile(path.replace("/", "\\"));
            
            auto getMimeType = [](const juce::String& filename) {
                if (filename.endsWithIgnoreCase(".html")) return "text/html";
                if (filename.endsWithIgnoreCase(".css"))  return "text/css";
                if (filename.endsWithIgnoreCase(".js"))   return "application/javascript";
                if (filename.endsWithIgnoreCase(".png"))  return "image/png";
                if (filename.endsWithIgnoreCase(".svg"))  return "image/svg+xml";
                if (filename.endsWithIgnoreCase(".ttf"))  return "font/ttf";
                if (filename.endsWithIgnoreCase(".woff")) return "font/woff";
                if (filename.endsWithIgnoreCase(".woff2")) return "font/woff2";
                return "application/octet-stream";
            };

            if (file.existsAsFile())
            {
                juce::MemoryBlock mb;
                file.loadFileAsData(mb);
                std::vector<std::byte> data(mb.getSize());
                std::memcpy(data.data(), mb.getData(), mb.getSize());
                return juce::WebBrowserComponent::Resource { std::move(data), getMimeType(file.getFileName()) };
            }

            auto getResourceFromBinary = [getMimeType] (const char* data, int size, const juce::String& filename) -> std::optional<juce::WebBrowserComponent::Resource> {
                if (data == nullptr) return std::nullopt;
                std::vector<std::byte> bytes (size);
                std::memcpy (bytes.data(), data, (size_t)size);
                return juce::WebBrowserComponent::Resource { std::move (bytes), getMimeType(filename) };
            };

            if (path == "index.html")   return getResourceFromBinary (BinaryData::index_html, BinaryData::index_htmlSize, "index.html");
            if (path == "script.js")    return getResourceFromBinary (BinaryData::script_js, BinaryData::script_jsSize, "script.js");
            if (path == "service.js")   return getResourceFromBinary (BinaryData::service_js, BinaryData::service_jsSize, "service.js");
            if (path == "service.css")  return getResourceFromBinary (BinaryData::service_css, BinaryData::service_cssSize, "service.css");
            if (path == "style.css")    return getResourceFromBinary (BinaryData::style_css, BinaryData::style_cssSize, "style.css");

            juce::String resourceName = path.replace("/", "_").replace(".", "_").replace("-", "_").replace(" ", "_");
            int binSize = 0;
            const char* binData = BinaryData::getNamedResource(resourceName.toRawUTF8(), binSize);
            
            // Fallback for numeric mangling (e.g. 0.png -> _0_png)
            if (binData == nullptr) {
                juce::String filename = path.fromLastOccurrenceOf("/", false, false);
                if (filename.isEmpty()) filename = path;
                juce::String flattenedName = filename.replace(".", "_").replace("-", "_").replace(" ", "_");
                if (juce::CharacterFunctions::isDigit(flattenedName[0])) flattenedName = "_" + flattenedName;
                binData = BinaryData::getNamedResource(flattenedName.toRawUTF8(), binSize);
            }

            if (binData != nullptr) return getResourceFromBinary(binData, binSize, path);

            return std::nullopt;
        })
        .withNativeFunction ("setParameter", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 2) {
                juce::String paramID = args[0].toString();
                float val = (float)args[1];
                juce::Logger::writeToLog("[JUNiO] setParameter JS CALL: ID=" + paramID + ", Value=" + juce::String(val));
                if (auto* param = audioProcessor.getAPVTS().getParameter(paramID))
                {
                    param->setValueNotifyingHost((float)val);
                    completion (juce::var::undefined());
                }
                else
                {
                    juce::Logger::writeToLog("[JUNiO] setParameter ERROR: Parameter not found: " + paramID);
                    completion({});
                }
            }
            else
            {
                completion({});
            }
        })
        .withNativeFunction ("getCalibrationParams", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            juce::ignoreUnused(args);
            juce::Array<juce::var> result;
            auto& cal = audioProcessor.getCalibrationSettings();
            for (const auto& p : cal.getAllParams()) {
                juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                obj->setProperty("id", juce::String(p.id));
                obj->setProperty("label", juce::String(p.label));
                obj->setProperty("category", juce::String(p.category));
                obj->setProperty("unit", juce::String(p.unit));
                obj->setProperty("tooltip", juce::String(p.tooltip));
                obj->setProperty("defaultValue", (double)p.defaultValue);
                obj->setProperty("currentValue", (double)p.currentValue);
                obj->setProperty("minValue", (double)p.minValue);
                obj->setProperty("maxValue", (double)p.maxValue);
                obj->setProperty("stepSize", (double)p.stepSize);
                result.add(juce::var(obj.get()));
            }
            completion(result);
        })
        .withNativeFunction ("setCalibrationParam", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 2) {
                juce::String id = args[0].toString();
                float val = (float)args[1];
                audioProcessor.getCalibrationSettings().setValue(id.toStdString(), val);
            }
            completion({});
        })
        .withNativeFunction ("serviceAction", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
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
                    else if (action == "resetParam") audioProcessor.getCalibrationSettings().resetParam(obj->getProperty("id").toString().toStdString());
                    else if (action == "resetCategory") audioProcessor.getCalibrationSettings().resetCategory(obj->getProperty("category").toString().toStdString());
                    else if (action == "exportCalibration") {
                        fileChooser = std::make_unique<juce::FileChooser>("Export Calibration JSON", 
                                                                          juce::File::getSpecialLocation(juce::File::userHomeDirectory), 
                                                                          "*.json");
                                                                          
                        fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles, 
                        [this](const juce::FileChooser& fc) {
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
                        [this](const juce::FileChooser& fc) {
                            auto result = fc.getResult();
                            if (result.existsAsFile()) {
                                audioProcessor.getCalibrationSettings().loadFromPath(result.getFullPathName().toStdString());
                                juce::Logger::writeToLog("[JUNiO] Calibration Imported successfully from: " + result.getFullPathName());
                                // Update UI by triggering a re-read of params if necessary, 
                                // though the manager usually notifies listeners.
                            }
                        });
                    }
                    else if (action == "hpfCycle") {
                        smm.startHpfCycle();
                    }
                    else if (action == "chorusCycle") {
                        smm.startChorusCycle();
                    }
                    else if (action == "autoTuneVCF") {
                        smm.startAutoVcfTune();
                    }
                }
            }
            completion({});
        })
        .withNativeFunction ("loadPreset", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 1) {
                int index = (int)args[0];
                audioProcessor.loadPreset(index);
            }
            completion({});
        })
        .withNativeFunction ("loadLibraryPreset", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 2) {
                int libIdx = (int)args[0];
                int prstIdx = (int)args[1];
                audioProcessor.loadLibraryPreset(libIdx, prstIdx);
            }
            completion({});
        })
        .withNativeFunction ("menuAction", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
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
                else if (action == "uiReady") {
                    writeLog("[JUNiO] UI READY - Performing full state dump");
                    sendPresetListUpdate();
                    if (auto* pm = audioProcessor.getPresetManager()) {
                        const auto& pr = pm->getCurrentPreset();
                        sendBankPatchUpdate(pr.originGroup, pr.originBank, pr.originPatch);
                        
                        juce::String lcdString = "P: " + juce::String(audioProcessor.getCurrentProgram() + 1) + " " + audioProcessor.getProgramName(audioProcessor.getCurrentProgram());
                        dispatchToJS ("onLCDUpdate", lcdString);
                    }
                }
                else if (action == "exit") {
                    if (auto* app = juce::JUCEApplication::getInstance())
                        app->systemRequestedQuit();
                }
                else if (action == "handleLoad" || action == "handleImportSysex") {
                    fileChooser = std::make_unique<juce::FileChooser>("Load patch or bank...", 
                        juce::File(audioProcessor.getPresetManager()->getLastPath()),
                        "*.jno;*.syx;*.wav;*.bin");
                    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                        [this](const juce::FileChooser& chooser) {
                            auto result = chooser.getResult();
                            if (result.existsAsFile()) {
                                audioProcessor.getPresetManager()->importPresetsFromFile(result);
                                audioProcessor.requestPatchDump();
                            }
                        });
                }
                else if (action == "handleSave" || action == "handleExportBank") {
                    fileChooser = std::make_unique<juce::FileChooser>("Save patch...", 
                        juce::File(audioProcessor.getPresetManager()->getLastPath()),
                        "*.jno;*.json");
                    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                        [this](const juce::FileChooser& chooser) {
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
                        [this](const juce::FileChooser& chooser) {
                            if (chooser.getResults().size() > 0)
                                audioProcessor.getPresetManager()->loadTape(chooser.getResults()[0]);
                        });
                }
                else if (action == "handleSaveTape") {
                    fileChooser = std::make_unique<juce::FileChooser>("Save Tape (.wav)...", 
                        juce::File(audioProcessor.getPresetManager()->getLastPath()),
                        "*.wav");
                    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                        [this](const juce::FileChooser& chooser) {
                            auto result = chooser.getResult();
                            if (result != juce::File()) {
                                audioProcessor.getPresetManager()->exportCurrentPresetToTape(result.withFileExtension(".wav"));
                            }
                        });
                }
                else if (action == "bank-inc" || action == "handleBankInc") {
                    int next = std::min(127, audioProcessor.getCurrentProgram() + 8);
                    audioProcessor.setCurrentProgram(next);
                }
                else if (action == "bank-inc" || action == "handleBankInc") {
                    if (auto* pm = audioProcessor.getPresetManager()) pm->nextBank();
                }
                else if (action == "bank-dec" || action == "handleBankDec") {
                    if (auto* pm = audioProcessor.getPresetManager()) pm->prevBank();
                }
                else if (action == "patch-inc" || action == "handlePatchInc") {
                    if (auto* pm = audioProcessor.getPresetManager()) pm->nextPatch();
                }
                else if (action == "patch-dec" || action == "handlePatchDec") {
                    if (auto* pm = audioProcessor.getPresetManager()) pm->prevPatch();
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
                else if (action == "showBrowser") {
                    dispatchToJS("showModal", "browser");
                }
            }
            completion({});
        })
        .withNativeFunction ("setBrowserData", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 1) {
                if (auto* pm = audioProcessor.getPresetManager()) {
                    // Use the var directly if it's already an object (JUCE 8 handles this)
                    const auto& var = args[0];
                    
                    juce::ValueTree root("BankManager");
                    auto* obj = var.getDynamicObject();
                    if (obj) {
                        // Extract categories
                        auto catsVar = obj->getProperty("categories");
                        if (auto* catsArr = catsVar.getArray()) {
                            juce::String catsCsv;
                            for (auto& cat : *catsArr) {
                                if (catsCsv.isNotEmpty()) catsCsv += ",";
                                catsCsv += cat.toString();
                            }
                            root.setProperty("categories", catsCsv, nullptr);
                        }

                        auto libsVar = obj->getProperty("libraries");
                        if (auto* libsArr = libsVar.getArray()) {
                            for (auto& libVar : *libsArr) {
                                if (auto* lObj = libVar.getDynamicObject()) {
                                    juce::ValueTree libVT("Library");
                                    libVT.setProperty("name", lObj->getProperty("name"), nullptr);
                                    
                                    auto patchesVar = lObj->getProperty("patches");
                                    if (auto* pArr = patchesVar.getArray()) {
                                        for (auto& pVar : *pArr) {
                                            if (auto* pObj = pVar.getDynamicObject()) {
                                                juce::ValueTree pVT("Preset");
                                                pVT.setProperty("name", pObj->getProperty("name"), nullptr);
                                                pVT.setProperty("category", pObj->getProperty("category"), nullptr);
                                                pVT.setProperty("author", pObj->getProperty("author"), nullptr);
                                                pVT.setProperty("tags", pObj->getProperty("tags"), nullptr);
                                                pVT.setProperty("notes", pObj->getProperty("notes"), nullptr);
                                                pVT.setProperty("date", pObj->getProperty("date"), nullptr);
                                                pVT.setProperty("favorite", pObj->getProperty("favorite"), nullptr);
                                                
                                                // Handle nested "data" or "state"
                                                auto dataVar = pObj->getProperty("data");
                                                if (dataVar.isObject()) {
                                                    // Convert to Parameters ValueTree
                                                    juce::ValueTree paramsVT("Parameters");
                                                    if (auto* dObj = dataVar.getDynamicObject()) {
                                                        for (auto& prop : dObj->getProperties())
                                                            paramsVT.setProperty(prop.name, prop.value, nullptr);
                                                    }
                                                    pVT.addChild(paramsVT, -1, nullptr);
                                                }
                                                libVT.addChild(pVT, -1, nullptr);
                                            }
                                        }
                                    }
                                    root.addChild(libVT, -1, nullptr);
                                }
                            }
                        }
                    }
                    pm->fromValueTree(root);
                    juce::Logger::writeToLog("[JUNiO] Browser Data Synced to C++ Engine");
                }
            }
            completion(juce::var::undefined());
        })
        .withNativeFunction ("getSynthState", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            juce::ignoreUnused(args);
            juce::DynamicObject::Ptr state = new juce::DynamicObject();
            for (auto* param : audioProcessor.getParameters()) {
                if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param)) {
                    state->setProperty(juce::Identifier(p->getParameterID()), (double)p->getValue());
                }
            }
            completion(juce::var(state.get()));
        })
        .withNativeFunction ("getBrowserData", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            juce::ignoreUnused(args);
            if (auto* pm = audioProcessor.getPresetManager()) {
                juce::DynamicObject::Ptr root = new juce::DynamicObject();
                juce::Array<juce::var> libs;
                for (int i=0; i < pm->getNumLibraries(); ++i) {
                    auto& lib = pm->getLibrary(i);
                    juce::DynamicObject::Ptr lObj = new juce::DynamicObject();
                    lObj->setProperty("name", lib.name);
                    lObj->setProperty("index", i);
                    juce::Array<juce::var> patches;
                    for (int j=0; j < (int)lib.patches.size(); ++j) {
                        auto& p = lib.patches[j];
                        juce::DynamicObject::Ptr pObj = new juce::DynamicObject();
                        pObj->setProperty("name", p.name);
                        pObj->setProperty("category", p.category);
                        pObj->setProperty("author", p.author);
                        pObj->setProperty("tags", p.tags);
                        pObj->setProperty("notes", p.notes);
                        pObj->setProperty("date", p.creationDate);
                        pObj->setProperty("favorite", p.isFavorite);
                        pObj->setProperty("index", j);

                        // Send patch data
                        juce::DynamicObject::Ptr dObj = new juce::DynamicObject();
                        const auto& state = p.state;
                        if (state.isValid()) {
                            for (int k=0; k < state.getNumProperties(); ++k) {
                                auto propName = state.getPropertyName(k).toString();
                                dObj->setProperty(propName, (double)state.getProperty(propName));
                            }
                        }
                        pObj->setProperty("data", juce::var(dObj.get()));

                        patches.add(juce::var(pObj.get()));
                    }
                    lObj->setProperty("patches", patches);
                    libs.add(juce::var(lObj.get()));
                }
                root->setProperty("libraries", libs);
                
                // Send categories
                juce::Array<juce::var> cats;
                for (const auto& c : pm->categories_) cats.add(c);
                root->setProperty("categories", cats);

                completion(juce::var(root.get()));
            } else {
                completion({});
            }
        })
        .withNativeFunction ("setFavorite", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 3) {
                if (auto* pm = audioProcessor.getPresetManager())
                    pm->setFavorite((int)args[0], (int)args[1], (bool)args[2]);
            }
            completion({});
        })
        .withNativeFunction ("updateMetadata", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 6) {
                if (auto* pm = audioProcessor.getPresetManager())
                    pm->updateMetadata((int)args[0], (int)args[1], args[2].toString(), args[3].toString(), args[4].toString(), args[5].toString());
            }
            completion({});
        })
        .withNativeFunction ("setBrowserData", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 1) {
                if (auto* pm = audioProcessor.getPresetManager()) {
                    // Simple path for now: Trigger a full save from internal state
                    // If JS sent us data, it was likely after updateMetadata was already called individuallly.
                    pm->saveBrowserData();
                }
            }
            completion({});
        })
        .withNativeFunction ("exportBank", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 1) {
                auto libObj = args[0];
                fileChooser = std::make_unique<juce::FileChooser>("Export Bank as JSON...", 
                    juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.json");
                
                fileChooser->launchAsync(juce::FileBrowserComponent::saveMode, [this, libObj](const juce::FileChooser& fc) {
                    auto result = fc.getResult();
                    if (result != juce::File()) {
                        // We could call PresetManager::exportLibraryToJson, 
                        // but JS already sent us the library data.
                        result.replaceWithText(juce::JSON::toString(libObj));
                    }
                });
            }
            completion({});
        })
        .withNativeFunction ("importBank", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            juce::ignoreUnused(args);
            fileChooser = std::make_unique<juce::FileChooser>("Import Bank JSON...", 
                juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.json");
            
            // Completion needs to stay alive for async
            auto safeCompletion = std::make_shared<juce::WebBrowserComponent::NativeFunctionCompletion>(std::move(completion));
            
            fileChooser->launchAsync(juce::FileBrowserComponent::openMode, [this, safeCompletion](const juce::FileChooser& fc) {
                auto result = fc.getResult();
                if (result.existsAsFile()) {
                    juce::var json;
                    if (juce::JSON::parse(result.loadFileAsString(), json).wasOk()) {
                        (*safeCompletion)(json);
                        return;
                    }
                }
                // Important: Always complete signal
                (*safeCompletion)(juce::var::undefined());
            });
        })
        .withNativeFunction ("savePresetDetailed", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 2) {
                if (auto* pm = audioProcessor.getPresetManager()) {
                    pm->selectPreset((int)args[0], (int)args[1]);
                    auto res = pm->saveCurrentPresetFromState(audioProcessor.getAPVTS());
                    if (res.failed()) dispatchToJS("alert", res.getErrorMessage());
                    else {
                        sendPresetListUpdate();
                    }
                }
            }
            completion({});
        })
        .withNativeFunction ("saveAsNewPresetDetailed", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 5) {
                if (auto* pm = audioProcessor.getPresetManager()) {
                    auto res = pm->saveAsNewPresetFromState(audioProcessor.getAPVTS(), 
                        args[0].toString(), // Name
                        args[1].toString(), // Category
                        args[2].toString(), // Author
                        args[3].toString(), // Tags
                        args[4].toString()  // Notes
                    );
                    if (res.failed()) dispatchToJS("alert", res.getErrorMessage());
                    else {
                        sendPresetListUpdate();
                    }
                }
            }
            completion({});
        })
        .withNativeFunction ("pianoNoteOn", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 2) audioProcessor.keyboardState.noteOn(1, (int)args[0], (float)args[1]);
            completion({});
        })
        .withNativeFunction ("pianoNoteOff", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 1) audioProcessor.keyboardState.noteOff(1, (int)args[0], 0.0f);
            completion({});
        })
        .withNativeFunction ("uiReady", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            juce::ignoreUnused(args);
            writeLog("[JUNiO] JS UI is Ready. Sending Initial State...");
            
            // Mandatory completion for JUCE 8
            completion (juce::var::undefined());

            // Use a small delay to ensure JS listeners are fully active
            juce::Timer::callAfterDelay(100, [this]() {
                writeLog("[JUNiO] Sending delayed state dump...");
                
                // 1. Version Info
                juce::String versionStr = "1.2.0 (Build " + juce::String(JUNO_BUILD_VERSION) + ")";
                dispatchToJS ("onVersionUpdate", versionStr);
                
                int prog = audioProcessor.getCurrentProgram();
                juce::String lcdString = "P: " + juce::String(prog + 1) + " " + audioProcessor.getProgramName(prog);
                dispatchToJS ("onLCDUpdate", lcdString);
                
                juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                obj->setProperty("bank", (prog / 8) + 1);
                obj->setProperty("patch", (prog % 8) + 1);
                dispatchToJS("onBankPatchUpdate", juce::var(obj.get()));

                // Initial parameter sync
                for (auto* param : audioProcessor.getParameters()) {
                    if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param)) {
                        updateParameterInJS(p->getParameterID(), p->getValue());
                    }
                }
                
                // Sync lastParams immediately
                audioProcessor.updateParamsFromAPVTS();
                audioProcessor.requestPatchDump();
                
                writeLog("[JUNiO] Initial State Sync Complete");
            });
        })
;

    webComponent = std::make_unique<juce::WebBrowserComponent> (options);
    addAndMakeVisible (webComponent.get());
    webComponent->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    // Listen to parameter changes
    const juce::StringArray paramIDs = {
        "masterVolume", "lfoRate", "lfoDelay", "lfoToDCO", "pwm", "pwmMode",
        "hpfFreq", "vcfFreq", "resonance", "envAmount", "vcfPolarity", "kybdTracking", "lfoToVCF",
        "vcaMode", "vcaLevel", "attack", "decay", "sustain", "release", "chorus1", "chorus2",
        "benderToDCO", "benderToVCF", "benderToLFO", "portamentoTime", "portamentoOn", "portamentoLegato",
        "polyMode", "tune", "dcoRange", "sawOn", "pulseOn", "subOsc", "noise", "bender", "midiOut",
        "midiChannel", "benderRange", "velocitySens", "lcdBrightness", "numVoices", "sustainInverted",
        "chorusHiss", "midiFunction", "unisonWidth", "aftertouchToVCF"
    };
    for (int i = 0; i < paramIDs.size(); ++i)
        audioProcessor.getAPVTS().addParameterListener (paramIDs[i], this);

    startTimerHz(30); 
    setColour (juce::ResizableWindow::backgroundColourId, juce::Colours::black);
    setSize (1200, 750);
}

WebViewEditor::~WebViewEditor()
{
    audioProcessor.editor = nullptr;
    const juce::StringArray paramIDs = {
        "masterVolume", "lfoRate", "lfoDelay", "lfoToDCO", "pwm", "pwmMode",
        "hpfFreq", "vcfFreq", "resonance", "envAmount", "vcfPolarity", "kybdTracking", "lfoToVCF",
        "vcaMode", "vcaLevel", "attack", "decay", "sustain", "release", "chorus1", "chorus2",
        "benderToDCO", "benderToVCF", "benderToLFO", "portamentoTime", "portamentoOn", "portamentoLegato",
        "polyMode", "tune", "dcoRange", "sawOn", "pulseOn", "subOsc", "noise", "bender", "midiOut",
        "midiChannel", "benderRange", "velocitySens", "lcdBrightness", "numVoices", "sustainInverted",
        "chorusHiss", "midiFunction", "unisonWidth", "aftertouchToVCF"
    };
    for (int i = 0; i < paramIDs.size(); ++i)
        audioProcessor.getAPVTS().removeParameterListener (paramIDs[i], this);
}

void WebViewEditor::parameterChanged (const juce::String& parameterID, float newValue)
{
    // [Build 18] Ensure engine refreshes its mirror parameters
    audioProcessor.paramsAreDirty.store(true);
    
    // [Build 21] Always normalize to 0..1 for UI sliders
    float normalizedValue = newValue;
    if (auto* param = audioProcessor.getAPVTS().getParameter(parameterID)) {
        normalizedValue = param->getValue(); // AudioProcessorParameter::getValue() is always 0..1
    }
    
    updateParameterInJS (parameterID, normalizedValue);
}

void WebViewEditor::updateParameterInJS (const juce::String& paramID, float value)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("id", paramID);
    obj->setProperty("value", value);
    dispatchToJS("onParameterChanged", juce::var(obj.get()));
}

void WebViewEditor::updateSysExInJS()
{
    auto msg = audioProcessor.getCurrentSysExData();
    if (msg.getRawDataSize() > 0 && msg.getRawData() != lastSysEx.getRawData()) {
        juce::String hex;
        for (int i=0; i < msg.getRawDataSize(); ++i) {
            hex += juce::String::toHexString((int)msg.getRawData()[i]).toUpperCase().paddedLeft('0', 2);
            if (i < msg.getRawDataSize() - 1) hex += " ";
        }
        dispatchToJS("onSysExUpdate", hex);
        lastSysEx = msg;
    }
}

void WebViewEditor::timerCallback()
{
    updateSysExInJS();

#ifndef ABD_PROCESSOR_HAS_TELEMETRY
#error "Wrong ABDSimpleJuno106AudioProcessor.h included! Check your include paths."
#endif
    if (audioProcessor.popMidiTrafficFlag())
        dispatchToJS("onMidiTraffic", true);
    if (auto* pm = audioProcessor.getPresetManager()) {
        int currentIdx = pm->getCurrentPresetIndex();
        if (currentIdx != lastPresetIndex) {
            lastPresetIndex = currentIdx;
            
            juce::DynamicObject::Ptr bp = new juce::DynamicObject();
            bp->setProperty("bank", (currentIdx / 8) + 1);
            bp->setProperty("patch", (currentIdx % 8) + 1);
            dispatchToJS("onBankPatchUpdate", juce::var(bp.get()));
            
            juce::String lcdText = "P: " + juce::String(currentIdx + 1) + " " + pm->getCurrentPresetName();
            dispatchToJS("onLCDUpdate", lcdText);
        }
    }

    // Monitor Tuning Title changes
    juce::String currentTuning = audioProcessor.getCurrentTuningName();
    if (currentTuning != lastTuningName) {
        lastTuningName = currentTuning;
        dispatchToJS("onTuningUpdate", currentTuning);
    }

    // LCD Status Badges (LC, A/B, WIP)
    juce::DynamicObject::Ptr status = new juce::DynamicObject();
    status->setProperty("lc", audioProcessor.getMidiLearnHandler().getIsLearning());
    status->setProperty("ab", audioProcessor.getActiveABSlot() == 0 ? "A" : "B");
    status->setProperty("wip", audioProcessor.getWipCount());
    dispatchToJS("onLCDStatusUpdate", juce::var(status.get()));
}

void WebViewEditor::dispatchToJS(const juce::Identifier& eventId, const juce::var& payload)
{
    if (webComponent) {
        // [OMEGA] Hardened JUCE 8 emission
        webComponent->emitEventIfBrowserIsVisible(eventId.toString(), payload);

        // Fallback for generic hostEvent listener
        juce::DynamicObject::Ptr fObj = new juce::DynamicObject();
        fObj->setProperty("id", eventId.toString());
        fObj->setProperty("payload", payload);
        webComponent->emitEventIfBrowserIsVisible("hostEvent", juce::var(fObj.get()));
    }
}

void WebViewEditor::showAboutCallback()       { dispatchToJS ("onShowAbout", {}); }
void WebViewEditor::showSettingsCallback()    { dispatchToJS ("onShowSettings", {}); }
void WebViewEditor::showServiceModeCallback() { dispatchToJS ("showModal", "serviceMode"); }

void WebViewEditor::sendPresetListUpdate()
{
    if (auto* pm = audioProcessor.getPresetManager())
    {
        juce::DynamicObject::Ptr root (new juce::DynamicObject());
        root->setProperty(juce::Identifier("currentLibrary"), juce::var(pm->getCurrentLibraryName()));
        root->setProperty(juce::Identifier("currentPresetIndex"), juce::var(pm->getCurrentPresetIndex()));

        juce::Array<juce::var> items;
        const auto& lib = pm->getCurrentLibrary();

        for (int i = 0; i < (int)lib.patches.size(); ++i)
        {
            const auto& p = lib.patches[(size_t)i];

            juce::DynamicObject::Ptr o (new juce::DynamicObject());
            o->setProperty(juce::Identifier("index"),    juce::var(i));
            o->setProperty(juce::Identifier("name"),     juce::var(p.name));
            o->setProperty(juce::Identifier("category"), juce::var(p.category));
            o->setProperty(juce::Identifier("author"),   juce::var(p.author));
            o->setProperty(juce::Identifier("tags"),     juce::var(p.tags));
            o->setProperty(juce::Identifier("favorite"), juce::var(p.isFavorite));

            o->setProperty(juce::Identifier("originGroup"), juce::var(p.originGroup));
            o->setProperty(juce::Identifier("originBank"),  juce::var(p.originBank));
            o->setProperty(juce::Identifier("originPatch"), juce::var(p.originPatch));

            items.add(juce::var(o.get()));
        }

        root->setProperty("items", juce::var(items));
        dispatchToJS("onPresetListUpdate", juce::var(root.get()));
    }
}

void WebViewEditor::sendBankPatchUpdate (int group, int bank, int patch)
{
    juce::DynamicObject::Ptr obj (new juce::DynamicObject());
    obj->setProperty(juce::Identifier("group"), juce::var(group));
    obj->setProperty(juce::Identifier("bank"),  juce::var(bank));
    obj->setProperty(juce::Identifier("patch"), juce::var(patch));

    dispatchToJS("onBankPatchUpdate", juce::var(obj.get()));
}

void WebViewEditor::updateLCDInJS (const juce::String& text)
{
    dispatchToJS("onLCDUpdate", text);
}

void WebViewEditor::postMessage (const juce::String& json)
{
    if (webComponent) webComponent->emitEventIfBrowserIsVisible ("hostEvent", json);
}

void WebViewEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void WebViewEditor::resized()
{
    if (webComponent) webComponent->setBounds (getLocalBounds());
}

void WebViewEditor::writeLog(const juce::String& msg)
{
    if (audioProcessor.getCalibrationSettings().getValue("enableLogging") > 0.5f)
        juce::Logger::writeToLog(msg);
}
