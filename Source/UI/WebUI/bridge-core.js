/**
 * ABD JUNiO 601 - Bridge Core
 * Global bridge infrastructure, event system, and app initialization.
 */

let lastPresetName = "INITIAL PATCH";
let lcdTimer = null;
let promiseId = 0;
let octaveShift = 0;
let lastSysExHex = "";
let currentGroupGlobal = 0;
let currentBankGlobal = 1;
let currentPatchGlobal = 1;
let isWriteArmed = false;
let targetBank = 1;
let targetPatch = 1;
let memoryProtectActive = false;
let globalUserName = "ABD USER";
let paramValuesCache = {};
let delaySyncBPM = 120;  // Last known DAW BPM for delay sync display

// Shared sync division names (matches JunoTapeEcho::kDivBeats)
const K_SYNC_DIV_NAMES = ['1/1','1/2','1/4','1/4T','1/8','1/8T','1/16','1/16T','1/32'];

let sysexMirror = new Array(23).fill(0);
sysexMirror[0] = 0xF0; sysexMirror[1] = 0x41; sysexMirror[2] = 0x30;
sysexMirror[22] = 0xF7;

const eventListenersInternal = {};
window.onJuceEvent = (name, data) => {
    console.log(`[JS Bridge] Received Event: ${name}`, data);
    if (eventListenersInternal[name]) {
        eventListenersInternal[name].forEach(cb => cb(data));
    }
};

function getBackend() {
    return window.juceBackend || (window.__JUCE__ && window.__JUCE__.backend) || window.__juce__ || (window.juce && typeof window.juce.addEventListener === 'function' ? window.juce : null);
}

function callNative(name, ...args) {
    const backend = getBackend();
    if (!backend) {
        console.error("No JUCE bridge available for", name);
        return Promise.reject("No bridge");
    }

    // Try direct function call on backend (e.g. if WebView2 or other bindings exposed it directly)
    if (typeof backend[name] === 'function') {
        try {
            return backend[name](...args);
        } catch (e) {
            console.warn(`[JS Bridge] Direct call to ${name} failed, falling back to emitEvent:`, e);
        }
    }

    // Fallback to emitEvent (which is JUCE 8 standard mechanism)
    if (typeof backend.emitEvent === 'function') {
        const id = promiseId++;
        const p = new Promise((resolve) => {
            const handler = backend.addEventListener("__juce__complete", (data) => {
                if (data && data.promiseId === id) {
                    backend.removeEventListener(handler);
                    resolve(data.result);
                }
            });
            setTimeout(() => { backend.removeEventListener(handler); resolve(null); }, 5000);
        });
        backend.emitEvent("__juce__invoke", { name, params: args, resultId: id });
        return p;
    }

    console.error(`[JS Bridge] Function ${name} not found on backend and emitEvent is not available`);
    return Promise.reject("Not found");
}

function listenEvent(eventName, callback) {
    if (!eventListenersInternal[eventName]) eventListenersInternal[eventName] = [];
    if (!eventListenersInternal[eventName].includes(callback)) {
        eventListenersInternal[eventName].push(callback);
    }
    const backend = getBackend();
    if (backend && typeof backend.addEventListener === 'function') {
        backend.addEventListener(eventName, callback);
    }
}

