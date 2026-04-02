#include "PresetManager.h"
#include "FactoryPresets.h"
#include "JunoTapeDecoder.h"
#include "JunoTapeEncoder.h"
#include "JunoConstants.h"
#include <fstream>

using Preset = ABD::Preset;

PresetManager::PresetManager() {
    loadFactoryPresets();
    loadBrowserData();
    
    // [Advanced Browser] Initialize WIP Library if it doesn't exist
    if (this->getLibraryIndex("WIP") < 0)
        this->addLibrary("WIP");
        
    this->currentLibIdx_ = 0;
    this->currentPresetIdx_ = 0;
}

PresetManager::~PresetManager() = default;

void PresetManager::loadFactoryPresets() {
    this->addLibrary("Factory");
    int factoryIdx = (int)this->libraries_.size() - 1;
    this->libraries_[factoryIdx].patches.clear();
    for (const auto& patch : junoFactoryPatches) {
        this->libraries_[factoryIdx].patches.push_back(createPresetFromJunoPatch(patch));
    }
}

juce::ValueTree PresetManager::bytesToState(const uint8_t* data, int size) const {
    if (size < 18) return juce::ValueTree("Parameters");
    
    juce::ValueTree vt("Parameters");
    auto toNorm = [](uint8_t b) { return static_cast<float>(b) / 127.0f; };
    
    vt.setProperty("lfoRate", toNorm(data[0]), nullptr);
    vt.setProperty("lfoDelay", toNorm(data[1]), nullptr);
    vt.setProperty("lfoToDCO", toNorm(data[2]), nullptr);
    vt.setProperty("pwm", toNorm(data[3]), nullptr);
    vt.setProperty("noise", toNorm(data[4]), nullptr);
    vt.setProperty("vcfFreq", toNorm(data[5]), nullptr);
    vt.setProperty("resonance", toNorm(data[6]), nullptr);
    vt.setProperty("envAmount", toNorm(data[7]), nullptr);
    vt.setProperty("lfoToVCF", toNorm(data[8]), nullptr);
    vt.setProperty("kybdTracking", toNorm(data[9]), nullptr);
    vt.setProperty("vcaLevel", toNorm(data[10]), nullptr);
    vt.setProperty("attack", toNorm(data[11]), nullptr);
    vt.setProperty("decay", toNorm(data[12]), nullptr);
    vt.setProperty("sustain", toNorm(data[13]), nullptr);
    vt.setProperty("release", toNorm(data[14]), nullptr);
    vt.setProperty("subOsc", toNorm(data[15]), nullptr);
    
    uint8_t sw1 = data[16];
    vt.setProperty("dcoRange", (sw1 & 0x07), nullptr);
    vt.setProperty("pulseOn", (sw1 & (1 << 3)) != 0, nullptr);
    vt.setProperty("sawOn", (sw1 & (1 << 4)) != 0, nullptr);
    vt.setProperty("chorus1", (sw1 & (1 << 5)) != 0, nullptr);
    vt.setProperty("chorus2", (sw1 & (1 << 6)) != 0, nullptr);

    uint8_t sw2 = data[17];
    vt.setProperty("pwmMode", (sw2 & (1 << 0)) != 0, nullptr);
    vt.setProperty("vcaMode", (sw2 & (1 << 1)) != 0, nullptr);
    vt.setProperty("vcfPolarity", (sw2 & (1 << 2)) != 0, nullptr); 
    vt.setProperty("hpfFreq", (int)((sw2 >> 3) & 0x03), nullptr);

    // Performance Defaults
    vt.setProperty("benderToDCO", 2.0f, nullptr);
    vt.setProperty("benderToVCF", 0.0f, nullptr);
    vt.setProperty("benderToLFO", 0.0f, nullptr);
    vt.setProperty("portamentoTime", 0.0f, nullptr);
    vt.setProperty("portamentoOn", false, nullptr);
    vt.setProperty("portamentoLegato", false, nullptr);

    return vt;
}

