#include "BridgeActions.h"
#include "../../Core/ABDSimpleJuno106AudioProcessor.h"
#include "../../Core/CalibrationSettings.h"
#include "../../Core/PresetManager.h"

namespace BridgeActions {

void setParameter(ABDSimpleJuno106AudioProcessor& audioProcessor,
                  const juce::Array<juce::var>& args,
                  juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    if (args.size() >= 2) {
        juce::String paramID = args[0].toString();
        float val = (float)args[1];
        if (auto* param = audioProcessor.getAPVTS().getParameter(paramID)) {
            param->setValueNotifyingHost(val);
            completion(juce::var::undefined());
        } else {
            completion({});
        }
    } else {
        completion({});
    }
}

void beginGesture(ABDSimpleJuno106AudioProcessor& audioProcessor,
                  const juce::Array<juce::var>& args,
                  juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    if (args.size() >= 1) {
        juce::String paramID = args[0].toString();
        if (auto* param = audioProcessor.getAPVTS().getParameter(paramID))
            param->beginChangeGesture();
    }
    completion(juce::var::undefined());
}

void endGesture(ABDSimpleJuno106AudioProcessor& audioProcessor,
                const juce::Array<juce::var>& args,
                juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    if (args.size() >= 1) {
        juce::String paramID = args[0].toString();
        if (auto* param = audioProcessor.getAPVTS().getParameter(paramID))
            param->endChangeGesture();
    }
    completion(juce::var::undefined());
}

void getCalibrationParams(ABDSimpleJuno106AudioProcessor& audioProcessor,
                          const juce::Array<juce::var>& args,
                          juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
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
}

void setCalibrationParam(ABDSimpleJuno106AudioProcessor& audioProcessor,
                         const juce::Array<juce::var>& args,
                         juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    if (args.size() >= 2) {
        juce::String id = args[0].toString();
        float val = (float)args[1];
        audioProcessor.getCalibrationSettings().setValue(id.toStdString(), val);
    }
    completion({});
}

void loadPreset(ABDSimpleJuno106AudioProcessor& audioProcessor,
                const juce::Array<juce::var>& args,
                juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    if (args.size() >= 1) {
        int index = (int)args[0];
        audioProcessor.loadPreset(index);
    }
    completion({});
}

void loadLibraryPreset(ABDSimpleJuno106AudioProcessor& audioProcessor,
                       const juce::Array<juce::var>& args,
                       juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    if (args.size() >= 2) {
        int libIdx = (int)args[0];
        int prstIdx = (int)args[1];
        audioProcessor.loadLibraryPreset(libIdx, prstIdx);
    }
    completion({});
}

void runSelfTest(ABDSimpleJuno106AudioProcessor& audioProcessor,
                 const juce::Array<juce::var>& args,
                 const std::function<void()>& notifySelfTestState,
                 juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    juce::ignoreUnused(args);
    auto res = audioProcessor.runSelfTest();
    notifySelfTestState();

    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("ok", res.ok);
    obj->setProperty("presetFailures", res.presetFailures);
    obj->setProperty("sysExOk", res.sysExOk);
    obj->setProperty("jsonOk", res.jsonOk);

    juce::Array<juce::var> failedArr;
    for (const auto& s : res.failedPresets) failedArr.add(s);
    obj->setProperty("failedPresets", failedArr);

    completion(juce::var(obj.get()));
}

void getSynthState(ABDSimpleJuno106AudioProcessor& audioProcessor,
                   const juce::Array<juce::var>& args,
                   juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    juce::ignoreUnused(args);
    juce::DynamicObject::Ptr state = new juce::DynamicObject();
    for (auto* param : audioProcessor.getParameters()) {
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            state->setProperty(juce::Identifier(p->getParameterID()), (double)p->getValue());
    }
    completion(juce::var(state.get()));
}

void getBrowserData(ABDSimpleJuno106AudioProcessor& audioProcessor,
                    const juce::Array<juce::var>& args,
                    juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    juce::ignoreUnused(args);
    if (auto* pm = audioProcessor.getPresetManager()) {
        juce::DynamicObject::Ptr root = new juce::DynamicObject();
        juce::Array<juce::var> libs;
        for (int i = 0; i < pm->getNumLibraries(); ++i) {
            auto& lib = pm->getLibrary(i);
            juce::DynamicObject::Ptr lObj = new juce::DynamicObject();
            lObj->setProperty("name", lib.name);
            lObj->setProperty("index", i);
            juce::Array<juce::var> patches;
            for (int j = 0; j < (int)lib.patches.size(); ++j) {
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
                juce::DynamicObject::Ptr dObj = new juce::DynamicObject();
                const auto& state = p.state;
                if (state.isValid()) {
                    for (int k = 0; k < state.getNumProperties(); ++k) {
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
        juce::Array<juce::var> cats;
        for (const auto& c : pm->categories_) cats.add(c);
        root->setProperty("categories", cats);
        completion(juce::var(root.get()));
    } else {
        completion({});
    }
}

void setFavorite(ABDSimpleJuno106AudioProcessor& audioProcessor,
                 const juce::Array<juce::var>& args,
                 juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    if (args.size() >= 3) {
        if (auto* pm = audioProcessor.getPresetManager())
            pm->setFavorite((int)args[0], (int)args[1], (bool)args[2]);
    }
    completion({});
}

void updateMetadata(ABDSimpleJuno106AudioProcessor& audioProcessor,
                    const juce::Array<juce::var>& args,
                    juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    if (args.size() >= 6) {
        if (auto* pm = audioProcessor.getPresetManager())
            pm->updateMetadata((int)args[0], (int)args[1], args[2].toString(), args[3].toString(), args[4].toString(), args[5].toString());
    }
    completion({});
}

void savePresetDetailed(ABDSimpleJuno106AudioProcessor& audioProcessor,
                        const juce::Array<juce::var>& args,
                        const std::function<void(const juce::String&, const juce::var&)>& dispatchToJS,
                        const std::function<void()>& sendPresetListUpdate,
                        juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    if (args.size() >= 2) {
        if (auto* pm = audioProcessor.getPresetManager()) {
            pm->selectPreset((int)args[0], (int)args[1]);
            auto res = pm->saveCurrentPresetFromState(audioProcessor.getAPVTS());
            if (res.failed())
                dispatchToJS("alert", res.getErrorMessage());
            else
                sendPresetListUpdate();
        }
    }
    completion({});
}

void saveAsNewPresetDetailed(ABDSimpleJuno106AudioProcessor& audioProcessor,
                             const juce::Array<juce::var>& args,
                             const std::function<void(const juce::String&, const juce::var&)>& dispatchToJS,
                             const std::function<void()>& sendPresetListUpdate,
                             juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    if (args.size() >= 5) {
        if (auto* pm = audioProcessor.getPresetManager()) {
            auto res = pm->saveAsNewPresetFromState(audioProcessor.getAPVTS(),
                args[0].toString(),
                args[1].toString(),
                args[2].toString(),
                args[3].toString(),
                args[4].toString());
            if (res.failed())
                dispatchToJS("alert", res.getErrorMessage());
            else
                sendPresetListUpdate();
        }
    }
    completion({});
}

void pianoNoteOn(ABDSimpleJuno106AudioProcessor& audioProcessor,
                 const juce::Array<juce::var>& args,
                 juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    if (args.size() >= 2)
        audioProcessor.keyboardState.noteOn(1, (int)args[0], (float)args[1]);
    completion({});
}

void pianoNoteOff(ABDSimpleJuno106AudioProcessor& audioProcessor,
                  const juce::Array<juce::var>& args,
                  juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    if (args.size() >= 1)
        audioProcessor.keyboardState.noteOff(1, (int)args[0], 0.0f);
    completion({});
}

void chooseDirectory(ABDSimpleJuno106AudioProcessor& audioProcessor,
                     std::unique_ptr<juce::FileChooser>& fileChooser,
                     const juce::Array<juce::var>& args,
                     juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    juce::File startPath = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    if (args.size() >= 1 && args[0].toString().isNotEmpty())
        startPath = juce::File(args[0].toString());
    fileChooser = std::make_unique<juce::FileChooser>("Choose Preset Library Path...", startPath);
    auto safeCompletion = std::make_shared<juce::WebBrowserComponent::NativeFunctionCompletion>(std::move(completion));
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
    [safeCompletion](const juce::FileChooser& fc) {
        auto result = fc.getResult();
        if (result != juce::File() && result.exists())
            (*safeCompletion)(juce::var(result.getFullPathName()));
        else
            (*safeCompletion)(juce::var::undefined());
    });
}

void getLibraryPath(ABDSimpleJuno106AudioProcessor& audioProcessor,
                    const juce::Array<juce::var>& args,
                    juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    juce::ignoreUnused(args);
    juce::String path = juce::String(audioProcessor.getCalibrationSettings().getLibraryPath());
    if (path.isEmpty())
        path = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getFullPathName();
    completion(juce::var(path));
}

void setLibraryPath(ABDSimpleJuno106AudioProcessor& audioProcessor,
                    const juce::Array<juce::var>& args,
                    juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    if (args.size() >= 1)
        audioProcessor.getCalibrationSettings().setLibraryPath(args[0].toString().toStdString());
    completion({});
}

void setBrowserData(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               const juce::Array<juce::var>& args,
               juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    if (args.size() >= 1) {
        if (auto* pm = audioProcessor.getPresetManager()) {
            const auto& var = args[0];

            juce::ValueTree root("BankManager");
            auto* obj = var.getDynamicObject();
            if (obj) {
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

                                        auto dataVar = pObj->getProperty("data");
                                        if (dataVar.isObject()) {
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
            pm->saveBrowserData();
            juce::Logger::writeToLog("[JUNiO] Browser Data Synced to C++ Engine and Persisted");
        }
    }
    completion(juce::var::undefined());
}

void exportBank(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               std::unique_ptr<juce::FileChooser>& fileChooser,
               const juce::Array<juce::var>& args,
               juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    if (args.size() >= 1) {
        auto libObj = args[0];
        fileChooser = std::make_unique<juce::FileChooser>("Export Bank as JSON...",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.json");

        fileChooser->launchAsync(juce::FileBrowserComponent::saveMode, [libObj](const juce::FileChooser& fc) {
            auto result = fc.getResult();
            if (result != juce::File()) {
                result.replaceWithText(juce::JSON::toString(libObj));
            }
        });
    }
    completion({});
}

void importBank(
                 ABDSimpleJuno106AudioProcessor& audioProcessor,
               std::unique_ptr<juce::FileChooser>& fileChooser,
               const juce::Array<juce::var>& args,
               juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    juce::ignoreUnused(args, audioProcessor);
    fileChooser = std::make_unique<juce::FileChooser>("Import Bank JSON...",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.json");

    auto safeCompletion = std::make_shared<juce::WebBrowserComponent::NativeFunctionCompletion>(std::move(completion));

    fileChooser->launchAsync(juce::FileBrowserComponent::openMode, [safeCompletion](const juce::FileChooser& fc) {
        auto result = fc.getResult();
        if (result.existsAsFile()) {
            juce::var json;
            if (juce::JSON::parse(result.loadFileAsString(), json).wasOk()) {
                (*safeCompletion)(json);
                return;
            }
        }
        (*safeCompletion)(juce::var::undefined());
    });
}

} // namespace BridgeActions
