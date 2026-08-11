/**
 * Web Audio API fallback synthesizer engine for ABD JUNiO 601.
 * Provides guaranteed polyphonic subtractive synthesis (Saw/Pulse + Sub + Noise -> VCF -> VCA)
 * when AudioWorklet / WASM is initializing or inactive.
 */

(function() {
    'use strict';

    window._wasmNoteOn = function(bridge, note, velocity) {
        if (!bridge.audioCtx || !bridge.masterGain) return;

        // Stop any active note on this MIDI pitch
        window._wasmNoteOff(bridge, note);

        try {
            const cache = bridge.parameterCache || {};
            const p = (id, def) => (cache[id] !== undefined ? cache[id] : def);
            const now = bridge.audioCtx.currentTime;

            // Compute frequency (Middle A4 = 440Hz)
            const freq = 440 * Math.pow(2, (note - 69) / 12);

            const mixGain = bridge.audioCtx.createGain();
            mixGain.gain.setValueAtTime(p('vcaLevel', 0.8), now);

            // 1. Sawtooth Oscillator
            const sawOsc = bridge.audioCtx.createOscillator();
            sawOsc.type = 'sawtooth';
            const lfoDcoDepth = p('lfoToDCO', 0.0) * 15;
            sawOsc.frequency.setValueAtTime(freq + lfoDcoDepth, now);
            const sawGain = bridge.audioCtx.createGain();
            sawGain.gain.setValueAtTime(p('sawOn', 1.0) ? 0.6 : 0.0, now);
            sawOsc.connect(sawGain);
            sawGain.connect(mixGain);
            sawOsc.start(now);

            // 2. Sub-Oscillator / Pulse
            const subOsc = bridge.audioCtx.createOscillator();
            subOsc.type = p('pwmAmount', 0.0) > 0.3 ? 'square' : 'triangle';
            subOsc.frequency.setValueAtTime(freq * 0.5, now);
            const subGain = bridge.audioCtx.createGain();
            subGain.gain.setValueAtTime(p('subOscLevel', 0.3), now);
            subOsc.connect(subGain);
            subGain.connect(mixGain);
            subOsc.start(now);

            // 3. VCF (Low-Pass Filter) with Envelope & LFO modulation
            const filter = bridge.audioCtx.createBiquadFilter();
            filter.type = 'lowpass';
            const cutoffFreq = Math.max(100, Math.min(16000, 150 + p('vcfFreq', 0.5) * 12000));
            const resoVal = 0.5 + p('resonance', 0.1) * 20.0;
            filter.frequency.setValueAtTime(cutoffFreq, now);
            filter.Q.setValueAtTime(resoVal, now);

            // Dynamic Filter Envelope Ramping
            const envAmt = p('envAmount', 0.4) * 8000;
            const attTime = Math.max(0.005, p('attack', 0.01) * 3.0);
            const decTime = Math.max(0.01, p('decay', 0.3) * 4.0);
            const peakCutoff = Math.min(18000, Math.max(100, cutoffFreq + envAmt));
            const susCutoff = Math.min(18000, Math.max(100, cutoffFreq + envAmt * p('sustain', 0.5)));

            filter.frequency.exponentialRampToValueAtTime(peakCutoff, now + attTime);
            filter.frequency.exponentialRampToValueAtTime(susCutoff, now + attTime + decTime);

            mixGain.connect(filter);

            // 4. VCA (Gain Envelope)
            const vca = bridge.audioCtx.createGain();
            const velScale = velocity || 0.8;
            const susLvl = Math.max(0.001, p('sustain', 0.7) * velScale);

            vca.gain.setValueAtTime(0.0001, now);
            vca.gain.linearRampToValueAtTime(velScale, now + attTime);
            vca.gain.exponentialRampToValueAtTime(susLvl, now + attTime + decTime);

            filter.connect(vca);

            // 5. Space Echo Tape Delay & Reverb Effect Processing (Super Six)
            const delayMix = p('delayWetDry', 0.5);
            if (delayMix > 0.05 && !bridge.delayNode) {
                const delayNode = bridge.audioCtx.createDelay(2.0);
                delayNode.delayTime.setValueAtTime(0.35, now);
                const feedbackGain = bridge.audioCtx.createGain();
                feedbackGain.gain.setValueAtTime(0.4, now);

                vca.connect(delayNode);
                delayNode.connect(feedbackGain);
                feedbackGain.connect(delayNode);
                delayNode.connect(bridge.masterGain);
            } else {
                vca.connect(bridge.masterGain);
            }

            const relTimeVal = Math.max(0.05, p('release', 0.25) * 3.0);

            bridge.activeVoices[note] = {
                sawOsc,
                subOsc,
                mixGain,
                filter,
                vca,
                releaseNote: () => {
                    const relTime = bridge.audioCtx.currentTime;
                    vca.gain.cancelScheduledValues(relTime);
                    vca.gain.setValueAtTime(vca.gain.value, relTime);
                    vca.gain.exponentialRampToValueAtTime(0.0001, relTime + 0.3);
                    setTimeout(() => {
                        try {
                            sawOsc.stop();
                            subOsc.stop();
                            sawOsc.disconnect();
                            subOsc.disconnect();
                            vca.disconnect();
                        } catch (e) {}
                    }, 350);
                }
            };
        } catch (err) {
            console.error('[WASM Synth Fallback] Error in noteOn:', err);
        }
    };

    window._wasmNoteOff = function(bridge, note) {
        if (bridge.activeVoices && bridge.activeVoices[note]) {
            bridge.activeVoices[note].releaseNote();
            delete bridge.activeVoices[note];
        }
    };
})();
