/**
 * ABD JUNiO 601 — WasmBridge Architecture (ABDEep Pattern)
 * Coordinates Web Audio API fallback synth, AudioWorklet, and native window.juce mock.
 */

(function() {
    'use strict';

    class WasmBridge {
        constructor() {
            this.audioCtx = null;
            this.masterGain = null;
            this.isAudioStarted = false;
            this.activeVoices = {};
            this.currentModel = 0; // 0=SuperSix, 1=J106, 2=J60, 3=J6
        }

        initAudioContext() {
            return typeof window._wasmInitAudioContext === 'function'
                ? window._wasmInitAudioContext(this)
                : false;
        }

        noteOn(note, velocity = 0.8) {
            this.initAudioContext();
            if (!this.audioCtx || !this.masterGain) return;

            if (this.audioCtx.state === 'suspended') {
                this.audioCtx.resume().catch(() => {});
            }

            if (typeof window._wasmNoteOn === 'function') {
                window._wasmNoteOn(this, note, velocity);
            }
        }

        noteOff(note) {
            if (typeof window._wasmNoteOff === 'function') {
                window._wasmNoteOff(this, note);
            }
        }

        setParameter(paramId, value) {
            this.parameterCache = this.parameterCache || {};
            this.parameterCache[paramId] = value;
            console.log(`[WASM Bridge] setParameter(${paramId}, ${value})`);

            if (window.onJuceEvent) {
                const hexVal = Math.floor(value * 127).toString(16).toUpperCase().padStart(2, '0');
                const sysexHex = `F0 41 32 00 00 ${hexVal} F7`;
                window.onJuceEvent('onSysExUpdate', sysexHex);
            }
        }

        setModel(model) {
            this.currentModel = model;
            const modelNames = ["SUPER SIX HYBRID", "JUNO-106", "JUNO-60", "JUNO-6"];
            const specEl = document.getElementById('active-model-spec');
            if (specEl) specEl.textContent = modelNames[model] || 'SUPER SIX HYBRID';
            console.log(`[WASM Bridge] Model switched to: ${modelNames[model]}`);
            
            // Sync calibrationProfile parameter to 3 (Super Six Hybrid Mode)
            if (model === 0 || model === 3) {
                this.setParameter('calibrationProfile', 3);
                if (window.syncUI) window.syncUI('calibrationProfile', 3);
            }
        }
    }

    window.WasmBridgeInstance = new WasmBridge();

    // Import Factory Patches structures directly from C++ FactoryPresets.h
    const junoFactoryPatchesData = [
        // Bank A (Patches 0-63)
        { name: "A11 Brass Set 1", lfoRate: 0.15, lfoDelay: 0.38, lfoToDCO: 0.0, pwmAmount: 0.4, noiseLevel: 0.0, vcfFreq: 0.27, resonance: 0.1, envAmount: 0.45, lfoToVCF: 0.0, vcfKeyTrack: 0.67, vcaLevel: 0.85, attack: 0.02, decay: 0.38, sustain: 0.35, release: 0.25, subOscLevel: 0.0, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "A12 Brass Swell", lfoRate: 0.05, lfoDelay: 0.37, lfoToDCO: 0.0, pwmAmount: 0.22, noiseLevel: 0.0, vcfFreq: 0.34, resonance: 0.13, envAmount: 0.2, lfoToVCF: 0.0, vcfKeyTrack: 0.65, vcaLevel: 0.58, attack: 0.5, decay: 0.92, sustain: 0.3, release: 0.29, subOscLevel: 0.55, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "A13 Trumpet", lfoRate: 0.4, lfoDelay: 0.35, lfoToDCO: 0.06, pwmAmount: 0.4, noiseLevel: 0.0, vcfFreq: 0.43, resonance: 0.27, envAmount: 0.19, lfoToVCF: 0.01, vcfKeyTrack: 0.46, vcaLevel: 1.0, attack: 0.04, decay: 0.51, sustain: 0.38, release: 0.12, subOscLevel: 0.0, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "A14 Flutes", lfoRate: 0.47, lfoDelay: 0.34, lfoToDCO: 0.01, pwmAmount: 0.0, noiseLevel: 0.0, vcfFreq: 0.43, resonance: 0.25, envAmount: 0.08, lfoToVCF: 0.09, vcfKeyTrack: 0.32, vcaLevel: 1.0, attack: 0.18, decay: 0.63, sustain: 0.0, release: 0.14, subOscLevel: 0.0, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "A15 Moving Strings", lfoRate: 0.49, lfoDelay: 0.0, lfoToDCO: 0.0, pwmAmount: 0.15, noiseLevel: 0.0, vcfFreq: 0.6, resonance: 0.16, envAmount: 0.03, lfoToVCF: 0.0, vcfKeyTrack: 0.87, vcaLevel: 0.27, attack: 0.1, decay: 0.68, sustain: 0.69, release: 0.27, subOscLevel: 0.11, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "A16 Brass & Strings", lfoRate: 0.27, lfoDelay: 0.0, lfoToDCO: 0.0, pwmAmount: 0.22, noiseLevel: 0.0, vcfFreq: 0.6, resonance: 0.13, envAmount: 0.03, lfoToVCF: 0.0, vcfKeyTrack: 0.32, vcaLevel: 0.61, attack: 0.34, decay: 0.51, sustain: 0.41, release: 0.34, subOscLevel: 0.18, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "A17 Choir", lfoRate: 0.46, lfoDelay: 0.11, lfoToDCO: 0.1, pwmAmount: 0.1, noiseLevel: 0.0, vcfFreq: 0.46, resonance: 0.74, envAmount: 0.02, lfoToVCF: 0.0, vcfKeyTrack: 0.48, vcaLevel: 1.0, attack: 0.53, decay: 0.09, sustain: 1.0, release: 0.38, subOscLevel: 0.0, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "A18 Piano I", lfoRate: 0.15, lfoDelay: 0.38, lfoToDCO: 0.0, pwmAmount: 0.31, noiseLevel: 0.0, vcfFreq: 0.51, resonance: 0.09, envAmount: 0.08, lfoToVCF: 0.0, vcfKeyTrack: 0.21, vcaLevel: 0.81, attack: 0.0, decay: 0.51, sustain: 0.0, release: 0.23, subOscLevel: 0.67, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "A21 Organ I", lfoRate: 0.42, lfoDelay: 0.12, lfoToDCO: 0.0, pwmAmount: 0.21, noiseLevel: 0.0, vcfFreq: 0.34, resonance: 0.6, envAmount: 0.11, lfoToVCF: 0.01, vcfKeyTrack: 1.0, vcaLevel: 0.78, attack: 0.0, decay: 0.08, sustain: 0.64, release: 0.0, subOscLevel: 0.18, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "A22 Organ II", lfoRate: 0.34, lfoDelay: 0.12, lfoToDCO: 0.0, pwmAmount: 0.21, noiseLevel: 0.0, vcfFreq: 0.42, resonance: 0.6, envAmount: 0.11, lfoToVCF: 0.01, vcfKeyTrack: 0.67, vcaLevel: 0.58, attack: 0.0, decay: 0.08, sustain: 0.64, release: 0.0, subOscLevel: 0.45, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "A23 Combo Organ", lfoRate: 0.58, lfoDelay: 0.16, lfoToDCO: 0.07, pwmAmount: 0.28, noiseLevel: 0.0, vcfFreq: 0.49, resonance: 0.55, envAmount: 0.03, lfoToVCF: 0.0, vcfKeyTrack: 0.85, vcaLevel: 0.75, attack: 0.0, decay: 0.38, sustain: 0.34, release: 0.36, subOscLevel: 0.48, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "A24 Calliope", lfoRate: 0.64, lfoDelay: 0.31, lfoToDCO: 0.09, pwmAmount: 0.0, noiseLevel: 0.0, vcfFreq: 0.68, resonance: 0.21, envAmount: 0.13, lfoToVCF: 0.0, vcfKeyTrack: 0.44, vcaLevel: 0.7, attack: 0.05, decay: 1.0, sustain: 1.0, release: 0.05, subOscLevel: 0.38, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "A25 Donald Pluck", lfoRate: 0.59, lfoDelay: 0.16, lfoToDCO: 0.07, pwmAmount: 0.28, noiseLevel: 0.0, vcfFreq: 0.57, resonance: 0.82, envAmount: 0.12, lfoToVCF: 0.0, vcfKeyTrack: 0.61, vcaLevel: 0.64, attack: 0.02, decay: 0.04, sustain: 0.34, release: 0.08, subOscLevel: 1.0, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "A26 Celeste", lfoRate: 0.22, lfoDelay: 0.0, lfoToDCO: 0.0, pwmAmount: 0.0, noiseLevel: 0.0, vcfFreq: 0.26, resonance: 0.19, envAmount: 0.42, lfoToVCF: 0.0, vcfKeyTrack: 0.3, vcaLevel: 0.75, attack: 0.0, decay: 0.35, sustain: 0.0, release: 0.63, subOscLevel: 0.12, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "A27 Elect Piano I", lfoRate: 0.46, lfoDelay: 0.0, lfoToDCO: 0.0, pwmAmount: 0.0, noiseLevel: 0.0, vcfFreq: 0.13, resonance: 0.24, envAmount: 0.48, lfoToVCF: 0.05, vcfKeyTrack: 0.27, vcaLevel: 1.0, attack: 0.01, decay: 0.66, sustain: 0.34, release: 0.31, subOscLevel: 0.0, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "A28 Elect Piano II", lfoRate: 0.0, lfoDelay: 0.0, lfoToDCO: 0.0, pwmAmount: 0.55, noiseLevel: 0.0, vcfFreq: 0.39, resonance: 0.54, envAmount: 0.05, lfoToVCF: 0.0, vcfKeyTrack: 0.63, vcaLevel: 0.8, attack: 0.0, decay: 0.53, sustain: 0.0, release: 0.17, subOscLevel: 0.0, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        // Bank B (Patches 64-127)
        { name: "B11 Strings", lfoRate: 0.44, lfoDelay: 0.35, lfoToDCO: 0.0, pwmAmount: 0.43, noiseLevel: 0.0, vcfFreq: 0.66, resonance: 0.0, envAmount: 0.0, lfoToVCF: 0.0, vcfKeyTrack: 0.85, vcaLevel: 0.41, attack: 0.46, decay: 0.25, sustain: 0.67, release: 0.31, subOscLevel: 0.0, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "B12 Violin", lfoRate: 0.51, lfoDelay: 0.35, lfoToDCO: 0.15, pwmAmount: 0.0, noiseLevel: 0.0, vcfFreq: 0.6, resonance: 0.0, envAmount: 0.0, lfoToVCF: 0.0, vcfKeyTrack: 0.94, vcaLevel: 0.86, attack: 0.34, decay: 0.35, sustain: 0.45, release: 0.2, subOscLevel: 0.0, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "B13 Chorus Vibes", lfoRate: 0.56, lfoDelay: 0.35, lfoToDCO: 0.0, pwmAmount: 0.0, noiseLevel: 0.0, vcfFreq: 0.08, resonance: 0.0, envAmount: 0.46, lfoToVCF: 0.0, vcfKeyTrack: 0.18, vcaLevel: 0.69, attack: 0.0, decay: 0.7, sustain: 0.37, release: 0.58, subOscLevel: 0.0, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "B14 Organ 1", lfoRate: 0.35, lfoDelay: 0.33, lfoToDCO: 0.0, pwmAmount: 0.57, noiseLevel: 0.0, vcfFreq: 0.47, resonance: 0.56, envAmount: 0.11, lfoToVCF: 0.0, vcfKeyTrack: 1.0, vcaLevel: 0.45, attack: 0.0, decay: 0.0, sustain: 0.0, release: 0.0, subOscLevel: 0.76, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "B15 Harpsichord 1", lfoRate: 0.18, lfoDelay: 0.0, lfoToDCO: 0.0, pwmAmount: 0.42, noiseLevel: 0.0, vcfFreq: 1.0, resonance: 1.0, envAmount: 0.7, lfoToVCF: 0.0, vcfKeyTrack: 0.67, vcaLevel: 0.98, attack: 0.0, decay: 0.47, sustain: 0.0, release: 0.24, subOscLevel: 0.27, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "B16 Recorder", lfoRate: 0.61, lfoDelay: 0.0, lfoToDCO: 0.0, pwmAmount: 0.06, noiseLevel: 0.0, vcfFreq: 0.05, resonance: 0.0, envAmount: 0.36, lfoToVCF: 0.0, vcfKeyTrack: 1.0, vcaLevel: 1.0, attack: 0.04, decay: 0.16, sustain: 1.0, release: 0.23, subOscLevel: 0.0, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "B17 Perc Pluck", lfoRate: 0.48, lfoDelay: 0.3, lfoToDCO: 0.06, pwmAmount: 0.0, noiseLevel: 0.0, vcfFreq: 0.0, resonance: 0.0, envAmount: 0.7, lfoToVCF: 0.0, vcfKeyTrack: 0.57, vcaLevel: 0.51, attack: 0.0, decay: 0.12, sustain: 0.61, release: 0.91, subOscLevel: 0.0, sawOn: 1, pulseOn: 0, vcaMode: 1 },
        { name: "B18 Noise Sweep", lfoRate: 1.0, lfoDelay: 0.0, lfoToDCO: 0.0, pwmAmount: 0.0, noiseLevel: 1.0, vcfFreq: 0.01, resonance: 0.0, envAmount: 0.6, lfoToVCF: 0.0, vcfKeyTrack: 0.81, vcaLevel: 0.72, attack: 0.11, decay: 0.61, sustain: 0.85, release: 0.94, subOscLevel: 0.0, sawOn: 0, pulseOn: 0, vcaMode: 1 }
    ];

    // Helper mapping full C++ PresetManager 26-bank global space (A to Z)
    window.getNativeLibraries = function() {
        const libs = [];
        const letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        
        for (let i = 0; i < 26; ++i) {
            const letter = letters[i];
            let name = `${letter} - Empty Bank`;
            
            if (i === 0) name = "A - Factory A";
            else if (i === 1) name = "B - Factory B";
            else if (i === 2) name = "C - Internal RAM";
            else if (i === 3) name = "D - Juno 60 Factory";
            else if (i === 4) name = "E - David Churcher A";
            else if (i === 5) name = "F - David Churcher B";
            
            const patches = [];
            for (let p = 0; p < 64; ++p) {
                const globalIdx = (i * 64) + p;
                let patchName = "INIT PATCH";
                let cat = "User";
                
                if (i === 0 || i === 1) {
                    const preset = junoFactoryPatchesData[globalIdx];
                    if (preset) {
                        patchName = preset.name;
                        cat = "Factory";
                    }
                } else if (i === 3 && p < 8) {
                    const j60Names = ["Strings 1", "Strings 2", "Strings 3", "Organ 1", "Organ 2", "Organ 3", "Brass", "Phase Brass"];
                    patchName = j60Names[p] || "J60 Patch";
                    cat = "Juno 60 Factory";
                } else if (i === 4 || i === 5) {
                    patchName = `Churcher Patch ${p + 1}`;
                    cat = "David Churcher";
                }
                
                patches.push({
                    id: p,
                    name: patchName,
                    category: cat,
                    favorite: false
                });
            }
            
            libs.push({ name, patches });
        }
        return libs;
    };

    window.juce = {
        isWasmMock: true,
        initialisationData: { productName: "ABD JUNiO Super SIX" },
        callNativeFunction: (name, args) => Promise.resolve(null),
        setParameter: (id, val) => {
            if (window.WasmBridgeInstance) window.WasmBridgeInstance.setParameter(id, val);
        },
        beginGesture: () => {},
        endGesture: () => {},
        pianoNoteOn: (note, vel) => {
            if (window.WasmBridgeInstance) window.WasmBridgeInstance.noteOn(note, vel);
        },
        pianoNoteOff: (note) => {
            if (window.WasmBridgeInstance) window.WasmBridgeInstance.noteOff(note);
        },
        loadPreset: (idx) => {
            const p = junoFactoryPatchesData[idx % junoFactoryPatchesData.length] || junoFactoryPatchesData[0];
            if (window.WasmBridgeInstance) window.WasmBridgeInstance.currentPresetIndex = idx;
            for (let k in p) {
                if (k !== 'name') {
                    if (window.WasmBridgeInstance) window.WasmBridgeInstance.setParameter(k, p[k]);
                    if (window.syncUI) window.syncUI(k, p[k]);
                }
            }
            if (window.onJuceEvent) {
                const fullSysex = "F0 41 32 00 36 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 10 F7";
                window.onJuceEvent('onBankPatchUpdate', { group: Math.floor(idx / 64), bank: Math.floor((idx % 64) / 8) + 1, patch: (idx % 8) + 1 });
                window.onJuceEvent('onLCDUpdate', p.name);
                window.onJuceEvent('onSysExUpdate', fullSysex);
            }
        },
        loadLibraryPreset: (libIdx, prstIdx) => {
            const actualIdx = (libIdx * 64) + prstIdx;
            return window.juce.loadPreset(actualIdx);
        },
        getBrowserData: () => Promise.resolve({
            libraries: window.getNativeLibraries(),
            currentLib: (window.WasmBridgeInstance ? Math.floor(window.WasmBridgeInstance.currentPresetIndex / 64) : 0),
            currentPatch: (window.WasmBridgeInstance ? (window.WasmBridgeInstance.currentPresetIndex % 64) : 0)
        }),
        getCalibrationParams: () => {
            const defaults = [
                { id: "calibrationProfile", name: "Default Calibration Profile", category: "GENERAL", defaultValue: 3, currentValue: 3, min: 0, max: 3, step: 1 },
                { id: "skinType", name: "UI Skin Theme", category: "GENERAL", defaultValue: 0, currentValue: 0, min: 0, max: 8, step: 1 },
                { id: "midiChannel", name: "Global MIDI Channel", category: "GENERAL", defaultValue: 1, currentValue: 1, min: 0, max: 16, step: 1 },
                { id: "numVoices", name: "Maximum Polyphony", category: "GENERAL", defaultValue: 16, currentValue: 16, min: 1, max: 16, step: 1 },
                { id: "benderRange", name: "Bender Pitch Range", category: "GENERAL", defaultValue: 2, currentValue: 2, min: 1, max: 12, step: 1 },
                { id: "velocitySens", name: "Velocity Sensitivity", category: "GENERAL", defaultValue: 0.5, currentValue: 0.5, min: 0, max: 1, step: 0.01 },
                
                // LFO
                { id: "lfoRateScale", name: "LFO Rate Scale", category: "LFO", defaultValue: 1.0, currentValue: 1.0, min: 0.1, max: 2.0, step: 0.05 },
                { id: "lfoRateScale_J6", name: "LFO Rate Scale (Juno-6)", category: "LFO", defaultValue: 1.0, currentValue: 1.0, min: 0.1, max: 2.0, step: 0.05 },
                { id: "lfoRateScale_J60", name: "LFO Rate Scale (Juno-60)", category: "LFO", defaultValue: 1.0, currentValue: 1.0, min: 0.1, max: 2.0, step: 0.05 },
                { id: "lfoRateScale_J106", name: "LFO Rate Scale (Juno-106)", category: "LFO", defaultValue: 1.0, currentValue: 1.0, min: 0.1, max: 2.0, step: 0.05 },
                
                // DCO
                { id: "dcoMixerGain", name: "DCO Mixer Gain", category: "DCO", defaultValue: 0.7, currentValue: 0.7, min: 0.1, max: 1.5, step: 0.05 },
                { id: "dcoMixerGain_J6", name: "DCO Mixer Gain (Juno-6)", category: "DCO", defaultValue: 0.7, currentValue: 0.7, min: 0.1, max: 1.5, step: 0.05 },
                { id: "dcoMixerGain_J60", name: "DCO Mixer Gain (Juno-60)", category: "DCO", defaultValue: 0.7, currentValue: 0.7, min: 0.1, max: 1.5, step: 0.05 },
                { id: "dcoMixerGain_J106", name: "DCO Mixer Gain (Juno-106)", category: "DCO", defaultValue: 0.7, currentValue: 0.7, min: 0.1, max: 1.5, step: 0.05 },
                
                // HPF
                { id: "hpfCutoffOffset", name: "HPF Cutoff Offset", category: "HPF", defaultValue: 0.0, currentValue: 0.0, min: -10, max: 10, step: 0.5 },
                { id: "hpfCutoffOffset_J6", name: "HPF Cutoff Offset (Juno-6)", category: "HPF", defaultValue: 0.0, currentValue: 0.0, min: -10, max: 10, step: 0.5 },
                { id: "hpfCutoffOffset_J60", name: "HPF Cutoff Offset (Juno-60)", category: "HPF", defaultValue: 0.0, currentValue: 0.0, min: -10, max: 10, step: 0.5 },
                { id: "hpfCutoffOffset_J106", name: "HPF Cutoff Offset (Juno-106)", category: "HPF", defaultValue: 0.0, currentValue: 0.0, min: -10, max: 10, step: 0.5 },
                
                // VCF
                { id: "vcfCutoffTrim", name: "VCF Cutoff Trim", category: "VCF", defaultValue: 0.0, currentValue: 0.0, min: -50, max: 50, step: 1.0 },
                { id: "vcfCutoffTrim_J6", name: "VCF Cutoff Trim (Juno-6)", category: "VCF", defaultValue: 0.0, currentValue: 0.0, min: -50, max: 50, step: 1.0 },
                { id: "vcfCutoffTrim_J60", name: "VCF Cutoff Trim (Juno-60)", category: "VCF", defaultValue: 0.0, currentValue: 0.0, min: -50, max: 50, step: 1.0 },
                { id: "vcfCutoffTrim_J106", name: "VCF Cutoff Trim (Juno-106)", category: "VCF", defaultValue: 0.0, currentValue: 0.0, min: -50, max: 50, step: 1.0 },
                
                // VCA
                { id: "vcaMasterGain", name: "VCA Master Gain", category: "VCA", defaultValue: 1.0, currentValue: 1.0, min: 0.1, max: 3.0, step: 0.05 },
                { id: "vcaMasterGain_J6", name: "VCA Master Gain (Juno-6)", category: "VCA", defaultValue: 1.0, currentValue: 1.0, min: 0.1, max: 3.0, step: 0.05 },
                { id: "vcaMasterGain_J60", name: "VCA Master Gain (Juno-60)", category: "VCA", defaultValue: 1.0, currentValue: 1.0, min: 0.1, max: 3.0, step: 0.05 },
                { id: "vcaMasterGain_J106", name: "VCA Master Gain (Juno-106)", category: "VCA", defaultValue: 1.0, currentValue: 1.0, min: 0.1, max: 3.0, step: 0.05 },
                
                // ADSR
                { id: "adsrAttackScale", name: "ADSR Attack Scale", category: "ADSR", defaultValue: 1.0, currentValue: 1.0, min: 0.1, max: 3.0, step: 0.05 },
                { id: "adsrAttackScale_J6", name: "ADSR Attack Scale (Juno-6)", category: "ADSR", defaultValue: 1.0, currentValue: 1.0, min: 0.1, max: 3.0, step: 0.05 },
                { id: "adsrAttackScale_J60", name: "ADSR Attack Scale (Juno-60)", category: "ADSR", defaultValue: 1.0, currentValue: 1.0, min: 0.1, max: 3.0, step: 0.05 },
                { id: "adsrAttackScale_J106", name: "ADSR Attack Scale (Juno-106)", category: "ADSR", defaultValue: 1.0, currentValue: 1.0, min: 0.1, max: 3.0, step: 0.05 },
                
                // CHORUS
                { id: "chorusBbdNoiseLevel", name: "Chorus BBD Noise Level", category: "CHORUS", defaultValue: 0.15, currentValue: 0.15, min: 0.0, max: 1.0, step: 0.01 },
                { id: "chorusBbdNoiseLevel_J6", name: "Chorus Noise (Juno-6)", category: "CHORUS", defaultValue: 0.15, currentValue: 0.15, min: 0.0, max: 1.0, step: 0.01 },
                { id: "chorusBbdNoiseLevel_J60", name: "Chorus Noise (Juno-60)", category: "CHORUS", defaultValue: 0.15, currentValue: 0.15, min: 0.0, max: 1.0, step: 0.01 },
                { id: "chorusBbdNoiseLevel_J106", name: "Chorus Noise (Juno-106)", category: "CHORUS", defaultValue: 0.15, currentValue: 0.15, min: 0.0, max: 1.0, step: 0.01 },

                // SPACE ECHO (Super Six)
                { id: "delayInputLevel", name: "Space Echo Input Level", category: "SPACE ECHO", defaultValue: 0.8, currentValue: 0.8, min: 0.0, max: 1.0, step: 0.01 },
                { id: "delayWetDry", name: "Space Echo Wet/Dry Mix", category: "SPACE ECHO", defaultValue: 0.5, currentValue: 0.5, min: 0.0, max: 1.0, step: 0.01 },
                { id: "delayReverbType", name: "Space Echo Reverb Type", category: "SPACE ECHO", defaultValue: 0, currentValue: 0, min: 0, max: 2, step: 1 },
                { id: "delayWowRate", name: "Wow LFO Speed", category: "SPACE ECHO", defaultValue: 0.5, currentValue: 0.5, min: 0.1, max: 5.0, step: 0.1 },
                { id: "delayFlutterRate", name: "Flutter LFO Speed", category: "SPACE ECHO", defaultValue: 8.0, currentValue: 8.0, min: 2.0, max: 20.0, step: 0.1 },
                { id: "delayTapeScrapeRate", name: "Tape Scrape LFO Speed", category: "SPACE ECHO", defaultValue: 12.0, currentValue: 12.0, min: 5.0, max: 30.0, step: 0.1 },
                { id: "delaySaturationInputGain", name: "Tape Saturation Drive", category: "SPACE ECHO", defaultValue: 1.5, currentValue: 1.5, min: 0.5, max: 3.0, step: 0.1 },
                { id: "delaySpringGain", name: "Spring Reverb Output Gain", category: "SPACE ECHO", defaultValue: 3.0, currentValue: 3.0, min: 0.5, max: 5.0, step: 0.1 },
                { id: "delaySchroederGain", name: "Schroeder Reverb Output Gain", category: "SPACE ECHO", defaultValue: 1.5, currentValue: 1.5, min: 0.5, max: 3.0, step: 0.1 }
            ];
            return Promise.resolve(defaults);
        },
        setCalibrationParam: (id, val) => {
            console.log(`[WASM Bridge] setCalibrationParam(${id}, ${val})`);
            return Promise.resolve(true);
        },
        menuAction: (action, ...args) => {
            console.log(`[WASM Bridge] menuAction: ${action}`, args);
            if (window.WasmBridgeInstance) {
                let cur = window.WasmBridgeInstance.currentPresetIndex || 0;
                if (action === 'handleBankInc') cur = (cur + 8) % 128;
                else if (action === 'handleBankDec') cur = (cur - 8 + 128) % 128;
                else if (action === 'handlePatchInc') cur = (cur + 1) % 128;
                else if (action === 'handlePatchDec') cur = (cur - 1 + 128) % 128;
                else if (action === 'handleManual') cur = 0;
                else if (action === 'panic') {
                    if (typeof window.WasmBridgeInstance.panic === 'function') window.WasmBridgeInstance.panic();
                    if (window.onJuceEvent) window.onJuceEvent('onLCDUpdate', 'PANIC SILENCE');
                    return Promise.resolve(true);
                } else if (action === 'handleRandomize') {
                    const rndIdx = Math.floor(Math.random() * 128);
                    window.juce.loadPreset(rndIdx);
                    if (window.onJuceEvent) window.onJuceEvent('onLCDUpdate', 'RANDOM PATCH');
                    return Promise.resolve(true);
                } else if (action === 'handleTest') {
                    if (window.onJuceEvent) window.onJuceEvent('onLCDUpdate', 'SELF TEST OK');
                    return Promise.resolve(true);
                } else if (action === 'handleWriteArm' || action === 'handleLoad') {
                    if (window.openPresetBrowser) window.openPresetBrowser();
                    return Promise.resolve(true);
                }
                window.juce.loadPreset(cur);
            }
            return Promise.resolve(true);
        },
        uiReady: () => {
            console.log("[WASM Bridge] UI ready received, triggering initial preset load...");
            setTimeout(() => {
                if (window.juce && window.juce.loadPreset) {
                    window.juce.loadPreset(0);
                }
            }, 100);
            return Promise.resolve(true);
        },
    };

    // Ensure window.onJuceEvent is global and triggers JUCE event listeners
    window.onJuceEvent = function(eventName, data) {
        if (typeof window.dispatchJuceEvent === 'function') {
            window.dispatchJuceEvent(eventName, data);
        }
    };

    // Auto-resume audio on first user gesture
    const resumeAudioOnGesture = function() {
        if (window.WasmBridgeInstance) {
            window.WasmBridgeInstance.initAudioContext();
        }
    };

    if (typeof window !== 'undefined' && typeof window.addEventListener === 'function') {
        window.addEventListener('mousedown', resumeAudioOnGesture, { capture: true, passive: true });
        window.addEventListener('pointerdown', resumeAudioOnGesture, { capture: true, passive: true });
        window.addEventListener('touchstart', resumeAudioOnGesture, { capture: true, passive: true });
        window.addEventListener('keydown', resumeAudioOnGesture, { capture: true, passive: true });
        window.addEventListener('click', resumeAudioOnGesture, { capture: true, passive: true });
    }

    document.addEventListener('DOMContentLoaded', () => {
        const audioBtn = document.getElementById('wasm-audio-toggle-btn');
        if (audioBtn) {
            audioBtn.addEventListener('click', (e) => {
                e.stopPropagation();
                window.WasmBridgeInstance.initAudioContext();
            });
        }

        const modelSelect = document.getElementById('model-profile-select');
        if (modelSelect) {
            modelSelect.addEventListener('change', (e) => {
                window.WasmBridgeInstance.setModel(parseInt(e.target.value, 10));
            });
        }

        setTimeout(() => {
            if (window.juce && window.juce.loadPreset) {
                window.juce.loadPreset(0);
            }
        }, 200);
    });
})();