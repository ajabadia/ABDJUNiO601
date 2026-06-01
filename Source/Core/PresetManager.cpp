#include "PresetManager.h"
#include "FactoryPresets.h"
#include "JunoConstants.h"
#include "JunoProtocol.h"
#include "JunoTapeDecoder.h"
#include "JunoTapeEncoder.h"
#include "Importers/TalImporter.h"
#include "Importers/JunoTapeImporter.h"
#include "Importers/JunoSysexImporter.h"
#include "Importers/JunoFormatConverter.h"
#include "Importers/JunoCsvImporter.h"
#include <cmath>
#include <fstream>

using Preset = ABD::Preset;
using Library = ABD::Library;

PresetManager::PresetManager() {
    this->libraries_.clear();
    
    // 1. Initialize all 26 slots
    for (int i = 0; i < kMaxLibraries; ++i) {
        ABD::Library lib;
        juce::String letter = juce::String::charToString((juce_wchar)('A' + i));
        lib.name = letter + " - Empty Bank";
        for (int p = 0; p < kMaxPatchesPerLibrary; ++p) {
            ABD::Preset init;
            init.name = "INIT PATCH";
            init.category = "Init";
            init.originGroup = i;
            init.originBank = (p / 8) + 1;
            init.originPatch = (p % 8) + 1;
            init.state = bytesToState(nullptr, 0);
            lib.patches.push_back(init);
        }
        this->libraries_.push_back(lib);
    }

    // 2. Load Content
    loadFactoryPresets();
    loadUserRam();
    loadBrowserData(); // This will merge existing user banks into slots D-Z
        
    this->currentLibIdx_ = 0;
    this->currentPresetIdx_ = 0;
}

PresetManager::~PresetManager() = default;

juce::ValueTree PresetManager::bytesToState(const uint8_t* data, int size) const {
    juce::ValueTree vt("Preset");
    auto toNorm = [](uint8_t val) { return juce::jlimit(0.0f, 1.0f, (float)val / 127.0f); };
    
    uint8_t safeData[18] = {0};
    safeData[3] = 64;   // PWM 50%
    safeData[5] = 127;  // VCF Freq open
    safeData[10] = 127; // VCA Level full
    safeData[11] = 2;   // Attack short
    safeData[12] = 20;  // Decay short
    safeData[13] = 0;   // Sustain 0 (plucked)
    safeData[14] = 2;   // Release short
    
    SynthParams defaultParams;
    defaultParams.dcoRange = 1; // 8'
    defaultParams.sawOn = true;
    defaultParams.pulseOn = false;
    defaultParams.pwmMode = 0; // LFO
    defaultParams.vcaMode = 0; // ENV (critical to prevent infinite sustain)
    defaultParams.hpfFreq = 0;
    
    safeData[16] = JunoProtocol::encodeSW1(defaultParams);
    safeData[17] = JunoProtocol::encodeSW2(defaultParams);

    const uint8_t* src = (data != nullptr && size >= 18) ? data : safeData;
    
    vt.setProperty("lfoRate", toNorm(src[0]), nullptr);
    vt.setProperty("lfoDelay", toNorm(src[1]), nullptr);
    vt.setProperty("lfoToDCO", toNorm(src[2]), nullptr);
    vt.setProperty("pwm", toNorm(src[3]), nullptr);
    vt.setProperty("noise", toNorm(src[4]), nullptr);
    vt.setProperty("vcfFreq", toNorm(src[5]), nullptr);
    vt.setProperty("resonance", toNorm(src[6]), nullptr);
    vt.setProperty("envAmount", toNorm(src[7]), nullptr);
    vt.setProperty("lfoToVCF", toNorm(src[8]), nullptr);
    vt.setProperty("kybdTracking", toNorm(src[9]), nullptr);
    vt.setProperty("vcaLevel", toNorm(src[10]), nullptr);
    vt.setProperty("attack", toNorm(src[11]), nullptr);
    vt.setProperty("decay", toNorm(src[12]), nullptr);
    vt.setProperty("sustain", toNorm(src[13]), nullptr);
    vt.setProperty("release", toNorm(src[14]), nullptr);
    vt.setProperty("subOsc", toNorm(src[15]), nullptr);
    
    SynthParams p;
    JunoProtocol::decodeSW1(src[16], p);
    JunoProtocol::decodeSW2(src[17], p);

    // [Hardened Safety Gate] Ensure VCA is in ENV mode for initialized patches
    if (data == nullptr) {
        vt.setProperty("vcaMode", 0, nullptr); // Force ENV
        vt.setProperty("release", toNorm(2), nullptr); // Force short release
    } else {
        vt.setProperty("vcaMode", p.vcaMode, nullptr);
    }

    vt.setProperty("dcoRange", p.dcoRange, nullptr);
    vt.setProperty("pulseOn", p.pulseOn, nullptr);
    vt.setProperty("sawOn", p.sawOn, nullptr);
    vt.setProperty("chorus1", p.chorus1, nullptr);
    vt.setProperty("chorus2", p.chorus2, nullptr);
    vt.setProperty("pwmMode", p.pwmMode, nullptr);
    vt.setProperty("vcfPolarity", p.vcfPolarity, nullptr);
    vt.setProperty("hpfFreq", p.hpfFreq, nullptr);

    // Modular Model Routing Defaults (0 = J6, 1 = J60, 2 = J106)
    vt.setProperty("modelDCO", 2, nullptr);
    vt.setProperty("modelHPF", 2, nullptr);
    vt.setProperty("modelVCF", 2, nullptr);
    vt.setProperty("modelADSR", 2, nullptr);
    vt.setProperty("modelChorus", 2, nullptr);
    vt.setProperty("modelArp", 0, nullptr);
    vt.setProperty("modelPoly", 2, nullptr);
    vt.setProperty("modelPorta", 2, nullptr);
    vt.setProperty("modelUnison", 2, nullptr);

    // Arpeggiator Defaults
    vt.setProperty("arpEnabled", false, nullptr);
    vt.setProperty("arpMode", 0, nullptr);
    vt.setProperty("arpRange", 0, nullptr);
    vt.setProperty("arpRate", 0.5f, nullptr);
    vt.setProperty("arpSync", false, nullptr);
    vt.setProperty("arpDivision", 6, nullptr);

    // Performance Defaults
    vt.setProperty("benderToDCO", 0.0f, nullptr); 
    vt.setProperty("benderToVCF", 0.0f, nullptr);
    vt.setProperty("benderToLFO", 0.0f, nullptr);
    vt.setProperty("portamentoTime", 0.0f, nullptr);
    vt.setProperty("portamentoOn", false, nullptr);
    vt.setProperty("portamentoLegato", false, nullptr);

    return vt;
}

