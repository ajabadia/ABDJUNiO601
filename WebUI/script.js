/**
 * ABD JUNiO 601 - WebUI Bridge (JUCE 8 Native Integration)
 */

let lastPresetName = "INITIAL PATCH";
let lcdTimer = null;
let promiseId = 0;
let octaveShift = 0;
let lastSysExHex = "";
let currentBankGlobal = 1;
let currentPatchGlobal = 1;

// [Audit] Persistent SysEx Mirror for stable UI display
let sysexMirror = new Array(23).fill(0);
sysexMirror[0] = 0xF0; sysexMirror[1] = 0x41; sysexMirror[2] = 0x30; // Default Header
sysexMirror[22] = 0xF7; // Default Tail

const eventListenersInternal = {};
window.onJuceEvent = (name, data) => {
    console.log(`[JS Bridge] Received Event: ${name}`, data);
    if (eventListenersInternal[name]) {
        eventListenersInternal[name].forEach(cb => cb(data));
    }
};

// Define juce object for inline onclick handlers from index.html
window.juce = {
    menuAction: (action) => callNative("menuAction", action),
    setParameter: (id, val) => callNative("setParameter", id, val),
    loadPreset: (idx) => callNative("loadPreset", idx),
    undo: () => callNative("menuAction", "undo"),
    redo: () => callNative("menuAction", "redo"),
    toggleMidiOut: () => callNative("menuAction", "toggleMidiOut"),
    copySysEx: () => copySysExToClipboard()
};

function getBackend() {
    return window.__JUCE__?.backend || window.juceBackend; 
}

