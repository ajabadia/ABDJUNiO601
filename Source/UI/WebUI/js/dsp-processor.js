/**
 * dsp-processor.js — AudioWorkletProcessor for ABD JUNiO 601 WASM DSP Engine.
 * 
 * Runs in the browser's high-priority audio thread.
 * Self-contained WASM binary bundled via SINGLE_FILE=1.
 */

import createABDJUNiO601 from '../wasm/abdjunio601_wasm.js';

// Global Polyfills for AudioWorklet scope (Chrome AudioWorkletGlobalScope polyfills)
if (typeof globalThis.WorkerGlobalScope === 'undefined') {
    globalThis.WorkerGlobalScope = true;
}
if (typeof globalThis.URL === 'undefined') {
    globalThis.URL = class {
        constructor(url, base) {
            this.href = (typeof base === 'string') ? base + '/' + url : url;
        }
    };
}

class ABDJUNiO601WasmProcessor extends AudioWorkletProcessor {
    constructor(options) {
        super();

        this.wasmModule = null;
        this.dsp = null;
        this.sampleRate = options.processorOptions?.sampleRate || 44100;
        this.blockSize = 128;

        this.bufferPtr = 0;
        this.isWasmReady = false;

        this.port.onmessage = this.handleMessage.bind(this);
        this.initWasm();
    }

    async initWasm() {
        try {
            const module = await createABDJUNiO601();

            this.wasmModule = module;

            this.dsp = {
                init: module.cwrap('wasm_init_engine', 'void', ['number', 'number', 'number']),
                process: module.cwrap('wasm_process_audio', 'void', ['number', 'number', 'number']),
                setParameter: module.cwrap('wasm_set_parameter', 'void', ['string', 'number']),
                setModel: module.cwrap('wasm_set_model', 'void', ['number']),
                noteOn: module.cwrap('wasm_note_on', 'void', ['number', 'number', 'number']),
                noteOff: module.cwrap('wasm_note_off', 'void', ['number', 'number', 'number']),
                panic: module.cwrap('wasm_panic', 'void', [])
            };

            this.dsp.init(this.sampleRate, this.blockSize, 0);

            const numBytes = this.blockSize * 2 * 4; // Stereo float32
            const mallocFn = module._malloc || (module.cwrap ? module.cwrap('malloc', 'number', ['number']) : null);
            if (mallocFn) {
                this.bufferPtr = mallocFn(numBytes);
            }

            if (!this.bufferPtr) {
                console.error('❌ [WasmProcessor] Failed to allocate WASM heap buffer via _malloc');
                return;
            }

            this.isWasmReady = true;
            this.port.postMessage({ type: 'status', ready: true });
            console.log('✅ [WasmProcessor] Motor DSP C++ inicializado correctamente en AudioWorklet.');
        } catch (e) {
            console.error('❌ [WasmProcessor] Error inicializando motor WebAssembly:', e);
        }
    }

    handleMessage(event) {
        const data = event.data;
        if (!data || !this.isWasmReady || !this.dsp) return;

        switch (data.type) {
            case 'note_on':
                this.dsp.noteOn(1, data.note, data.velocity || 0.8);
                break;
            case 'note_off':
                this.dsp.noteOff(1, data.note, 0);
                break;
            case 'set_param':
                this.dsp.setParameter(data.paramId, data.value);
                break;
            case 'set_params':
                if (data.params) {
                    Object.keys(data.params).forEach(k => {
                        this.dsp.setParameter(k, data.params[k]);
                    });
                }
                break;
            case 'set_model':
                this.dsp.setModel(data.model || 0);
                break;
            case 'panic':
                this.dsp.panic();
                break;
        }
    }

    process(inputs, outputs) {
        const output = outputs[0];
        if (!output || output.length < 2) return true;

        const outL = output[0];
        const outR = output[1];

        if (this.isWasmReady && this.dsp && this.bufferPtr) {
            this.dsp.process(this.bufferPtr, this.blockSize, 0);

            const heapOffset = this.bufferPtr >> 2;
            outL.set(this.wasmModule.HEAPF32.subarray(heapOffset, heapOffset + this.blockSize));
            outR.set(this.wasmModule.HEAPF32.subarray(heapOffset + this.blockSize, heapOffset + this.blockSize * 2));
        } else {
            outL.fill(0);
            outR.fill(0);
        }

        return true;
    }
}

registerProcessor('abdjunio601-dsp-processor', ABDJUNiO601WasmProcessor);