std::vector<uint8_t>
PresetManager::stateToBytes(const juce::ValueTree &state) const {
  std::vector<uint8_t> bytes(18, 0);
  auto fromNorm = [](float val) {
    return static_cast<uint8_t>(
        juce::jlimit(0, 127, (int)std::round(val * 127.0f)));
  };

  bytes[0] = fromNorm(state.getProperty("lfoRate", 0.5f));
  bytes[1] = fromNorm(state.getProperty("lfoDelay", 0.0f));
  bytes[2] = fromNorm(state.getProperty("lfoToDCO", 0.0f));
  bytes[3] = fromNorm(state.getProperty("pwm", 0.0f));
  bytes[4] = fromNorm(state.getProperty("noise", 0.0f));
  bytes[5] = fromNorm(state.getProperty("vcfFreq", 1.0f));
  bytes[6] = fromNorm(state.getProperty("resonance", 0.0f));
  bytes[7] = fromNorm(state.getProperty("envAmount", 0.5f));
  bytes[8] = fromNorm(state.getProperty("lfoToVCF", 0.0f));
  bytes[9] = fromNorm(state.getProperty("kybdTracking", 0.0f));
  bytes[10] = fromNorm(state.getProperty("vcaLevel", 1.0f));
  bytes[11] = fromNorm(state.getProperty("attack", 0.0f));
  bytes[12] = fromNorm(state.getProperty("decay", 0.0f));
  bytes[13] = fromNorm(state.getProperty("sustain", 1.0f));
  bytes[14] = fromNorm(state.getProperty("release", 0.0f));
  bytes[15] = fromNorm(state.getProperty("subOsc", 0.0f));

  // Map State back to SynthParams to use JunoProtocol
  SynthParams p;
  p.dcoRange = (int)std::round((float)state.getProperty("dcoRange", 1));
  p.pulseOn = (bool)state.getProperty("pulseOn", false);
  p.sawOn = (bool)state.getProperty("sawOn", true);
  p.chorus1 = (bool)state.getProperty("chorus1", false);
  p.chorus2 = (bool)state.getProperty("chorus2", false);
  p.pwmMode = (int)std::round((float)state.getProperty("pwmMode", 0));
  p.vcfPolarity = (int)std::round((float)state.getProperty("vcfPolarity", 0));
  p.vcaMode = (int)std::round((float)state.getProperty("vcaMode", 0));
  p.hpfFreq = (int)std::round((float)state.getProperty("hpfFreq", 0));

  bytes[16] = JunoProtocol::encodeSW1(p);
  bytes[17] = JunoProtocol::encodeSW2(p);

  return bytes;
}

juce::File PresetManager::getUserPresetsDirectory() const {
  auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                 .getChildFile("ABD")
                 .getChildFile("JUNiO-601")
                 .getChildFile("Presets");
  if (!dir.exists())
    dir.createDirectory();
  return dir;
}

