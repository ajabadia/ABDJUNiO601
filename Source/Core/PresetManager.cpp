#include "PresetManager.h"
#include "FactoryPresets.h"
#include "JunoTapeDecoder.h"

PresetManager::PresetManager() {
    addLibrary("Factory");
    loadFactoryPresets();
    addLibrary("User");
    loadUserPresets();
    currentLibraryIndex = 0;
    currentPresetIndex = 0;
}

PresetManager::~PresetManager() = default;

void PresetManager::addLibrary(const juce::String& name) {
    for (const auto& lib : libraries) if (lib.name == name) return;
    Library lib;
    lib.name = name;
    libraries.push_back(lib);
}

void PresetManager::selectLibrary(int index) {
    currentLibraryIndex = juce::jlimit(0, getNumLibraries() - 1, index);
    currentPresetIndex = 0;
}

juce::Result PresetManager::loadTape(const juce::File& wavFile) {
    auto result = JunoTapeDecoder::decodeWavFile(wavFile);
    if (!result.success) return juce::Result::fail(result.errorMessage);
    setLastPath(wavFile.getParentDirectory().getFullPathName());
    addLibrary(wavFile.getFileNameWithoutExtension());
    int newLibIdx = getNumLibraries() - 1;
    int patchCount = juce::jmin((int)result.data.size() / 18, 128);
    for (int p = 0; p < patchCount; ++p) { 
        libraries[newLibIdx].patches.push_back(createPresetFromJunoBytes(juce::String(p + 1).paddedLeft('0', 2), &result.data[p * 18]));
    }
    selectLibrary(newLibIdx);
    return juce::Result::ok();
}

PresetManager::Preset PresetManager::createPresetFromJunoBytes(const juce::String& name, const unsigned char* bytes) {
    juce::ValueTree state("Parameters");
    auto toNorm = [](unsigned char b) { return static_cast<float>(b) / 127.0f; };
    state.setProperty("lfoRate", toNorm(bytes[0]), nullptr);
    state.setProperty("lfoDelay", toNorm(bytes[1]), nullptr);
    state.setProperty("lfoToDCO", toNorm(bytes[2]), nullptr);
    state.setProperty("pwm", toNorm(bytes[3]), nullptr);
    state.setProperty("noise", toNorm(bytes[4]), nullptr);
    state.setProperty("vcfFreq", toNorm(bytes[5]), nullptr);
    state.setProperty("resonance", toNorm(bytes[6]), nullptr);
    state.setProperty("envAmount", toNorm(bytes[7]), nullptr);
    state.setProperty("lfoToVCF", toNorm(bytes[8]), nullptr);
    state.setProperty("kybdTracking", toNorm(bytes[9]), nullptr);
    state.setProperty("vcaLevel", toNorm(bytes[10]), nullptr);
    state.setProperty("attack", toNorm(bytes[11]), nullptr);
    state.setProperty("decay", toNorm(bytes[12]), nullptr);
    state.setProperty("sustain", toNorm(bytes[13]), nullptr);
    state.setProperty("release", toNorm(bytes[14]), nullptr);
    state.setProperty("subOsc", toNorm(bytes[15]), nullptr);
    unsigned char sw1 = bytes[16];
    int range = (sw1 & (1 << 0)) ? 0 : (sw1 & (1 << 1) ? 1 : 2);
    state.setProperty("dcoRange", range, nullptr);
    state.setProperty("pulseOn", (sw1 & (1 << 3)) != 0, nullptr);
    state.setProperty("sawOn", (sw1 & (1 << 4)) != 0, nullptr);
    bool chorusOn = (sw1 & (1 << 5)) == 0;
    bool chorusI = (sw1 & (1 << 6)) != 0;
    state.setProperty("chorus1", chorusOn && chorusI, nullptr);
    state.setProperty("chorus2", chorusOn && !chorusI, nullptr);
    unsigned char sw2 = bytes[17];
    state.setProperty("pwmMode", (sw2 & (1 << 0)) != 0, nullptr);     
    state.setProperty("vcaMode", (sw2 & (1 << 1)) != 0, nullptr);     
    state.setProperty("vcfPolarity", (sw2 & (1 << 2)) != 0, nullptr); 
    state.setProperty("hpfFreq", 3 - ((sw2 >> 3) & 0x03), nullptr); 
    return Preset(name, "User", state);
}

