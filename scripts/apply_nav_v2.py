import os

target = r'D:\desarrollos\ABDJUNiO601\Source\UI\WebView\WebViewEditor.cpp'
if not os.path.exists(target):
    print(f"Error: {target} not found")
    exit(1)

with open(target, 'r', encoding='utf-8') as f:
    lines = f.readlines()

print(f"Read {len(lines)} lines")

start_idx = -1
for i, l in enumerate(lines):
    if 'setProgram' in l and 'withNativeFunction' in l:
        start_idx = i
        print(f"Found target at line {i+1}")
        break

if start_idx == -1:
    print("Error: Target identifier not found")
    exit(1)

# Find end of function registration
end_idx = -1
for i in range(start_idx, len(lines)):
    if ');' in lines[i]:
        end_idx = i
        break

if end_idx == -1:
    print("Error: End of registration not found")
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