PresetManager::ImportResult PresetManager::loadTape(const juce::File &wavFile) {
  auto result = ABD::JunoTapeImporter::loadFromFile(wavFile);
  if (result.result.failed())
      return { false, result.result.getErrorMessage() };

  int banksNeeded = (int)result.libraries.size();
  auto emptyIndices = findEmptyBankIndices(banksNeeded);

  if (emptyIndices.size() < (size_t)banksNeeded) {
      return { false, "Error: Not enough empty banks to load all tape data (Max 26)." };
  }

  for (size_t i = 0; i < (size_t)banksNeeded; ++i) {
      int targetIdx = emptyIndices[i];
      libraries_[targetIdx].patches = result.libraries[i].patches;
      libraries_[targetIdx].name = getLibraryLetter(targetIdx) + " - " + result.libraries[i].name;
  }
  
  saveBrowserData();
  return { true, "Tape loaded successfully into " + juce::String(banksNeeded) + " banks." };
}

void PresetManager::addLibraryFromSysEx(const uint8_t *data, int size) {
    juce::ignoreUnused(data, size);
    // This is for live SysEx reception, but we use the File Importer for .syx files
}

PresetManager::ImportResult PresetManager::importPresetsFromFile(const juce::File &file, bool ignoreSelection) {
  juce::String ext = file.getFileExtension().toLowerCase();
  
  if (ext == ".pjunoxl") {
    auto res = ABD::TalImporter::loadFromFile(file);
    if (res.result.failed()) return { false, res.result.getErrorMessage() };
    
    if (res.presets.size() == 1) {
      int slot = -1;
      bool overwriting = false;
      if (!ignoreSelection && currentLibIdx_ >= 2) {
        slot = (currentLibIdx_ << 16) | currentPresetIdx_;
        overwriting = true;
      } else {
        slot = findFirstEmptySlot();
      }

      if (slot < 0) return { false, "Error: Out of user preset space (Banks C-Z full)." };
      
      int libIdx = slot >> 16;
      int prstIdx = slot & 0xFFFF;
      
      libraries_[libIdx].patches[prstIdx] = res.presets[0];
      if (libraries_[libIdx].name.contains("Empty Bank")) {
        libraries_[libIdx].name = getLibraryLetter(libIdx) + " - TAL Import";
      }
      
      currentLibIdx_ = libIdx;
      currentPresetIdx_ = prstIdx;
      saveBrowserData();
      return { true, "Preset imported into " + getLibraryLetter(libIdx) + (overwriting ? " (Overwritten current)" : " (First empty slot)") };
    } else {
      auto emptyIndices = findEmptyBankIndices(1);
      if (emptyIndices.empty()) return { false, "Error: No empty banks available for bulk import." };
      
      int targetIdx = emptyIndices[0];
      libraries_[targetIdx].patches = res.presets;
      libraries_[targetIdx].name = getLibraryLetter(targetIdx) + " - TAL " + file.getFileNameWithoutExtension();
      
      while (libraries_[targetIdx].patches.size() < 64) {
        ABD::Preset init;
        init.name = "INIT PATCH";
        libraries_[targetIdx].patches.push_back(init);
      }
      
      currentLibIdx_ = targetIdx;
      currentPresetIdx_ = 0;
      saveBrowserData();
      return { true, "Full bank imported into " + getLibraryLetter(targetIdx) };
    }
  }
  
  if (ext == ".syx" || ext == ".mid") {
    auto res = ABD::JunoSysexImporter::loadFromFile(file);
    if (res.result.failed()) return { false, res.result.getErrorMessage() };
    
    int banksNeeded = (int)res.libraries.size();
    auto emptyIndices = findEmptyBankIndices(banksNeeded);
    if (emptyIndices.size() < (size_t)banksNeeded) {
      return { false, "Error: Not enough empty banks (Needed " + juce::String(banksNeeded) + ")." };
    }

    for (int i = 0; i < banksNeeded; ++i) {
      int targetIdx = emptyIndices[i];
      libraries_[targetIdx].patches = res.libraries[i].patches;
      libraries_[targetIdx].name = getLibraryLetter(targetIdx) + " - " + res.libraries[i].name;
    }
    
    saveBrowserData();
    return { true, "Imported " + juce::String(banksNeeded) + " banks from SysEx." };
  }

  if (ext == ".wav" || ext == ".aif") {
    return loadTape(file);
  }

  if (ext == ".csv") {
    auto res = ABD::JunoCsvImporter::loadFromFile(file);
    if (res.result.failed()) return { false, res.result.getErrorMessage() };

    int banksNeeded = (int)std::ceil(res.presets.size() / 64.0f);
    if (banksNeeded == 0) banksNeeded = 1;
    auto emptyIndices = findEmptyBankIndices(banksNeeded);
    if (emptyIndices.size() < (size_t)banksNeeded) {
      return { false, "Error: Not enough empty banks (Needed " + juce::String(banksNeeded) + ")." };
    }

    for (int i = 0; i < banksNeeded; ++i) {
      int targetIdx = emptyIndices[i];
      int startIdx = i * 64;
      int endIdx = std::min(startIdx + 64, (int)res.presets.size());
      
      libraries_[targetIdx].patches.clear();
      for (int p = startIdx; p < endIdx; ++p) {
         libraries_[targetIdx].patches.push_back(res.presets[p]);
      }
      libraries_[targetIdx].name = getLibraryLetter(targetIdx) + " - CSV Import";
      
      while (libraries_[targetIdx].patches.size() < 64) {
        ABD::Preset init;
        init.name = "INIT PATCH";
        libraries_[targetIdx].patches.push_back(init);
      }
    }
    saveBrowserData();
    return { true, "Imported " + juce::String((int)res.presets.size()) + " presets from CSV." };
  }

  // Native XML Logic
  auto xmlString = file.loadFileAsString();
  std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(xmlString));
  if (xml == nullptr) return { false, "Invalid XML format" };
  
  auto vt = juce::ValueTree::fromXml(*xml);
  if (!vt.isValid()) return { false, "Invalid XML preset file" };

  ABD::Preset p;
  p.fromValueTree(vt);

  int slot = -1;
  if (!ignoreSelection && currentLibIdx_ >= 2) slot = (currentLibIdx_ << 16) | currentPresetIdx_;
  else slot = findFirstEmptySlot();

  if (slot < 0) return { false, "Error: Out of user preset space." };
  
  int libIdx = slot >> 16;
  int prstIdx = slot & 0xFFFF;
  
  if (libIdx < 0 || libIdx >= (int)libraries_.size()) return { false, "Error: Invalid destination bank." };

  libraries_[libIdx].patches[prstIdx] = p;
  
  if (libraries_[libIdx].name.contains("Empty Bank")) 
    libraries_[libIdx].name = getLibraryLetter(libIdx) + " - User Imports";
      
  saveBrowserData();
  return { true, "Preset imported successfully into " + getLibraryLetter(libIdx) };
}