void PresetManager::loadFactoryPresets() {
    if (libraries.empty()) return;
    libraries[0].patches.clear();
    for (const auto& data : junoFactoryPresets) 
        libraries[0].patches.push_back(createPresetFromJunoBytes(data.name, data.bytes));
}

void PresetManager::loadUserPresets() {
    int idx = -1;
    for(int i=0; i<(int)libraries.size(); ++i) if(libraries[i].name == "User") idx = i;
    if (idx == -1) return;
    
    Library& lib = libraries[idx];
    lib.patches.clear();
    auto userDir = getUserPresetsDirectory();
    if (!userDir.exists()) userDir.createDirectory();
    
    auto files = userDir.findChildFiles(juce::File::findFiles, false, "*.json");
    for (const auto& f : files) {
        auto json = juce::JSON::parse(f);
        if (json.isObject()) {
            auto obj = json.getDynamicObject();
            if (obj && obj->hasProperty("state"))
                lib.patches.push_back(Preset(obj->getProperty("name").toString(), "User", juce::ValueTree::fromXml(obj->getProperty("state").toString())));
        }
    }
}

void PresetManager::saveUserPreset(const juce::String& name, const juce::ValueTree& state) {
    if (!state.isValid()) return;
    auto userDir = getUserPresetsDirectory();
    if (!userDir.exists()) userDir.createDirectory();
    
    juce::String safeName = juce::File::createLegalFileName(name);
    auto file = userDir.getChildFile(safeName + ".json");
    
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("name", name);
    obj->setProperty("state", state.toXmlString());
    
    if (file.replaceWithText(juce::JSON::toString(juce::var(obj.get())))) {
        loadUserPresets();
        for(int i=0; i<(int)libraries.size(); ++i) {
            if(libraries[i].name == "User") {
                currentLibraryIndex = i;
                for(int k=0; k<(int)libraries[i].patches.size(); ++k) {
                    if(libraries[i].patches[k].name == name) { currentPresetIndex = k; break; }
                }
                break;
            }
        }
    }
}

juce::Result PresetManager::importPresetsFromFile(const juce::File& file) {
    juce::MemoryBlock mb;
    if (!file.loadFileAsData(mb)) return juce::Result::fail("Read error");
    setLastPath(file.getParentDirectory().getFullPathName());
    const uint8_t* d = (const uint8_t*)mb.getData();
    int s = (int)mb.getSize();
    struct RawP { std::vector<uint8_t> b; };
    std::vector<RawP> found;
    
    for (int i=0; i < s - 22; ++i) {
        if (d[i] == 0xF0 && d[i+1] == 0x41 && d[i+2] == 0x30) {
            RawP p; for(int k=0; k<18; ++k) p.b.push_back(d[i+5+k]);
            found.push_back(p); i += 23;
        }
    }
    if (found.empty() && s >= 18) { RawP p; for(int k=0; k<18; ++k) p.b.push_back(d[k]); found.push_back(p); }
    if (found.empty()) return juce::Result::fail("No patches");

    if (found.size() > 1) {
        addLibrary(file.getFileNameWithoutExtension());
        int libIdx = getNumLibraries() - 1;
        for(int k=0; k<(int)found.size(); ++k)
            libraries[libIdx].patches.push_back(createPresetFromJunoBytes(file.getFileNameWithoutExtension() + " " + juce::String(k+1).paddedLeft('0',2), found[k].b.data()));
        selectLibrary(libIdx);
    } else {
        saveUserPreset(file.getFileNameWithoutExtension(), createPresetFromJunoBytes(file.getFileNameWithoutExtension(), found[0].b.data()).state);
    }
    return juce::Result::ok();
}

void PresetManager::exportLibraryToJson(const juce::File& file) {
    if (currentLibraryIndex >= getNumLibraries()) return;
    setLastPath(file.getParentDirectory().getFullPathName());
    juce::Array<juce::var> arr;
    for (const auto& p : libraries[currentLibraryIndex].patches) {
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty("name", p.name); o->setProperty("state", p.state.toXmlString()); arr.add(juce::var(o.get()));
    }
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty("libraryName", libraries[currentLibraryIndex].name); root->setProperty("presets", arr);
    file.replaceWithText(juce::JSON::toString(juce::var(root.get())));
}

