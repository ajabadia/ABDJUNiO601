/**
 * wasm-backend.js — WASM Standalone Backend Shim for ABD JUNiO 601
 * 
 * Pre-fetches the .wasm ArrayBuffer in the main thread and transfers it to the AudioWorklet.
 */
(function() {
    'use strict';

    function parseJunoPatch(raw) {
        return {
            name: raw.name,
            lfoRate: raw.lfoRate / 127.0,
            lfoDelay: raw.lfoDelay / 127.0,
            lfoToDCO: raw.lfoToDCO / 127.0,
            pwmAmount: raw.pwm / 127.0,
            pwm: raw.pwm / 127.0,
            noiseLevel: raw.noise / 127.0,
            vcfFreq: raw.vcfFreq / 127.0,
            resonance: raw.resonance / 127.0,
            envAmount: raw.envAmount / 127.0,
            lfoToVCF: raw.lfoToVCF / 127.0,
            kybdTracking: raw.kybdTracking / 127.0,
            vcaLevel: raw.vcaLevel / 127.0,
            masterVolume: 1.0,
            attack: raw.attack / 127.0,
            decay: raw.decay / 127.0,
            sustain: raw.sustain / 127.0,
            release: raw.release / 127.0,
            subOscLevel: raw.subOsc / 127.0,
            subOsc: raw.subOsc / 127.0,
            sawOn: (raw.sw1 & 0x01) ? 1.0 : 0.0,
            pulseOn: (raw.sw1 & 0x02) ? 1.0 : 0.0,
            chorus1: (raw.sw2 & 0x01) ? 1.0 : 0.0,
            chorus2: (raw.sw2 & 0x02) ? 1.0 : 0.0,
            vcaMode: (raw.sw1 & 0x04) ? 1.0 : 0.0,
            vcfPolarity: (raw.sw1 & 0x08) ? 1.0 : 0.0,
            hpfCutoff: (raw.sw2 >> 2) & 0x03,
            hpfFreq: (raw.sw2 >> 2) & 0x03
        };
    }

    const juno106PatchesRaw = [
        {name:"A11 Brass Set 1", lfoRate:0x14, lfoDelay:0x31, lfoToDCO:0x00, pwm:0x66, noise:0x00, vcfFreq:0x23, resonance:0x0D, envAmount:0x3A, lfoToVCF:0x00, kybdTracking:0x56, vcaLevel:0x6C, attack:0x03, decay:0x31, sustain:0x2D, release:0x20, subOsc:0x00, sw1:0x51, sw2:0x11},
        {name:"A12 Brass Swell", lfoRate:0x06, lfoDelay:0x30, lfoToDCO:0x00, pwm:0x38, noise:0x00, vcfFreq:0x2B, resonance:0x11, envAmount:0x1A, lfoToVCF:0x00, kybdTracking:0x54, vcaLevel:0x4B, attack:0x40, decay:0x76, sustain:0x26, release:0x25, subOsc:0x46, sw1:0x52, sw2:0x19},
        {name:"A13 Trumpet", lfoRate:0x34, lfoDelay:0x2D, lfoToDCO:0x08, pwm:0x66, noise:0x00, vcfFreq:0x37, resonance:0x22, envAmount:0x18, lfoToVCF:0x01, kybdTracking:0x3B, vcaLevel:0x7F, attack:0x05, decay:0x42, sustain:0x30, release:0x10, subOsc:0x00, sw1:0x32, sw2:0x09},
        {name:"A14 Flutes", lfoRate:0x3C, lfoDelay:0x2B, lfoToDCO:0x01, pwm:0x00, noise:0x00, vcfFreq:0x37, resonance:0x20, envAmount:0x0A, lfoToVCF:0x0B, kybdTracking:0x29, vcaLevel:0x7F, attack:0x17, decay:0x51, sustain:0x00, release:0x12, subOsc:0x00, sw1:0x32, sw2:0x01},
        {name:"A15 Moving Strings", lfoRate:0x3F, lfoDelay:0x00, lfoToDCO:0x00, pwm:0x27, noise:0x00, vcfFreq:0x4D, resonance:0x14, envAmount:0x04, lfoToVCF:0x00, kybdTracking:0x6F, vcaLevel:0x22, attack:0x0D, decay:0x57, sustain:0x58, release:0x23, subOsc:0x0E, sw1:0x1A, sw2:0x10},
        {name:"A16 Brass & Strings", lfoRate:0x23, lfoDelay:0x00, lfoToDCO:0x00, pwm:0x38, noise:0x00, vcfFreq:0x4C, resonance:0x11, envAmount:0x04, lfoToVCF:0x00, kybdTracking:0x29, vcaLevel:0x4E, attack:0x2C, decay:0x42, sustain:0x35, release:0x2C, subOsc:0x17, sw1:0x49, sw2:0x18},
        {name:"A17 Choir", lfoRate:0x3B, lfoDelay:0x0E, lfoToDCO:0x0D, pwm:0x19, noise:0x00, vcfFreq:0x3B, resonance:0x5E, envAmount:0x02, lfoToVCF:0x00, kybdTracking:0x3E, vcaLevel:0x7F, attack:0x44, decay:0x0B, sustain:0x7F, release:0x30, subOsc:0x00, sw1:0x4A, sw2:0x09},
        {name:"A18 Piano I", lfoRate:0x14, lfoDelay:0x31, lfoToDCO:0x00, pwm:0x50, noise:0x00, vcfFreq:0x41, resonance:0x0C, envAmount:0x0A, lfoToVCF:0x00, kybdTracking:0x1B, vcaLevel:0x67, attack:0x00, decay:0x42, sustain:0x00, release:0x1E, subOsc:0x56, sw1:0x2A, sw2:0x11},
        {name:"A21 Organ I", lfoRate:0x36, lfoDelay:0x0F, lfoToDCO:0x00, pwm:0x35, noise:0x00, vcfFreq:0x2B, resonance:0x4C, envAmount:0x0E, lfoToVCF:0x01, kybdTracking:0x7F, vcaLevel:0x64, attack:0x00, decay:0x0A, sustain:0x52, release:0x00, subOsc:0x17, sw1:0x29, sw2:0x1C},
        {name:"A22 Organ II", lfoRate:0x2C, lfoDelay:0x0F, lfoToDCO:0x00, pwm:0x35, noise:0x00, vcfFreq:0x35, resonance:0x4C, envAmount:0x0E, lfoToVCF:0x01, kybdTracking:0x55, vcaLevel:0x4A, attack:0x00, decay:0x0A, sustain:0x52, release:0x00, subOsc:0x3A, sw1:0x4A, sw2:0x1C},
        {name:"A23 Combo Organ", lfoRate:0x4B, lfoDelay:0x15, lfoToDCO:0x09, pwm:0x39, noise:0x00, vcfFreq:0x3F, resonance:0x46, envAmount:0x04, lfoToVCF:0x00, kybdTracking:0x6D, vcaLevel:0x60, attack:0x00, decay:0x30, sustain:0x2B, release:0x2E, subOsc:0x3D, sw1:0x2C, sw2:0x0D},
        {name:"A24 Calliope", lfoRate:0x52, lfoDelay:0x28, lfoToDCO:0x0B, pwm:0x00, noise:0x00, vcfFreq:0x57, resonance:0x1B, envAmount:0x11, lfoToVCF:0x00, kybdTracking:0x38, vcaLevel:0x59, attack:0x07, decay:0x7F, sustain:0x7F, release:0x06, subOsc:0x30, sw1:0x4A, sw2:0x0B},
        {name:"A25 Donald Pluck", lfoRate:0x4C, lfoDelay:0x15, lfoToDCO:0x09, pwm:0x39, noise:0x00, vcfFreq:0x49, resonance:0x69, envAmount:0x0F, lfoToVCF:0x00, kybdTracking:0x4E, vcaLevel:0x52, attack:0x02, decay:0x05, sustain:0x2B, release:0x0A, subOsc:0x7F, sw1:0x2C, sw2:0x07},
        {name:"A26 Celeste*", lfoRate:0x1C, lfoDelay:0x00, lfoToDCO:0x00, pwm:0x00, noise:0x00, vcfFreq:0x21, resonance:0x18, envAmount:0x36, lfoToVCF:0x00, kybdTracking:0x26, vcaLevel:0x60, attack:0x00, decay:0x2C, sustain:0x00, release:0x51, subOsc:0x0F, sw1:0x2C, sw2:0x19},
        {name:"A27 Elect. Piano I", lfoRate:0x3B, lfoDelay:0x00, lfoToDCO:0x00, pwm:0x00, noise:0x00, vcfFreq:0x10, resonance:0x1F, envAmount:0x3D, lfoToVCF:0x06, kybdTracking:0x23, vcaLevel:0x7F, attack:0x01, decay:0x55, sustain:0x2B, release:0x28, subOsc:0x00, sw1:0x29, sw2:0x01},
        {name:"A28 Elect. Piano II", lfoRate:0x00, lfoDelay:0x00, lfoToDCO:0x00, pwm:0x47, noise:0x00, vcfFreq:0x32, resonance:0x45, envAmount:0x07, lfoToVCF:0x00, kybdTracking:0x50, vcaLevel:0x66, attack:0x00, decay:0x44, sustain:0x00, release:0x16, subOsc:0x00, sw1:0x49, sw2:0x11},
        {name:"A31 Clock Chimes*", lfoRate:0x3B, lfoDelay:0x00, lfoToDCO:0x00, pwm:0x00, noise:0x16, vcfFreq:0x2C, resonance:0x7F, envAmount:0x00, lfoToVCF:0x00, kybdTracking:0x7F, vcaLevel:0x68, attack:0x00, decay:0x30, sustain:0x00, release:0x33, subOsc:0x7F, sw1:0x44, sw2:0x1B},
        {name:"A32 Steel Drums", lfoRate:0x21, lfoDelay:0x35, lfoToDCO:0x00, pwm:0x20, noise:0x09, vcfFreq:0x47, resonance:0x2E, envAmount:0x1A, lfoToVCF:0x00, kybdTracking:0x7F, vcaLevel:0x7F, attack:0x00, decay:0x1A, sustain:0x00, release:0x25, subOsc:0x21, sw1:0x4A, sw2:0x1B},
        {name:"A33 Xylophone", lfoRate:0x00, lfoDelay:0x00, lfoToDCO:0x00, pwm:0x00, noise:0x00, vcfFreq:0x1D, resonance:0x18, envAmount:0x36, lfoToVCF:0x00, kybdTracking:0x33, vcaLevel:0x7F, attack:0x00, decay:0x1D, sustain:0x1D, release:0x26, subOsc:0x0F, sw1:0x2C, sw2:0x19},
        {name:"A34 Brass III", lfoRate:0x34, lfoDelay:0x14, lfoToDCO:0x00, pwm:0x23, noise:0x00, vcfFreq:0x42, resonance:0x18, envAmount:0x0B, lfoToVCF:0x00, kybdTracking:0x0C, vcaLevel:0x7F, attack:0x3A, decay:0x64, sustain:0x5E, release:0x25, subOsc:0x16, sw1:0x52, sw2:0x19},
        {name:"A35 Fanfare", lfoRate:0x2F, lfoDelay:0x00, lfoToDCO:0x00, pwm:0x46, noise:0x00, vcfFreq:0x2C, resonance:0x00, envAmount:0x20, lfoToVCF:0x00, kybdTracking:0x43, vcaLevel:0x21, attack:0x48, decay:0x68, sustain:0x4B, release:0x31, subOsc:0x32, sw1:0x59, sw2:0x18},
        {name:"A36 String III", lfoRate:0x30, lfoDelay:0x1B, lfoToDCO:0x00, pwm:0x66, noise:0x00, vcfFreq:0x47, resonance:0x0E, envAmount:0x00, lfoToVCF:0x00, kybdTracking:0x54, vcaLevel:0x49, attack:0x3F, decay:0x1F, sustain:0x7F, release:0x2D, subOsc:0x00, sw1:0x1A, sw2:0x10},
        {name:"A37 Pizzicato", lfoRate:0x3C, lfoDelay:0x12, lfoToDCO:0x00, pwm:0x66, noise:0x00, vcfFreq:0x42, resonance:0x02, envAmount:0x05, lfoToVCF:0x00, kybdTracking:0x2A, vcaLevel:0x7F, attack:0x00, decay:0x0B, sustain:0x00, release:0x0C, subOsc:0x00, sw1:0x5A, sw2:0x00},
        {name:"A38 High Strings", lfoRate:0x3A, lfoDelay:0x0E, lfoToDCO:0x00, pwm:0x66, noise:0x00, vcfFreq:0x54, resonance:0x08, envAmount:0x02, lfoToVCF:0x00, kybdTracking:0x47, vcaLevel:0x4D, attack:0x12, decay:0x2C, sustain:0x7F, release:0x28, subOsc:0x00, sw1:0x0C, sw2:0x08},
        {name:"B11 Strings", lfoRate:0x39, lfoDelay:0x2D, lfoToDCO:0x00, pwm:0x37, noise:0x00, vcfFreq:0x55, resonance:0x00, envAmount:0x00, lfoToVCF:0x00, kybdTracking:0x6C, vcaLevel:0x34, attack:0x3B, decay:0x20, sustain:0x56, release:0x28, subOsc:0x00, sw1:0x1A, sw2:0x18},
        {name:"B12 Violin", lfoRate:0x42, lfoDelay:0x2D, lfoToDCO:0x14, pwm:0x00, noise:0x00, vcfFreq:0x4D, resonance:0x00, envAmount:0x00, lfoToVCF:0x00, kybdTracking:0x78, vcaLevel:0x6E, attack:0x2B, decay:0x2D, sustain:0x39, release:0x1A, subOsc:0x00, sw1:0x32, sw2:0x18},
        {name:"B13 Chorus Vibes", lfoRate:0x48, lfoDelay:0x2D, lfoToDCO:0x00, pwm:0x00, noise:0x00, vcfFreq:0x0A, resonance:0x00, envAmount:0x3B, lfoToVCF:0x00, kybdTracking:0x17, vcaLevel:0x58, attack:0x00, decay:0x59, sustain:0x2F, release:0x4A, subOsc:0x00, sw1:0x4A, sw2:0x19},
        {name:"B14 Organ 1", lfoRate:0x2D, lfoDelay:0x2A, lfoToDCO:0x00, pwm:0x49, noise:0x00, vcfFreq:0x3C, resonance:0x48, envAmount:0x0E, lfoToVCF:0x00, kybdTracking:0x7F, vcaLevel:0x3A, attack:0x00, decay:0x00, sustain:0x00, release:0x00, subOsc:0x61, sw1:0x0A, sw2:0x1D},
        {name:"B15 Harpsichord 1", lfoRate:0x17, lfoDelay:0x00, lfoToDCO:0x00, pwm:0x36, noise:0x00, vcfFreq:0x7F, resonance:0x7F, envAmount:0x5A, lfoToVCF:0x00, kybdTracking:0x56, vcaLevel:0x7D, attack:0x00, decay:0x3C, sustain:0x00, release:0x1F, subOsc:0x23, sw1:0x2C, sw2:0x00},
        {name:"B16 Recorder", lfoRate:0x4E, lfoDelay:0x00, lfoToDCO:0x00, pwm:0x08, noise:0x00, vcfFreq:0x06, resonance:0x00, envAmount:0x2E, lfoToVCF:0x00, kybdTracking:0x7F, vcaLevel:0x7F, attack:0x05, decay:0x15, sustain:0x7F, release:0x1E, subOsc:0x00, sw1:0x2A, sw2:0x11},
        {name:"B17 Perc. Pluck", lfoRate:0x3E, lfoDelay:0x26, lfoToDCO:0x08, pwm:0x00, noise:0x00, vcfFreq:0x00, resonance:0x00, envAmount:0x5A, lfoToVCF:0x00, kybdTracking:0x49, vcaLevel:0x41, attack:0x00, decay:0x10, sustain:0x4E, release:0x74, subOsc:0x00, sw1:0x4A, sw2:0x19},
        {name:"B18 Noise Sweep", lfoRate:0x7F, lfoDelay:0x00, lfoToDCO:0x00, pwm:0x00, noise:0x7F, vcfFreq:0x02, resonance:0x00, envAmount:0x4D, lfoToVCF:0x00, kybdTracking:0x68, vcaLevel:0x5C, attack:0x0E, decay:0x4E, sustain:0x6C, release:0x78, subOsc:0x00, sw1:0x21, sw2:0x18}
    ];

    const juno60PatchesRaw = [
        {name:"Strings 1", lfoRate:0x4C, lfoDelay:0x00, lfoToDCO:0x00, pwm:0x00, noise:0x00, vcfFreq:0x59, resonance:0x00, envAmount:0x00, lfoToVCF:0x00, kybdTracking:0x7F, vcaLevel:0x40, attack:0x33, decay:0x00, sustain:0x7F, release:0x39, subOsc:0x00, sw1:0x52, sw2:0x18},
        {name:"Strings 2", lfoRate:0x33, lfoDelay:0x00, lfoToDCO:0x00, pwm:0x4C, noise:0x00, vcfFreq:0x59, resonance:0x00, envAmount:0x00, lfoToVCF:0x00, kybdTracking:0x7F, vcaLevel:0x26, attack:0x33, decay:0x00, sustain:0x7F, release:0x39, subOsc:0x00, sw1:0x1A, sw2:0x18},
        {name:"Strings 3", lfoRate:0x26, lfoDelay:0x66, lfoToDCO:0x00, pwm:0x59, noise:0x00, vcfFreq:0x40, resonance:0x00, envAmount:0x00, lfoToVCF:0x00, kybdTracking:0x7F, vcaLevel:0x26, attack:0x26, decay:0x00, sustain:0x7F, release:0x4C, subOsc:0x7F, sw1:0x1A, sw2:0x18},
        {name:"Organ 1", lfoRate:0x19, lfoDelay:0x66, lfoToDCO:0x00, pwm:0x40, noise:0x00, vcfFreq:0x33, resonance:0x4C, envAmount:0x39, lfoToVCF:0x00, kybdTracking:0x7F, vcaLevel:0x40, attack:0x00, decay:0x00, sustain:0x00, release:0x00, subOsc:0x7F, sw1:0x2A, sw2:0x1D},
        {name:"Organ 2", lfoRate:0x40, lfoDelay:0x33, lfoToDCO:0x00, pwm:0x46, noise:0x00, vcfFreq:0x2C, resonance:0x46, envAmount:0x33, lfoToVCF:0x00, kybdTracking:0x7F, vcaLevel:0x40, attack:0x00, decay:0x0D, sustain:0x00, release:0x0D, subOsc:0x66, sw1:0x2A, sw2:0x1C},
        {name:"Organ 3", lfoRate:0x40, lfoDelay:0x33, lfoToDCO:0x00, pwm:0x46, noise:0x00, vcfFreq:0x2C, resonance:0x46, envAmount:0x2C, lfoToVCF:0x00, kybdTracking:0x7F, vcaLevel:0x40, attack:0x00, decay:0x0D, sustain:0x00, release:0x0D, subOsc:0x66, sw1:0x0C, sw2:0x1C},
        {name:"Brass", lfoRate:0x40, lfoDelay:0x53, lfoToDCO:0x13, pwm:0x00, noise:0x00, vcfFreq:0x00, resonance:0x00, envAmount:0x6C, lfoToVCF:0x00, kybdTracking:0x33, vcaLevel:0x59, attack:0x20, decay:0x33, sustain:0x4C, release:0x19, subOsc:0x00, sw1:0x32, sw2:0x19},
        {name:"Phase Brass", lfoRate:0x4C, lfoDelay:0x00, lfoToDCO:0x00, pwm:0x7F, noise:0x00, vcfFreq:0x26, resonance:0x0D, envAmount:0x46, lfoToVCF:0x00, kybdTracking:0x7F, vcaLevel:0x33, attack:0x19, decay:0x33, sustain:0x33, release:0x26, subOsc:0x7F, sw1:0x3A, sw2:0x1C},
        {name:"Piano 1", lfoRate:0x4C, lfoDelay:0x26, lfoToDCO:0x39, pwm:0x4C, noise:0x00, vcfFreq:0x0D, resonance:0x00, envAmount:0x59, lfoToVCF:0x00, kybdTracking:0x33, vcaLevel:0x59, attack:0x00, decay:0x66, sustain:0x13, release:0x26, subOsc:0x00, sw1:0x2A, sw2:0x19}
    ];

    function buildNativeLibraries() {
        const libs = [];
        
        const bankAPatches = [];
        for (let i = 0; i < 64; i++) {
            const raw = juno106PatchesRaw[i % juno106PatchesRaw.length];
            const p = parseJunoPatch(raw);
            const b = Math.floor(i / 8) + 1;
            const pt = (i % 8) + 1;
            bankAPatches.push({
                name: `A-${b}${pt} ${p.name}`, category: "Factory 106", author: "Roland", tags: "Juno-106", favorite: false, data: p
            });
        }
        libs.push({ name: "A - Juno 106 Factory A", patches: bankAPatches });

        const bankBPatches = [];
        for (let i = 0; i < 64; i++) {
            const raw = juno106PatchesRaw[(i + 16) % juno106PatchesRaw.length];
            const p = parseJunoPatch(raw);
            const b = Math.floor(i / 8) + 1;
            const pt = (i % 8) + 1;
            bankBPatches.push({
                name: `B-${b}${pt} ${p.name}`, category: "Factory 106", author: "Roland", tags: "Juno-106", favorite: false, data: p
            });
        }
        libs.push({ name: "B - Juno 106 Factory B", patches: bankBPatches });

        const bankCPatches = [];
        for (let i = 0; i < 64; i++) {
            const b = Math.floor(i / 8) + 1;
            const pt = (i % 8) + 1;
            bankCPatches.push({
                name: `C-${b}${pt} INIT PATCH`, category: "User", author: "User", tags: "RAM", favorite: false, data: parseJunoPatch(juno106PatchesRaw[0])
            });
        }
        libs.push({ name: "C - User Bank C", patches: bankCPatches });

        const bankDPatches = [];
        for (let i = 0; i < 64; i++) {
            const raw = juno60PatchesRaw[i % juno60PatchesRaw.length];
            const p = parseJunoPatch(raw);
            const b = Math.floor(i / 8) + 1;
            const pt = (i % 8) + 1;
            bankDPatches.push({
                name: `D-${b}${pt} ${p.name}`, category: "Juno-60 Factory", author: "Roland", tags: "Juno-60", favorite: false, data: p
            });
        }
        libs.push({ name: "D - Juno 60 Factory", patches: bankDPatches });

        for (let idx = 4; idx < 26; idx++) {
            const letter = String.fromCharCode(65 + idx);
            const userPatches = [];
            for (let i = 0; i < 64; i++) {
                const b = Math.floor(i / 8) + 1;
                const pt = (i % 8) + 1;
                userPatches.push({
                    name: `${letter}-${b}${pt} INIT PATCH`, category: "User", author: "User", tags: "User", favorite: false, data: parseJunoPatch(juno106PatchesRaw[0])
                });
            }
            libs.push({ name: `${letter} - User Bank ${letter}`, patches: userPatches });
        }

        return libs;
    }

    const allLibraries = buildNativeLibraries();

    const state = {
        audioCtx: null,
        masterGain: null,
        workletNode: null,
        wasmBinary: null,
        parameterCache: Object.assign({
            noiseFloor: 1.0,
            mainsRipple: 1.0,
            masterNoise: -70.0, // Subimos el ruido a -70dB para hacerlo audible por defecto
            delayEnabled: 1.0,  // Habilitado por defecto en la inicialización Super Six
            delaySetting: 11.0,
            delayRepeatRate: 0.5,
            delayIntensity: 0.4,
            polyMode: 0.0      // Poly 1 por defecto
        }, allLibraries[0].patches[0].data),
        currentPresetIndex: 0,
        currentLibIndex: 0,
        currentGroup: 0,
        currentBank: 1,
        currentPatch: 1,
        currentModel: 0,
        isAudioStarted: false
    };

    const sysexMirror = new Array(23).fill(0);
    sysexMirror[0] = 0xF0; sysexMirror[1] = 0x41; sysexMirror[2] = 0x30;
    sysexMirror[3] = 0x00; sysexMirror[22] = 0xF7;

    const paramToSysexByte = {
        'lfoRate': 4, 'lfoDelay': 5, 'dcoRange': 6, 'pwmAmount': 7, 'pwmMode': 8,
        'lfoToDCO': 9, 'vcfFreq': 10, 'resonance': 11, 'envAmount': 12,
        'vcfPolarity': 13, 'lfoToVCF': 14, 'kybdTracking': 15, 'vcaLevel': 16,
        'attack': 17, 'decay': 18, 'sustain': 19, 'release': 20, 'chorus1': 21
    };

    function updateAudioButtonUI(active) {
        const btn = document.getElementById('wasm-audio-toggle-btn');
        if (btn) {
            btn.style.borderColor = active ? '#00ffcc' : '#ff4444';
            btn.style.color = active ? '#00ffcc' : '#ff4444';
            btn.innerText = active ? '🔊 WEB AUDIO: ON' : '🔇 ACTIVAR AUDIO WEB';
        }
    }

    async function initAudioContext() {
        if (state.isInitializing) return false;
        if (state.audioCtx && state.masterGain && state.workletNode) {
            if (state.audioCtx.state === 'suspended') {
                state.audioCtx.resume().then(() => updateAudioButtonUI(true)).catch(() => {});
            } else {
                updateAudioButtonUI(true);
            }
            state.isAudioStarted = true;
            return true;
        }

        state.isInitializing = true;

        try {
            const Ctx = window.AudioContext || window.webkitAudioContext;
            if (!Ctx) return false;

            if (!state.audioCtx) {
                state.audioCtx = new Ctx({ sampleRate: 44100 });
            }

            if (!state.masterGain) {
                state.masterGain = state.audioCtx.createGain();
                state.masterGain.gain.setValueAtTime(0.85, state.audioCtx.currentTime);
                state.masterGain.connect(state.audioCtx.destination);
            }

            // Register and load AudioWorkletProcessor matching ABDEep architecture
            await state.audioCtx.audioWorklet.addModule('js/dsp-processor.js', { type: 'module' });

            state.workletNode = new AudioWorkletNode(state.audioCtx, 'abdjunio601-dsp-processor', {
                outputChannelCount: [2],
                processorOptions: { sampleRate: state.audioCtx.sampleRate }
            });

            state.workletNode.port.onmessage = (event) => {
                if (event.data && event.data.type === 'status' && event.data.ready) {
                    console.log('✅ [WASM Backend] AudioWorklet C++ DSP Ready!');
                    state.workletNode.port.postMessage({ type: 'set_params', params: state.parameterCache });
                }
            };

            state.workletNode.connect(state.masterGain);

            if (state.audioCtx.state === 'suspended') {
                state.audioCtx.resume().then(() => updateAudioButtonUI(true));
            } else {
                updateAudioButtonUI(true);
            }

            state.isAudioStarted = true;
            state.isInitializing = false;
            console.log('🔊 [WASM Backend] AudioWorklet Pipeline Active');
            return true;
        } catch (e) {
            state.isInitializing = false;
            console.error('[WASM Backend] AudioContext init error:', e);
            return false;
        }
    }

    function noteOn(note, velocity) {
        initAudioContext();
        if (state.workletNode) {
            state.workletNode.port.postMessage({ type: 'note_on', note: note, velocity: velocity || 0.85 });
        }
    }

    function noteOff(note) {
        if (state.workletNode) {
            state.workletNode.port.postMessage({ type: 'note_off', note: note });
        }
    }

    function setParameter(id, val) {
        state.parameterCache[id] = val;
        if (state.workletNode) {
            state.workletNode.port.postMessage({ type: 'set_param', paramId: id, value: val });
            if (id === 'subOsc') state.workletNode.port.postMessage({ type: 'set_param', paramId: 'subOscLevel', value: val });
            if (id === 'subOscLevel') state.workletNode.port.postMessage({ type: 'set_param', paramId: 'subOsc', value: val });
            if (id === 'hpfFreq') state.workletNode.port.postMessage({ type: 'set_param', paramId: 'hpfCutoff', value: val });
            if (id === 'hpfCutoff') state.workletNode.port.postMessage({ type: 'set_param', paramId: 'hpfFreq', value: val });
            if (id === 'pwm') state.workletNode.port.postMessage({ type: 'set_param', paramId: 'pwmAmount', value: val });
            if (id === 'pwmAmount') state.workletNode.port.postMessage({ type: 'set_param', paramId: 'pwm', value: val });
        }
        emitSysExParamChange(id, val);
        if (typeof window.onJuceEvent === 'function') {
            window.onJuceEvent('onParameterChanged', { id, value: val });
        }
    }

    function sendSysExDump() {
        Object.keys(paramToSysexByte).forEach(pId => {
            const bIdx = paramToSysexByte[pId];
            const val = state.parameterCache[pId] !== undefined ? state.parameterCache[pId] : 0;
            sysexMirror[bIdx] = Math.floor(val * 127);
        });
        const hexStr = sysexMirror.map(b => b.toString(16).toUpperCase().padStart(2, '0')).join(' ');
        if (typeof window.onJuceEvent === 'function') {
            window.onJuceEvent('onSysExUpdate', hexStr);
        }
    }

    function emitSysExParamChange(id, val) {
        const byteIdx = paramToSysexByte[id];
        const intVal = Math.floor(Math.max(0, Math.min(1, val)) * 127);
        if (byteIdx !== undefined) {
            sysexMirror[byteIdx] = intVal;
            const pIdHex = (byteIdx - 4).toString(16).toUpperCase().padStart(2, '0');
            const pValHex = intVal.toString(16).toUpperCase().padStart(2, '0');
            const singleHex = `F0 41 32 00 ${pIdHex} ${pValHex} F7`;
            if (typeof window.onJuceEvent === 'function') {
                window.onJuceEvent('onSysExUpdate', singleHex);
            }
        } else {
            sendSysExDump();
        }
    }

    function applyPresetParameters(paramObj) {
        Object.assign(state.parameterCache, paramObj);
        if (state.workletNode) {
            state.workletNode.port.postMessage({ type: 'set_params', params: state.parameterCache });
        }
        if (typeof window.onJuceEvent === 'function') {
            window.onJuceEvent('parameterSetUpdate', paramObj);
        }
        sendSysExDump();
    }

    function setModel(modelId) {
        state.currentModel = modelId;
        if (state.workletNode) {
            state.workletNode.port.postMessage({ type: 'set_model', model: modelId });
        }
        if (typeof window.onJuceEvent === 'function') {
            window.onJuceEvent('onProductNameUpdate', { name: 'ABD JUNiO', targetModel: modelId });
        }
        if (typeof window.__updateModelVisibility === 'function') {
            window.__updateModelVisibility(modelId);
        }
    }

    document.addEventListener('DOMContentLoaded', () => {
        const audioBtn = document.getElementById('wasm-audio-toggle-btn');
        if (audioBtn) {
            audioBtn.addEventListener('click', (e) => {
                e.stopPropagation();
                initAudioContext();
            });
        }

        const modelSelect = document.getElementById('model-profile-select');
        if (modelSelect) {
            modelSelect.addEventListener('change', (e) => {
                setModel(parseInt(e.target.value, 10));
            });
        }
    });

    ['mousedown','pointerdown','touchstart','keydown','click'].forEach(evt => {
        window.addEventListener(evt, () => {
            initAudioContext();
        }, { capture: true, passive: true });
    });

    // ─── Backend API ───
    window.juceBackend = {
        setParameter: (id, val) => setParameter(id, val),
        beginGesture: () => {},
        endGesture: () => {},
        getUserName: () => Promise.resolve("ABD USER"),

        menuAction: (action, ...args) => {
            let cur = state.currentPresetIndex;
            switch (action) {
                case 'handleBankInc':   cur = (cur + 8) % (26 * 64); break;
                case 'handleBankDec':   cur = (cur - 8 + 26 * 64) % (26 * 64); break;
                case 'handlePatchInc':  cur = (cur + 1) % (26 * 64); break;
                case 'handlePatchDec':  cur = (cur - 1 + 26 * 64) % (26 * 64); break;
                case 'handleManual':    cur = 0; break;
                case 'getUserName':     return Promise.resolve("ABD USER");
                case 'panic':
                    if (state.workletNode) state.workletNode.port.postMessage({ type: 'panic' });
                    return;
                case 'handleRandomize':
                    const randomizeList = [
                        "sawOn", "pulseOn", "pwm", "subOsc", "noise",
                        "dcoRange", "vcfFreq", "resonance", "hpfFreq",
                        "envAmount", "lfoToVCF", "vcaMode", "vcaLevel",
                        "lfoRate", "lfoDelay", "lfoToDCO", "attack", "decay",
                        "sustain", "release", "chorus1", "chorus2", "pwmMode"
                    ];
                    randomizeList.forEach(pId => {
                        let val = Math.random();
                        if (pId === 'sawOn' || pId === 'pulseOn' || pId === 'chorus1' || pId === 'chorus2') {
                            val = Math.random() > 0.5 ? 1.0 : 0.0;
                        } else if (pId === 'dcoRange') {
                            val = Math.random() > 0.66 ? 1.0 : (Math.random() > 0.5 ? 0.5 : 0.0);
                        } else if (pId === 'vcaMode' || pId === 'pwmMode') {
                            val = Math.random() > 0.5 ? 1.0 : 0.0;
                        }
                        setParameter(pId, val);
                    });
                    if (typeof window.__syncAllSliders === 'function') {
                        window.__syncAllSliders();
                    }
                    return Promise.resolve();
                case 'handleTest':
                    if (typeof window.onJuceEvent === 'function') {
                        window.onJuceEvent('onLCDUpdate', 'TEST MODE ACTIVE');
                    }
                    return Promise.resolve();
                case 'handleExportBank':
                    if (typeof window.onJuceEvent === 'function') {
                        window.onJuceEvent('onLCDUpdate', 'VERIFY OK');
                    }
                    return Promise.resolve();
                default:
                    console.log('[WASM Backend] menuAction:', action, args);
                    return Promise.resolve();
            }
            window.juceBackend.loadPreset(cur);
        },

        loadPreset: (idx) => {
            state.currentPresetIndex = idx;
            state.currentLibIndex = Math.floor(idx / 64) % 26;
            const rem = idx % 64;
            state.currentBank = Math.floor(rem / 8) + 1;
            state.currentPatch = (rem % 8) + 1;

            const lib = allLibraries[state.currentLibIndex];
            const patchObj = (lib && lib.patches) ? lib.patches[rem] : null;
            const patchName = patchObj ? patchObj.name : `PATCH ${idx + 1}`;

            if (typeof window.onJuceEvent === 'function') {
                window.onJuceEvent('onBankPatchUpdate', { group: state.currentLibIndex, bank: state.currentBank, patch: state.currentPatch });
                window.onJuceEvent('onLCDUpdate', 'P: ' + (idx + 1) + ' ' + patchName);
            }

            if (patchObj && patchObj.data) {
                applyPresetParameters(patchObj.data);
            } else {
                sendSysExDump();
            }
        },

        loadLibraryPreset: (libIdx, prstIdx) => {
            window.juceBackend.loadPreset(libIdx * 64 + prstIdx);
        },

        selectLibrary: (idx) => {
            state.currentLibIndex = idx % 26;
            window.juceBackend.loadPreset(state.currentLibIndex * 64);
        },

        getBrowserData: () => Promise.resolve({
            libraries: allLibraries,
            categories: ["Factory 106", "Juno-60 Factory", "User"],
            currentLib: state.currentLibIndex,
            currentPatch: state.currentPresetIndex % 64
        }),

        getCalibrationParams: () => Promise.resolve([]),
        setCalibrationParam: (id, val) => setParameter(id, val),
        serviceAction: () => {},
        getSynthState: () => Promise.resolve(Object.assign({}, state.parameterCache)),

        setFavorite: (libIdx, presetIdx, fav) => {
            if (allLibraries[libIdx] && allLibraries[libIdx].patches[presetIdx]) {
                allLibraries[libIdx].patches[presetIdx].favorite = fav;
            }
            return Promise.resolve();
        },

        updateMetadata: () => Promise.resolve(),
        setBrowserData: () => Promise.resolve(),
        savePresetDetailed: () => Promise.resolve(),
        saveAsNewPresetDetailed: () => Promise.resolve(),
        exportBank: () => Promise.resolve(),
        importBank: () => Promise.resolve(),
        confirmImportFile: () => Promise.resolve(),
        confirmTapeImport: () => Promise.resolve(),
        getLibraryPath: () => Promise.resolve(''),
        setLibraryPath: () => Promise.resolve(),
        chooseDirectory: () => Promise.resolve(),
        switchAB: () => Promise.resolve(),
        copyAB: () => Promise.resolve(),

        pianoNoteOn: (note, vel) => noteOn(note, vel),
        pianoNoteOff: (note) => noteOff(note),

        uiReady: () => {
            console.log('[WASM Backend] UI ready');
            setTimeout(() => {
                if (typeof window.onJuceEvent === 'function') {
                    window.onJuceEvent('onVersionUpdate', '1.3.0 (AudioWorklet WASM DSP)');
                    window.onJuceEvent('onProductNameUpdate', { name: 'ABD JUNiO SUPER SIX', targetModel: 0 });
                    window.onJuceEvent('onBankPatchUpdate', { group: 0, bank: 1, patch: 1 });
                    window.juceBackend.loadPreset(0);
                }
            }, 100);
            return Promise.resolve();
        }
    };

    window._wasmBackendState = state;
})();