void PresetManager::randomizeCurrentParameters(
    juce::AudioProcessorValueTreeState &apvts) {
  auto &random = juce::Random::getSystemRandom();

  // Whitelist for musical randomization (excluding system, master and hidden
  // settings)
  static const juce::StringArray whitelist{
      "sawOn",     "pulseOn",  "pwm",         "subOsc",  "noise",
      "dcoRange",  "vcfFreq",  "resonance",   "hpfFreq", "kybdTracking",
      "envAmount", "lfoToVCF", "vcfPolarity", "vcaMode", "vcaLevel",
      "lfoRate",   "lfoDelay", "lfoToDCO",    "attack",  "decay",
      "sustain",   "release",  "chorus1",     "chorus2", "pwmMode"};

  for (auto *param : apvts.processor.getParameters()) {
    if (auto *p = dynamic_cast<juce::AudioProcessorParameterWithID *>(param)) {
      juce::String id = p->getParameterID();
      if (whitelist.contains(id)) {
        float val = random.nextFloat();

        // [IA Musical] Apply synthesis constraints to avoid digital artifacts
        if (id == "resonance")
          val *= 0.88f; // Avoid extreme screaming feedback
        if (id == "lfoRate")
          val *= 0.85f; // Avoid high-speed FM noise
        if (id == "attack")
          val = 0.002f + val * 0.998f; // Avoid hard digital clicks
        if (id == "subOsc")
          val *= 0.80f; // Avoid extreme bass clipping
        if (id == "noise")
          val *= 0.50f; // Keep noise level controlled

        p->setValueNotifyingHost(val);
      }
    }
  }

  // [Safety Check] Ensure at least one primary oscillator is active
  auto *saw = dynamic_cast<juce::AudioProcessorParameterWithID *>(
      apvts.getParameter("sawOn"));
  auto *pulse = dynamic_cast<juce::AudioProcessorParameterWithID *>(
      apvts.getParameter("pulseOn"));
  auto *sub = dynamic_cast<juce::AudioProcessorParameterWithID *>(
      apvts.getParameter("subOsc"));

  if (saw && pulse && sub) {
    if (saw->getValue() < 0.5f && pulse->getValue() < 0.5f &&
        sub->getValue() < 0.2f) {
      // Force one primary source if the patch is too silent/noisy
      if (random.nextBool())
        saw->setValueNotifyingHost(1.0f);
      else
        pulse->setValueNotifyingHost(1.0f);
    }
  }
}

void PresetManager::triggerMemoryCorruption(
    juce::AudioProcessorValueTreeState &apvts) {
  randomizeCurrentParameters(apvts);
}

