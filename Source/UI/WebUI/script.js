/**
 * ABD JUNiO 601 - WebUI Bridge (JUCE 8 Native Integration)
 */

let lastPresetName = "INITIAL PATCH";
let lcdTimer = null;
let promiseId = 0;
let octaveShift = 0;
let lastSysExHex = "";

// Define juce object for inline onclick handlers from index.html
window.juce = {
    menuAction: (action) => callNative("menuAction", action),
    setParameter: (id, val) => callNative("setParameter", id, val),
    loadPreset: (idx) => callNative("loadPreset", idx),
    undo: () => callNative("menuAction", "undo"),
    redo: () => callNative("menuAction", "redo"),
    toggleMidiOut: () => callNative("menuAction", "toggleMidiOut")
};

function callNative(name, ...args) {
    if (typeof window.__JUCE__ === "undefined" || typeof window.__JUCE__.backend === "undefined") {
        return Promise.reject("No bridge");
    }
    const id = promiseId++;
    const p = new Promise((resolve) => {
        const handler = window.__JUCE__.backend.addEventListener("__juce__complete", (data) => {
            if (data.promiseId === id) {
                window.__JUCE__.backend.removeEventListener(handler);
                resolve(data.result);
            }
        });
    });
    window.__JUCE__.backend.emitEvent("__juce__invoke", {
        name: name,
        params: args,
        resultId: id
    });
    return p;
}

function listenEvent(eventName, callback) {
    if (typeof window.__JUCE__ !== "undefined" && window.__JUCE__.backend) {
        window.__JUCE__.backend.addEventListener(eventName, callback);
    }
}

