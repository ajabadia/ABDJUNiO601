#include "JunoTapeImporter.h"

namespace ABD {

JunoTapeImporter::ImportResult JunoTapeImporter::loadFromFile(const juce::File& file) {
    ImportResult result;

    if (!file.existsAsFile()) {
        result.result = juce::Result::fail("File does not exist: " + file.getFullPathName());
        return result;
    }

    auto decodeResult = JunoTapeDecoder::decodeWavFile(file);
    if (!decodeResult.success) {
        result.result = juce::Result::fail(decodeResult.errorMessage);
        return result;
    }

    juce::String baseLibName = file.getFileNameWithoutExtension();
    int patchCount = (int)decodeResult.data.size() / 18;
    int numLibsNeeded = (patchCount + 63) / 64;

    for (int l = 0; l < numLibsNeeded; ++l) {
        ABD::Library lib;
        lib.name = baseLibName + (numLibsNeeded > 1 ? " " + juce::String((char)('A' + l)) : "");
        lib.patches.clear();

        for (int p = 0; p < 64; ++p) {
            size_t dataIdx = (l * 64 + p) * 18;
            if (dataIdx + 17 < decodeResult.data.size()) {
                // Use the FormatConverter to create the preset
                lib.patches.push_back(JunoFormatConverter::createPresetFromJunoBytes(
                    "Tape Patch " + juce::String((l * 64 + p) + 1),
                    &decodeResult.data[dataIdx],
                    juce::ValueTree() // Template not strictly needed if Converter handles it
                ));
            }
        }
        
        if (!lib.patches.empty()) {
            result.libraries.push_back(lib);
        }
    }

    return result;
}

} // namespace ABD