function callNative(name, ...args) {
    console.log(`[JS Bridge] Calling ${name} with:`, args);
    const backend = getBackend();
    if (!backend) {
        console.error("No JUCE bridge available for", name);
        return Promise.reject("No bridge");
    }

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

    backend.emitEvent("__juce__invoke", {
        name: name,
        params: args,
        resultId: id
    });
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

// =============================
// DOM READY
// =============================
function initApp() {
    listenEvent("onParameterChanged", (data) => syncUI(data.id, data.value));
    listenEvent("onLCDUpdate", (text) => updateLCD(text, false));
    listenEvent("showModal", (data) => {
        if (data === "preferences") showSettings();
        else if (data === "about") showAbout();
    });
    listenEvent("onShowAbout", () => showAbout());
    listenEvent("onShowSettings", () => showSettings());
    listenEvent("onTuningUpdate", (name) => {
        const info = document.getElementById('tuning-info');
        if (info) info.innerText = name.toUpperCase();
    });

    listenEvent("onBankPatchUpdate", (data) => {
        if (!data) return;
        currentBankGlobal = data.bank || 1;
        currentPatchGlobal = data.patch || 1;
        updateSevenSegment();
    });

    listenEvent("onVisualUpdate", (data) => {
        const c1Led = document.getElementById('led-chorus1');
        const c2Led = document.getElementById('led-chorus2');
        if (c1Led && c1Led.classList.contains('active')) {
            c1Led.style.opacity = 0.5 + Math.sin(data.c1 * Math.PI * 2) * 0.5;
        } else if (c1Led) c1Led.style.opacity = 1;

        if (c2Led && c2Led.classList.contains('active')) {
            c2Led.style.opacity = 0.5 + Math.sin(data.c2 * Math.PI * 2) * 0.5;
        } else if (c2Led) c2Led.style.opacity = 1;
    });

    listenEvent("onSysExUpdate", (hex) => {
        const log = document.getElementById('sysex-log');
        if (!log) return;
        
        const bytes = hex.split(' ').map(h => parseInt(h, 16));
        const changedIndices = [];
        
        // [Audit Fix] Handle Patch Dump (23 bytes) vs Param Change (7 bytes)
        if (bytes.length === 23) {
            // Compare with mirror to find differences for highlighting
            bytes.forEach((byte, i) => {
                if (sysexMirror[i] !== byte) changedIndices.push(i);
            });
            sysexMirror = [...bytes];
        } else if (bytes.length === 7 && bytes[2] === 0x32) {
            // Param Change: ID at bytes[4], VAL at bytes[5]
            const paramId = bytes[4];
            const paramVal = bytes[5];
            const bodyIndex = 4 + paramId; 
            if (bodyIndex < sysexMirror.length - 1) { // -1 to avoid F7
                if (sysexMirror[bodyIndex] !== paramVal) {
                    sysexMirror[bodyIndex] = paramVal;
                    changedIndices.push(bodyIndex);
                }
            }
        }

        // Render mirror as hex grid
        let html = '';
        sysexMirror.forEach((byte, i) => {
            const h = byte.toString(16).toUpperCase().padStart(2, '0');
            const label = i === 20 ? 'SW1' : (i === 21 ? 'SW2' : '');
            const highlight = changedIndices.includes(i) ? 'changed' : 'normal';
            
            html += `<span class="hex-byte ${highlight}" title="Index ${i} ${label}">${h}</span> `;
            if (i === 11) html += '<br>';
        });
        
        log.innerHTML = html;
        lastSysExHex = hex;
        
        // Remove highlights after a while
        setTimeout(() => {
            log.querySelectorAll('.changed').forEach(el => el.classList.remove('changed'));
        }, 1000);
    });

    setupSliders();
    setupButtons();
    setupBender();
    setupKeyboard();
    setupMenus();
    setupOctaveButtons();

    callNative("uiReady");

    // Splash screen timeout
    setTimeout(() => {
        const splash = document.getElementById('splash-screen');
        if (splash) {
            splash.style.opacity = '0';
            setTimeout(() => splash.style.display = 'none', 1000);
        }
    }, 5000);
}

document.addEventListener('DOMContentLoaded', () => {
    // Retry bridge detection if not immediately available
    let retries = 0;
    const checkBridge = setInterval(() => {
        if (getBackend() || retries > 20) {
            clearInterval(checkBridge);
            initApp();
        }
        retries++;
    }, 100);
});

function copySysExToClipboard() {
    if (lastSysExHex) {
        navigator.clipboard.writeText(lastSysExHex).then(() => {
            updateLCD("SYSEX COPIED", true);
        }).catch(err => {
            console.error('Clipboard error:', err);
            updateLCD("COPY ERROR", true);
        });
    } else {
        updateLCD("NO SYSEX DATA", true);
    }
}

// =============================
// LCD
// =============================
function updateLCD(text, isTemporary) {
    const lcd = document.getElementById('lcd-text');
    if (!lcd) return;
    if (lcdTimer) clearTimeout(lcdTimer);

    if (isTemporary) {
        lcd.innerText = text;
        lcd.style.color = "#ff8888";
        lcdTimer = setTimeout(() => {
            lcd.innerText = lastPresetName;
            lcd.style.color = "#ff3c3c";
            lcdTimer = null;
        }, 1500);
    } else {
        lastPresetName = text;
        lcd.innerText = text;
        lcd.style.color = "#ff3c3c";
        if (lcdTimer) {
            clearTimeout(lcdTimer);
            lcdTimer = null;
        }
    }
}

// =============================
// SLIDERS & KNOBS (INTERACTION)
// =============================
function setupSliders() {
    document.querySelectorAll('.v-slider, .v-slider-mini, .b-track').forEach(container => {
        const pod = container.closest('[data-param]');
        if (!pod) return;
        const paramID = pod.getAttribute('data-param');

        const move = (e) => {
            const rect = container.getBoundingClientRect();
            let val = 1.0 - (e.clientY - rect.top) / rect.height;
            val = Math.max(0, Math.min(1, val));

            if (paramID === 'hpfFreq') val = Math.round(val * 3) / 3;
            if (paramID === 'dcoRange') val = Math.round(val * 2) / 2;

            // Immediate local feedback
            syncUI(paramID, val);
            callNative("setParameter", paramID, val);

            let displayVal = val.toFixed(2);
            if (paramID === 'hpfFreq') displayVal = Math.round(val * 3);
            updateLCD(paramID.toUpperCase() + ": " + displayVal, true);
        };

        container.addEventListener('pointerdown', (e) => {
            e.preventDefault();
            container.setPointerCapture(e.pointerId);
            move(e);
            const onMove = (ev) => move(ev);
            const onUp = () => {
                container.removeEventListener('pointermove', onMove);
                container.removeEventListener('pointerup', onUp);
            };
            container.addEventListener('pointermove', onMove);
            container.addEventListener('pointerup', onUp);
        });
    });

    document.querySelectorAll('.knob-ring').forEach(ring => {
        const pod = ring.closest('[data-param]');
        if (!pod) return;
        const paramID = pod.getAttribute('data-param');
        let startY = 0;
        let startVal = 0;

        ring.addEventListener('pointerdown', (e) => {
            e.preventDefault();
            ring.setPointerCapture(e.pointerId);
            startY = e.clientY;

            const knob = ring.querySelector('.knob');
            let currentRotation = 0;
            if (knob && knob.style.transform) {
                const match = knob.style.transform.match(/rotate\(([^deg]+)deg\)/);
                if (match) currentRotation = parseFloat(match[1]);
            }
            startVal = (currentRotation + 135) / 270;

            const onMove = (ev) => {
                const deltaY = startY - ev.clientY;
                const divisor = (paramID === 'tune') ? 3000 : 500;
                let val = startVal + (deltaY / divisor);
                val = Math.max(0, Math.min(1, val));
                
                syncUI(paramID, val);
                callNative("setParameter", paramID, val);

                let displayVal = val.toFixed(2);
                if (paramID === 'tune') displayVal = ((val * 100) - 50).toFixed(1);
                updateLCD(paramID.toUpperCase() + ": " + displayVal, true);
            };

            const onUp = () => {
                ring.removeEventListener('pointermove', onMove);
                ring.removeEventListener('pointerup', onUp);
            };
            ring.addEventListener('pointermove', onMove);
            ring.addEventListener('pointerup', onUp);
        });
    });
}

function setupOctaveButtons() {
    const upBtn = document.querySelector('.perf-octave-zone .oct-btn.up');
    const downBtn = document.querySelector('.perf-octave-zone .oct-btn.down');

    if (upBtn) {
        upBtn.addEventListener('pointerdown', (e) => {
            e.preventDefault();
            e.stopPropagation();
            octaveShift = Math.min(2, octaveShift + 1);
            updateLCD("OCTAVE: " + octaveShift, true);
        });
    }
    if (downBtn) {
        downBtn.addEventListener('pointerdown', (e) => {
            e.preventDefault();
            e.stopPropagation();
            octaveShift = Math.max(-2, octaveShift - 1);
            updateLCD("OCTAVE: " + octaveShift, true);
        });
    }
}

// =============================
// BUTTONS (MOMENTARY & TOGGLE)
// =============================
function setupButtons() {
    document.querySelectorAll('.sq[data-param], .tiny-btn[data-param], .juno-btn[data-param], .poly-btn[data-param]').forEach(btn => {
        const paramID = btn.getAttribute('data-param');

        btn.addEventListener('pointerdown', (e) => {
            e.preventDefault();
            btn.classList.add('pushed');

            if (btn.classList.contains('range-btn')) {
                const val = parseFloat(btn.getAttribute('data-val'));
                syncUI("dcoRange", val);
                callNative("setParameter", "dcoRange", val);
            } else {
                const isActive = btn.getAttribute('data-active') === 'true';
                const nextVal = isActive ? 0 : 1;
                syncUI(paramID, nextVal);
                callNative("setParameter", paramID, nextVal);
            }
        });

        btn.addEventListener('pointerup', () => btn.classList.remove('pushed'));
        btn.addEventListener('pointerleave', () => btn.classList.remove('pushed'));
    });

    document.querySelectorAll('.nav-arrow, .num-btn, #random-btn, #panic-btn, [data-action]').forEach(btn => {
        btn.addEventListener('pointerdown', (e) => {
            e.preventDefault();
            btn.classList.add('pushed');

            const actionID = btn.getAttribute('data-action');
            if (actionID) {
                if (actionID === 'handleManual' || actionID === 'handleTest') {
                    const isActive = btn.getAttribute('data-active') === 'true';
                    document.querySelectorAll('[data-action="handleManual"], [data-action="handleTest"]').forEach(b => {
                        b.setAttribute('data-active', 'false');
                        b.classList.remove('active-mode');
                    });
                    if (!isActive) {
                        btn.setAttribute('data-active', 'true');
                        btn.classList.add('active-mode');
                    }
                }
                callNative("menuAction", actionID);
                return;
            }

            if (btn.classList.contains('num-btn')) {
                const idx = parseInt(btn.getAttribute('data-bank'));
                const testBtn = document.getElementById('test-btn');

                if (testBtn && testBtn.getAttribute('data-active') === 'true') {
                    callNative("menuAction", "handleTestProgram", idx);
                } else {
                    const presetToLoad = (typeof currentBankGlobal !== 'undefined' ? currentBankGlobal - 1 : 0) * 8 + idx;
                    callNative("loadPreset", presetToLoad);
                }
            } else if (btn.id === 'random-btn') {
                callNative("menuAction", "handleRandomize");
            } else if (btn.id === 'panic-btn') {
                callNative("menuAction", "panic");
            } else {
                const actionMap = { 'bank-dec': 'handleBankDec', 'bank-inc': 'handleBankInc', 'patch-dec': 'handlePatchDec', 'patch-inc': 'handlePatchInc' };
                if (actionMap[btn.id]) callNative("menuAction", actionMap[btn.id]);
            }
        });

        btn.addEventListener('pointerup', () => btn.classList.remove('pushed'));
        btn.addEventListener('pointerleave', () => btn.classList.remove('pushed'));
    });

    document.querySelectorAll('.poly-mode-btn').forEach(btn => {
        btn.addEventListener('pointerdown', (e) => {
            e.preventDefault();
            const val = parseFloat(btn.getAttribute('data-poly-val'));
            const isActive = btn.getAttribute('data-active') === 'true';
            const nextVal = isActive ? 0.0 : val;
            syncUI("polyMode", nextVal);
            callNative("setParameter", "polyMode", nextVal);
        });
    });

    document.querySelectorAll('.sw-unit[data-param], .sw-col[data-param]').forEach(sw => {
        const paramID = sw.getAttribute('data-param');
        sw.addEventListener('pointerdown', (e) => {
            e.preventDefault();
            const current = parseFloat(sw.getAttribute('data-state') || "0");
            const next = current > 0.5 ? 0.0 : 1.0;
            syncUI(paramID, next);
            callNative("setParameter", paramID, next);
        });
    });
}

function setupBender() {
    const stick = document.getElementById('bender-stick');
    const housing = document.getElementById('stick-housing');
    if (!stick || !housing) return;

    housing.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        housing.setPointerCapture(e.pointerId);
        const move = (ev) => {
            const rect = housing.getBoundingClientRect();
            let x = (ev.clientX - rect.left) / rect.width;
            x = Math.max(0, Math.min(1, x));
            stick.style.left = (x * 100) + '%';
            callNative("setParameter", "bender", x);
        };
        move(e);
        const onMove = (ev) => move(ev);
        const onUp = () => {
            housing.removeEventListener('pointermove', onMove);
            housing.removeEventListener('pointerup', onUp);
            stick.style.left = '50%';
            callNative("setParameter", "bender", 0.5);
        };
        housing.addEventListener('pointermove', onMove);
        housing.addEventListener('pointerup', onUp);
    });
}

