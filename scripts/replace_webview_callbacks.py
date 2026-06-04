#!/usr/bin/env python3
"""
Replace extracted withNativeFunction callbacks in WebViewEditor.cpp.
Uses a simpler approach: identifies blocks by function name, finds end
by counting balanced braces/parens with proper string/comment handling.
"""

import re
import os
import sys

PROJECT_ROOT = "D:/desarrollos/ABDSynths/ABDJUNiO601"
CPP_PATH = os.path.join(PROJECT_ROOT, "Source/UI/WebView/WebViewEditor.cpp")

KEEP_INLINE = {"serviceAction", "menuAction", "uiReady", "exportBank", "importBank", "setBrowserData"}

class CppBlockFinder:
    """Find balanced blocks in C++ code."""
    
    def __init__(self, text):
        self.text = text
        self.n = len(text)
    
    def find_next_block(self, start_pos, target_name):
        """Find the .withNativeFunction("target_name", ...) block starting >= start_pos.
        Returns (block_start, block_end) or None."""
        
        # Find the function call start
        pattern = f'.withNativeFunction ("{target_name}"'
        pos = self.text.find(pattern, start_pos)
        if pos == -1:
            return None
        
        # Find the closing of the .withNativeFunction(...) call
        # Strategy: count balanced pairs, handling strings properly
        i = pos
        paren_depth = 0
        brace_depth = 0
        in_str = False
        str_char = None
        
        while i < self.n:
            c = self.text[i]
            
            # Handle string literals
            if in_str:
                if c == '\\' and i + 1 < self.n:
                    i += 2
                    continue
                if c == str_char:
                    in_str = False
                i += 1
                continue
            
            if c == '"' or c == "'":
                in_str = True
                str_char = c
                i += 1
                continue
            
            # Handle line comments
            if c == '/' and i + 1 < self.n and self.text[i+1] == '/':
                next_newline = self.text.find('\n', i)
                if next_newline == -1:
                    i = self.n
                else:
                    i = next_newline + 1
                continue
            
            # Handle block comments
            if c == '/' and i + 1 < self.n and self.text[i+1] == '*':
                end_comment = self.text.find('*/', i + 2)
                if end_comment == -1:
                    i = self.n
                else:
                    i = end_comment + 2
                continue
            
            if c == '(':
                paren_depth += 1
            elif c == ')':
                paren_depth -= 1
            elif c == '{':
                brace_depth += 1
            elif c == '}':
                brace_depth -= 1
            
            # End: all balanced and we've scanned past the start
            if paren_depth == 0 and brace_depth == 0 and i > pos + len(pattern):
                return (pos, i + 1)  # include the closing char
            
            i += 1
        
        return None  # unbalanced


def main():
    with open(CPP_PATH, 'r', encoding='utf-8') as f:
        text = f.read()
    
    finder = CppBlockFinder(text)
    
    # Define all function names and their replacements
    all_replacements = {
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
        "runSelfTest": """        .withNativeFunction ("runSelfTest", [this](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            BridgeActions::runSelfTest(audioProcessor, args,
                [this]() { notifySelfTestState(); },
                std::move(completion));
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
    
    # Find all blocks
    blocks = []
    search_pos = 0
    for name in [
        "setParameter", "beginGesture", "endGesture", "getCalibrationParams", "setCalibrationParam",
        "serviceAction", "loadPreset", "loadLibraryPreset", "runSelfTest", "menuAction",
        "setBrowserData", "getSynthState", "getBrowserData", "setFavorite", "updateMetadata",
        "exportBank", "importBank", "savePresetDetailed", "saveAsNewPresetDetailed",
        "confirmImportFile", "confirmTapeImport", "pianoNoteOn", "pianoNoteOff",
        "chooseDirectory", "getLibraryPath", "setLibraryPath", "uiReady"
    ]:
        result = finder.find_next_block(search_pos, name)
        if result:
            start, end = result
            blocks.append((name, start, end))
            print(f"  {name:30s} pos {start:6d}-{end:6d} ({end-start:6d} chars)")
            search_pos = end
        else:
            print(f"  {name:30s} NOT FOUND")
    
    # Apply replacements in reverse order
    replace_count = 0
    for name, start, end in sorted(blocks, key=lambda x: x[1], reverse=True):
        if name in KEEP_INLINE:
            continue
        if name in all_replacements:
            new_text = all_replacements[name]
            print(f"  REPLACING '{name}': {end-start} chars -> {len(new_text)} chars")
            text = text[:start] + new_text + text[end:]
            replace_count += 1
    
    with open(CPP_PATH, 'w', encoding='utf-8') as f:
        f.write(text)
    
    print(f"\nDone: {replace_count} replacements applied")
    print(f"Kept: {', '.join(sorted(KEEP_INLINE))}")

if __name__ == "__main__":
    main()