void PresetManager::exportCurrentPresetToJson(const juce::File &file) {
  auto p = this->getCurrentPreset();
  auto vt = p.toValueTree();
  file.replaceWithText(vt.toXmlString());
}

void PresetManager::exportCurrentPresetToTape(const juce::File &file) {
  auto p = this->getCurrentPreset();
  auto bytes = stateToBytes(p.state);
  auto buffer = JunoTapeEncoder::encodePatches({bytes}, 44100.0);

  auto options = juce::AudioFormatWriterOptions()
                     .withSampleRate(44100.0)
                     .withNumChannels(1)
                     .withBitsPerSample(16);

  std::unique_ptr<juce::OutputStream> outStream(file.createOutputStream());
  if (outStream != nullptr) {
    juce::WavAudioFormat wavFormat;
    if (auto writer = wavFormat.createWriterFor(outStream, options)) {
      writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
    }
  }
}

void PresetManager::exportLibraryToJson(const juce::File &file) {
  auto vt = toValueTree();
  file.replaceWithText(vt.toXmlString());
}

void PresetManager::exportLibraryToCSV(const juce::File &file) {
  if (this->getActiveLibraryIndex() < 0) return;
  auto& patches = this->libraries_[this->getActiveLibraryIndex()].patches;
  ABD::JunoCsvImporter::exportToFile(file, patches);
}

ABD::Preset
PresetManager::createPresetFromJunoPatch(const struct JunoPatch &p) {
    return ABD::JunoFormatConverter::createPresetFromJunoPatch(p);
}

ABD::Preset
PresetManager::createPresetFromJunoBytes(const juce::String &name,
                                         const unsigned char *bytes) {
    return ABD::JunoFormatConverter::createPresetFromJunoBytes(name, bytes, juce::ValueTree());
}

void PresetManager::selectPresetByBankAndPatch(int group, int bank, int patch) {
    // Standard Juno Mapping: 8 banks of 8 patches
    int libIdx = group;
    int pIdx = ((bank - 1) * 8) + (patch - 1);
    selectPreset(libIdx, pIdx);
}

juce::Result PresetManager::saveCurrentPresetFromState(
    juce::AudioProcessorValueTreeState &apvts) {
  if (this->getActiveLibraryIndex() < 0 || this->getCurrentPresetIndex() < 0)
    return juce::Result::fail("Selection error");
  this->libraries_[this->getActiveLibraryIndex()]
      .patches[this->getCurrentPresetIndex()]
      .state = apvts.copyState();
  ABD::PresetManagerBase::writeUserPresets();
  this->saveBrowserData();
  return juce::Result::ok();
}

juce::Result PresetManager::saveAsNewPresetFromState(
    juce::AudioProcessorValueTreeState &apvts, const juce::String &newName,
    const juce::String &category, const juce::String &author,
    const juce::String &tags, const juce::String &notes) {
  int userIdx = this->getLibraryIndex("User");
  if (userIdx < 0) {
    this->addLibrary("User");
    userIdx = (int)this->libraries_.size() - 1;
  }

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
  root.setProperty("currentLib", this->getActiveLibraryIndex(), nullptr);
  root.setProperty("currentPreset", this->getCurrentPresetIndex(), nullptr);
  for (const auto &lib : this->libraries_) {
    juce::ValueTree libVT("Library");
    libVT.setProperty("name", lib.name, nullptr);
    for (const auto &p : lib.patches)
      libVT.addChild(p.toValueTree(), -1, nullptr);
    root.addChild(libVT, -1, nullptr);
  }
  return root;
}

void PresetManager::saveBrowserData() {
  auto dir = getUserPresetsDirectory();
  if (!dir.exists()) dir.createDirectory();
  
  auto file = dir.getChildFile("browser_state.xml");
  juce::Logger::writeToLog("[JUNiO] Saving browser state to: " + file.getFullPathName());

  juce::ValueTree root("BankManager");
  root.setProperty("currentLib", currentLibIdx_, nullptr);
  root.setProperty("currentPreset", currentPresetIdx_, nullptr);
  
  juce::String cats;
  for (const auto& c : categories_) {
      if (cats.isNotEmpty()) cats += ",";
      cats += c;
  }
  root.setProperty("categories", cats, nullptr);

  for (const auto& lib : libraries_) {
      juce::ValueTree libVT("Library");
      libVT.setProperty("name", lib.name, nullptr);
      for (const auto& p : lib.patches)
          libVT.addChild(p.toValueTree(), -1, nullptr);
      root.addChild(libVT, -1, nullptr);
  }
  
  file.replaceWithText(root.toXmlString());
}

