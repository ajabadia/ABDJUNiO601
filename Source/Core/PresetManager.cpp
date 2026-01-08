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
    
    addLibrary(wavFile.getFileNameWithoutExtension());
    int newLibIdx = getNumLibraries() - 1;
    Library& lib = libraries[newLibIdx];
    
    const int autoDetectedPatches = (int)result.data.size() / 18;
    if (autoDetectedPatches == 0) return juce::Result::fail("No valid patches found.");
    
    int patchCount = juce::jmin(autoDetectedPatches, 128);
    for (int p = 0; p < patchCount; ++p) { 
        const unsigned char* patchBytes = &result.data[p * 18];
        juce::String patchName = juce::String(p + 1).paddedLeft('0', 2);
        lib.patches.push_back(createPresetFromJunoBytes(patchName, patchBytes));
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
    int range = 1; 
    if (sw1 & (1 << 0)) range = 0; 
    else if (sw1 & (1 << 1)) range = 1; 
    else if (sw1 & (1 << 2)) range = 2; 
    state.setProperty("dcoRange", range, nullptr);
    state.setProperty("pulseOn", (sw1 & (1 << 3)) != 0, nullptr);
    state.setProperty("sawOn", (sw1 & (1 << 4)) != 0, nullptr);
    
    bool chorusOn = (sw1 & (1 << 5)) == 0;
    bool chorusModeII = (sw1 & (1 << 6)) != 0; 
    state.setProperty("chorus1", chorusOn && !chorusModeII, nullptr);
    state.setProperty("chorus2", chorusOn && chorusModeII, nullptr);

    unsigned char sw2 = bytes[17];
    state.setProperty("pwmMode", (sw2 & (1 << 0)) != 0, nullptr);
    state.setProperty("vcfPolarity", (sw2 & (1 << 1)) != 0, nullptr);
    state.setProperty("vcaMode", (sw2 & (1 << 2)) != 0, nullptr);
    state.setProperty("hpfFreq", (sw2 >> 3) & 0x03, nullptr); 
    
    return Preset(name, "Factory", state);
}

void PresetManager::loadFactoryPresets() {
    if (libraries.empty()) return;
    Library& lib = libraries[0]; 
    lib.patches.clear();
    for (const auto& data : junoFactoryPresets) {
        lib.patches.push_back(createPresetFromJunoBytes(data.name, data.bytes));
    }
}

void PresetManager::loadUserPresets() {
    int idx = -1;
    for(int i=0; i<libraries.size(); ++i) if(libraries[i].name == "User") idx = i;
    if (idx == -1) return;
    
    Library& lib = libraries[idx];
    lib.patches.clear();
    
    auto userDir = getUserPresetsDirectory();
    if (!userDir.exists()) return;
    
    auto files = userDir.findChildFiles(juce::File::findFiles, false, "*.json");
    for (const auto& file : files) {
        auto json = juce::JSON::parse(file);
        if (json.isObject()) {
            auto obj = json.getDynamicObject();
            if (obj != nullptr) {
                juce::String name = obj->getProperty("name").toString();
                auto stateVar = obj->getProperty("state");
                if (stateVar.isObject()) {
                    juce::ValueTree state = juce::ValueTree::fromXml(stateVar.toString());
                    if (state.isValid()) lib.patches.push_back(Preset(name, "User", state));
                }
            }
        }
    }
}

void PresetManager::saveUserPreset(const juce::String& name, const juce::ValueTree& state) {
    auto userDir = getUserPresetsDirectory();
    if (!userDir.exists()) userDir.createDirectory();
    auto file = userDir.getChildFile(name + ".json");
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("name", name);
    obj->setProperty("state", state.toXmlString());
    file.replaceWithText(juce::JSON::toString(juce::var(obj.get())));
    loadUserPresets();
}

void PresetManager::deleteUserPreset(const juce::String& name) {
    auto userDir = getUserPresetsDirectory();
    auto file = userDir.getChildFile(name + ".json");
    file.deleteFile();
    loadUserPresets();
}

void PresetManager::addLibraryFromSysEx(const uint8_t* data, int size) {
    addLibrary("Imported SysEx");
    int newLibIdx = getNumLibraries() - 1;
    Library& lib = libraries[newLibIdx];
    
    int patchCount = 0;
    int i = 0;
    while (i < size && patchCount < 64) {
        if (data[i] != 0xF0) { i++; continue; }
        if (i + 22 < size && data[i+1] == 0x41 && data[i+4] == 0x30) {
            const unsigned char* patchBytes = &data[i + 6]; 
            juce::String name = juce::String(patchCount + 1).paddedLeft('0', 2);
            lib.patches.push_back(createPresetFromJunoBytes(name, patchBytes));
            patchCount++;
            int msgEnd = i + 6 + 18;
            while(msgEnd < size && data[msgEnd] != 0xF7) msgEnd++;
            i = msgEnd + 1;
        } else {
            i++;
        }
    }
    selectLibrary(newLibIdx);
}

