#include "PresetManager.h"
#include "FactoryPresets.h"
#include "JunoTapeDecoder.h"
#include "JunoTapeEncoder.h"
#include <fstream>

PresetManager::PresetManager() {
    loadFactoryPresets();
    ABD::PresetManagerBase::loadUserPresets();
    currentLibIdx_ = 0;
    currentPresetIdx_ = 0;
}

PresetManager::~PresetManager() = default;

void PresetManager::loadFactoryPresets() {
    addLibrary("Factory");
    int factoryIdx = (int)libraries_.size() - 1;
    libraries_[factoryIdx].patches.clear();
    for (const auto& patch : junoFactoryPatches) {
        libraries_[factoryIdx].patches.push_back(createPresetFromJunoPatch(patch));
    }
}

juce::ValueTree PresetManager::bytesToState(const uint8_t* data, int size) const {
    if (size < 18) return juce::ValueTree("Parameters");
    
    juce::ValueTree state("Parameters");
    auto toNorm = [](uint8_t b) { return static_cast<float>(b) / 127.0f; };
    
    state.setProperty("lfoRate", toNorm(data[0]), nullptr);
    state.setProperty("lfoDelay", toNorm(data[1]), nullptr);
    state.setProperty("lfoToDCO", toNorm(data[2]), nullptr);
    state.setProperty("pwm", toNorm(data[3]), nullptr);
    state.setProperty("noise", toNorm(data[4]), nullptr);
    state.setProperty("vcfFreq", toNorm(data[5]), nullptr);
    state.setProperty("resonance", toNorm(data[6]), nullptr);
    state.setProperty("envAmount", toNorm(data[7]), nullptr);
    state.setProperty("lfoToVCF", toNorm(data[8]), nullptr);
    state.setProperty("kybdTracking", toNorm(data[9]), nullptr);
    state.setProperty("vcaLevel", toNorm(data[10]), nullptr);
    state.setProperty("attack", toNorm(data[11]), nullptr);
    state.setProperty("decay", toNorm(data[12]), nullptr);
    state.setProperty("sustain", toNorm(data[13]), nullptr);
    state.setProperty("release", toNorm(data[14]), nullptr);
    state.setProperty("subOsc", toNorm(data[15]), nullptr);
    
    uint8_t sw1 = data[16];
    state.setProperty("dcoRange", (sw1 & 0x07), nullptr);
    state.setProperty("pulseOn", (sw1 & (1 << 3)) != 0, nullptr);
    state.setProperty("sawOn", (sw1 & (1 << 4)) != 0, nullptr);
    state.setProperty("chorus1", (sw1 & (1 << 5)) != 0, nullptr);
    state.setProperty("chorus2", (sw1 & (1 << 6)) != 0, nullptr);

    uint8_t sw2 = data[17];
    state.setProperty("pwmMode", (sw2 & (1 << 0)) != 0, nullptr);
    state.setProperty("vcaMode", (sw2 & (1 << 1)) != 0, nullptr);
    state.setProperty("vcfPolarity", (sw2 & (1 << 2)) != 0, nullptr); 
    state.setProperty("hpfFreq", 3 - ((sw2 >> 3) & 0x03), nullptr); // Reflect hardware inversion

    // Performance Defaults
    state.setProperty("benderToDCO", 2.0f, nullptr);
    state.setProperty("benderToVCF", 0.0f, nullptr);
    state.setProperty("benderToLFO", 0.0f, nullptr);
    state.setProperty("portamentoTime", 0.0f, nullptr);
    state.setProperty("portamentoOn", false, nullptr);
    state.setProperty("portamentoLegato", false, nullptr);

    return state;
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
    if ((bool)state["pwmMode"])     sw2 |= (1 << 0);
    if ((bool)state["vcaMode"])     sw2 |= (1 << 1);
    if ((bool)state["vcfPolarity"]) sw2 |= (1 << 2);
    int hwHpf = 3 - juce::jlimit(0, 3, (int)state["hpfFreq"]);
    sw2 |= (uint8_t)((hwHpf & 0x03) << 3);
    bytes[17] = sw2;
    
    return bytes;
}

juce::File PresetManager::getUserPresetsDirectory() const {
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("JUNiO601").getChildFile("UserPresets");
}

