import codecs
import os

# We transform in-place after copying from backup
target = r"d:\desarrollos\ABDJUNiO601\Source\ABD-SynthEngine\Protocol\Presets\PresetManagerBase.h"

print(f"Normalizing: {target}")
try:
    # Read as UTF-16 (Little Endian without BOM as verified by hex)
    with codecs.open(target, 'r', 'utf-16') as f:
        content = f.read()
    print(f"Read {len(content)} characters.")

    # Missing methods injection
    old_block = """    int getLibraryIndex(const juce::String& name) const
    {
        for (int i = 0; i < (int)libraries_.size(); ++i)
            if (libraries_[i].name == name) return i;
        return -1;
    }"""
    
    new_methods = """

    void setCurrentPreset(int index) noexcept { currentPresetIdx_ = index; }
    int getCurrentPresetIndex() const noexcept { return currentPresetIdx_; }"""

    if old_block in content:
        print("Found target block, inserting methods...")
        content = content.replace(old_block, old_block + new_methods)
    else:
        print("Target block not found precisely, searching for partial...")
        if "juce::String getCurrentPresetName()" in content:
             content = content.replace("juce::String getCurrentPresetName()", new_methods + "\n\n    juce::String getCurrentPresetName()")

    # Save as UTF-8 with BOM
    with codecs.open(target, 'w', 'utf-8-sig') as f:
        f.write(content)
    print("Successfully normalized and updated.")

except Exception as e:
    print(f"Error: {e}")