std::vector<uint8_t> PresetManager::stateToBytes(const juce::ValueTree& state) const {
    std::vector<uint8_t> bytes(18, 0);
    auto fromNorm = [](float val) { return static_cast<uint8_t>(juce::jlimit(0, 127, (int)(val * 127.0f))); };
    
    bytes[0] = fromNorm(state["lfoRate"]);
    bytes[1] = fromNorm(state["lfoDelay"]);
    bytes[2] = fromNorm(state["lfoToDCO"]);
    bytes[3] = fromNorm(state["pwm"]);
    bytes[4] = fromNorm(state["noise"]);
    bytes[5] = fromNorm(state["vcfFreq"]);
    bytes[6] = fromNorm(state["resonance"]);
    bytes[7] = fromNorm(state["envAmount"]);
    bytes[8] = fromNorm(state["lfoToVCF"]);
    bytes[9] = fromNorm(state["kybdTracking"]);
    bytes[10] = fromNorm(state["vcaLevel"]);
    bytes[11] = fromNorm(state["attack"]);
    bytes[12] = fromNorm(state["decay"]);
    bytes[13] = fromNorm(state["sustain"]);
    bytes[14] = fromNorm(state["release"]);
    bytes[15] = fromNorm(state["subOsc"]);
    
    uint8_t sw1 = (uint8_t)((int)state["dcoRange"] & 0x07);
    if ((bool)state["pulseOn"]) sw1 |= (1 << 3);
    if ((bool)state["sawOn"])   sw1 |= (1 << 4);
    if ((bool)state["chorus1"]) sw1 |= (1 << 5);
    if ((bool)state["chorus2"]) sw1 |= (1 << 6);
    bytes[16] = sw1;

    uint8_t sw2 = 0;
    if ((bool)state["pwmMode"]) sw2 |= (1 << 0);
    if ((bool)state["vcaMode"]) sw2 |= (1 << 1);
    if ((bool)state["vcfPolarity"]) sw2 |= (1 << 2);
    sw2 |= (((int)state["hpfFreq"] & 0x03) << 3);
    bytes[17] = sw2;

    return bytes;
}

juce::File PresetManager::getUserPresetsDirectory() const {
    auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("ABD")
        .getChildFile("JUNiO-601")
        .getChildFile("Presets");
    if (!dir.exists()) dir.createDirectory();
    return dir;
}

juce::Result PresetManager::loadTape(const juce::File& wavFile) {
    auto result = JunoTapeDecoder::decodeWavFile(wavFile);
    if (!result.success) return juce::Result::fail(result.errorMessage);
    
    this->addLibrary(wavFile.getFileNameWithoutExtension());
    int libIdx = (int)this->libraries_.size() - 1;
    // Data is a flat vector of bytes, 18 bytes per patch
    for (size_t i = 0; i + 17 < result.data.size(); i += 18) {
        this->libraries_[libIdx].patches.push_back(createPresetFromJunoBytes("Tape Patch " + juce::String((i/18)+1), &result.data[i]));
    }
    return juce::Result::ok();
}

void PresetManager::addLibraryFromSysEx(const uint8_t* data, int size) {
    juce::ignoreUnused(data, size);
}

juce::Result PresetManager::importPresetsFromFile(const juce::File& file) {
    auto vt = juce::ValueTree::fromXml(file.loadFileAsString());
    if (!vt.isValid()) return juce::Result::fail("Invalid XML preset file");
    
    ABD::Preset p;
    p.fromValueTree(vt);
    
    int userIdx = this->getLibraryIndex("User");
    if (userIdx < 0) { this->addLibrary("User"); userIdx = (int)this->libraries_.size() - 1; }
    
    this->libraries_[userIdx].patches.push_back(p);
    ABD::PresetManagerBase::writeUserPresets();
    return juce::Result::ok();
}

void PresetManager::randomizeCurrentParameters(juce::AudioProcessorValueTreeState& apvts) {
    auto& random = juce::Random::getSystemRandom();
    for (auto* param : apvts.processor.getParameters()) {
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param)) {
            p->setValueNotifyingHost(random.nextFloat());
        }
    }
}

void PresetManager::triggerMemoryCorruption(juce::AudioProcessorValueTreeState& apvts) {
    randomizeCurrentParameters(apvts);
}

void PresetManager::exportCurrentPresetToJson(const juce::File& file) {
    auto p = this->getCurrentPreset();
    auto vt = p.toValueTree();
    file.replaceWithText(vt.toXmlString());
}

void PresetManager::exportCurrentPresetToTape(const juce::File& file) {
    auto p = this->getCurrentPreset();
    auto bytes = stateToBytes(p.state);
    
    auto buffer = JunoTapeEncoder::encodePatches({bytes}, 44100.0);
    
    juce::WavAudioFormat wavFormat;
    if (auto writer = std::unique_ptr<juce::AudioFormatWriter>(wavFormat.createWriterFor(new juce::FileOutputStream(file), 44100.0, 1, 16, {}, 0)))
    {
        writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
    }
}

void PresetManager::exportLibraryToJson(const juce::File& file) {
    auto vt = toValueTree();
    file.replaceWithText(vt.toXmlString());
}