function setupKeyboard() {
    const bed = document.getElementById('ivory-keys-bed');
    if (!bed) return;
    bed.innerHTML = '';
    const whiteNotes = [];
    for (let i = 0; i < 49; i++) {
        const note = 36 + i;
        const pc = note % 12;
        if (![1, 3, 6, 8, 10].includes(pc)) whiteNotes.push({ note, i });
    }
    const totalWhite = whiteNotes.length;
    whiteNotes.forEach(({ note }, idx) => {
        const k = document.createElement('div');
        k.className = 'key white';
        k.style.width = (100 / totalWhite) + '%';

        k.addEventListener('pointerdown', (e) => {
            k.setPointerCapture(e.pointerId);
            k.classList.add('pushed');
            callNative("pianoNoteOn", note + (octaveShift * 12), 0.8);
        });
        const release = () => { k.classList.remove('pushed'); callNative("pianoNoteOff", note + (octaveShift * 12)); };
        k.addEventListener('pointerup', release);
        k.addEventListener('pointerleave', release);
        bed.appendChild(k);

        const pc = note % 12;
        if ([0, 2, 5, 7, 9].includes(pc)) {
            const bNote = note + 1;
            const b = document.createElement('div');
            b.className = 'key black';
            b.style.left = (((idx + 0.68) / totalWhite) * 100) + '%';
            b.style.width = (0.7 / totalWhite * 100) + '%';
            b.addEventListener('pointerdown', (e) => {
                b.setPointerCapture(e.pointerId);
                b.classList.add('pushed');
                callNative("pianoNoteOn", bNote + (octaveShift * 12), 0.8);
            });
            const bRel = () => { b.classList.remove('pushed'); callNative("pianoNoteOff", bNote + (octaveShift * 12)); };
            b.addEventListener('pointerup', bRel);
            b.addEventListener('pointerleave', bRel);
            bed.appendChild(b);
        }
    });
}