void PresetManager::loadBrowserData() {
    auto dir = getUserPresetsDirectory();
    auto file = dir.getChildFile("browser_state.xml");
    
    if (!file.existsAsFile()) {
        ABD::PresetManagerBase::loadBrowserData();
        return;
    }
    
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr) return;
    
    auto vt = juce::ValueTree::fromXml(*xml);
    if (!vt.isValid() || !vt.hasType("BankManager")) return;
    
    fromValueTree(vt);
    juce::Logger::writeToLog("[JUNiO] Browser State Loaded from Disk");
}

void PresetManager::fromValueTree(const juce::ValueTree& vt) {
    if (!vt.hasType("BankManager")) return;
    
    // We already have 26 libraries initialized in constructor.
    // We will now merge loaded libraries into the existing slots.
    
    for (int i = 0; i < vt.getNumChildren(); ++i) {
        auto libVT = vt.getChild(i);
        if (libVT.hasType("Library")) {
            juce::String libName = libVT.getProperty("name", "Unknown");
            
            // Extract index from name if it follows "Letter - Name"
            int targetIdx = -1;
            if (libName.length() >= 4 && libName[1] == ' ' && libName[2] == '-' && libName[3] == ' ') {
                targetIdx = (int)(libName[0] - 'A');
            } else {
                // Legacy support: try to find by name or just skip to next available user slot
                if (libName == "Factory" || libName == "Factory A") targetIdx = 0;
                else if (libName == "Factory B") targetIdx = 1;
                else if (libName == "INTERNAL RAM") targetIdx = 2;
            }

            if (targetIdx >= 0 && targetIdx < kMaxLibraries) {
                // Merge patches into existing slot
                auto& lib = this->libraries_[targetIdx];
                
                // Enforce valid library naming convention for persistence
                if (targetIdx >= 3) {
                    lib.name = libName;
                    ensureValidLibraryName(targetIdx);
                }

                lib.patches.clear();
                for (int j = 0; j < libVT.getNumChildren() && j < kMaxPatchesPerLibrary; ++j) {
                    auto pVT = libVT.getChild(j);
                    if (pVT.hasType("Preset")) { 
                        ABD::Preset p; 
                        p.fromValueTree(pVT); 
                        p.originGroup = targetIdx;
                        p.originBank = (j / 8) + 1;
                        p.originPatch = (j % 8) + 1;
                        lib.patches.push_back(p); 
                    }
                }
                
                // Fill rest with INIT if needed
                while (lib.patches.size() < kMaxPatchesPerLibrary) {
                    ABD::Preset init;
                    init.name = "INIT PATCH";
                    init.category = "Init";
                    init.originGroup = targetIdx;
                    init.originBank = ((int)lib.patches.size() / 8) + 1;
                    init.originPatch = ((int)lib.patches.size() % 8) + 1;
                    init.state = bytesToState(nullptr, 0);
                    lib.patches.push_back(init);
                }
            }
        }
    }
    this->currentLibIdx_ = juce::jlimit(0, 25, (int)vt.getProperty("currentLib", 0));
    this->currentPresetIdx_ = (int)vt.getProperty("currentPreset", 0);
}

// --- Bank & Slot Management Helpers ---

juce::String PresetManager::getLibraryLetter(int index) {
    if (index < 0 || index >= 26) return "?";
    return juce::String::charToString((juce_wchar)('A' + index));
}

void PresetManager::ensureValidLibraryName(int index) {
    if (index < 0 || index >= (int)libraries_.size()) return;
    
    juce::String letter = getLibraryLetter(index);
    juce::String currentName = libraries_[index].name;

    // Check if it already has the [Letter] - format
    if (currentName.startsWith(letter + " - ")) return;

    // Fix name: preserve the descriptive part if possible
    if (currentName.length() >= 4 && currentName[1] == ' ' && currentName[2] == '-' && currentName[3] == ' ') {
        // Just fix the letter if it was wrong
        libraries_[index].name = letter + currentName.substring(1);
    } else {
        libraries_[index].name = letter + " - " + currentName;
    }
}

int PresetManager::findFirstEmptySlot(int startLib) {
    for (int i = startLib; i < (int)libraries_.size(); ++i) {
        auto& lib = libraries_[i];
        for (int p = 0; p < (int)lib.patches.size(); ++p) {
            if (lib.patches[p].name == "INIT PATCH") {
                return (i << 16) | p; // Compound index
            }
        }
    }
    return -1;
}

std::vector<int> PresetManager::findEmptyBankIndices(int count) {
    std::vector<int> found;
    for (int i = 3; i < (int)libraries_.size(); ++i) { // Start after Factory A/B and Internal RAM
        if (libraries_[i].name.contains("Empty Bank")) {
            found.push_back(i);
            if (found.size() >= (size_t)count) break;
        }
    }
    return found;
}