ABD::Preset PresetManager::createPresetFromJunoPatch(const struct JunoPatch& p) {
    uint8_t b[18];
    b[0] = p.lfoRate; b[1] = p.lfoDelay; b[2] = p.lfoToDCO; b[3] = p.pwm; b[4] = p.noise;
    b[5] = p.vcfFreq; b[6] = p.resonance; b[7] = p.envAmount; b[8] = p.lfoToVCF;
    b[9] = p.kybdTracking; b[10] = p.vcaLevel; b[11] = p.attack; b[12] = p.decay;
    b[13] = p.sustain; b[14] = p.release; b[15] = p.subOsc; b[16] = p.sw1; b[17] = p.sw2;
    
    auto state = bytesToState(b, 18);
    ABD::Preset preset;
    preset.name = p.name;
    preset.category = "Factory";
    preset.state = state;

    for (int i = 0; i < 128; ++i) {
        if (&junoFactoryPatches[i] == &p) {
            preset.originGroup = i / 64;
            preset.originBank  = ((i % 64) / 8) + 1;
            preset.originPatch = (i % 8) + 1;
            break;
        }
    }
    return preset;
}

ABD::Preset PresetManager::createPresetFromJunoBytes(const juce::String& name, const unsigned char* bytes) {
    auto state = bytesToState(bytes, 18);
    ABD::Preset preset;
    preset.name = name;
    preset.category = "Imported";
    preset.state = state;
    return preset;
}

void PresetManager::selectPreset(int libIdx, int presetIdx) {
    this->selectLibrary(libIdx);
    this->setCurrentPreset(presetIdx);
}

void PresetManager::selectPresetByBankAndPatch(int group, int bank, int patch) {
    int factoryIdx = this->getLibraryIndex("Factory");
    if (factoryIdx < 0) return;
    int absIdx = (group * 64) + ((bank - 1) * 8) + (patch - 1);
    this->selectPreset(factoryIdx, absIdx);
}

juce::Result PresetManager::saveCurrentPresetFromState(juce::AudioProcessorValueTreeState& apvts) {
    if (this->currentLibIdx_ < 0 || this->currentPresetIdx_ < 0) return juce::Result::fail("Selection error");
    this->libraries_[this->currentLibIdx_].patches[this->currentPresetIdx_].state = apvts.copyState();
    ABD::PresetManagerBase::writeUserPresets();
    this->saveBrowserData();
    return juce::Result::ok();
}

juce::Result PresetManager::saveAsNewPresetFromState(juce::AudioProcessorValueTreeState& apvts, const juce::String& newName, const juce::String& category, const juce::String& author, const juce::String& tags, const juce::String& notes) {
    int userIdx = this->getLibraryIndex("User");
    if (userIdx < 0) { this->addLibrary("User"); userIdx = (int)this->libraries_.size() - 1; }
    
    ABD::Preset p;
    p.name = newName;
    p.category = category.isEmpty() ? "User" : category;
    p.author = author;
    p.tags = tags;
    p.notes = notes;
    p.creationDate = juce::Time::getCurrentTime().toString(true, true);
    p.isFavorite = false;
    p.state = apvts.copyState();
    
    this->libraries_[userIdx].patches.push_back(p);
    ABD::PresetManagerBase::writeUserPresets();
    this->saveBrowserData();
    return juce::Result::ok();
}

juce::ValueTree PresetManager::toValueTree() const {
    juce::ValueTree root("BankManager");
    root.setProperty("currentLib", this->currentLibIdx_, nullptr);
    root.setProperty("currentPreset", this->currentPresetIdx_, nullptr);
    for (const auto& lib : this->libraries_) {
        juce::ValueTree libVT("Library");
        libVT.setProperty("name", lib.name, nullptr);
        for (const auto& p : lib.patches) libVT.addChild(p.toValueTree(), -1, nullptr);
        root.addChild(libVT, -1, nullptr);
    }
    return root;
}

void PresetManager::saveBrowserData() {
    auto dir = this->getUserPresetsDirectory();
    auto file = dir.getSiblingFile("browser_state.xml");
    juce::Logger::writeToLog("[JUNiO] Saving browser state to: " + file.getFullPathName());
    auto vt = toValueTree();
    std::unique_ptr<juce::XmlElement> xml (vt.createXml());
    if (xml != nullptr) {
        if (xml->writeTo(file))
            juce::Logger::writeToLog("[JUNiO] Browser state saved successfully");
        else
            juce::Logger::writeToLog("[JUNiO] FAILED to save browser state to: " + file.getFullPathName());
    }
}

void PresetManager::loadBrowserData() {
    auto dir = this->getUserPresetsDirectory();
    auto file = dir.getSiblingFile("browser_state.xml");
    if (file.existsAsFile()) {
        std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse(file));
        if (xml != nullptr) fromValueTree(juce::ValueTree::fromXml(*xml));
    } else {
        // Only load legacy JSONs if advanced state is missing
        ABD::PresetManagerBase::loadBrowserData();
    }
}