function setupMenus() {
    document.querySelectorAll('.menu-item').forEach(item => {
        item.addEventListener('pointerdown', (e) => {
            if (e.target.closest('.dropdown')) return;
            e.stopPropagation();
            const dd = item.querySelector('.dropdown');
            const wasOpen = dd.style.display === 'flex';
            document.querySelectorAll('.dropdown').forEach(d => d.style.display = 'none');
            if (!wasOpen) dd.style.display = 'flex';
        });
    });
    document.addEventListener('pointerdown', (e) => {
        if (!e.target.closest('.menu-item')) document.querySelectorAll('.dropdown').forEach(d => d.style.display = 'none');
    });
}

// =============================
// UI SYNC
// =============================
function syncUI(id, val) {
    document.querySelectorAll('[data-param="' + id + '"]').forEach(pod => {
        const handle = pod.querySelector('.handle, .b-handle');
        if (handle) {
            const track = pod.querySelector('.track, .b-track') || pod;
            const containerH = track.getBoundingClientRect().height;
            const handleH = handle.getBoundingClientRect().height;
            const availableSpace = containerH - handleH;
            if (availableSpace > 0) {
                const top = (1.0 - val) * availableSpace;
                handle.style.top = top + 'px';
            }
        }

        const knob = pod.querySelector('.knob');
        if (knob) knob.style.transform = 'translateX(-50%) rotate(' + ((val * 270) - 135) + 'deg)';

        const peg = pod.querySelector('.sw-peg');
        if (peg) {
            peg.style.bottom = (val > 0.5) ? '0px' : '38px';
            pod.setAttribute('data-state', val > 0.5 ? "1" : "0");
        }

        const btn = pod.tagName === 'BUTTON' ? pod : pod.querySelector('button');
        if (btn) {
            const isActive = val > 0.5;
            btn.setAttribute('data-active', isActive ? 'true' : 'false');
            if (btn.classList.contains('sq') || btn.classList.contains('juno-btn')) {
                btn.classList.toggle('active-mode', isActive);
            }
            if (id === 'power') btn.innerText = isActive ? "ON" : "OFF";
        }

        if (pod.tagName === 'SELECT') {
            let denormalized = val;
            if (id === 'midiChannel') denormalized = Math.round(val * 15 + 1);
            if (id === 'benderRange') denormalized = Math.round(val * 11 + 1);
            if (id === 'numVoices') denormalized = Math.round(val * 15 + 1);
            if (id === 'sustainInverted') denormalized = val > 0.5 ? 1 : 0;
            pod.value = denormalized;
        }
        if (pod.tagName === 'INPUT' && pod.type === 'range') {
            pod.value = val;
            const lbl = document.getElementById('val-' + id);
            if (lbl) {
                if (id === 'velocitySens' || id === 'lcdBrightness' || id === 'unisonWidth' || id === 'chorusMix' || id === 'aftertouchToVCF') {
                    lbl.innerText = Math.round(val * 100) + '%';
                } else if (id === 'chorusHiss') {
                    lbl.innerText = Math.round(val * 50) + '%';
                } else if (id === 'unisonDetune') {
                    lbl.innerText = Math.round(val * 50) + 'c';
                } else {
                    lbl.innerText = val.toFixed(2);
                }
            }
        }
    });

    if (id === 'polyMode') {
        document.querySelectorAll('.poly-mode-btn').forEach(btn => {
            const btnVal = parseFloat(btn.getAttribute('data-poly-val'));
            const match = Math.abs(val - btnVal) < 0.2;
            btn.setAttribute('data-active', match ? 'true' : 'false');
            btn.classList.toggle('active-mode', match);
        });
    }

    const led = document.getElementById('led-' + id);
    if (led) led.classList.toggle('active', val > 0.5);

    if (id === "midiOut") {
        const item = document.getElementById("menu-midi-tx");
        if (item) item.classList.toggle("checked", val > 0.5);
    }
    if (id === 'dcoRange') {
        document.querySelectorAll('.range-btn').forEach(b => {
            const btnVal = parseFloat(b.getAttribute('data-val'));
            const match = Math.abs(val - btnVal) < 0.15;
            b.setAttribute('data-active', match ? 'true' : 'false');
            const rLed = document.getElementById('led-dcoRange-' + Math.round(btnVal * 2));
            if (rLed) rLed.classList.toggle('active', match);
        });
    }
}

