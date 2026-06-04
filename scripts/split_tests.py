"""
Split JunoUnitTests.cpp into separate test files by category.
Creates Source/Core/Tests/ directory with individual test files.
"""
import os, re

SRC = "Source/Core/JunoUnitTests.cpp"
OUTDIR = "Source/Core/Tests"
os.makedirs(OUTDIR, exist_ok=True)

with open(SRC, "r", encoding="utf-8") as f:
    content = f.read()

# ── Common preamble used by all test files ──
PREAMBLE = '''#define JUCE_UNIT_TESTS 1
#include <JuceHeader.h>
#include <cstdlib>
'''

# ── Map category → (test_class_names or regex) ──
# We'll extract each class by scanning for "class XxxTests :"
CATEGORIES = {
    "JunoDCOTests": {
        "classes": ["JunoDCOTests", "JunoSubOscTests", "JunoNoiseTests"],
        "extra_includes": '#include "../Synth/JunoDCO.h"\n',
        "label": "DCO",
    },
    "JunoADSRTests": {
        "classes": ["JunoADSRTests", "JunoADSRTimingTest"],
        "extra_includes": '#include "../Synth/JunoADSR.h"\n#include "BaseClass/ADSRGeneric.h"\n',
        "label": "ADSR",
    },
    "JunoVCFTests": {
        "classes": ["JunoVCFTests"],
        "extra_includes": '#include "../Synth/JunoVCF.h"\n',
        "label": "VCF",
    },
    "ChorusBBDTests": {
        "classes": ["ChorusBBDTests"],
        "extra_includes": '#include "../Synth/ChorusBBD.h"\n',
        "label": "Chorus",
    },
    "JunoSysExTests": {
        "classes": ["JunoSysExTests"],
        "extra_includes": '#include "JunoSysExEngine.h"\n#include "SynthParams.h"\n',
        "label": "SysEx",
    },
    "JunoTapeTests": {
        "classes": ["JunoTapeTests"],
        "extra_includes": '#include "JunoTapeEncoder.h"\n#include "JunoTapeDecoder.h"\n',
        "label": "Tape",
    },
    "JunoFormatConverterTests": {
        "classes": ["JunoFormatConverterTests", "JunoDcbCorrectorTests"],
        "extra_includes": '#include "JunoTapeDecoder.h"\n#include "JunoTapeEncoder.h"\n#include "Importers/JunoFormatConverter.h"\n#include "JunoConstants.h"\n',
        "label": "FormatConverter",
    },
    "JunoSmartTapeTests": {
        "classes": ["JunoSmartTapeTests"],
        "extra_includes": '#include "JunoTapeEncoder.h"\n#include "JunoTapeDecoder.h"\n',
        "label": "SmartTape",
    },
    "JunoMemoryTests": {
        "classes": ["JunoMemoryTests"],
        "extra_includes": '#include "PresetManager.h"\n',
        "label": "Memory",
    },
    "JunoUnisonTests": {
        "classes": ["JunoUnisonTests"],
        "extra_includes": '#include "../Synth/JunoVoice.h"\n#include "SynthParams.h"\n',
        "label": "Unison",
    },
}

def extract_class(content, class_name, start_search=0):
    """Extract a class definition starting from 'class {class_name}'."""
    pattern = rf'class {re.escape(class_name)}\s+:\s+public\s+juce::UnitTest\s*{{'
    m = re.search(pattern, content[start_search:])
    if not m:
        print(f"  WARNING: Could not find class {class_name}")
        return None, start_search
    start = start_search + m.start()
    # Find the matching closing brace of the class.
    # We need to track brace depth starting from after the opening { of the class body.
    # First, find the opening brace of the class body (after the constructor).
    brace_start = content.index("{", start)
    depth = 1
    pos = brace_start + 1
    while depth > 0 and pos < len(content):
        if content[pos] == "{":
            depth += 1
        elif content[pos] == "}":
            depth -= 1
        pos += 1
    end = pos
    return content[start:end], end


def write_test_file(category_key, info):
    """Extract classes and write test file."""
    classes = info["classes"]
    label = info["label"]
    extra_includes = info.get("extra_includes", "")
    
    # Extract each class
    extracted = []
    search_pos = 0
    for cls in classes:
        result, search_pos = extract_class(content, cls, search_pos)
        if result:
            extracted.append(result)
        else:
            print(f"  FAILED to extract {cls}")
            return False
    
    # Build file content
    lines = [PREAMBLE]
    # Add extra includes
    if extra_includes:
        lines.append(extra_includes)
    
    # All test classes
    for cls_text in extracted:
        lines.append(cls_text)
        lines.append("")
    
    out_path = os.path.join(OUTDIR, f"{label}Tests.cpp")
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    
    print(f"  Created {out_path} ({len(extracted)} classes)")
    return True

print("=" * 60)
print("Splitting JunoUnitTests.cpp into categorized files")
print("=" * 60)

success = 0
for key, info in CATEGORIES.items():
    if write_test_file(key, info):
        success += 1
    else:
        print(f"  ERROR: Failed to create {key}")

print(f"\nDone: {success}/{len(CATEGORIES)} files created in {OUTDIR}/")
print("\nCategories:")
for key, info in CATEGORIES.items():
    print(f"  {info['label']:20s} → {', '.join(info['classes'])}")
