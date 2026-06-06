#include "JunoSysexImporter.h"

namespace ABD {

// ============================================================
// Helper: scan raw SysEx data for individual patch dump messages
// Format: F0 41 30 <ch> [18 bytes] [optional checksum] F7
// Returns extracted 18-byte patch blocks.
// ============================================================
static std::vector<std::vector<uint8_t>> scanIndividualPatchDumps(const uint8_t* data, size_t size) {
    std::vector<std::vector<uint8_t>> patches;
    size_t pos = 0;

    while (pos + 22 < size) {
        // Look for SysEx start
        if (data[pos] != 0xF0) { ++pos; continue; }
        if (data[pos + 1] != 0x41) { ++pos; continue; }

        uint8_t msgType = data[pos + 2];

        // Find end marker F7 starting from pos+1
        size_t endPos = pos + 1;
        while (endPos < size && data[endPos] != 0xF7)
            ++endPos;

        if (endPos >= size)
        {
            // Malformed message without end marker — skip this byte and continue
            ++pos;
            continue;
        }

        // Only process patch dump messages (type 0x30)
        if (msgType == 0x30) {
            // bodySize = bytes between header (4 bytes) and F7
            size_t bodySize = endPos - (pos + 4);

            if (bodySize >= 18) {
                std::vector<uint8_t> patchData(18);
                memcpy(patchData.data(), data + pos + 4, 18);
                patches.push_back(std::move(patchData));
            }
        }

        // Skip past this complete message
        pos = endPos + 1;
    }

    return patches;
}

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

    // Must start with Roland SysEx header
    if (data[0] != 0xF0 || data[1] != 0x41) {
        result.result = juce::Result::fail("Not a Roland SysEx file");
        return result;
    }

    std::vector<ABD::Preset> allPatches;

    // ============================================================
    // Format A: Bulk Dump — F0 41 3n 02 01 [1152 bytes data] F7
    // Used by TAL-U-NO-LX exports and some librarian software.
    // ============================================================
    if (data[3] == 0x02) {
        int functionCode = data[4];

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
            // Single Patch Dump
            const uint8_t* patchData = data + 5;
            allPatches.push_back(JunoFormatConverter::createPresetFromJunoBytes(
                 file.getFileNameWithoutExtension(),
                 patchData,
                 juce::ValueTree()
            ));
        } else {
            // Format A with unknown function code
            // Only try Format B fallback if file is NOT a bulk dump size
            // (avoids false positives scanning random data inside bulk dump bodies)
            if (size < 1000) {
                auto individualPatches = scanIndividualPatchDumps(data, size);
                if (!individualPatches.empty()) {
                    for (auto& pd : individualPatches) {
                        allPatches.push_back(JunoFormatConverter::createPresetFromJunoBytes(
                            "SysEx Patch " + juce::String((int)allPatches.size() + 1),
                            pd.data(),
                            juce::ValueTree()
                        ));
                    }
                } else {
                    result.result = juce::Result::fail("Unsupported Juno-106 SysEx command: " + juce::String(functionCode));
                    return result;
                }
            } else {
                result.result = juce::Result::fail("Unsupported Juno-106 SysEx command: " + juce::String(functionCode));
                return result;
            }
        }
    }
    // ============================================================
    // Format B: Individual Patch Dumps — F0 41 30 <ch> [18 bytes] [csum] F7
    // Real Juno-106 hardware output. Multiple may be concatenated.
    // ============================================================
    else {
        auto individualPatches = scanIndividualPatchDumps(data, size);

        if (individualPatches.empty()) {
            result.result = juce::Result::fail("No valid Juno-106 patch dumps found in file");
            return result;
        }

        for (auto& pd : individualPatches) {
            allPatches.push_back(JunoFormatConverter::createPresetFromJunoBytes(
                "SysEx Patch " + juce::String((int)allPatches.size() + 1),
                pd.data(),
                juce::ValueTree()
            ));
        }
    }

    if (!allPatches.empty()) {
        ABD::Library lib;
        lib.name = file.getFileNameWithoutExtension();
        lib.patches = allPatches;
        result.libraries.push_back(lib);
    }

    return result;
}

} // namespace ABD
