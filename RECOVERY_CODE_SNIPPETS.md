# RECOVERY CODE SNIPPETS (BUILD 100)

## 1. OmegaUiBridge::triggerEvent (Critical for JUCE 8)
Replace the old indirect call with this direct emission:
```cpp
void OmegaUiBridge::triggerEvent(const juce::String& eventName, const juce::var& parameters)
{
    // [OMEGA] JUCE 8 transition: emitEvent directly to trigger backend.addEventListener in JS.
    webComponent.emitEventIfBrowserIsVisible(eventName, parameters);
}
```

## 2. PluginProcessor::processBlock Sync
Add this flag check at the START of processBlock (before any voice processing):
```cpp
if (paramsAreDirty.exchange(false)) {
    updateParamsFromAPVTS();
    voiceManager.updateParams(currentParams);
}
```

## 3. script.js showModal Listener
Add this to initApp() so the modal triggers work:
```javascript
listenEvent("showModal", (type) => {
    console.log(`[JS] showModal received: ${type}`);
    if (type === 'browser') showBrowser();
    else if (type === 'serviceMode') showGlobalSettings('diagnostics'); 
    else if (type === 'about') showAbout();
});
```