juce::Result PresetManager::loadTape(const juce::File& wavFile) {
    auto result = JunoTapeDecoder::decodeWavFile(wavFile);
    if (!result.success) return juce::Result::fail(result.errorMessage);
    setLastPath(wavFile.getParentDirectory().getFullPathName());
    addLibrary(wavFile.getFileNameWithoutExtension());
    int newLibIdx = (int)libraries_.size() - 1;
    int patchCount = juce::jmin((int)result.data.size() / 18, 128);
    for (int p = 0; p < patchCount; ++p) { 
        libraries_[newLibIdx].patches.push_back(createPresetFromJunoBytes(juce::String(p + 1).paddedLeft('0', 2), &result.data[p * 18]));
    }
    selectLibrary(newLibIdx);
    return juce::Result::ok();
}

PresetManager::Preset PresetManager::createPresetFromJunoPatch(const JunoPatch& p) {
    auto state = bytesToState(p.bytes, 18);
    return Preset(p.name, "Factory", state);
}

PresetManager::Preset PresetManager::createPresetFromJunoBytes(const juce::String& name, const unsigned char* bytes) {
    return Preset(name, "User", bytesToState(bytes, 18));
}

juce::Result PresetManager::importPresetsFromFile(const juce::File& file) {
    juce::MemoryBlock mb;
    if (file.getFileExtension().equalsIgnoreCase(".wav")) {
        auto decodeResult = JunoTapeDecoder::decodeWavFile(file);
        if (!decodeResult.success) return juce::Result::fail(decodeResult.errorMessage);
        mb.append(decodeResult.data.data(), decodeResult.data.size());
    } else {
        if (!file.loadFileAsData(mb)) return juce::Result::fail("Read error");
    }
    setLastPath(file.getParentDirectory().getFullPathName());
    const uint8_t* d = (const uint8_t*)mb.getData();
    int s = (int)mb.getSize();
    
    std::vector<std::vector<uint8_t>> found;
    for (int i=0; i < s - 22; ++i) {
        if (d[i] == 0xF0 && d[i+1] == 0x41 && d[i+2] == 0x30) {
            std::vector<uint8_t> p; for(int k=0; k<18; ++k) p.push_back(d[i+4+k]);
            found.push_back(p); i += 22;
        }
    }
    if (found.empty() && s >= 18) { std::vector<uint8_t> p; for(int k=0; k<18; ++k) p.push_back(d[k]); found.push_back(p); }
    if (found.empty()) return juce::Result::fail("No patches");

    if (found.size() > 1) {
        addLibrary(file.getFileNameWithoutExtension());
        int libIdx = (int)libraries_.size() - 1;
        for(int k=0; k<(int)found.size(); ++k)
            libraries_[libIdx].patches.push_back(createPresetFromJunoBytes(file.getFileNameWithoutExtension() + " " + juce::String(k+1).paddedLeft('0',2), found[k].data()));
        selectLibrary(libIdx);
    } else {
        saveUserPreset(file.getFileNameWithoutExtension(), bytesToState(found[0].data(), 18));
    }
    return juce::Result::ok();
}

void PresetManager::selectPresetByBankAndPatch(int g, int b, int p) { 
    currentPresetIdx_ = (g * 64) + ((b - 1) * 8) + (p - 1); 
}

juce::String PresetManager::getLastPath() const {
    juce::PropertiesFile::Options o; o.applicationName = "JUNiO601"; o.filenameSuffix = ".settings";
    juce::PropertiesFile props(o); return props.getValue("lastPath", juce::File::getSpecialLocation(juce::File::userHomeDirectory).getFullPathName());
}

void PresetManager::setLastPath(const juce::String& path) {
    juce::PropertiesFile::Options o; o.applicationName = "JUNiO601"; o.filenameSuffix = ".settings";
    juce::PropertiesFile props(o); props.setValue("lastPath", path); props.saveIfNeeded();
}

void PresetManager::setP(juce::AudioProcessorValueTreeState& apvts, juce::String id, float v) { 
    if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(v); 
}
void PresetManager::setI(juce::AudioProcessorValueTreeState& apvts, juce::String id, int v) { 
    if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1((float)v)); 
}
void PresetManager::setB(juce::AudioProcessorValueTreeState& apvts, juce::String id, bool v) { 
    if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(v ? 1.0f : 0.0f); 
}

