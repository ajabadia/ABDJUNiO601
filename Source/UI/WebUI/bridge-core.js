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
    return (window.__JUCE__ && window.__JUCE__.backend) || window.juce || window.__juce__ || window.juceBackend;
}

function callNative(name, ...args) {
    const backend = getBackend();
    if (!backend) {
        console.error("No JUCE bridge available for", name);
        return Promise.reject("No bridge");
    }
    if (typeof backend.callNativeFunction === 'function') {
        return backend.callNativeFunction(name, args);
    }
    if (typeof backend[name] === 'function') {
        return backend[name](...args);
    }
    if (name === "serviceAction" || name === "getCalibrationParams" || name === "setCalibrationParam") {
        if (backend.withNativeFunction) {
             return backend.withNativeFunction(name)(...args);
        }
    }
    console.log(`[JS Bridge] Calling ${name} via emitEvent with:`, args);
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

function listenEvent(eventName, callback) {
    if (!eventListenersInternal[eventName]) eventListenersInternal[eventName] = [];
    if (!eventListenersInternal[eventName].includes(callback)) {
        eventListenersInternal[eventName].push(callback);
    }
    const backend = getBackend();
    if (backend) {
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

    let currentProductName = "ABD JUNiO 601";
    if (initData && initData.productName) {
        currentProductName = initData.productName;
    }

    const updateUIProductNames = (name) => {
        currentProductName = name;
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
    };

    if (initData && initData.buildVersion) {
        const vStr = initData.buildVersion;
        document.querySelectorAll('.splash-version, #app-title-mini, .about-version').forEach(el => {
            if (el.id === 'app-title-mini') el.innerText = currentProductName + " v" + vStr;
            else el.innerText = "Version " + vStr;
        });
        updateUIProductNames(currentProductName);
    }

    listenEvent("onVersionUpdate", (version) => {
        document.querySelectorAll('.splash-version, #app-title-mini, .about-version').forEach(el => {
            if (el.id === 'app-title-mini') el.innerText = currentProductName + " v" + version;
            else el.innerText = "Version " + version;
        });
    });

    listenEvent("onProductNameUpdate", (name) => updateUIProductNames(name));

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

    setupSliders();
    setupButtons();
    setupBender();
    setupKeyboard();
    setupMenus();
    setupOctaveButtons();

    if (window.ServiceMode && typeof window.ServiceMode.refreshParams === 'function') {
        window.ServiceMode.refreshParams().then(() => {
            updateThemeAndSkins();
            updatePainterTapes();
        });
    } else {
        updateThemeAndSkins();
        updatePainterTapes();
    }

    callNative("uiReady");

    setTimeout(() => {
        const splash = document.getElementById('splash-screen');
        if (splash) {
            splash.style.opacity = '0';
            setTimeout(() => splash.style.display = 'none', 1000);
        }
    }, 5000);
}

document.addEventListener('DOMContentLoaded', () => {
    const splashVersion = document.getElementById('splash-version-info');
    let retries = 0;
    const checkBridge = setInterval(() => {
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