window.juce = {
    menuAction: (action, ...args) => callNative("menuAction", action, ...args),
    setParameter: (id, val) => callNative("setParameter", id, val),
    beginGesture: (id) => callNative("beginGesture", id),
    endGesture: (id) => callNative("endGesture", id),
    loadPreset: (idx) => callNative("loadPreset", idx),
    confirmTapeImport: (baud) => callNative("confirmTapeImport", baud),
    confirmImportFile: () => callNative("confirmImportFile"),
    chooseDirectory: (path) => callNative("chooseDirectory", path),
    getLibraryPath: () => callNative("getLibraryPath"),
    setLibraryPath: (path) => callNative("setLibraryPath", path),
    getCalibrationParams: () => callNative("getCalibrationParams"),
    setCalibrationParam: (id, val) => callNative("setCalibrationParam", id, val),
    serviceAction: (data) => callNative("serviceAction", data),
    getBrowserData: () => callNative("getBrowserData"),
    setFavorite: (lib, prst, fav) => callNative("setFavorite", lib, prst, fav),
    updateMetadata: (lib, prst, name, aut, tag, nts) => callNative("updateMetadata", lib, prst, name, aut, tag, nts),
    switchAB: (slot) => callNative("switchAB", slot),
    copyAB: () => callNative("copyAB"),
    exportBank: (lib) => callNative("exportBank", lib),
    importBank: () => callNative("importBank"),
    setBrowserData: (data) => callNative("setBrowserData", data),
    loadLibraryPreset: (libIdx, prstIdx) => callNative("loadLibraryPreset", libIdx, prstIdx),
    savePresetDetailed: (libIdx, prstIdx) => callNative("savePresetDetailed", libIdx, prstIdx),
    saveAsNewPresetDetailed: (name, cat, aut, tag, nts) => callNative("saveAsNewPresetDetailed", name, cat, aut, tag, nts),
    selectLibrary: (idx) => callNative("selectLibrary", idx)
};

// =============================
// LCD
// =============================
function updateLCD(text, isTemporary) {
    const lcd = document.getElementById('lcd-text');
    if (!lcd) return;
    if (lcdTimer) clearTimeout(lcdTimer);

    const applyText = (target, str) => {
        if (str.length > 16) {
            target.innerHTML = `<div class="marquee-scroller">${str} &nbsp;&nbsp;&nbsp;&nbsp; ${str}</div>`;
        } else {
            target.innerText = str;
        }
    };

    if (isTemporary) {
        applyText(lcd, text);
        lcd.style.color = "#ff8888";
        lcdTimer = setTimeout(() => {
            applyText(lcd, lastPresetName);
            lcd.style.color = "#ff3c3c";
            lcdTimer = null;
        }, 3000);
    } else {
        lastPresetName = text;
        applyText(lcd, text);
        lcd.style.color = "#ff3c3c";
        if (lcdTimer) {
            clearTimeout(lcdTimer);
            lcdTimer = null;
        }
    }
}

