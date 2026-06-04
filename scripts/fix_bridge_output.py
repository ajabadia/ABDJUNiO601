#!/usr/bin/env python3
"""
Fix issues in generated BridgeActions.cpp and BridgeImport.cpp, then update
WebViewEditor.cpp to use the extracted functions.
"""

import re
import os

PROJECT_ROOT = "D:/desarrollos/ABDSynths/ABDJUNiO601"

# ============================================================
# STEP 1: Fix BridgeActions.cpp
# ============================================================

def fix_bridge_actions():
    path = os.path.join(PROJECT_ROOT, "Source/UI/WebView/BridgeActions.cpp")
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    changes = []
    
    # Fix 1: Bad comment separators
    # "// ======================================================================// setParameter" -> "// =======================================================================\n// setParameter"
    content = re.sub(
        r'// ={70,}(?://\s*(\w+))',
        lambda m: f'// {"=" * 70}\n// {m.group(1)}',
        content
    )
    
    # Fix 2: Add dispatchToJS parameter to serviceAction
    old_svc_sig = """void serviceAction(
    ABDSimpleJuno106AudioProcessor& audioProcessor,
    std::unique_ptr<juce::FileChooser>& fileChooser,
    const juce::Array<juce::var>& args,
    juce::WebBrowserComponent::NativeFunctionCompletion completion)"""
    new_svc_sig = """void serviceAction(
    ABDSimpleJuno106AudioProcessor& audioProcessor,
    std::unique_ptr<juce::FileChooser>& fileChooser,
    const juce::Array<juce::var>& args,
    const std::function<void(const juce::String&, const juce::var&)>& dispatchToJS,
    juce::WebBrowserComponent::NativeFunctionCompletion completion)"""
    if old_svc_sig in content:
        content = content.replace(old_svc_sig, new_svc_sig)
        changes.append("Added dispatchToJS to serviceAction signature")
    
    # Fix 3: Fix importBank - remove safeCompletion parameter (it's created locally)
    old_import_sig = """void importBank(
    ABDSimpleJuno106AudioProcessor& audioProcessor,
    std::unique_ptr<juce::FileChooser>& fileChooser,
    std::shared_ptr<juce::WebBrowserComponent::NativeFunctionCompletion>& safeCompletion,
    const juce::Array<juce::var>& args,
    const std::function<void(const juce::String&, const juce::var&)>& dispatchToJS,
    juce::WebBrowserComponent::NativeFunctionCompletion completion)"""
    new_import_sig = """void importBank(
    ABDSimpleJuno106AudioProcessor& audioProcessor,
    std::unique_ptr<juce::FileChooser>& fileChooser,
    const juce::Array<juce::var>& args,
    const std::function<void(const juce::String&, const juce::var&)>& dispatchToJS,
    juce::WebBrowserComponent::NativeFunctionCompletion completion)"""
    if old_import_sig in content:
        content = content.replace(old_import_sig, new_import_sig)
        changes.append("Removed safeCompletion param from importBank (created locally)")
    
    # Fix 4: Fix exportBank - remove dispatchToJS param (not needed)
    # Actually exportBank doesn't need dispatchToJS. Let me check...
    # The body has: result.replaceWithText(juce::JSON::toString(libObj));
    # And: fileChooser->launchAsync(..., [this, libObj](...) { ... })
    # dispatchToJS is not used. So remove it from signature.
    old_export_sig = """void exportBank(
    ABDSimpleJuno106AudioProcessor& audioProcessor,
    std::unique_ptr<juce::FileChooser>& fileChooser,
    const juce::Array<juce::var>& args,
    const std::function<void(const juce::String&, const juce::var&)>& dispatchToJS,
    juce::WebBrowserComponent::NativeFunctionCompletion completion)"""
    new_export_sig = """void exportBank(
    ABDSimpleJuno106AudioProcessor& audioProcessor,
    std::unique_ptr<juce::FileChooser>& fileChooser,
    const juce::Array<juce::var>& args,
    juce::WebBrowserComponent::NativeFunctionCompletion completion)"""
    if old_export_sig in content:
        content = content.replace(old_export_sig, new_export_sig)
        changes.append("Removed dispatchToJS param from exportBank (not used)")
    
    # Fix 5: Fix internal [this] captures
    # exportBank: [this, libObj] → [libObj] (this not used)
    content = content.replace('[this, libObj]', '[libObj]')
    changes.append("Fixed exportBank: [this, libObj] -> [libObj]")
    
    # importBank: [this, safeCompletion] → [safeCompletion] (this not used)
    content = content.replace('[this, safeCompletion]', '[safeCompletion]')
    changes.append("Fixed importBank: [this, safeCompletion] -> [safeCompletion]")
    
    # chooseDirectory: check if [this] exists (already uses [safeCompletion])
    # No fix needed there.
    
    # Fix 6: setBrowserData - the body uses "const auto& var = args[0];" which shadows
    # the type name "var". This is technically fine in C++ but could confuse. Leave as-is.
    
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)
    
    print(f"BridgeActions.cpp: {len(changes)} fixes applied")
    for c in changes:
        print(f"  - {c}")