void PresetManager::randomizeCurrentParameters(juce::AudioProcessorValueTreeState& apvts) {
    auto& r = juce::Random::getSystemRandom();
    setI(apvts, "dcoRange", r.nextInt(3));
    setI(apvts, "pwmMode", r.nextInt(2));
    setI(apvts, "vcfPolarity", r.nextInt(2));
    setI(apvts, "vcaMode", r.nextInt(2));
    setI(apvts, "hpfFreq", r.nextInt(4));
    
    setP(apvts, "pwm", r.nextFloat()); 
    setP(apvts, "subOsc", r.nextFloat() * 0.8f); 
    setP(apvts, "noise", r.nextFloat() * 0.3f);
    setP(apvts, "lfoToDCO", r.nextFloat() * 0.2f); 
    setP(apvts, "vcfFreq", 0.2f + (r.nextFloat() * 0.8f)); 
    setP(apvts, "resonance", r.nextFloat() * 0.7f);
    setP(apvts, "envAmount", r.nextFloat()); 
    setP(apvts, "lfoToVCF", r.nextFloat() * 0.4f); 
    setP(apvts, "kybdTracking", r.nextFloat());
    setP(apvts, "vcaLevel", 0.6f + (r.nextFloat() * 0.4f));
    setP(apvts, "attack", r.nextFloat() * 0.5f); 
    setP(apvts, "decay", 0.1f + r.nextFloat() * 0.9f); 
    setP(apvts, "sustain", 0.2f + r.nextFloat() * 0.8f);
    setP(apvts, "release", 0.1f + r.nextFloat() * 0.7f); 
    setP(apvts, "lfoRate", r.nextFloat()); 
    setP(apvts, "lfoDelay", r.nextFloat() * 0.5f);

    bool cOn = r.nextFloat() < 0.7f; 
    if (cOn) { 
        int m = r.nextInt(3); 
        setB(apvts, "chorus1", m == 0 || m == 2); 
        setB(apvts, "chorus2", m == 1 || m == 2); 
    } else { 
        setB(apvts, "chorus1", false); 
        setB(apvts, "chorus2", false); 
    }
    
    bool s = r.nextBool(); 
    setB(apvts, "sawOn", s); 
    setB(apvts, "pulseOn", !s || r.nextBool());
}

void PresetManager::triggerMemoryCorruption(juce::AudioProcessorValueTreeState& apvts) {
    auto& r = juce::Random::getSystemRandom();
    juce::StringArray sliders = {
        "lfoRate", "lfoDelay", "lfoToDCO", "pwm", "noise", "vcfFreq", "resonance",
        "envAmount", "lfoToVCF", "kybdTracking", "vcaLevel", "attack", "decay", "sustain", "release", "subOsc"
    };

    for (const auto& id : sliders) {
        if (r.nextFloat() < 0.2f) { 
            if (auto* p = apvts.getParameter(id)) {
                float v = p->getValue();
                uint8_t byte = static_cast<uint8_t>(v * 127.0f);
                if (r.nextBool()) byte ^= (1 << r.nextInt(7)); 
                else byte = (r.nextBool() ? 0x00 : 0x7F);
                p->setValueNotifyingHost(static_cast<float>(byte) / 127.0f);
            }
        }
    }
}

void PresetManager::exportCurrentPresetToTape(const juce::File& file) {
    if (currentLibIdx_ < 0 || currentPresetIdx_ < 0) return;
    auto patchBytes = stateToBytes(getPreset(currentPresetIdx_)->state);
    double sampleRate = 44100.0;
    auto audioBuffer = JunoTapeEncoder::encodePatches({ patchBytes }, sampleRate);
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::OutputStream> outStream = file.createOutputStream();
    if (outStream != nullptr) {
        auto options = juce::AudioFormatWriterOptions().withSampleRate(sampleRate).withNumChannels(1).withBitsPerSample(16);
        if (auto writer = std::unique_ptr<juce::AudioFormatWriter>(wavFormat.createWriterFor(outStream, options))) {
            writer->writeFromAudioSampleBuffer(audioBuffer, 0, audioBuffer.getNumSamples());
        }
    }
}
