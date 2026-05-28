#include "JunoSysexImporter.h"

namespace ABD {

JunoSysexImporter::ImportResult JunoSysexImporter::loadFromFile(const juce::File& file) {
    ImportResult result;

    if (!file.existsAsFile()) {
        result.result = juce::Result::fail("File does not exist: " + file.getFullPathName());
        return result;
    }

    juce::MemoryBlock mb;
    if (!file.loadFileAsData(mb)) {
        result.result = juce::Result::fail("Failed to read SysEx file");
        return result;
    }

    const uint8_t* data = (const uint8_t*)mb.getData();
    size_t size = mb.getSize();

    if (size < 10) {
        result.result = juce::Result::fail("File too small to be a valid SysEx");
        return result;
    }

    // Basic Validation (Roland Juno-106 Bulk Dump)
    // F0 41 3n 02 01 (1152 bytes) F7
    if (data[0] != 0xF0 || data[1] != 0x41) {
        result.result = juce::Result::fail("Not a Roland SysEx file");
        return result;
    }

    if (data[3] != 0x02) {
        result.result = juce::Result::fail("Not a Juno-106 SysEx message");
        return result;
    }

    int functionCode = data[4];
    
    // Total patches handled in this file
    std::vector<ABD::Preset> allPatches;

    if (functionCode == 0x01 && size >= 1152 + 5) {
        // Bulk Dump: 1152 bytes = 64 patches * 18 bytes
        const uint8_t* patchData = data + 5;
        for (int i = 0; i < 64; ++i) {
            allPatches.push_back(JunoFormatConverter::createPresetFromJunoBytes(
                "SysEx Patch " + juce::String(i + 1),
                patchData + (i * 18),
                juce::ValueTree()
            ));
        }
    } else if (functionCode == 0x03) {
        // Single Patch Dump (Approx 18-20 bytes)
        const uint8_t* patchData = data + 5;
        allPatches.push_back(JunoFormatConverter::createPresetFromJunoBytes(
             file.getFileNameWithoutExtension(),
             patchData,
             juce::ValueTree()
        ));
    } else {
        result.result = juce::Result::fail("Unsupported Juno-106 SysEx command: " + juce::String(functionCode));
        return result;
    }

    if (!allPatches.empty()) {
        ABD::Library lib;
        lib.name = file.getFileNameWithoutExtension();
        lib.patches = allPatches;
        
        // Split into 64-patch libraries if needed (though Sysex usually is one bank)
        result.libraries.push_back(lib);
    }

    return result;
}

} // namespace ABD