# ============================================================
# STEP 2: Fix BridgeImport.cpp
# ============================================================

def fix_bridge_import():
    path = os.path.join(PROJECT_ROOT, "Source/UI/WebView/BridgeImport.cpp")
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    changes = []
    
    # Fix bad comment separators
    content = re.sub(
        r'// ={70,}(?://\s*(\w+))',
        lambda m: f'// {"=" * 70}\n// {m.group(1)}',
        content
    )
    
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)
    
    print(f"BridgeImport.cpp: {len(changes)} fixes applied")
    for c in changes:
        print(f"  - {c}")


# ============================================================
# STEP 3: Update WebViewEditor.cpp to use the new files
# ============================================================

def update_webview_editor():
    """Update WebViewEditor.cpp to:
    1. Add includes for BridgeActions.h and BridgeImport.h
    2. Remove extracted function bodies, replace with BridgeActions::xxx() calls
    3. Keep complex functions (menuAction, serviceAction, uiReady, exportBank, importBank)
    """
    path = os.path.join(PROJECT_ROOT, "Source/UI/WebView/WebViewEditor.cpp")
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    changes = []
    
    # Add includes after existing includes
    old_includes_end = '#include "JunoModelConfig.h"'
    new_includes = """#include "JunoModelConfig.h"
#include "BridgeActions.h"
#include "BridgeImport.h"
"""
    if old_includes_end in content:
        content = content.replace(old_includes_end, new_includes)
        changes.append("Added BridgeActions.h and BridgeImport.h includes")
    
    # Now replace each extracted function's withNativeFunction block
    # with a one-line call to BridgeActions::xxx() or BridgeImport::xxx()
    
    # Functions to replace with BridgeActions:: calls
    # These are the "easy" ones without [this] issues
    replace_actions = {
        "setParameter": """        .withNativeFunction ("setParameter", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::setParameter(audioProcessor, args, std::move(completion));
        })""",
        
        "beginGesture": """        .withNativeFunction ("beginGesture", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::beginGesture(audioProcessor, args, std::move(completion));
        })""",
        
        "endGesture": """        .withNativeFunction ("endGesture", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::endGesture(audioProcessor, args, std::move(completion));
        })""",
        
        "getCalibrationParams": """        .withNativeFunction ("getCalibrationParams", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::getCalibrationParams(audioProcessor, args, std::move(completion));
        })""",
        
        "setCalibrationParam": """        .withNativeFunction ("setCalibrationParam", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::setCalibrationParam(audioProcessor, args, std::move(completion));
        })""",
        
        "loadPreset": """        .withNativeFunction ("loadPreset", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::loadPreset(audioProcessor, args, std::move(completion));
        })""",
        
        "loadLibraryPreset": """        .withNativeFunction ("loadLibraryPreset", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::loadLibraryPreset(audioProcessor, args, std::move(completion));
        })""",
        
        "runSelfTest": """        .withNativeFunction ("runSelfTest", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            juce::Logger::writeToLog("[JUNiO] IPC: runSelfTest requested by WebUI");
            auto res = audioProcessor.runSelfTest();
            notifySelfTestState();
            
            juce::DynamicObject::Ptr obj = new juce::DynamicObject();
            obj->setProperty("ok", res.ok);
            obj->setProperty("presetFailures", res.presetFailures);
            obj->setProperty("sysExOk", res.sysExOk);
            obj->setProperty("jsonOk", res.jsonOk);
            
            juce::Array<juce::var> failedArr;
            for (const auto& s : res.failedPresets) failedArr.add(s);
            obj->setProperty("failedPresets", failedArr);
            
            completion(juce::var(obj.get()));
        })""",
        
        "getSynthState": """        .withNativeFunction ("getSynthState", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::getSynthState(audioProcessor, args, std::move(completion));
        })""",
        
        "getBrowserData": """        .withNativeFunction ("getBrowserData", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::getBrowserData(audioProcessor, args, std::move(completion));
        })""",
        
        "setFavorite": """        .withNativeFunction ("setFavorite", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::setFavorite(audioProcessor, args, std::move(completion));
        })""",
        
        "updateMetadata": """        .withNativeFunction ("updateMetadata", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::updateMetadata(audioProcessor, args, std::move(completion));
        })""",
        
        "savePresetDetailed": """        .withNativeFunction ("savePresetDetailed", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::savePresetDetailed(audioProcessor, args,
                [this](const juce::String& e, const juce::var& v) { dispatchToJS(e, v); },
                [this]() { sendPresetListUpdate(); },
                std::move(completion));
        })""",
        
        "saveAsNewPresetDetailed": """        .withNativeFunction ("saveAsNewPresetDetailed", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::saveAsNewPresetDetailed(audioProcessor, args,
                [this](const juce::String& e, const juce::var& v) { dispatchToJS(e, v); },
                [this]() { sendPresetListUpdate(); },
                std::move(completion));
        })""",
        
        "pianoNoteOn": """        .withNativeFunction ("pianoNoteOn", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::pianoNoteOn(audioProcessor, args, std::move(completion));
        })""",
        
        "pianoNoteOff": """        .withNativeFunction ("pianoNoteOff", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::pianoNoteOff(audioProcessor, args, std::move(completion));
        })""",
        
        "getLibraryPath": """        .withNativeFunction ("getLibraryPath", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::getLibraryPath(audioProcessor, args, std::move(completion));
        })""",
        
        "setLibraryPath": """        .withNativeFunction ("setLibraryPath", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::setLibraryPath(audioProcessor, args, std::move(completion));
        })""",
        
        "chooseDirectory": """        .withNativeFunction ("chooseDirectory", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::chooseDirectory(audioProcessor, fileChooser, args, std::move(completion));
        })""",
    }
    
    replace_imports = {
        "confirmImportFile": """        .withNativeFunction ("confirmImportFile", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeImport::confirmImportFile(audioProcessor, pendingImportFile, pendingImportFormat, args,
                [this](const juce::String& e, const juce::var& v) { dispatchToJS(e, v); },
                [this]() { sendPresetListUpdate(); },
                [this]() { audioProcessor.requestPatchDump(); },
                std::move(completion));
        })""",
        
        "confirmTapeImport": """        .withNativeFunction ("confirmTapeImport", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeImport::confirmTapeImport(audioProcessor, pendingTapeFile, pendingSmartResult,
                selectedDecoderIndex, args,
                [this](const juce::String& e, const juce::var& v) { dispatchToJS(e, v); },
                std::move(completion));
        })""",
    }
    
    # Functions to keep (complex [this] captures)
    keep_inline = {"serviceAction", "menuAction", "uiReady", "exportBank", "importBank", "setBrowserData"}
    
    # Build old->new replacement map
    # We need to find each block's original text and replace it
    # Strategy: Find the .withNativeFunction("name", [this]...) block by matching
    # from the function name through the closing brace
    
    for name, replacement in {**replace_actions, **replace_imports}.items():
        if name in keep_inline:
            continue
        
        # Find the original block
        # Pattern: .withNativeFunction ("name", ... complete block ending with "})"
        # We need to find the exact string from the beginning of the .withNativeFunction call
        # to the matching ")" that closes the lambda + the outer function call
        
        search_start = content.find(f'.withNativeFunction ("{name}"')
        if search_start == -1:
            print(f"  WARNING: Could not find function '{name}' in WebViewEditor.cpp")
            continue
        
        # Find the end of this block
        # We need to count balanced parentheses from the start of .withNativeFunction(...)
        brace_depth = 0
        paren_depth = 0
        in_lambda = False
        in_string = False
        string_char = None
        end_pos = search_start
        
        i = search_start
        while i < len(content):
            c = content[i]
            
            # Track string literals
            if in_string:
                if c == '\\' and i + 1 < len(content):
                    i += 2
                    continue
                if c == string_char:
                    in_string = False
                i += 1
                continue
            
            if c == '"':
                in_string = True
                string_char = '"'
                i += 1
                continue
            
            if c == '{':
                brace_depth += 1
            elif c == '}':
                brace_depth -= 1
            elif c == '(':
                paren_depth += 1
            elif c == ')':
                paren_depth -= 1
            
            # The lambda ends when brace_depth == 0 and paren_depth == 0
            if brace_depth == 0 and paren_depth == 0 and i > search_start:
                end_pos = i
                break
            
            i += 1
        
        if end_pos == search_start:
            print(f"  ERROR: Could not find end for '{name}'")
            continue
        
        old_text = content[search_start:end_pos + 1]
        
        # Verify it starts with .withNativeFunction
        if not old_text.startswith(f'.withNativeFunction ("{name}"'):
            print(f"  ERROR: Text mismatch for '{name}'")
            continue
        
        content = content.replace(old_text, replacement, 1)
        changes.append(f"Replaced '{name}' lambda with BridgeActions/BridgeImport call")
    
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)
    
    print(f"\nWebViewEditor.cpp: {len(changes)} changes applied")
    for c in changes:
        print(f"  - {c}")


if __name__ == "__main__":
    print("=" * 60)
    print("STEP 1: Fix BridgeActions.cpp")
    print("=" * 60)
    fix_bridge_actions()
    
    print("\n" + "=" * 60)
    print("STEP 2: Fix BridgeImport.cpp")
    print("=" * 60)
    fix_bridge_import()
    
    print("\n" + "=" * 60)
    print("STEP 3: Update WebViewEditor.cpp")
    print("=" * 60)
    update_webview_editor()
