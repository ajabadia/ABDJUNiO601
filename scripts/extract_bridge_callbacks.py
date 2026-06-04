#!/usr/bin/env python3
"""
Extract withNativeFunction callback bodies from WebViewEditor.cpp
into separate BridgeActions.cpp and BridgeImport.cpp files.
"""

import re
import os

PROJECT_ROOT = "D:/desarrollos/ABDSynths/ABDJUNiO601"
CPP_PATH = os.path.join(PROJECT_ROOT, "Source/UI/WebView/WebViewEditor.cpp")

IMPORT_FUNCTIONS = {"confirmImportFile", "confirmTapeImport"}


def find_callbacks(source):
    """
    Find all .withNativeFunction("name", [](args) { body }) blocks
    by scanning, counting braces, and tracking positions.
    Returns list of dicts.
    """
    pattern = re.compile(
        r'\.withNativeFunction\s*\(\s*"([^"]+)"\s*,\s*\[this\]\s*\('
        r'(?:const\s+)?juce::Array<juce::var>(?:\s*&\s*)?(\w+)?(?:\s*,\s*)?'
        r'(?:juce::WebBrowserComponent::NativeFunctionCompletion\s+(\w+))?\s*\)\s*\{',
        re.DOTALL
    )
    
    results = []
    pos = 0
    while pos < len(source):
        m = pattern.search(source, pos)
        if not m:
            break
        
        name = m.group(1)
        args_var = m.group(2) or "args"
        compl_var = m.group(3) or "completion"
        
        body_start = m.end()
        # Count braces to find the matching closing brace
        depth = 1
        i = body_start
        while i < len(source) and depth > 0:
            c = source[i]
            if c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
            i += 1
        
        if depth != 0:
            print(f"  UNBALANCED: {name} at {m.start()}")
            pos = m.start() + 1
            continue
        
        body = source[body_start:i-1]  # exclude the closing }
        
        results.append({
            "name": name,
            "body": body.strip(),
            "full_start": m.start(),
            "full_end": i,
            "completion_var": compl_var,
        })
        
        pos = i
    
    return results


def transform_body(body, name, params):
    """
    Transform the lambda body from capturing `this` to using function parameters.
    The key transformations:
      - `this->` is removed (all member accesses go to parameters)
      - Member function calls like `audioProcessor.getAPVTS()` stay as-is
        because `audioProcessor` is a parameter by the same name
      - `dispatchToJS(...)`, `sendPresetListUpdate()`, `notifySelfTestState()`,
        `writeLog(...)`, `showAboutCallback()`, `showSettingsCallback()`,
        `showServiceModeCallback()`, `sendBankPatchUpdate(...)` are parameters
      - Parameter references like `fileChooser`, `pendingTapeFile`, etc. stay as-is
    """
    return body


def generate_bridge_actions_cpp(bodies):
    """Generate BridgeActions.cpp with all non-import callbacks."""
    includes = """#include "BridgeActions.h"
#include "BridgeImport.h"
#include "../../Core/ABDSimpleJuno106AudioProcessor.h"
#include "../../Core/CalibrationSettings.h"
#include "../../Core/ServiceModeManager.h"
#include "../../Core/PresetManager.h"
#include "../../Core/JunoTapeEncoder.h"
#include "../../Core/JunoTapeDecoder.h"
#include "../../Core/Importers/JunoSysexImporter.h"
#include "../../Core/Importers/JunoCsvImporter.h"
#include "JunoModelConfig.h"
#include <optional>

namespace BridgeActions {
"""
    parts = [includes]
    
    for b in bodies:
        name = b["name"]
        if name in IMPORT_FUNCTIONS:
            continue
        
        body = transform_body(b["body"], name, {})
        parts.append(f"\n// {'=' * 70}")
        parts.append(f"// {name}")
        parts.append(f"// {'=' * 70}")
        parts.append("void " + name + "(\n")
        
        # Determine params based on function name
        parts.append("    ABDSimpleJuno106AudioProcessor& audioProcessor,\n")
        
        if name in ("menuAction", "serviceAction", "exportBank", "importBank", "chooseDirectory"):
            parts.append("    std::unique_ptr<juce::FileChooser>& fileChooser,\n")
        
        if name == "menuAction":
            parts.append("    juce::File& pendingTapeFile,\n")
            parts.append("    JunoTapeDecoder::SmartDecodeResult& pendingSmartResult,\n")
            parts.append("    int& selectedDecoderIndex,\n")
            parts.append("    juce::File& pendingImportFile,\n")
            parts.append("    juce::String& pendingImportFormat,\n")
        
        if name == "importBank":
            parts.append("    std::shared_ptr<juce::WebBrowserComponent::NativeFunctionCompletion>& safeCompletion,\n")
        
        parts.append("    const juce::Array<juce::var>& args,\n")
        
        needs_dispatch = name in ("menuAction", "confirmImportFile", "confirmTapeImport", "setBrowserData",
                                  "savePresetDetailed", "saveAsNewPresetDetailed", "exportBank", "importBank", "uiReady")
        needs_send_update = name in ("menuAction", "savePresetDetailed", "saveAsNewPresetDetailed", "uiReady")
        needs_notify = name in ("runSelfTest", "uiReady")
        needs_show_about = name == "menuAction"
        needs_show_settings = name == "menuAction"
        needs_show_service = name == "menuAction"
        needs_writelog = name in ("menuAction", "setBrowserData", "uiReady")
        needs_send_bank_patch = name == "menuAction"  # for uiReady sub-action
        needs_request_patch_dump = name == "confirmImportFile"
        
        if needs_dispatch:
            parts.append("    const std::function<void(const juce::String&, const juce::var&)>& dispatchToJS,\n")
        if needs_send_update:
            parts.append("    const std::function<void()>& sendPresetListUpdate,\n")
        if needs_show_about:
            parts.append("    const std::function<void()>& showAboutCallback,\n")
        if needs_show_settings:
            parts.append("    const std::function<void()>& showSettingsCallback,\n")
        if needs_show_service:
            parts.append("    const std::function<void()>& showServiceModeCallback,\n")
        if needs_writelog:
            parts.append("    const std::function<void(const juce::String&)>& writeLog,\n")
        if needs_notify:
            parts.append("    const std::function<void()>& notifySelfTestState,\n")
        if needs_send_bank_patch:
            parts.append("    const std::function<void(int, int, int)>& sendBankPatchUpdate,\n")
        if needs_request_patch_dump:
            parts.append("    const std::function<void()>& requestPatchDump,\n")
        
        parts.append("    juce::WebBrowserComponent::NativeFunctionCompletion completion)\n")
        parts.append("{\n")
        
        # Write body lines with proper indentation
        for line in body.split('\n'):
            parts.append(line + '\n')
        
        if not body.rstrip().endswith('}'):
            parts.append("}\n")
        else:
            parts.append("\n")
    
    parts.append("\n} // namespace BridgeActions\n")
    return ''.join(parts)