void PresetManager::exportLibraryToJson(const juce::File& file) {
    if (currentLibraryIndex >= getNumLibraries()) return;
    const Library& lib = libraries[currentLibraryIndex];
    juce::Array<juce::var> presetsArray;
    for (const auto& preset : lib.patches) {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty("name", preset.name);
        obj->setProperty("state", preset.state.toXmlString());
        presetsArray.add(juce::var(obj.get()));
    }
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty("libraryName", lib.name);
    root->setProperty("presets", presetsArray);
    file.replaceWithText(juce::JSON::toString(juce::var(root.get())));
}

juce::StringArray PresetManager::getPresetNames() const {
    juce::StringArray names;
    if (currentLibraryIndex < getNumLibraries()) {
        const auto& lib = libraries[currentLibraryIndex];
        for (const auto& p : lib.patches) names.add(p.name);
    }
    return names;
}

const PresetManager::Preset* PresetManager::getPreset(int index) const {
    if (currentLibraryIndex < getNumLibraries()) {
        const auto& lib = libraries[currentLibraryIndex];
        if (index >= 0 && index < (int)lib.patches.size()) return &lib.patches[index];
    }
    return nullptr;
}

void PresetManager::setCurrentPreset(int index) {
    currentPresetIndex = index;
}

void PresetManager::selectPresetByBankAndPatch(int group, int bank, int patch) {
    currentPresetIndex = (group * 64) + ((bank - 1) * 8) + (patch - 1);
}

juce::ValueTree PresetManager::getCurrentPresetState() const {
    const auto* p = getPreset(currentPresetIndex);
    return p ? p->state : juce::ValueTree();
}

juce::String PresetManager::getCurrentPresetName() const {
    const auto* p = getPreset(currentPresetIndex);
    return p ? p->name : "Init";
}

juce::File PresetManager::getUserPresetsDirectory() const {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("JUNiO601").getChildFile("UserPresets");
}

void PresetManager::randomizeCurrentParameters(juce::AudioProcessorValueTreeState& apvts) {
    auto& rand = juce::Random::getSystemRandom();
    auto setP = [&](juce::String id, float val) {
        if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(val);
    };
    auto setI = [&](juce::String id, int val) {
        if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1((float)val));
    };
    auto setB = [&](juce::String id, bool val) {
        if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(val ? 1.0f : 0.0f);
    };

    // 1. DCO - Garantizar sonido
    setI("dcoRange", rand.nextInt(3));
    bool saw = rand.nextBool();
    setB("sawOn", saw);
    setB("pulseOn", !saw || rand.nextBool());
    setP("pwm", rand.nextFloat());
    setI("pwmMode", rand.nextInt(2));
    setP("subOsc", rand.nextFloat() * 0.8f);
    setP("noise", rand.nextFloat() * 0.3f);
    setP("lfoToDCO", rand.nextFloat() * 0.2f);

    // 2. VCF - Musical
    setP("vcfFreq", 0.2f + (rand.nextFloat() * 0.8f));
    setP("resonance", rand.nextFloat() * 0.7f);
    setP("envAmount", rand.nextFloat());
    setI("vcfPolarity", rand.nextInt(2));
    setP("lfoToVCF", rand.nextFloat() * 0.4f);
    setP("kybdTracking", rand.nextFloat());
    setI("hpfFreq", rand.nextInt(4));

    // 3. VCA
    setI("vcaMode", rand.nextInt(2));
    setP("vcaLevel", 0.6f + (rand.nextFloat() * 0.4f));

    // 4. ENV - Evitar silencios
    setP("attack", rand.nextFloat() * 0.5f);
    setP("decay", 0.1f + rand.nextFloat() * 0.9f);
    setP("sustain", 0.2f + rand.nextFloat() * 0.8f);
    setP("release", 0.1f + rand.nextFloat() * 0.7f);

    // 5. LFO
    setP("lfoRate", rand.nextFloat());
    setP("lfoDelay", rand.nextFloat() * 0.5f);

    // 6. Chorus - 70% Probabilidad
    bool cOn = rand.nextFloat() < 0.7f;
    if (cOn) {
        int m = rand.nextInt(3); // 0=I, 1=II, 2=I+II
        setB("chorus1", m == 0 || m == 2);
        setB("chorus2", m == 1 || m == 2);
    } else {
        setB("chorus1", false);
        setB("chorus2", false);
    }
}
