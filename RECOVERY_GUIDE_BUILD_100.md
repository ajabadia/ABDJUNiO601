# JUNiO 601 Recovery Guide: Sync & Bridge Hardening (Post-13:23 Advances)

This guide summarizes the successful improvements made between 13:23 and 00:30 (Build 100) that should be re-applied to the restored 13:23 backup.

## [KEEP] Successful Advances
These changes fixed the "Silent Bridge" and "LCD Lag" issues:

### 1. C++ Bridge Driver (OmegaUiBridge.cpp)
- **Direct Event Emission**: Change `triggerEvent` to use `webComponent.emitEventIfBrowserIsVisible(eventName, parameters);`.
- **Protocol Alignment**: Ensure `updateLCD` triggers `onLCDUpdate` (matching the `listenEvent` in `script.js`).
- **Bank/Patch Updates**: Add `updateBankPatch(int bank, int patch)` to emit the `onBankPatchUpdate` event.

### 2. Audio Engine Sync (PluginProcessor.cpp & JunoVoice.cpp)
- **Instant Parameter Snap**: In `loadPreset`, call `voiceManager.forceUpdate()`. This bypasses smoothing and makes the sound change instantly with the UI.
- **Atomic Dirty Flag**: Use `std::atomic<bool> paramsAreDirty` in `processBlock` to safely sync APVTS to the DSP thread.

### 3. UI Synchronization (WebViewEditor.cpp)
- **Bank/Patch Navigation**: Standardize `handleBankInc/Dec` and `handlePatchInc/Dec` in the `menuAction` handler to update both the engine and the UI digits.
- **Initial State Dump**: Call `sendStateToUI()` inside the `uiReady` handler.

### 4. JS Bridge (script.js)
- **Modal Listener**: Add `listenEvent("showModal", ...)` to handle "browser", "about", and "preferences" triggers from the engine.
- **Bank/Patch Digits**: Ensure `onBankPatchUpdate` is listening to update the LED segments.

---

## [AVOID] Errors & Regressions to Skip
Avoid these mistakes made during the Build 99/100 rush:

1.  **Overwriting browser.js**: Do NOT replace the working 13:23 functional browser with the experimental "Advanced" version unless it is fully compatible with the `PresetManager` data structure.
2.  **Redundant menuActions**: Do not have both `bank-inc` and `handleBankInc`. Use only the `handle` prefixed version for consistency.
3.  **Mismatching Events**: Never use `lcdUpdate` in C++ and `onLCDUpdate` in JS; verify every event name string.
4.  **Losing Asset Paths**: Ensure `assets/led/` prefix is consistent for the segment display.

## Verification Checklist for Re-implementation
1. [ ] Sound changes immediately when clicking patches.
2. [ ] Bank digits (1-8) update when clicking Bank +/-.
3. [ ] Preset Browser opens when clicking File > Preset Browser.
4. [ ] Sliders move in UI when the engine changes (e.g., A/B switch).