def generate_bridge_import_cpp(bodies):
    """Generate BridgeImport.cpp with import callbacks."""
    includes = """#include "BridgeImport.h"
#include "../../Core/ABDSimpleJuno106AudioProcessor.h"
#include "../../Core/JunoTapeDecoder.h"
#include "../../Core/PresetManager.h"
#include <JuceHeader.h>

namespace BridgeImport {
"""
    parts = [includes]
    
    for b in bodies:
        name = b["name"]
        if name not in IMPORT_FUNCTIONS:
            continue
        
        body = transform_body(b["body"], name, {})
        parts.append(f"\n// {'=' * 70}")
        parts.append(f"// {name}")
        parts.append(f"// {'=' * 70}")
        parts.append("void " + name + "(\n")
        parts.append("    ABDSimpleJuno106AudioProcessor& audioProcessor,\n")
        
        if name == "confirmImportFile":
            parts.append("    juce::File& pendingImportFile,\n")
            parts.append("    juce::String& pendingImportFormat,\n")
        elif name == "confirmTapeImport":
            parts.append("    juce::File& pendingTapeFile,\n")
            parts.append("    JunoTapeDecoder::SmartDecodeResult& pendingSmartResult,\n")
            parts.append("    int& selectedDecoderIndex,\n")
        
        parts.append("    const juce::Array<juce::var>& args,\n")
        parts.append("    const std::function<void(const juce::String&, const juce::var&)>& dispatchToJS,\n")
        
        if name == "confirmImportFile":
            parts.append("    const std::function<void()>& sendPresetListUpdate,\n")
            parts.append("    const std::function<void()>& requestPatchDump,\n")
        
        parts.append("    juce::WebBrowserComponent::NativeFunctionCompletion completion)\n")
        parts.append("{\n")
        
        for line in body.split('\n'):
            parts.append(line + '\n')
        
        if not body.rstrip().endswith('}'):
            parts.append("}\n")
        else:
            parts.append("\n")
    
    parts.append("\n} // namespace BridgeImport\n")
    return ''.join(parts)


def main():
    with open(CPP_PATH, 'r', encoding='utf-8') as f:
        source = f.read()
    
    print(f"Source file: {len(source)} chars")
    
    bodies = find_callbacks(source)
    print(f"\nFound {len(bodies)} callbacks:")
    for b in bodies:
        lbs = b["body"].count('\n') + 1
        print(f"  {b['name']:30s}  {lbs:4d} lines  ({len(b['body']):6d} chars)")
    
    actions_bodies = [b for b in bodies if b["name"] not in IMPORT_FUNCTIONS]
    import_bodies = [b for b in bodies if b["name"] in IMPORT_FUNCTIONS]
    
    # Generate BridgeActions.cpp
    actions_cpp = generate_bridge_actions_cpp(bodies)
    actions_path = os.path.join(PROJECT_ROOT, "Source/UI/WebView/BridgeActions.cpp")
    with open(actions_path, 'w', encoding='utf-8') as f:
        f.write(actions_cpp)
    print(f"\nBridgeActions.cpp: {len(actions_cpp):6d} chars  ({actions_bodies[0]['name']}..{actions_bodies[-1]['name']})")
    
    # Generate BridgeImport.cpp
    import_cpp = generate_bridge_import_cpp(bodies)
    import_path = os.path.join(PROJECT_ROOT, "Source/UI/WebView/BridgeImport.cpp")
    with open(import_path, 'w', encoding='utf-8') as f:
        f.write(import_cpp)
    print(f"BridgeImport.cpp: {len(import_cpp):6d} chars  ({', '.join(b['name'] for b in import_bodies)})")


if __name__ == "__main__":
    main()
