/**
 * ABD JUNiO 601 - UI Sliders & Buttons
 * All slider/knob/button/keyboard interaction and UI synchronization.
 */

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
            syncUI(paramID, val);
            callNative("setParameter", paramID, val);
            let displayVal = val.toFixed(2);
            if (paramID === 'hpfFreq') displayVal = Math.round(val * 3);
            updateLCD(paramID.toUpperCase() + ": " + displayVal, true);
        };

        container.addEventListener('pointerdown', (e) => {
            e.preventDefault();
            container.setPointerCapture(e.pointerId);
            callNative("beginGesture", paramID);
            move(e);
            const onMove = (ev) => move(ev);
            const onUp = () => {
                container.removeEventListener('pointermove', onMove);
                container.removeEventListener('pointerup', onUp);
                callNative("endGesture", paramID);
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
            callNative("beginGesture", paramID);
            startVal = paramValuesCache[paramID] !== undefined ? paramValuesCache[paramID] : 0.5;

            const onMove = (ev) => {
                const deltaY = startY - ev.clientY;
                const isDiscrete = (paramID === 'arpMode' || paramID === 'arpRange');
                const divisor = (paramID === 'tune') ? 3000 : (isDiscrete ? 150 : 500);
                let val = startVal + (deltaY / divisor);
                val = Math.max(0, Math.min(1, val));
                if (isDiscrete) val = Math.round(val * 2) / 2;
                syncUI(paramID, val);
                callNative("setParameter", paramID, val);
                let displayVal = val.toFixed(2);
                if (paramID === 'tune') displayVal = ((val * 100) - 50).toFixed(1);
                updateLCD(paramID.toUpperCase() + ": " + displayVal, true);
            };

            const onUp = () => {
                ring.removeEventListener('pointermove', onMove);
                ring.removeEventListener('pointerup', onUp);
                callNative("endGesture", paramID);
            };
            ring.addEventListener('pointermove', onMove);
            ring.addEventListener('pointerup', onUp);
        });
    });

    document.querySelectorAll('.knob-tick-lbl').forEach(lbl => {
        lbl.style.cursor = 'pointer';
        lbl.addEventListener('pointerdown', (e) => {
            e.preventDefault();
            e.stopPropagation();
            const wrapper = lbl.closest('.knob-tick-wrapper[data-param]');
            if (!wrapper) return;
            const paramID = wrapper.getAttribute('data-param');
            let val = 0.0;
            if (lbl.classList.contains('pos-center')) val = 0.5;
            else if (lbl.classList.contains('pos-right')) val = 1.0;
            syncUI(paramID, val);
            callNative("setParameter", paramID, val);
            const displayNames = {
                'arpMode': { 0.0: 'UP', 0.5: 'DN', 1.0: 'U/D' },
                'arpRange': { 0.0: '1 OCT', 0.5: '2 OCT', 1.0: '3 OCT' }
            };
            const labelStr = displayNames[paramID] ? displayNames[paramID][val] : val.toFixed(2);
            updateLCD(paramID.toUpperCase() + ": " + labelStr, true);
        });
    });
}

function updateOctaveLEDs() {
    const upLed = document.getElementById('led-octave-up');
    const downLed = document.getElementById('led-octave-down');

    [upLed, downLed].forEach(led => {
        if (!led) return;
        led.classList.remove('active', 'blink', 'blink-fast');
    });

    if (octaveShift > 0) {
        if (upLed) upLed.classList.add(octaveShift >= 2 ? 'blink' : 'active');
    } else if (octaveShift < 0) {
        if (downLed) downLed.classList.add(octaveShift <= -2 ? 'blink' : 'active');
    }
}

function setupOctaveButtons() {
    const upBtn = document.querySelector('.perf-octave-zone .oct-btn.up');
    const downBtn = document.querySelector('.perf-octave-zone .oct-btn.down');
    if (upBtn) {
        upBtn.addEventListener('pointerdown', (e) => {
            e.preventDefault(); e.stopPropagation();
            octaveShift = Math.min(2, octaveShift + 1);
            updateOctaveLEDs();
            updateLCD("OCTAVE: " + octaveShift, true);
        });
    }
    if (downBtn) {
        downBtn.addEventListener('pointerdown', (e) => {
            e.preventDefault(); e.stopPropagation();
            octaveShift = Math.max(-2, octaveShift - 1);
            updateOctaveLEDs();
            updateLCD("OCTAVE: " + octaveShift, true);
        });
    }
}

