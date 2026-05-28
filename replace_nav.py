import os

target = r'Source\UI\WebView\WebViewEditor.cpp'
with open(target, 'r', encoding='utf-8') as f:
    content = f.read()

nav_code = """
        .withNativeFunction ("bankInc", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (auto* pm = audioProcessor.getPresetManager()) pm->nextBank();
            completion(juce::var::undefined());
        })
        .withNativeFunction ("bankDec", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (auto* pm = audioProcessor.getPresetManager()) pm->prevBank();
            completion(juce::var::undefined());
        })
        .withNativeFunction ("patchInc", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (auto* pm = audioProcessor.getPresetManager()) pm->nextPatch();
            completion(juce::var::undefined());
        })
        .withNativeFunction ("patchDec", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (auto* pm = audioProcessor.getPresetManager()) pm->prevPatch();
            completion(juce::var::undefined());
        })"""

# Find setProgram registration block
# It ends with });
if 'setProgram' in content:
    idx = content.find('setProgram')
    end_idx = content.find(');', idx)
    if end_idx != -1:
        new_content = content[:end_idx+2] + nav_code + content[end_idx+2:]
        with open(target, 'w', encoding='utf-8') as f:
            f.write(new_content)
        print("Successfully replaced content.")
    else:
        print("Error: Could not find end of setProgram block.")
else:
    print("Error: Could not find setProgram.")
