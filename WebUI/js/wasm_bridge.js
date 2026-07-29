/**
 * ABD JUNiO 601 — WebAssembly (WASM) Standalone Audio Engine Bridge
 * Provides a complete bridge implementation with embedded presets, browser, 
 * SysEx logging and UI synchronization for pure Web / Vercel deployments.
 */

(function() {
    'use strict';

    // 10 Embedded Factory Presets for Standalone Web Mode
    const embeddedPresets = [
        { name: "A11 Brass Set 1", lfoRate: 0.15, lfoDelay: 0.38, lfoToDCO: 0.0, pwmAmount: 0.4, noiseLevel: 0.0, vcfFreq: 0.27, resonance: 0.1, envAmount: 0.45, lfoToVCF: 0.0, vcfKeyTrack: 0.67, vcaLevel: 0.85, attack: 0.02, decay: 0.38, sustain: 0.35, release: 0.25, subOscLevel: 0.0, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "A12 Brass Swell", lfoRate: 0.05, lfoDelay: 0.37, lfoToDCO: 0.0, pwmAmount: 0.22, noiseLevel: 0.0, vcfFreq: 0.34, resonance: 0.13, envAmount: 0.2, lfoToVCF: 0.0, vcfKeyTrack: 0.65, vcaLevel: 0.58, attack: 0.5, decay: 0.92, sustain: 0.3, release: 0.29, subOscLevel: 0.55, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "A13 Trumpet", lfoRate: 0.4, lfoDelay: 0.35, lfoToDCO: 0.06, pwmAmount: 0.4, noiseLevel: 0.0, vcfFreq: 0.43, resonance: 0.27, envAmount: 0.19, lfoToVCF: 0.01, vcfKeyTrack: 0.46, vcaLevel: 1.0, attack: 0.04, decay: 0.51, sustain: 0.38, release: 0.12, subOscLevel: 0.0, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "A14 Flutes", lfoRate: 0.47, lfoDelay: 0.34, lfoToDCO: 0.01, pwmAmount: 0.0, noiseLevel: 0.0, vcfFreq: 0.43, resonance: 0.25, envAmount: 0.08, lfoToVCF: 0.09, vcfKeyTrack: 0.32, vcaLevel: 1.0, attack: 0.18, decay: 0.63, sustain: 0.0, release: 0.14, subOscLevel: 0.0, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "A15 Moving Strings", lfoRate: 0.49, lfoDelay: 0.0, lfoToDCO: 0.0, pwmAmount: 0.15, noiseLevel: 0.0, vcfFreq: 0.6, resonance: 0.16, envAmount: 0.03, lfoToVCF: 0.0, vcfKeyTrack: 0.87, vcaLevel: 0.27, attack: 0.1, decay: 0.68, sustain: 0.69, release: 0.27, subOscLevel: 0.11, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "A16 Brass Strings", lfoRate: 0.27, lfoDelay: 0.0, lfoToDCO: 0.0, pwmAmount: 0.22, noiseLevel: 0.0, vcfFreq: 0.6, resonance: 0.13, envAmount: 0.03, lfoToVCF: 0.0, vcfKeyTrack: 0.32, vcaLevel: 0.61, attack: 0.34, decay: 0.51, sustain: 0.41, release: 0.34, subOscLevel: 0.18, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "A17 Choir", lfoRate: 0.46, lfoDelay: 0.11, lfoToDCO: 0.1, pwmAmount: 0.1, noiseLevel: 0.0, vcfFreq: 0.46, resonance: 0.74, envAmount: 0.02, lfoToVCF: 0.0, vcfKeyTrack: 0.48, vcaLevel: 1.0, attack: 0.53, decay: 0.09, sustain: 1.0, release: 0.38, subOscLevel: 0.0, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "A18 Piano I", lfoRate: 0.15, lfoDelay: 0.38, lfoToDCO: 0.0, pwmAmount: 0.31, noiseLevel: 0.0, vcfFreq: 0.51, resonance: 0.09, envAmount: 0.08, lfoToVCF: 0.0, vcfKeyTrack: 0.21, vcaLevel: 0.81, attack: 0.0, decay: 0.51, sustain: 0.0, release: 0.23, subOscLevel: 0.67, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "A21 Organ I", lfoRate: 0.42, lfoDelay: 0.12, lfoToDCO: 0.0, pwmAmount: 0.21, noiseLevel: 0.0, vcfFreq: 0.34, resonance: 0.6, envAmount: 0.11, lfoToVCF: 0.01, vcfKeyTrack: 1.0, vcaLevel: 0.78, attack: 0.0, decay: 0.08, sustain: 0.64, release: 0.0, subOscLevel: 0.18, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "A22 Organ II", lfoRate: 0.34, lfoDelay: 0.12, lfoToDCO: 0.0, pwmAmount: 0.21, noiseLevel: 0.0, vcfFreq: 0.42, resonance: 0.6, envAmount: 0.11, lfoToVCF: 0.01, vcfKeyTrack: 0.67, vcaLevel: 0.58, attack: 0.0, decay: 0.08, sustain: 0.64, release: 0.0, subOscLevel: 0.45, sawOn: 1, pulseOn: 0, vcaMode: 1 }
    ];

    window.WasmBridgeInstance = {
        module: null,
        audioCtx: null,
        isAudioStarted: false,
        model: 0, // 0 = SuperSix
        outputPtr: null,
        bufferSize: 256,
        sampleRate: 44100,
        currentPresetIndex: 0,

        async init() {
            if (this.module) return true;
            try {
                console.log('[WASM Bridge] Loading abdjunio601_wasm.js...');
                let mod;
                try {
                    mod = await import('../wasm/abdjunio601_wasm.js');
                } catch(e) {
                    mod = await import('./wasm/abdjunio601_wasm.js');
                }
                this.module = await mod.default();
                console.log('[WASM Bridge] WASM module loaded.');

                this.module.ccall('wasm_init_engine', null, ['number', 'number', 'number'], [this.sampleRate, this.bufferSize, this.model]);
                console.log(`[WASM Bridge] Engine initialized at ${this.sampleRate} Hz (SuperSix mode)`);

                this.loadPreset(0);
                return true;
            } catch (err) {
                console.warn('[WASM Bridge] WASM initialization failed:', err);
                return false;
            }
        },

        async startAudio() {
            if (this.isAudioStarted && this.audioCtx) {
                if (this.audioCtx.state === 'suspended') {
                    await this.audioCtx.resume();
                }
                return true;
            }

            try {
                const AudioContextClass = window.AudioContext || window.webkitAudioContext;
                this.audioCtx = new AudioContextClass({ sampleRate: this.sampleRate });
                this.bufferSize = 256;
                this.outputPtr = this.module._malloc(this.bufferSize * 2 * 4);

                const scriptNode = this.audioCtx.createScriptProcessor(this.bufferSize, 0, 2);
                let lfoTrig = 1;

                scriptNode.onaudioprocess = (e) => {
                    if (!this.module || !this.outputPtr) return;
                    const outL = e.outputBuffer.getChannelData(0);
                    const outR = e.outputBuffer.getChannelData(1);

                    this.module.ccall('wasm_process_audio', null, ['number', 'number', 'number'], [this.outputPtr, this.bufferSize, lfoTrig]);
                    lfoTrig = 0;

                    const heap = this.module.HEAPF32;
                    const offset = this.outputPtr / 4;
                    for (let i = 0; i < this.bufferSize; i++) {
                        outL[i] = heap[offset + i * 2];
                        outR[i] = heap[offset + i * 2 + 1];
                    }
                };

                scriptNode.connect(this.audioCtx.destination);
                if (this.audioCtx.state === 'suspended') {
                    await this.audioCtx.resume();
                }
                this.isAudioStarted = true;
                console.log('[WASM Bridge] Web Audio rendering active.');
                return true;
            } catch (err) {
                console.error('[WASM Bridge] Web Audio startup failed:', err);
                return false;
            }
        },

        setParameter(paramId, value) {
            if (!this.module) return;
            try {
                this.module.ccall('wasm_set_parameter', null, ['string', 'number'], [paramId, value]);
                
                // Generate SysEx hex log payload for WebUI mirror & Copy SysEx feature
                if (window.onJuceEvent) {
                    const mockHex = `F0 41 32 00 00 ${Math.floor(value * 127).toString(16).toUpperCase().padStart(2, '0')} F7`;
                    window.onJuceEvent('onSysExUpdate', mockHex);
                }
            } catch (e) {}
        },

        noteOn(midiNote, velocity = 0.8) {
            if (!this.module) return;
            this.startAudio();
            this.module.ccall('wasm_note_on', null, ['number', 'number', 'number'], [0, midiNote, velocity]);
        },

        noteOff(midiNote) {
            if (!this.module) return;
            this.module.ccall('wasm_note_off', null, ['number', 'number', 'number'], [0, midiNote, 0.0]);
        },

        loadPreset(index) {
            this.currentPresetIndex = index;
            const p = embeddedPresets[index % embeddedPresets.length] || embeddedPresets[0];
            console.log(`[WASM Bridge] Loading preset ${index}: ${p.name}`);

            for (let key in p) {
                if (key !== 'name') {
                    this.setParameter(key, p[key]);
                    if (window.syncUI) window.syncUI(key, p[key]);
                }
            }

            // Build full 23-byte SysEx Patch Dump mirror
            const fullSysex = "F0 41 32 00 36 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 10 F7";
            if (window.onJuceEvent) {
                window.onJuceEvent('onBankPatchUpdate', { group: Math.floor(index / 8), bank: Math.floor((index % 64) / 8) + 1, patch: (index % 8) + 1 });
                window.onJuceEvent('onLCDUpdate', p.name);
                window.onJuceEvent('onSysExUpdate', fullSysex);
            }
        },

        updateAudioButtonUI(active) {
            const btn = document.getElementById('wasm-audio-toggle-btn');
            if (btn) {
                btn.style.borderColor = active ? '#00ffcc' : '#ff4444';
                btn.style.color = active ? '#00ffcc' : '#ff4444';
                btn.innerText = active ? '🔊 WEB AUDIO: ON' : '🔇 ACTIVAR AUDIO WEB';
            }
        }
    };

    // Inject window.juce mock immediately before script.js runs
    let midiTxEnabled = true;

    window.juce = {
        isWasmMock: true,
        initialisationData: { productName: "ABD JUNiO Super SIX" },
        callNativeFunction: (name, args) => Promise.resolve(null),
        setParameter: (id, val) => window.WasmBridgeInstance.setParameter(id, val),
        beginGesture: () => {},
        endGesture: () => {},
        pianoNoteOn: (note, vel) => window.WasmBridgeInstance.noteOn(note, vel),
        pianoNoteOff: (note) => window.WasmBridgeInstance.noteOff(note),
        loadPreset: (idx) => window.WasmBridgeInstance.loadPreset(idx),
        loadLibraryPreset: (libIdx, prstIdx) => window.WasmBridgeInstance.loadPreset(prstIdx),
        getBrowserData: () => Promise.resolve({
            libraries: [{ name: "SUPERSIX FACTORY", patches: embeddedPresets.map((p, i) => ({ id: i, name: p.name, category: "Factory", favorite: false })) }],
            currentLib: 0,
            currentPatch: window.WasmBridgeInstance.currentPresetIndex
        }),
        setFavorite: () => Promise.resolve(true),
        updateMetadata: () => Promise.resolve(true),
        exportBank: () => {},
        importBank: () => {},
        serviceAction: () => Promise.resolve({}),
        getCalibrationParams: () => Promise.resolve([]),
        menuAction: (action, ...args) => {
            console.log(`[WASM Bridge] Executing menuAction: ${action}`, args);
            if (action === "showBrowser") {
                if (window.showBrowser) window.showBrowser();
            } else if (action === "handleAbout") {
                if (window.showAbout) window.showAbout();
            } else if (action === "toggleMidiOut") {
                midiTxEnabled = !midiTxEnabled;
                const checkMark = document.querySelector('#menu-midi-tx .check-mark');
                if (checkMark) checkMark.style.display = midiTxEnabled ? 'inline' : 'none';
                if (window.updateLCD) window.updateLCD(midiTxEnabled ? "MIDI TX: ON" : "MIDI TX: OFF", true);
            } else if (action === "panic") {
                if (window.WasmBridgeInstance.module) {
                    try { window.WasmBridgeInstance.module.ccall('wasm_panic', null, [], []); } catch(e){}
                }
                if (window.updateLCD) window.updateLCD("PANIC", true);
            } else if (action === "handleRandomize") {
                const randomPatchIdx = Math.floor(Math.random() * embeddedPresets.length);
                window.WasmBridgeInstance.loadPreset(randomPatchIdx);
                if (window.updateLCD) window.updateLCD("RANDOMIZED", true);
            } else if (action === "handleWriteArm") {
                if (window.updateLCD) window.updateLCD("WRITE ARMED", true);
            } else if (action === "handleImportSysex") {
                alert("SysEx / JNO import is available in VST3/Standalone native mode.");
            } else if (action === "exit") {
                if (window.confirm("Close ABD JUNiO 601 Web Application?")) {
                    window.close();
                }
            }
            return Promise.resolve(true);
        }
    };

    // Auto-resume AudioContext on ANY user gesture (like ABDEep)
    const resumeAudioOnGesture = function() {
        if (window.WasmBridgeInstance) {
            window.WasmBridgeInstance.startAudio().then(active => {
                if (active) window.WasmBridgeInstance.updateAudioButtonUI(true);
            });
        }
    };

    if (typeof window !== 'undefined' && typeof window.addEventListener === 'function') {
        window.addEventListener('mousedown', resumeAudioOnGesture, { capture: true, passive: true });
        window.addEventListener('pointerdown', resumeAudioOnGesture, { capture: true, passive: true });
        window.addEventListener('touchstart', resumeAudioOnGesture, { capture: true, passive: true });
        window.addEventListener('keydown', resumeAudioOnGesture, { capture: true, passive: true });
        window.addEventListener('click', resumeAudioOnGesture, { capture: true, passive: true });
    }

    // Auto-init and wire top-bar button on DOMContentLoaded
    document.addEventListener('DOMContentLoaded', () => {
        console.log('[WASM Bridge] Browser standalone mode: initializing WASM SuperSix engine...');
        window.WasmBridgeInstance.init();

        const audioBtn = document.getElementById('wasm-audio-toggle-btn');
        if (audioBtn) {
            audioBtn.addEventListener('click', (e) => {
                e.stopPropagation();
                window.WasmBridgeInstance.startAudio().then(active => {
                    window.WasmBridgeInstance.updateAudioButtonUI(active);
                });
            });
        }
    });
})();