function setupButtons() {
    document.querySelectorAll('.sq[data-param], .tiny-btn[data-param], .juno-btn[data-param], .poly-btn[data-param]').forEach(btn => {
        const paramID = btn.getAttribute('data-param');
        btn.addEventListener('pointerdown', (e) => {
            e.preventDefault();
            btn.classList.add('pushed');
            if (paramID === 'memoryProtect') {
                const nextVal = memoryProtectActive ? 0 : 1;
                callNative("setParameter", paramID, nextVal);
                return;
            }
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
                if (actionID === 'handleSave' || actionID === 'handleWriteArm') {
                    if (memoryProtectActive) { updateLCD("MEMORY PROTECTED", true); return; }
                    if (btn.id === 'write-btn') {
                        isWriteArmed = !isWriteArmed;
                        if (isWriteArmed) {
                            targetBank = currentBankGlobal;
                            targetPatch = currentPatchGlobal;
                            updateLCD("WRITE TO? " + targetBank + "-" + targetPatch, false);
                        } else { updateLCD(lastPresetName, false); }
                        callNative("menuAction", "handleWriteArm");
                    } else if (btn.id === 'save-patch-btn') {
                        if (isWriteArmed) showSaveModal(targetBank, targetPatch);
                        else updateLCD("PRESS WRITE FIRST", true);
                    }
                    return;
                }
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
                const idx = parseInt(btn.getAttribute('data-bank')) + 1;
                if (isWriteArmed) {
                    targetPatch = idx;
                    updateLCD("WRITE TO? " + String.fromCharCode(65 + currentGroupGlobal) + targetBank + "-" + targetPatch, false);
                    return;
                }
                const isPatchButton = btn.classList.contains('patch-btn');
                const isBankButton = btn.classList.contains('bank-btn');
                if (isPatchButton) {
                    callNative("loadPreset", (currentGroupGlobal * 64) + ((currentBankGlobal - 1) * 8) + (idx - 1));
                } else if (isBankButton) {
                    callNative("loadPreset", (currentGroupGlobal * 64) + ((idx - 1) * 8) + (currentPatchGlobal - 1));
                } else {
                    callNative("loadPreset", (currentGroupGlobal * 64) + (currentBankGlobal - 1) * 8 + (idx - 1));
                }
            } else if (btn.id === 'random-btn') {
                callNative("menuAction", "handleRandomize");
            } else if (btn.id === 'panic-btn') {
                callNative("menuAction", "panic");
            } else {
                const actionMap = { 'bank-dec': 'handleBankDec', 'bank-inc': 'handleBankInc', 'patch-dec': 'handlePatchDec', 'patch-inc': 'handlePatchInc' };
                if (actionMap[btn.id]) {
                    if (isWriteArmed) {
                        if (btn.id === 'bank-inc') targetBank = (targetBank % 16) + 1;
                        if (btn.id === 'bank-dec') targetBank = targetBank === 1 ? 16 : targetBank - 1;
                        if (btn.id === 'patch-inc') targetPatch = (targetPatch % 8) + 1;
                        if (btn.id === 'patch-dec') targetPatch = targetPatch === 1 ? 8 : targetPatch - 1;
                        updateLCD("WRITE TO? " + targetBank + "-" + targetPatch, false);
                    } else { callNative("menuAction", actionMap[btn.id]); }
                }
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

    const offBtn = document.getElementById('chorus-off-btn');
    if (offBtn) {
        offBtn.addEventListener('pointerdown', (e) => {
            e.preventDefault();
            offBtn.classList.add('pushed');
            syncUI("chorus1", 0.0);
            syncUI("chorus2", 0.0);
            callNative("setParameter", "chorus1", 0.0);
            callNative("setParameter", "chorus2", 0.0);
            updateLCD("CHORUS: OFF", true);
        });
        offBtn.addEventListener('pointerup', () => offBtn.classList.remove('pushed'));
        offBtn.addEventListener('pointerleave', () => offBtn.classList.remove('pushed'));
    }
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
    for (let i = 0; i < 48; i++) {
        const note = 36 + i;
        const pc = note % 12;
        if (![1, 3, 6, 8, 10].includes(pc)) whiteNotes.push({ note, i });
    }
    const totalWhite = whiteNotes.length;
    whiteNotes.forEach(({ note }, idx) => {
        const k = document.createElement('div');
        k.className = 'key white';
        k.setAttribute('data-note', note);
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
            b.setAttribute('data-note', bNote);
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

function syncUI(id, val) {
    paramValuesCache[id] = val;
    if (id === 'arpMode' || id === 'arpRange') {
        const wrapper = document.querySelector(`.knob-tick-wrapper[data-param="${id}"]`);
        if (wrapper) {
            const labels = wrapper.querySelectorAll('.knob-tick-lbl');
            const activeIdx = Math.round(val * 2);
            labels.forEach((lbl, idx) => lbl.classList.toggle('active', idx === activeIdx));
        }
    }
    document.querySelectorAll('[data-param="' + id + '"]').forEach(pod => {
        const handle = pod.querySelector('.handle, .b-handle');
        if (handle) {
            const track = pod.querySelector('.track, .b-track') || pod;
            const containerH = track.getBoundingClientRect().height;
            const handleH = handle.getBoundingClientRect().height;
            const availableSpace = Math.max(0, containerH - handleH);
            if (availableSpace >= 0) handle.style.top = ((1.0 - val) * availableSpace) + 'px';
        }
        const knob = pod.querySelector('.knob');
        if (knob) knob.style.transform = 'translateX(-50%) rotate(' + ((val * 270) - 135) + 'deg)';
        const peg = pod.querySelector('.sw-peg');
        if (peg) {
            peg.style.bottom = (val > 0.5) ? ((id === 'vcaMode') ? '38px' : '0px') : ((id === 'vcaMode') ? '0px' : '38px');
            pod.setAttribute('data-state', val > 0.5 ? "1" : "0");
        }
        const btn = pod.tagName === 'BUTTON' ? pod : pod.querySelector('button');
        if (btn) {
            const isActive = val > 0.5;
            btn.setAttribute('data-active', isActive ? 'true' : 'false');
            if (btn.classList.contains('sq') || btn.classList.contains('juno-btn')) btn.classList.toggle('active-mode', isActive);
            if (id === 'power') btn.innerText = isActive ? "ON" : "OFF";
        }
        if (pod.tagName === 'SELECT') {
            let denormalized = val;
            if (id === 'midiChannel') denormalized = Math.round(val * 15 + 1);
            else if (id === 'benderRange') denormalized = Math.round(val * 11 + 1);
            else if (id === 'numVoices') denormalized = Math.round(val * 15 + 1);
            else if (id === 'sustainInverted') denormalized = val > 0.5 ? 1 : 0;
            else if (id.startsWith('model') || id === 'arpMode' || id === 'arpRange') denormalized = Math.round(val * 2) / 2;
            else if (id === 'arpDivision') denormalized = Math.round(val * 8) / 8;
            else if (id === 'arpEnabled' || id === 'arpSync') denormalized = val > 0.5 ? 1 : 0;
            pod.value = denormalized;
        }
        if (pod.tagName === 'INPUT' && pod.type === 'range') {
            pod.value = val;
            const lbl = document.getElementById('val-' + id);
            if (lbl) {
                if (id === 'velocitySens' || id === 'lcdBrightness' || id === 'unisonWidth' || id === 'chorusMix' || id === 'aftertouchToVCF') lbl.innerText = Math.round(val * 100) + '%';
                else if (id === 'chorusHiss') lbl.innerText = Math.round(val * 50) + '%';
                else if (id === 'unisonDetune') lbl.innerText = Math.round(val * 50) + 'c';
                else lbl.innerText = val.toFixed(2);
            }
        }
    });
    if (id === 'arpRate') {
        const rateValEl = document.getElementById('val-arpRate-perf');
        if (rateValEl) rateValEl.innerText = val.toFixed(2);
    }
    if (id === 'chorus1' || id === 'chorus2') {
        const c1 = id === 'chorus1' ? val : (paramValuesCache['chorus1'] || 0.0);
        const c2 = id === 'chorus2' ? val : (paramValuesCache['chorus2'] || 0.0);
        const isOff = (c1 < 0.5 && c2 < 0.5);
        const offBtn = document.getElementById('chorus-off-btn');
        if (offBtn) { offBtn.setAttribute('data-active', isOff ? 'true' : 'false'); offBtn.classList.toggle('active-mode', isOff); }
    }
    if (id === 'memoryProtect') {
        memoryProtectActive = val > 0.5;
        const pBtn = document.getElementById('protect-btn');
        if (pBtn) { pBtn.classList.toggle('active-mode', memoryProtectActive); pBtn.setAttribute('data-active', memoryProtectActive ? 'true' : 'false'); }
    }
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
    if (id === 'portamentoOn') {
        const led = document.getElementById('led-tab-port');
        if (led) led.classList.toggle('active', val > 0.5);
    }
    if (id === 'arpEnabled') {
        const led = document.getElementById('led-tab-arp');
        if (led) led.classList.toggle('active', val > 0.5);
    }
    if (id === 'polyMode') {
        const led1 = document.getElementById('led-tab-poly1');
        const led2 = document.getElementById('led-tab-poly2');
        if (led1) led1.classList.toggle('active', Math.abs(val - 0.5) < 0.2);
        if (led2) led2.classList.toggle('active', Math.abs(val - 1.0) < 0.2);
    }
    if (id === 'calibrationProfile' || id === 'skinType') updateThemeAndSkins();
    if (id.startsWith('model') || id === 'polyMode') updatePainterTapes();
}

function updateSevenSegment() {
    const b = document.getElementById('bank-digit');
    const p = document.getElementById('patch-digit');
    if (b) b.innerText = currentBankGlobal;
    if (p) p.innerText = currentPatchGlobal;
}