function updateSevenSegment() {
    const b = document.getElementById('bank-digit');
    const p = document.getElementById('patch-digit');
    if (b) b.innerText = currentBankGlobal;
    if (p) p.innerText = currentPatchGlobal;
}

function showSettings() {
    const el = document.getElementById('settings-overlay');
    if (el) el.style.display = 'flex';
}

function hideSettings() {
    const el = document.getElementById('settings-overlay');
    if (el) el.style.display = 'none';
}

function showAbout() {
    const el = document.getElementById('about-overlay');
    if (el) el.style.display = 'flex';
}

function hideAbout() {
    const el = document.getElementById('about-overlay');
    if (el) el.style.display = 'none';
}

function updatePref(el) {
    const id = el.getAttribute('data-param');
    let val = el.value;
    let normalized = val;
    if (id === 'midiChannel') normalized = (val - 1) / 15;
    if (id === 'benderRange') normalized = (val - 1) / 11;
    if (id === 'numVoices') normalized = (val - 1) / 15;
    if (id === 'sustainInverted') normalized = val; 
    if (id === 'chorusHiss') normalized = val / 2.0;
    if (id === 'midiFunction') normalized = val / 2.0;
    if (id === 'unisonWidth') normalized = val;
    if (id === 'unisonDetune') normalized = val;
    if (id === 'chorusMix') normalized = val;
    if (id === 'aftertouchToVCF') normalized = val;
    
    callNative("setParameter", id, parseFloat(normalized));
    syncUI(id, parseFloat(normalized));
}

function handleTuningClick(type) {
    console.log("handleTuningClick:", type);
    const info = document.getElementById('tuning-info');
    if (type === 'load') {
        if (info) info.innerText = "OPENING FILE SELECTOR...";
        juce.menuAction('handleLoadTuning');
    } else {
        juce.menuAction('handleResetTuning');
        if (info) info.innerText = "RESETTING...";
    }
}