std::vector<const ABD::Preset *>
PresetManager::getFilteredPresets(const juce::String &category,
                                  const juce::String &searchText,
                                  bool favoritesOnly) const {
  std::vector<const ABD::Preset *> filtered;
  for (const auto &lib : this->libraries_) {
    for (const auto &p : lib.patches) {
      if (favoritesOnly && !p.isFavorite)
        continue;
      if (category.isNotEmpty() && category != "All" && p.category != category)
        continue;
      if (searchText.isNotEmpty() && !p.name.containsIgnoreCase(searchText) &&
          !p.author.containsIgnoreCase(searchText) &&
          !p.tags.containsIgnoreCase(searchText))
        continue;
      filtered.push_back(&p);
    }
  }
  return filtered;
}

void PresetManager::setFavorite(int libIdx, int presetIdx, bool isFav) {
  if (libIdx >= 0 && libIdx < (int)this->libraries_.size() && presetIdx >= 0 &&
      presetIdx < (int)this->libraries_[libIdx].patches.size()) {
    this->libraries_[libIdx].patches[presetIdx].isFavorite = isFav;
    if (this->libraries_[libIdx].name == "User")
      ABD::PresetManagerBase::writeUserPresets();
    this->saveBrowserData();
  }
}

void PresetManager::updateMetadata(int libIdx, int presetIdx, const juce::String& newName, const juce::String& author, const juce::String& tags, const juce::String& notes) {
    if (libIdx >= 0 && libIdx < (int)this->libraries_.size() && presetIdx >= 0 && presetIdx < (int)this->libraries_[libIdx].patches.size()) {
        auto& p = this->libraries_[libIdx].patches[presetIdx];
        if (newName.isNotEmpty()) p.name = newName;
        p.author = author; p.tags = tags; p.notes = notes;
        
        // Save logic: Internal RAM (C) goes to its own file, others go to browser_state.xml
        if (libIdx == kInternalRamLibIdx) saveUserRam();
        
        this->saveBrowserData();
    }
}

void PresetManager::loadFactoryPresets() {
    // Fill A and B (0, 1) with actual factory data
    for (int g = 0; g < 2; ++g) {
        int libIdx = g;
        auto& lib = this->libraries_[libIdx];
        lib.name = juce::String::charToString((juce_wchar)('A' + g)) + " - Factory";
        lib.patches.clear();

        for (int p_idx = 0; p_idx < kMaxPatchesPerLibrary; ++p_idx) {
            int globalIdx = (g * 64) + p_idx;
            if (globalIdx < 128) {
                ABD::Preset p = createPresetFromJunoPatch(junoFactoryPatches[globalIdx]);
                p.originGroup = g;
                p.originBank = (p_idx / 8) + 1;
                p.originPatch = (p_idx % 8) + 1;
                lib.patches.push_back(p);
            }
        }
        
        while (lib.patches.size() < kMaxPatchesPerLibrary) {
            ABD::Preset init;
            init.name = "INIT PATCH";
            init.state = bytesToState(nullptr, 0);
            lib.patches.push_back(init);
        }
    }
}

void PresetManager::setCurrentPreset(int index) noexcept {
    // [Harden] Ensure absolute index stays within bounds of the 26-bank global space (1664 slots)
    int absoluteIdx = juce::jlimit(0, 1663, index);
    
    this->currentLibIdx_ = absoluteIdx / kMaxPatchesPerLibrary;
    this->currentPresetIdx_ = absoluteIdx % kMaxPatchesPerLibrary;
    
    // Sync origin metrics for UI display fidelity
    if (currentLibIdx_ >= 0 && currentLibIdx_ < (int)libraries_.size()) {
        auto& patches = libraries_[currentLibIdx_].patches;
        if (currentPresetIdx_ >= 0 && currentPresetIdx_ < (int)patches.size()) {
            auto& p = patches[currentPresetIdx_];
            p.originGroup = (uint8_t)currentLibIdx_;
            p.originBank  = (uint8_t)((currentPresetIdx_ / 8) + 1);
            p.originPatch = (uint8_t)((currentPresetIdx_ % 8) + 1);
        }
    }

    // Notify base class (expects local index for the active library)
    ABD::PresetManagerBase::setCurrentPreset(this->currentPresetIdx_);
}

void PresetManager::selectPreset(int libraryIndex, int presetIndex) {
    setCurrentPreset((libraryIndex * kMaxPatchesPerLibrary) + presetIndex);
}