// =============================
// DOM READY - INIT
// =============================
function initApp() {
    listenEvent("onParameterChanged", (data) => syncUI(data.id, data.value));
    listenEvent("parameterSetUpdate", (data) => {
        if (data) {
            for (let id in data) syncUI(id, data[id]);
        }
    });
    listenEvent("showModal", (data) => {
        if (data === "preferences") showGlobalSettings('general');
        else if (data === "about") showAbout();
        else if (data === "serviceMode") showGlobalSettings('calibration');
        else if (data === "browser") showBrowser();
    });
    listenEvent("onShowAbout", () => showAbout());
    listenEvent("onShowSettings", () => showSettings());
    listenEvent("onTuningUpdate", (name) => {
        const info = document.getElementById('tuning-info');
        if (info) info.innerText = name.toUpperCase();
    });
    listenEvent("sysexLog", (hex) => updateSysExLog(hex));

    function updateSysExLog(hex) {
        const log = document.getElementById('sysex-log');
        if (!log) return;
        const parts = hex.split(' ');
        let formatted = '';
        parts.forEach((p, i) => {
            if (i === 0 || i === parts.length - 1) formatted += `<span style="color: #fa0">${p}</span> `;
            else formatted += p + ' ';
        });
        log.innerHTML = formatted;
        log.scrollTop = log.scrollHeight;
    }

    const updateDigit = (elementId, char) => {
        const el = document.getElementById(elementId);
        if (!el) return;
        let filename = char.toString().toUpperCase();
        if (filename === " ") filename = "SPACE";
        if (filename === "-") filename = "DASH";
        const spritePath = `assets/led/${filename}.png`;
        let textSpan = el.querySelector('.led-text-fallback');
        if (!textSpan) {
            textSpan = document.createElement('span');
            textSpan.className = 'led-text-fallback';
            el.appendChild(textSpan);
        }
        textSpan.innerText = char;
        let img = el.querySelector('img.led-sprite');
        if (!img) {
            img = document.createElement('img');
            img.className = 'led-sprite';
            img.style.height = "100%";
            img.style.display = "none";
            img.onerror = () => { img.style.display = "none"; textSpan.style.display = "inline"; };
            img.onload = () => { img.style.display = "block"; textSpan.style.display = "none"; };
            el.appendChild(img);
        }
        if (img.src.indexOf(spritePath) === -1) img.src = spritePath;
    };

    const handleBankPatch = (data) => {
        if (!data) return;
        currentGroupGlobal = data.group || 0;
        const groupChar = String.fromCharCode(65 + currentGroupGlobal);
        updateDigit('bank-digit', groupChar);
        currentBankGlobal = data.bank || 1;
        updateDigit('patch-digit-1', currentBankGlobal);
        currentPatchGlobal = data.patch || 1;
        updateDigit('patch-digit-2', currentPatchGlobal);
    };
    listenEvent("onBankPatchUpdate", handleBankPatch);

    listenEvent("onLCDUpdate", (text) => updateLCD(text, false));

    listenEvent("onMidiTraffic", (active) => {
        const led = document.getElementById('front-midi-badge');
        if (led) {
            led.classList.toggle('active', active);
            if (active) setTimeout(() => led.classList.remove('active'), 100);
        }
    });

    const backend = getBackend();
    const initData = window.__JUCE__ ? window.__JUCE__.initialisationData : 
                    (backend ? (backend.initialisationData || (typeof backend.getInitialisationData === 'function' ? backend.getInitialisationData() : null)) : null);

    let currentTargetModel = 0;
    let currentProductName = "ABD JUNiO 601";
    if (initData) {
        if (initData.productName) currentProductName = initData.productName;
        if (initData.targetModel !== undefined) currentTargetModel = initData.targetModel;
    }

    function getModelSpecLabel(model) {
        const labels = {
            0: 'SUPER SIX HYBRID',
            1: 'JUNO-106',
            2: 'JUNO-60',
            3: 'JUNO-6'
        };
        return labels[model] || 'SUPER SIX HYBRID';
    }
    
    function updateModelVisibility(model) {
        // model: 0=SuperSix, 1=J106, 2=J60, 3=J6
        // Note: These are JUNO_TARGET_MODEL values, different from SynthParams.h
        // routing values (where 0=J6, 1=J60, 2=J106).
        const isSuperSix = (model === 0);
        const isJ106 = (model === 1);
        const isJ60 = (model === 2);
        const isJ6 = (model === 3);
        
        // Update model spec label in top bar
        const specEl = document.getElementById('active-model-spec');
        if (specEl) specEl.innerText = getModelSpecLabel(model);
        
        // In SuperSix (HYBRID) mode, show ALL sections — routing is dynamic.
        // Only hide sections when compiled for a fixed model.
        // The C++ engine (applyChorus, JunoDCO::getNextSample, etc.) already
        // handles model-specific behavior via GET_MODEL_* macros.
        
        // DCO section: J6 has no sub-oscillator or PWM
        const dcoLfoPwm = document.getElementById('dco-lfo-pwm');
        const dcoMix = document.getElementById('dco-mix');
        if (dcoLfoPwm) dcoLfoPwm.classList.toggle('hidden', isJ6);
        if (dcoMix) dcoMix.classList.toggle('hidden', isJ6);
        
        // CHORUS section: Only J106 has hardware chorus
        // Show in SuperSix and J106; hide in J60 and J6
        const chorusSection = document.getElementById('chorus');
        if (chorusSection) chorusSection.classList.toggle('hidden', !isJ106 && !isSuperSix);
        
        // VCF Polarity: Only J106 has POS/NEG switch; hide for J60 and J6
        // Also hide the parent .ctrl-group.center wrapper to prevent empty space in flex layout
        const vcfPolarity = document.getElementById('vcf-polarity');
        if (vcfPolarity) {
            vcfPolarity.classList.toggle('hidden', isJ60 || isJ6);
            const vcfCenterGroup = vcfPolarity.closest('.ctrl-group.center');
            if (vcfCenterGroup) vcfCenterGroup.classList.toggle('hidden', isJ60 || isJ6);
        }
        
        // VCA Mode: Only J106 has GATE/ENV switch; hide for J60 and J6
        const vcaMode = document.getElementById('vca-mode');
        if (vcaMode) vcaMode.classList.toggle('hidden', isJ60 || isJ6);
        // When VCA mode hidden, VCA controls should center LEVEL
        const vcaControls = document.querySelector('#vca .controls');
        if (vcaControls) vcaControls.style.justifyContent = (isJ60 || isJ6) ? 'center' : '';
        
        // Voice Count selector: Only show in Super Six (hybrid mode); hide for fixed models
        // For single-model builds, force voice count to 6 (CLASSIC)
        const numVoicesRow = document.getElementById('setting-numVoices-row');
        if (numVoicesRow) numVoicesRow.classList.toggle('hidden', !isSuperSix);
        
        // DCO separators: hide when adjacent groups are hidden (J-6 has LFO/PWM and SUB/NOISE hidden)
        const dcoSection = document.getElementById('dco');
        if (dcoSection) {
            dcoSection.querySelectorAll('.separator').forEach(sep => {
                sep.classList.toggle('hidden', isJ6);
            });
        }
        
        // Header panel grid: adjust columns when sysex-zone AND protect-zone are hidden (J-6)
        // Default grid: 230px 60px 1fr 400px; J-6: both protect-zone (60px) and sysex-zone (400px)
        // are display:none, so grid must be 230px 1fr to avoid lcd-bezel landing in empty 60px track
        const headerPanel = document.getElementById('header-panel');
        if (headerPanel) {
            if (isJ6) {
                headerPanel.style.gridTemplateColumns = '230px 1fr';
            } else {
                headerPanel.style.gridTemplateColumns = '';
            }
        }
        
        // Engine row 2: when chorus is hidden (J-60, J-6), env module needs no right border
        // since it becomes the last element in the row
        const envModule = document.getElementById('env');
        if (envModule) {
            const isChorusHidden = !isJ106 && !isSuperSix;
            if (isChorusHidden) {
                envModule.style.borderRight = 'none';
            } else {
                envModule.style.borderRight = '';
            }
        }
        
        // Force numVoices to 6 (CLASSIC) for single-model builds
        // Only call setCalibrationParam when value differs to avoid redundant saves
        if (!isSuperSix && paramValuesCache['numVoices'] !== 6.0) {
            paramValuesCache['numVoices'] = 6.0;
            if (typeof juce !== 'undefined' && juce.setCalibrationParam) {
                juce.setCalibrationParam('numVoices', 6.0);
            }
        }
        
        // Force calibrationProfile for single-model builds (profile is fixed at compile time)
        if (!isSuperSix) {
            let targetProfile = 2; // J-106 default
            if (isJ60) targetProfile = 1;
            else if (isJ6) targetProfile = 0;
            
            if (paramValuesCache['calibrationProfile'] !== targetProfile) {
                paramValuesCache['calibrationProfile'] = targetProfile;
                if (typeof juce !== 'undefined' && juce.setCalibrationParam) {
                    juce.setCalibrationParam('calibrationProfile', targetProfile);
                }
            }
        }

        // Force calibrationProfile to Super Six (3.0) for Super Six builds
        // This ensures the Hybrid profile is always active on Super Six hardware,
        // even if a previous calibration.json was saved with profile 2.0 (J-106).
        // Uses setCalibrationParam (not setParameter) because calibrationProfile
        // is in CalibrationSettings, NOT in APVTS.
        if (isSuperSix) {
            const targetProfile = 3;
            const curProfile = paramValuesCache['calibrationProfile'] !== undefined
                ? Math.round(paramValuesCache['calibrationProfile']) : -1;
            if (curProfile !== targetProfile) {
                paramValuesCache['calibrationProfile'] = targetProfile;
                if (typeof juce !== 'undefined' && juce.setCalibrationParam) {
                    juce.setCalibrationParam('calibrationProfile', targetProfile);
                }
            }

            // Also initialize routing params (APVTS) to Super Six defaults
            // This ensures the audio engine actually runs in Super Six mode,
            // not just the UI. These are soft-updated (only if at previous default)
            // to avoid overriding manual tweaks between sessions.
            const routingDefaults = {
                "modelDCO": 1.0,
                "modelHPF": 1.0,
                "modelVCF": 1.0,
                "modelADSR": 1.0,
                "modelChorus": 1.0,
                "modelArp": 0.5,
                "modelPoly": 1.0,
                "modelPorta": 1.0,
                "modelUnison": 1.0
            };
            for (const [paramId, newVal] of Object.entries(routingDefaults)) {
                const cacheVal = paramValuesCache[paramId];
                // Only set if not yet in cache (first init) OR still at the default
                if (cacheVal === undefined) {
                    paramValuesCache[paramId] = newVal;
                    if (typeof juce !== 'undefined' && juce.setParameter) {
                        juce.setParameter(paramId, newVal);
                    }
                }
            }
        }

        // DELAY tab: only visible when Super Six hardware AND profile is 3
        // This must run AFTER the profile force above so cache is populated.
        {
            const profileForDelay = paramValuesCache['calibrationProfile'] !== undefined
                ? Math.round(paramValuesCache['calibrationProfile']) : -1;
            const canShowDelay = isSuperSix && profileForDelay === 3;
            const delayTab = document.getElementById('tab-btn-delay');
            if (delayTab) delayTab.classList.toggle('hidden', !canShowDelay);
            const tabNavL = document.getElementById('tab-nav-left');
            const tabNavR = document.getElementById('tab-nav-right');
            if (tabNavL) tabNavL.classList.toggle('hidden', !canShowDelay);
            if (tabNavR) tabNavR.classList.toggle('hidden', !canShowDelay);
        }
        
        // ARP tab: Only J6 and J60 have arpeggiator (hide for J106)
        // NOTE: Only toggle tab button visibility, NOT the panel itself.
        // Panel visibility is managed exclusively by switchPerformanceTab / deactivatePerformanceTabs.
        const arpTab = document.getElementById('tab-btn-arp');
        if (arpTab) arpTab.classList.toggle('hidden', isJ106);
        
        // ROUTING tab: Only visible in Super Six mode
        const routingTab = document.getElementById('tab-routing');
        if (routingTab) routingTab.classList.toggle('hidden', !isSuperSix);
        
        // CALIBRATION tab: "SHOW ALL" label — "SUPER SIX MODE" is misleading for single-model builds
        const showAllLabel = document.getElementById('calibration-show-all-label');
        if (showAllLabel) {
            showAllLabel.innerText = isSuperSix ? 'SHOW ALL (SUPER SIX MODE)' : 'SHOW ALL PARAMS';
        }
        
        // =============================================
        // JUNO-6 (model 3): No patch memory on hardware
        // Hide bank/patch navigation, 7-segment display,
        // patch grid, protect switch, and preset management
        // utility buttons. Keep: RANDOM, PANIC, TEST.
        // =============================================
        
        // Navigation row: BK-, BK+, PT-, PT+
        // J-6: keep visible for quick preset browsing (no patch memory, but BK/PT buttons navigate)
        // The patch grid (1-8 buttons) is hidden below, but the nav arrows remain.
        const navRow = document.getElementById('nav-row');
        if (navRow) navRow.classList.toggle('hidden', false);
        
        // SysEx zone: hide for J-6 (no hardware SysEx display), keep for J-106, J-60, Super Six
        const sysexZone = document.getElementById('sysex-zone');
        if (sysexZone) sysexZone.classList.toggle('hidden', isJ6);
        
        // Patch grid (1-8 buttons)
        const patchGrid = document.getElementById('patch-grid-container');
        if (patchGrid) patchGrid.classList.toggle('hidden', isJ6);
        
        // Top-left display: hide only the 7-segment LED and BANK/PATCH label
        // Keep the front-midi-badge visible — J-6 needs MIDI for note input
        const sevenSegDisplay = document.getElementById('seven-segment-display');
        if (sevenSegDisplay) sevenSegDisplay.classList.toggle('hidden', isJ6);
        const zoneLabel = document.querySelector('#top-left-display .zone-label');
        if (zoneLabel) zoneLabel.classList.toggle('hidden', isJ6);
        
        // Protect zone: memory protect switch (J-6 has no patch memory)
        const protectZone = document.getElementById('protect-zone');
        if (protectZone) protectZone.classList.toggle('hidden', isJ6);
        
        // Utility buttons: hide preset management for J-6
        const utilPresetBtns = ['util-manual', 'util-write', 'util-save', 'util-verify', 'util-load'];
        utilPresetBtns.forEach(id => {
            const el = document.getElementById(id);
            if (el) el.classList.toggle('hidden', isJ6);
        });
    }

    const updateUIProductNames = (name, targetModel) => {
        const model = (targetModel !== undefined) ? targetModel : currentTargetModel;
        currentProductName = name;
        currentTargetModel = model;
        document.title = name + " - Gold Standard";
        const splashH1 = document.querySelector('#splash-screen h1');
        if (splashH1) splashH1.innerText = name;
        const aboutTitle = document.querySelector('.about-title');
        if (aboutTitle) aboutTitle.innerText = name;
        const aboutP = document.querySelector('.about-content p');
        if (aboutP) {
            if (name.includes("Super")) aboutP.innerText = 'A meticulous "Gold Standard" hybrid emulation combining Juno-6, Juno-60 and Juno-106 circuits.';
            else if (name.includes("601")) aboutP.innerText = 'A meticulous "Gold Standard" emulation of the classic 1984 analog poly-synth (Juno-106).';
            else if (name.includes("06")) aboutP.innerText = 'A meticulous "Gold Standard" emulation of the classic 1982 analog poly-synth (Juno-60).';
            else if (name.includes("SIX")) aboutP.innerText = 'A meticulous "Gold Standard" emulation of the classic 1982 analog poly-synth (Juno-6).';
        }
        const miniTitle = document.getElementById('app-title-mini');
        if (miniTitle) {
            const vMatch = miniTitle.innerText.match(/v\d+\.\d+\.\d+/);
            const vStr = vMatch ? " " + vMatch[0] : "";
            miniTitle.innerText = name + vStr;
        }
        document.querySelectorAll('.dropdown li').forEach(li => {
            if (li.innerText.startsWith("About ABD") || li.innerText.startsWith("About JUNiO")) {
                li.innerText = "About " + name + "...";
            }
        });

        // Show wrench button only on Super Six (model 0)
        const repairBtn = document.getElementById('about-repair-btn');
        if (repairBtn) {
            repairBtn.style.display = (model === 0) ? 'inline-block' : 'none';
        }
        
        // Update model-specific visibility
        updateModelVisibility(model);
    };

    // [FIX] Update version + dismiss splash FIRST — before any risky DOM operations.
    // If updateUIProductNames / updateModelVisibility throw, splash still hides.
    const buildVer = (initData && initData.buildVersion) ? initData.buildVersion : null;
    if (buildVer) {
        document.querySelectorAll('.splash-version, #app-title-mini, .about-version').forEach(el => {
            if (el.id === 'app-title-mini') el.innerText = currentProductName + " v" + buildVer;
            else el.innerText = "Version " + buildVer;
        });
    }
    setTimeout(() => {
        const splash = document.getElementById('splash-screen');
        if (splash) {
            splash.style.opacity = '0';
            setTimeout(() => splash.style.display = 'none', 1000);
        }
        // Trigger app fade-in (CSS animation on #synth-app.app-visible)
        document.getElementById('synth-app')?.classList.add('app-visible');
    }, 3000);

    updateUIProductNames(currentProductName, currentTargetModel);

    listenEvent("onVersionUpdate", (version) => {
        document.querySelectorAll('.splash-version, #app-title-mini, .about-version').forEach(el => {
            if (el.id === 'app-title-mini') el.innerText = currentProductName + " v" + version;
            else el.innerText = "Version " + version;
        });
    });

    listenEvent("onProductNameUpdate", (data) => {
        if (typeof data === 'string') {
            updateUIProductNames(data, currentTargetModel);
        } else if (typeof data === 'object') {
            updateUIProductNames(data.name || currentProductName, data.targetModel !== undefined ? data.targetModel : currentTargetModel);
        }
    });

    listenEvent("onVisualUpdate", (data) => {
        const c1Led = document.getElementById('led-chorus1');
        const c2Led = document.getElementById('led-chorus2');
        if (c1Led && c1Led.classList.contains('active')) c1Led.style.opacity = 0.5 + Math.sin(data.c1 * Math.PI * 2) * 0.5;
        else if (c1Led) c1Led.style.opacity = 1;
        if (c2Led && c2Led.classList.contains('active')) c2Led.style.opacity = 0.5 + Math.sin(data.c2 * Math.PI * 2) * 0.5;
        else if (c2Led) c2Led.style.opacity = 1;
    });

    listenEvent("onLCDStatusUpdate", (data) => {
        const lcLed = document.getElementById('badge-lc');
        const abLed = document.getElementById('badge-ab');
        const wipLed = document.getElementById('badge-wip');
        if (lcLed) { lcLed.classList.toggle('active', data.lc); lcLed.innerText = data.lc ? 'LRN' : 'LC'; }
        if (abLed) { abLed.innerText = data.ab; abLed.classList.toggle('b-slot', data.ab === 'B'); }
        if (wipLed) { wipLed.innerText = data.wip; wipLed.classList.toggle('has-wip', data.wip > 0); }
    });

    listenEvent("onDelayBPMUpdate", (bpm) => {
        // Store last known BPM for preset-change display
        delaySyncBPM = Math.round(bpm);

        // --- DELAY SYNC LCD ---
        const delaySyncSwitch = document.querySelector('.sw-unit.mini[data-param="delaySyncEnabled"]');
        if (delaySyncSwitch && delaySyncSwitch.getAttribute('data-state') === "1") {
            updateLCD("SYNC: " + delaySyncBPM + " BPM", true);
        }

        // Update SVG BPM indicator (delay tab)
        const bpmEl = document.getElementById('svg-bpm-indicator');
        if (bpmEl) {
            const syncEnabled = paramValuesCache['delaySyncEnabled'] !== undefined ?
                paramValuesCache['delaySyncEnabled'] > 0.5 : false;
            if (syncEnabled) {
                bpmEl.textContent = delaySyncBPM + ' BPM';
            }
        }

        // --- ARP SYNC BPM INDICATOR (performance panel) ---
        const arpBpmEl = document.getElementById('arp-bpm-indicator');
        const arpSync = paramValuesCache['arpSync'] !== undefined ?
            paramValuesCache['arpSync'] > 0.5 : false;

        if (arpBpmEl) {
            if (arpSync) {
                arpBpmEl.textContent = delaySyncBPM + ' BPM';
                arpBpmEl.classList.remove('arp-bpm-hidden');
                arpBpmEl.classList.add('arp-bpm-visible');
            } else {
                arpBpmEl.classList.remove('arp-bpm-visible');
                arpBpmEl.classList.add('arp-bpm-hidden');
            }
        }

        // --- ARP SYNC BPM INDICATOR (Settings modal) ---
        const arpBpmSettingsEl = document.getElementById('arp-bpm-indicator-settings');
        const arpBpmRow = document.getElementById('arp-bpm-row-settings');
        if (arpBpmSettingsEl && arpBpmRow) {
            if (arpSync) {
                arpBpmSettingsEl.textContent = delaySyncBPM + ' BPM';
                arpBpmRow.style.display = '';
            } else {
                arpBpmRow.style.display = 'none';
            }
        }

        if (arpSync) {
            updateLCD("ARP SYNC: " + delaySyncBPM + " BPM", true);
        }

        // Start ARP sync blink at new tempo
        if (typeof startArpSyncBlink === 'function') {
            startArpSyncBlink();
        }

        // Restart peak LED animation with new tempo when sync is active
        // Peak LED animation is now controlled by delay effect state (delayEffectActive)
        // via the delayEchoCancel syncUI handler in ui-sliders.js
    });

    listenEvent("onSysExUpdate", (hex) => {
        const log = document.getElementById('sysex-log');
        if (!log) return;
        const parts = hex.trim().split(' ');
        const bytes = parts.filter(p => p.length > 0).map(h => parseInt(h, 16));
        if (bytes.length === 23) { sysexMirror = [...bytes]; }
        else if (bytes.length === 7 && bytes[2] === 0x32) {
            const paramId = bytes[4];
            const paramVal = bytes[5];
            const bodyIndex = 4 + paramId;
            if (bodyIndex < sysexMirror.length - 1) sysexMirror[bodyIndex] = paramVal;
        }
        let html = '';
        sysexMirror.forEach((byte, i) => {
            const h = byte.toString(16).toUpperCase().padStart(2, '0');
            const label = i === 20 ? 'SW1' : (i === 21 ? 'SW2' : '');
            const highlight = (bytes.length === 7 && (i === (4 + bytes[4]))) ? 'changed' : 'normal';
            html += `<span class="hex-byte ${highlight}" title="Index ${i} ${label}">${h}</span> `;
            if (i === 11) html += '<br>';
        });
        log.innerHTML = html;
        lastSysExHex = hex;
        setTimeout(() => { log.querySelectorAll('.changed').forEach(el => el.classList.remove('changed')); }, 1000);
    });

    try {
        setupSliders();
        setupButtons();
        setupBender();
        setupKeyboard();
        setupMenus();
        setupOctaveButtons();
    } catch (e) {
        console.error('[JUNiO] Error during UI setup:', e);
    }

    try {
        if (window.ServiceMode && typeof window.ServiceMode.refreshParams === 'function') {
            window.ServiceMode.refreshParams().then(() => {
                updateThemeAndSkins();
                updatePainterTapes();
            });
        } else {
            updateThemeAndSkins();
            updatePainterTapes();
        }
    } catch (e) {
        console.error('[JUNiO] Error during theme/skin init:', e);
    }

    // Expose updateModelVisibility globally so theme-manager.js can call it
    // when the user changes calibrationProfile, adapting all UI sections
    // (chorus, VCF polarity, VCA mode, etc.) to the selected profile.
    window.__updateModelVisibility = updateModelVisibility;

    try {
        if (typeof switchPerformanceTab === 'function') {
            switchPerformanceTab('voltune');
        }
    } catch (e) {
        console.error('[JUNiO] Error initializing performance tab:', e);
    }

    console.log(`[DEBUG] initApp complete. targetModel=${currentTargetModel} calibrationProfile=${paramValuesCache['calibrationProfile']}`);

    callNative("uiReady");
}