void PresetManager::exportAllLibrariesToJson(const juce::File& file) {
    setLastPath(file.getParentDirectory().getFullPathName());
    juce::Array<juce::var> libs;
    for (const auto& lib : libraries) {
        juce::Array<juce::var> arr;
        for (const auto& p : lib.patches) {
            juce::DynamicObject::Ptr o = new juce::DynamicObject();
            o->setProperty("name", p.name); o->setProperty("state", p.state.toXmlString()); arr.add(juce::var(o.get()));
        }
        juce::DynamicObject::Ptr lObj = new juce::DynamicObject();
        lObj->setProperty("libraryName", lib.name); lObj->setProperty("presets", arr); libs.add(juce::var(lObj.get()));
    }
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty("allLibraries", libs);
    file.replaceWithText(juce::JSON::toString(juce::var(root.get())));
}

juce::StringArray PresetManager::getPresetNames() const {
    juce::StringArray n; if (currentLibraryIndex < getNumLibraries()) for (const auto& p : libraries[currentLibraryIndex].patches) n.add(p.name); return n;
}
const PresetManager::Preset* PresetManager::getPreset(int index) const {
    if (currentLibraryIndex < getNumLibraries() && index >= 0 && index < (int)libraries[currentLibraryIndex].patches.size()) return &libraries[currentLibraryIndex].patches[index];
    return nullptr;
}
void PresetManager::setCurrentPreset(int index) { currentPresetIndex = index; }
void PresetManager::selectPresetByBankAndPatch(int g, int b, int p) { currentPresetIndex = (g * 64) + ((b - 1) * 8) + (p - 1); }
juce::ValueTree PresetManager::getCurrentPresetState() const { const auto* p = getPreset(currentPresetIndex); return p ? p->state : juce::ValueTree(); }
juce::String PresetManager::getCurrentPresetName() const { const auto* p = getPreset(currentPresetIndex); return p ? p->name : "Init"; }
juce::File PresetManager::getUserPresetsDirectory() const { return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("JUNiO601").getChildFile("UserPresets"); }

juce::String PresetManager::getLastPath() const {
    juce::PropertiesFile::Options o; o.applicationName = "JUNiO601"; o.filenameSuffix = ".settings";
    juce::PropertiesFile props(o); return props.getValue("lastPath", juce::File::getSpecialLocation(juce::File::userHomeDirectory).getFullPathName());
}
void PresetManager::setLastPath(const juce::String& path) {
    juce::PropertiesFile::Options o; o.applicationName = "JUNiO601"; o.filenameSuffix = ".settings";
    juce::PropertiesFile props(o); props.setValue("lastPath", path); props.saveIfNeeded();
}

void PresetManager::randomizeCurrentParameters(juce::AudioProcessorValueTreeState& apvts) {
    auto& r = juce::Random::getSystemRandom();
    auto setP = [&](juce::String id, float v) { if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(v); };
    auto setI = [&](juce::String id, int v) { if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1((float)v)); };
    auto setB = [&](juce::String id, bool v) { if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(v ? 1.0f : 0.0f); };
    setI("dcoRange", r.nextInt(3)); bool s = r.nextBool(); setB("sawOn", s); setB("pulseOn", !s || r.nextBool());
    setP("pwm", r.nextFloat()); setI("pwmMode", r.nextInt(2)); setP("subOsc", r.nextFloat() * 0.8f); setP("noise", r.nextFloat() * 0.3f);
    setP("lfoToDCO", r.nextFloat() * 0.2f); setP("vcfFreq", 0.2f + (r.nextFloat() * 0.8f)); setP("resonance", r.nextFloat() * 0.7f);
    setP("envAmount", r.nextFloat()); setI("vcfPolarity", r.nextInt(2)); setP("lfoToVCF", r.nextFloat() * 0.4f); setP("kybdTracking", r.nextFloat());
    setI("hpfFreq", r.nextInt(4)); setI("vcaMode", r.nextInt(2)); setP("vcaLevel", 0.6f + (r.nextFloat() * 0.4f));
    setP("attack", r.nextFloat() * 0.5f); setP("decay", 0.1f + r.nextFloat() * 0.9f); setP("sustain", 0.2f + r.nextFloat() * 0.8f);
    setP("release", 0.1f + r.nextFloat() * 0.7f); setP("lfoRate", r.nextFloat()); setP("lfoDelay", r.nextFloat() * 0.5f);
    bool cOn = r.nextFloat() < 0.7f; if (cOn) { int m = r.nextInt(3); setB("chorus1", m == 0 || m == 2); setB("chorus2", m == 1 || m == 2); } else { setB("chorus1", false); setB("chorus2", false); }
}
