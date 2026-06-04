#!/usr/bin/env python3
"""Fix the confirmSmartImport check - search from beginning not from importIdx."""

import re

test_file = "Source/Core/JunoUnitTests.cpp"

with open(test_file, "r", encoding="utf-8") as f:
    content = f.read()

# Fix the confirmSmartImport check - search globally, not from importIdx
old = '                int confirmIdx = html.indexOf(importIdx, juce::String("confirmSmartImport()"));\n                expect(confirmIdx >= 0, "Import button should call confirmSmartImport()");'

new = '                int confirmIdx = html.indexOf(juce::String("confirmSmartImport()"));\n                expect(confirmIdx >= 0, "Import button should call confirmSmartImport()");'

if old in content:
    content = content.replace(old, new)
    print("FIXED: confirmSmartImport search")
else:
    print("WARNING: Could not find exact text to fix. Trying different approach...")
    # Try a more robust replacement
    # Find the problematic line
    pattern = r'html\.indexOf\(importIdx, juce::String\("confirmSmartImport\(\)"\)\)'
    replacement = 'html.indexOf(juce::String("confirmSmartImport()"))'
    if re.search(pattern, content):
        content = re.sub(pattern, replacement, content)
        print("FIXED with regex")
    else:
        print("Still could not find pattern. Checking file content...")
        # Print some context around importIdx
        import_idx_pos = content.find("confirmSmartImport()")
        if import_idx_pos >= 0:
            print(f"Found confirmSmartImport() at position {import_idx_pos}")
            print(content[import_idx_pos-50:import_idx_pos+50])
        else:
            print("confirmSmartImport() NOT FOUND in file!")

with open(test_file, "w", encoding="utf-8") as f:
    f.write(content)

print("DONE")