document.addEventListener('DOMContentLoaded', () => {
    const splashVersion = document.getElementById('splash-version-info');

    // [FIX] Defensive fallback: ALWAYS dismiss splash after 10 seconds,
    // even if initApp() throws and its internal timer never fires.
    let splashDismissed = false;
    function forceDismissSplash() {
        if (splashDismissed) return;
        splashDismissed = true;
        const splash = document.getElementById('splash-screen');
        if (splash) {
            splash.style.transition = 'opacity 0.5s';
            splash.style.opacity = '0';
            setTimeout(() => { splash.style.display = 'none'; }, 600);
        }
        // Trigger app fade-in even if initApp() never ran
        document.getElementById('synth-app')?.classList.add('app-visible');
    }
    setTimeout(forceDismissSplash, 10000);

    let retries = 0;
    const checkBridge = setInterval(() => {
        try {
            const backend = getBackend();
            if (backend) {
                clearInterval(checkBridge);
                if (splashVersion) splashVersion.innerText = "BRIDGE DETECTED, INITIALIZING...";
                initApp();
            } else {
                retries++;
                if (retries === 20 && splashVersion) splashVersion.innerText = "WAITING FOR JUCE BRIDGE (Retrying...)";
                if (retries === 50 && splashVersion) splashVersion.innerText = "NO BRIDGE DETECTED - CONTACT SUPPORT";
                if (retries > 100) { clearInterval(checkBridge); initApp(); }
            }
        } catch (e) {
            console.error('[JUNiO] Bridge check / initApp error:', e);
            clearInterval(checkBridge);
            forceDismissSplash();
        }
    }, 100);
});

// =============================
// Clipboard
// =============================
function copySysExToClipboard() {
    if (lastSysExHex) {
        navigator.clipboard.writeText(lastSysExHex).then(() => updateLCD("SYSEX COPIED", true))
            .catch(err => { console.error('Clipboard error:', err); updateLCD("COPY ERROR", true); });
    } else { updateLCD("NO SYSEX DATA", true); }
}