// =============================
// DOM READY
// =============================
document.addEventListener('DOMContentLoaded', () => {
    if (typeof window.__JUCE__ === "undefined") return;

    listenEvent("onParameterChanged", (data) => syncUI(data.id, data.value));
    listenEvent("onLCDUpdate", (text) => updateLCD(text, false));

    listenEvent("onBankPatchUpdate", (data) => {
        const b = document.getElementById('bank-digit');
        const p = document.getElementById('patch-digit');
        if (b) b.innerText = data.bank;
        if (p) p.innerText = data.patch;
        currentBankGlobal = parseInt(data.bank);
        currentPatchGlobal = parseInt(data.patch);
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
        const currentBytes = hex.split(' ');
        const lastBytes = lastSysExHex.split(' ');
        let html = '';
        currentBytes.forEach((byte, i) => {
            const isChanged = lastBytes[i] && lastBytes[i] !== byte;
            html += `<span class="hex-byte ${isChanged ? 'changed' : 'normal'}">${byte}</span> `;
        });
        log.innerHTML = html;
        log.scrollTop = log.scrollHeight;

        // Remove changed class after 2 seconds
        setTimeout(() => {
            log.querySelectorAll('.changed').forEach(el => el.classList.remove('changed'));
        }, 2000);

        lastSysExHex = hex;
    });

    setupSliders();
    setupButtons();
    setupBender();
    setupKeyboard();
    setupMenus();
    setupOctaveButtons();

    callNative("uiReady");
});

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
    // Vertical Sliders (Absolute position)
    document.querySelectorAll('.v-slider, .v-slider-mini, .b-track').forEach(container => {
        const pod = container.closest('[data-param]');
        if (!pod) return;
        const paramID = pod.getAttribute('data-param');

        const move = (e) => {
            const rect = container.getBoundingClientRect();
            let val = 1.0 - (e.clientY - rect.top) / rect.height;
            val = Math.max(0, Math.min(1, val));

            if (paramID === 'hpfFreq') {
                val = Math.round(val * 3) / 3;
            }
            if (paramID === 'dcoRange') {
                val = Math.round(val * 2) / 2;
            }

            callNative("setParameter", paramID, val);

            let displayVal = val.toFixed(2);
            if (paramID === 'hpfFreq') {
                displayVal = Math.round(val * 3);
            }
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

    // Knobs (Relative Drag for fine-tuning)
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

            // Get current value from UI if possible (approximate)
            const knob = ring.querySelector('.knob');
            let currentRotation = 0;
            if (knob && knob.style.transform) {
                const match = knob.style.transform.match(/rotate\(([^deg]+)deg\)/);
                if (match) currentRotation = parseFloat(match[1]);
            }
            startVal = (currentRotation + 135) / 270;

            const onMove = (ev) => {
                const deltaY = startY - ev.clientY;
                // Calibrated sensitivity
                const divisor = (paramID === 'tune') ? 3000 : 500;
                let val = startVal + (deltaY / divisor);
                val = Math.max(0, Math.min(1, val));
                callNative("setParameter", paramID, val);

                let displayVal = val.toFixed(2);
                if (paramID === 'tune') {
                    // Normalize 0..1 to -50..50
                    displayVal = ((val * 100) - 50).toFixed(1);
                }
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
    const container = document.getElementById('keyboard-container');
    if (!container) return;
    const octDown = document.createElement('button');
    octDown.className = 'oct-btn down';
    octDown.innerText = 'OCT -';
    const octUp = document.createElement('button');
    octUp.className = 'oct-btn up';
    octUp.innerText = 'OCT +';

    octDown.addEventListener('pointerdown', () => {
        octaveShift = Math.max(-2, octaveShift - 1);
        updateLCD("OCTAVE: " + octaveShift, true);
    });
    octUp.addEventListener('pointerdown', () => {
        octaveShift = Math.min(2, octaveShift + 1);
        updateLCD("OCTAVE: " + octaveShift, true);
    });

    container.appendChild(octDown);
    container.appendChild(octUp);
}

// =============================
// BUTTONS (MOMENTARY & TOGGLE)
// =============================
function setupButtons() {
    // 1. Toggle Buttons & Multi-state Params
    document.querySelectorAll('.sq[data-param], .tiny-btn[data-param], .juno-btn[data-param], .poly-btn[data-param]').forEach(btn => {
        const paramID = btn.getAttribute('data-param');

        btn.addEventListener('pointerdown', (e) => {
            e.preventDefault();
            btn.classList.add('pushed');

            // Toggle Logic
            if (btn.classList.contains('poly-btn')) {
                // Legacy cyclic button - keep if index.html still has old one, 
                // but new index.html uses .poly-mode-btn
            } else if (btn.classList.contains('range-btn')) {
                const val = parseFloat(btn.getAttribute('data-val'));
                callNative("setParameter", "dcoRange", val);
                // Ensure other LEDs in stack turn off is handled by onParameterChanged
            } else {
                const curr = btn.getAttribute('data-active') === 'true' ? 1 : 0;
                callNative("setParameter", paramID, curr > 0.5 ? 0 : 1);
            }
        });

        btn.addEventListener('pointerup', () => btn.classList.remove('pushed'));
        btn.addEventListener('pointerleave', () => btn.classList.remove('pushed'));
    });

    // 2. Momentary Buttons (BK+/-, PT+/-, Num Grid, Random, Panic, Actions)
    document.querySelectorAll('.nav-arrow, .num-btn, #random-btn, #panic-btn, [data-action]').forEach(btn => {
        btn.addEventListener('pointerdown', (e) => {
            e.preventDefault();
            btn.classList.add('pushed');

            const actionID = btn.getAttribute('data-action');
            if (actionID) {
                if (actionID === 'handleManual' || actionID === 'handleTest') {
                    // Simulate toggle
                    const isActive = btn.getAttribute('data-active') === 'true';
                    document.querySelectorAll('[data-action="handleManual"], [data-action="handleTest"]').forEach(b => {
                        b.setAttribute('data-active', 'false');
                        b.classList.remove('pushed');
                    });
                    if (!isActive) {
                        btn.setAttribute('data-active', 'true');
                        btn.classList.add('pushed'); // keep it depressed
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
                // Nav arrows
                const actionMap = { 'bank-dec': 'handleBankDec', 'bank-inc': 'handleBankInc', 'patch-dec': 'handlePatchDec', 'patch-inc': 'handlePatchInc' };
                if (actionMap[btn.id]) callNative("menuAction", actionMap[btn.id]);
            }
        });

        btn.addEventListener('pointerup', () => {
            if (btn.getAttribute('data-action') !== 'handleManual' && btn.getAttribute('data-action') !== 'handleTest') {
                btn.classList.remove('pushed');
            }
        });
        btn.addEventListener('pointerleave', () => {
            if (btn.getAttribute('data-action') !== 'handleManual' && btn.getAttribute('data-action') !== 'handleTest') {
                btn.classList.remove('pushed');
            }
        });
    });

    // 3. Poly Mode Buttons (I, II) -> 0.0=Mono, 0.5=Poly1, 1.0=Poly2
    document.querySelectorAll('.poly-mode-btn').forEach(btn => {
        btn.addEventListener('pointerdown', (e) => {
            e.preventDefault();
            const val = parseFloat(btn.getAttribute('data-poly-val'));
            const isActive = btn.getAttribute('data-active') === 'true';

            // If clicking active button -> turn OFF (MONO)
            // If clicking inactive -> switch to it
            callNative("setParameter", "polyMode", isActive ? 0.0 : val);
        });
    });

    // 4. Switches
    document.querySelectorAll('.sw-unit[data-param], .sw-col[data-param]').forEach(sw => {
        const paramID = sw.getAttribute('data-param');
        sw.addEventListener('pointerdown', (e) => {
            e.preventDefault();
            const current = parseFloat(sw.getAttribute('data-state') || "0");
            const next = current > 0.5 ? 0.0 : 1.0;
            callNative("setParameter", paramID, next);
        });
    });
}

// =============================
// BENDER
// =============================
function setupBender() {
    const stick = document.getElementById('stick-handle');
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
            callNative("setParameter", "bender", (x * 2) - 1);
        };
        move(e);
        const onMove = (ev) => move(ev);
        const onUp = () => {
            housing.removeEventListener('pointermove', onMove);
            housing.removeEventListener('pointerup', onUp);
            stick.style.left = '50%';
            callNative("setParameter", "bender", 0);
        };
        housing.addEventListener('pointermove', onMove);
        housing.addEventListener('pointerup', onUp);
    });
}

// =============================
// KEYBOARD
// =============================
function setupKeyboard() {
    const bed = document.getElementById('ivory-keys-bed');
    if (!bed) return;
    bed.innerHTML = '';
    const whiteNotes = [];
    for (let i = 0; i < 61; i++) {
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

// =============================
// MENUS
// =============================
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
        // Slider handles
        const handle = pod.querySelector('.handle, .b-handle');
        if (handle) {
            // Fix: Clamp handle positioning to internal track bounds
            const track = pod.querySelector('.track, .b-track') || pod;
            const containerH = track.getBoundingClientRect().height;
            const handleH = handle.getBoundingClientRect().height;
            const availableSpace = containerH - handleH;
            const top = (1.0 - val) * availableSpace;
            handle.style.top = Math.max(0, Math.min(availableSpace, top)) + 'px';
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
            const active = val > 0.5;
            btn.setAttribute('data-active', active);
            // Persistent pushed state for toggle buttons
            if (btn.classList.contains('sq') || btn.classList.contains('juno-btn')) {
                btn.classList.toggle('pushed', active);
            }
            if (id === 'power') btn.innerText = active ? "ON" : "OFF";
        }
    });

    if (id === 'polyMode') {
        document.querySelectorAll('.poly-mode-btn').forEach(btn => {
            const btnVal = parseFloat(btn.getAttribute('data-poly-val'));
            // Match tolerance
            const match = Math.abs(val - btnVal) < 0.2;
            btn.setAttribute('data-active', match);
            btn.classList.toggle('pushed', match);
        });
    }

    const led = document.getElementById('led-' + id);
    if (led) led.classList.toggle('active', val > 0.5);

    if (id === 'dcoRange') {
        document.querySelectorAll('.range-btn').forEach(b => {
            const btnVal = parseFloat(b.getAttribute('data-val'));
            const match = Math.abs(val - btnVal) < 0.15;
            b.setAttribute('data-active', match);
            b.classList.toggle('pushed', match);
            const rLed = document.getElementById('led-dcoRange-' + Math.round(btnVal * 2));
            if (rLed) rLed.classList.toggle('active', match);
        });
    }
}
