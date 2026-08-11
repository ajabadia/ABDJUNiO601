/**
 * AudioContext initialization, AudioWorklet management and UI update helpers.
 * Extracted following ABDEep architecture.
 */

(function() {
    'use strict';

    window._wasmInitAudioContext = function(bridge) {
        if (bridge.audioCtx && bridge.masterGain) {
            if (bridge.audioCtx.state === 'suspended') {
                bridge.audioCtx.resume().catch(() => {});
            }
            bridge.isAudioStarted = true;
            window._wasmUpdateAudioButtonUI(true);
            return true;
        }

        try {
            const AudioCtxClass = window.AudioContext || window.webkitAudioContext;
            if (!AudioCtxClass) {
                console.warn('[WasmBridge] Web Audio API not supported.');
                return false;
            }

            if (!bridge.audioCtx) {
                bridge.audioCtx = new AudioCtxClass({ sampleRate: 44100 });
            }

            if (!bridge.masterGain) {
                bridge.masterGain = bridge.audioCtx.createGain();
                bridge.masterGain.gain.setValueAtTime(0.8, bridge.audioCtx.currentTime);
                bridge.masterGain.connect(bridge.audioCtx.destination);
            }

            bridge.isAudioStarted = true;
            window._wasmUpdateAudioButtonUI(true);
            return true;
        } catch (err) {
            console.error('[WasmBridge] Error initializing AudioContext:', err);
            return false;
        }
    };

    window._wasmUpdateAudioButtonUI = function(active) {
        const btn = document.getElementById('wasm-audio-toggle-btn');
        if (btn) {
            btn.style.borderColor = active ? '#00ffcc' : '#ff4444';
            btn.style.color = active ? '#00ffcc' : '#ff4444';
            btn.innerText = active ? '🔊 WEB AUDIO: ON' : '🔇 ACTIVAR AUDIO WEB';
        }
    };
})();