void PresetManager::loadUserRam() {
    int ramIdx = kInternalRamLibIdx; // Always C
    auto& lib = this->libraries_[ramIdx];
    lib.name = "C - Internal RAM";
    
    auto dir = getUserPresetsDirectory();
    auto file = dir.getChildFile("user_ram.xml");
    
    if (file.existsAsFile()) {
        auto vt = juce::ValueTree::fromXml(file.loadFileAsString());
        if (vt.isValid() && vt.hasType("Library")) {
            lib.patches.clear();
            for (int i = 0; i < vt.getNumChildren() && i < kMaxPatchesPerLibrary; ++i) {
                ABD::Preset p;
                p.fromValueTree(vt.getChild(i));
                p.originGroup = ramIdx;
                p.originBank = (i / 8) + 1;
                p.originPatch = (i % 8) + 1;
                lib.patches.push_back(p);
            }
        }
    } else {
        // First run: Clone Factory A into RAM
        lib.patches.clear();
        for (int i = 0; i < 64; ++i) {
            auto p = createPresetFromJunoPatch(junoFactoryPatches[i]);
            p.category = "RAM";
            p.originGroup = ramIdx;
            p.originBank = (i / 8) + 1;
            p.originPatch = (i % 8) + 1;
            lib.patches.push_back(p);
        }
        saveUserRam();
    }

    // Ensure 64 patches
    while (lib.patches.size() < kMaxPatchesPerLibrary) {
        ABD::Preset init;
        init.name = "INIT PATCH";
        init.state = bytesToState(nullptr, 0);
        lib.patches.push_back(init);
    }
}

void PresetManager::saveUserRam() {
    int ramIdx = getLibraryIndex("INTERNAL RAM");
    if (ramIdx < 0) return;
    
    juce::ValueTree vt("Library");
    vt.setProperty("name", "INTERNAL RAM", nullptr);
    for (const auto& p : libraries_[ramIdx].patches) {
        vt.addChild(p.toValueTree(), -1, nullptr);
    }
    
    auto dir = getUserPresetsDirectory();
    auto file = dir.getChildFile("user_ram.xml");
    file.replaceWithText(vt.toXmlString());
}

juce::Result PresetManager::writeToInternalSlot(int group, int bank, int patch, 
                                                 const juce::ValueTree& state,
                                                 const juce::String& name,
                                                 const juce::String& author) {
    juce::ignoreUnused(group);
    int ramIdx = getLibraryIndex("INTERNAL RAM");
    if (ramIdx < 0) return juce::Result::fail("Internal RAM not found");
    
    int absIdx = ((bank - 1) * 8) + (patch - 1);
    if (absIdx < 0 || absIdx >= (int)libraries_[ramIdx].patches.size())
        return juce::Result::fail("Invalid slot index (Bank/Patch out of range for Internal RAM)");
        
    auto& p = libraries_[ramIdx].patches[absIdx];
    p.state = state.createCopy();
    if (name.isNotEmpty()) p.name = name;
    if (author.isNotEmpty()) p.author = author;
    p.category = "RAM"; // Ensure category is correct
    p.creationDate = juce::Time::getCurrentTime().toString(true, true);
    
    // Set origin for UI feedback
    p.originGroup = ramIdx;
    p.originBank  = bank;
    p.originPatch = patch;

    // Update current selection
    currentLibIdx_ = ramIdx;
    currentPresetIdx_ = absIdx;

    saveUserRam();
    saveBrowserData();
    return juce::Result::ok();
}

bool PresetManager::isUserRamActive() const {
    int ramIdx = getLibraryIndex("INTERNAL RAM");
    return this->currentLibIdx_ == ramIdx;
}

void PresetManager::nextBank() {
    // Juno "Bank Plus" jumps 8 patches (one bank) forward
    int nextIdx = (this->currentLibIdx_ * kMaxPatchesPerLibrary) + this->currentPresetIdx_ + 8;
    int maxTotal = kMaxLibraries * kMaxPatchesPerLibrary;
    if (nextIdx >= maxTotal) nextIdx %= maxTotal; 
    setCurrentPreset(nextIdx);
}

void PresetManager::prevBank() {
    // Juno "Bank Minus" jumps 8 patches backward
    int prevIdx = (this->currentLibIdx_ * kMaxPatchesPerLibrary) + this->currentPresetIdx_ - 8;
    int maxTotal = kMaxLibraries * kMaxPatchesPerLibrary;
    if (prevIdx < 0) prevIdx += maxTotal; 
    setCurrentPreset(prevIdx);
}

void PresetManager::nextPatch() {
    int nextIdx = (this->currentLibIdx_ * kMaxPatchesPerLibrary) + this->currentPresetIdx_ + 1;
    int maxTotal = kMaxLibraries * kMaxPatchesPerLibrary;
    if (nextIdx >= maxTotal) nextIdx = 0;
    setCurrentPreset(nextIdx);
}

void PresetManager::prevPatch() {
    int prevIdx = (this->currentLibIdx_ * kMaxPatchesPerLibrary) + this->currentPresetIdx_ - 1;
    int maxTotal = kMaxLibraries * kMaxPatchesPerLibrary;
    if (prevIdx < 0) prevIdx = maxTotal - 1;
    setCurrentPreset(prevIdx);
}