void PresetManager::fromValueTree(const juce::ValueTree& vt) {
    if (!vt.hasType("BankManager")) return;
    this->libraries_.clear();
    loadFactoryPresets(); // Ensure factory presets are always present
    for (int i = 0; i < vt.getNumChildren(); ++i) {
        auto libVT = vt.getChild(i);
        if (libVT.hasType("Library")) {
            juce::String libName = libVT.getProperty("name", "Unknown");
            if (libName == "Factory") continue; // Skip as we just loaded it
            
            ABD::Library lib;
            lib.name = libName;
            for (int j = 0; j < libVT.getNumChildren(); ++j) {
                auto pVT = libVT.getChild(j);
                if (pVT.hasType("Preset")) { ABD::Preset p; p.fromValueTree(pVT); lib.patches.push_back(p); }
            }
            this->libraries_.push_back(lib);
        }
    }
    this->currentLibIdx_ = juce::jlimit(0, std::max(0, (int)this->libraries_.size() - 1), (int)vt.getProperty("currentLib", 0));
    this->currentPresetIdx_ = (int)vt.getProperty("currentPreset", 0);
}

std::vector<const ABD::Preset*> PresetManager::getFilteredPresets(const juce::String& category, const juce::String& searchText, bool favoritesOnly) const {
    std::vector<const ABD::Preset*> filtered;
    for (const auto& lib : this->libraries_) {
        for (const auto& p : lib.patches) {
            if (favoritesOnly && !p.isFavorite) continue;
            if (category.isNotEmpty() && category != "All" && p.category != category) continue;
            if (searchText.isNotEmpty() && !p.name.containsIgnoreCase(searchText) && !p.author.containsIgnoreCase(searchText) && !p.tags.containsIgnoreCase(searchText)) continue;
            filtered.push_back(&p);
        }
    }
    return filtered;
}

void PresetManager::setFavorite(int libIdx, int presetIdx, bool isFav) {
    if (libIdx >= 0 && libIdx < (int)this->libraries_.size() && presetIdx >= 0 && presetIdx < (int)this->libraries_[libIdx].patches.size()) {
        this->libraries_[libIdx].patches[presetIdx].isFavorite = isFav;
        if (this->libraries_[libIdx].name == "User") ABD::PresetManagerBase::writeUserPresets();
        this->saveBrowserData();
    }
}

void PresetManager::updateMetadata(int libIdx, int presetIdx, const juce::String& newName, const juce::String& author, const juce::String& tags, const juce::String& notes) {
    if (libIdx >= 0 && libIdx < (int)this->libraries_.size() && presetIdx >= 0 && presetIdx < (int)this->libraries_[libIdx].patches.size()) {
        auto& p = this->libraries_[libIdx].patches[presetIdx];
        if (newName.isNotEmpty()) p.name = newName;
        p.author = author; p.tags = tags; p.notes = notes;
        if (this->libraries_[libIdx].name == "User") ABD::PresetManagerBase::writeUserPresets();
        this->saveBrowserData();
    }
}

void PresetManager::nextBank() {
    int factoryIdx = getLibraryIndex("Factory");
    if (currentLibIdx_ != factoryIdx) return;
    int g = currentPresetIdx_ / 64;
    int b = ((currentPresetIdx_ % 64) / 8) + 1;
    int p = (currentPresetIdx_ % 8) + 1;
    b++; if (b > 8) b = 1;
    selectPresetByBankAndPatch(g, b, p);
}

void PresetManager::prevBank() {
    int factoryIdx = getLibraryIndex("Factory");
    if (currentLibIdx_ != factoryIdx) return;
    int g = currentPresetIdx_ / 64;
    int b = ((currentPresetIdx_ % 64) / 8) + 1;
    int p = (currentPresetIdx_ % 8) + 1;
    b--; if (b < 1) b = 8;
    selectPresetByBankAndPatch(g, b, p);
}

void PresetManager::nextPatch() {
    int factoryIdx = getLibraryIndex("Factory");
    if (currentLibIdx_ != factoryIdx) return;
    int g = currentPresetIdx_ / 64;
    int b = ((currentPresetIdx_ % 64) / 8) + 1;
    int p = (currentPresetIdx_ % 8) + 1;
    p++; if (p > 8) p = 1;
    selectPresetByBankAndPatch(g, b, p);
}

void PresetManager::prevPatch() {
    int factoryIdx = getLibraryIndex("Factory");
    if (currentLibIdx_ != factoryIdx) return;
    int g = currentPresetIdx_ / 64;
    int b = ((currentPresetIdx_ % 64) / 8) + 1;
    int p = (currentPresetIdx_ % 8) + 1;
    p--; if (p < 1) p = 8;
    selectPresetByBankAndPatch(g, b, p);
}

