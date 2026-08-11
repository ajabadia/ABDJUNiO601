import os

target = r'Source\UI\WebView\WebViewEditor.cpp'
with open(target, 'r', encoding='utf-8') as f:
    lines = f.readlines()

start_idx = -1
for i, l in enumerate(lines):
    if 'setProgram' in l and 'withNativeFunction' in l:
        start_idx = i
        break

if start_idx == -1:
    print("Error: 'setProgram' not found")
    exit(1)

# Find end of setProgram call
end_idx = -1
for i in range(start_idx, len(lines)):
    if ');' in lines[i]:
        end_idx = i
        break

if end_idx == -1:
    print("Error: End of 'setProgram' not found")
    exit(1)

nav_handlers = [
    '\n',
    '        .withNativeFunction ("bankInc", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {\n',
    '            if (auto* pm = audioProcessor.getPresetManager()) pm->nextBank();\n',
    '            completion(juce::var::undefined());\n',
    '        })\n',
    '        .withNativeFunction ("bankDec", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {\n',
    '            if (auto* pm = audioProcessor.getPresetManager()) pm->prevBank();\n',
    '            completion(juce::var::undefined());\n',
    '        })\n',
    '        .withNativeFunction ("patchInc", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {\n',
    '            if (auto* pm = audioProcessor.getPresetManager()) pm->nextPatch();\n',
    '            completion(juce::var::undefined());\n',
    '        })\n',
    '        .withNativeFunction ("patchDec", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {\n',
    '            if (auto* pm = audioProcessor.getPresetManager()) pm->prevPatch();\n',
    '            completion(juce::var::undefined());\n',
    '        })\n'
]

new_lines = lines[:end_idx+1] + nav_handlers + lines[end_idx+1:]

with open(target, 'w', encoding='utf-8') as f:
    f.writelines(new_lines)

print("Successfully injected navigation handlers.")
