// service.js - Service Mode Logic for JUNiO 601
const ServiceMode = {
    params: [],

    async init() {
        console.log("ServiceMode: Initializing...");
        try {
            await this.refreshParams();
        } catch (e) {
            console.error("ServiceMode: Failed to load calibration params. Render continue.", e);
        }
        this.renderVoices();
        
        // Render GENERAL tab dynamically if container exists
        this.renderCategory("GENERAL", "general-params-list");
    },

    async refreshParams() {
        console.log("ServiceMode: Refreshing params...");
        try {
            this.params = await juce.getCalibrationParams();
            if (!this.params) this.params = [];
            console.log(`ServiceMode: Loaded ${this.params.length} params`);
            this.renderParams();
        } catch (e) {
            console.error("Failed to get calibration params:", e);
            this.params = [];
            this.renderParams();
        }
    },

    renderParams() {
        const container = document.getElementById('service-params-list');
        if (!container) return;

        container.innerHTML = '';
        
        // [Build 35] Order modules: LFO, DCO, HPF, VCF, VCA, ADSR, CHORUS, THERMAL, AGING, SYSTEM (GENERAL handled separately)
        const categories = ["LFO", "DCO", "HPF", "VCF", "VCA", "ADSR", "CHORUS", "THERMAL", "AGING", "SYSTEM"];
        
        categories.forEach(cat => {
            this.internalRenderCategory(cat, container);
        });
        console.log("ServiceMode: Params refreshed and rendered to", container.id);
    },

    renderCategory(cat, containerId) {
        const container = document.getElementById(containerId);
        if (!container) return;
        container.innerHTML = '';
        this.internalRenderCategory(cat, container);
    },

    internalRenderCategory(cat, container) {
        const catParams = this.params.filter(p => p.category === cat);
        if (catParams.length === 0 && cat !== "GENERAL") return;

        const header = document.createElement('div');
        header.className = 'service-cat-header';
        header.innerHTML = `
            <span>${cat}</span>
            <button class="service-reset-btn cat-reset" onclick="ServiceMode.resetCategory('${cat}')" title="Reset this section to factory defaults">RESET SECTION</button>
        `;
        container.appendChild(header);

        if (catParams.length === 0) {
            const empty = document.createElement('div');
            empty.className = 'service-param-empty';
            empty.innerText = 'No adjustable parameters';
            container.appendChild(empty);
        } else {
            const grid = document.createElement('div');
            grid.className = 'service-param-grid';
            catParams.forEach(p => {
                const row = document.createElement('div');
                row.className = 'service-param-row';
                row.title = p.tooltip || '';
                
                // Format value (handles integers like MIDI channel)
                const fmtVal = p.isInteger ? Math.round(p.currentValue) : p.currentValue.toFixed(2);
                const fmtDef = p.isInteger ? Math.round(p.defaultValue) : p.defaultValue.toFixed(2);
                const unitStr = p.unit || '';

                // List of parameters that should be rendered as SELECT instead of SLIDER
                const choiceParams = {
                    "midiChannel": { 0: "OMNI", 1: "1", 2: "2", 3: "3", 4: "4", 5: "5", 6: "6", 7: "7", 8: "8", 9: "9", 10: "10", 11: "11", 12: "12", 13: "13", 14: "14", 15: "15", 16: "16" },
                    "numVoices": { 1: "1 (MONO)", 2: "2", 4: "4", 6: "6 (CLASSIC)", 8: "8", 12: "12", 16: "16 (MAX)" },
                    "benderRange": { 1: "1 SEMI", 2: "2 SEMIS", 3: "3 SEMIS", 4: "4 SEMIS", 7: "5th", 12: "OCTAVE" },
                    "midiFunction": { 0: "I (NOTES)", 1: "II (+PATCH)", 2: "III (+SYSEX)" },
                    "midiFunction": { 0: "I (NOTES)", 1: "II (+PATCH)", 2: "III (+SYSEX)" },
                    "sustainPedalInvert": { 0: "NORMAL", 1: "INVERTED" },
                    "enableLogging": { 0: "OFF", 1: "ON" }
                };

                if (choiceParams[p.id]) {
                    const options = Object.entries(choiceParams[p.id]).map(([val, lbl]) => 
                        `<option value="${val}" ${Math.round(p.currentValue) == val ? 'selected' : ''}>${lbl}</option>`
                    ).join('');

                    row.innerHTML = `
                        <div class="service-param-info">
                            <span class="service-param-label">${p.label}</span>
                            <div class="service-param-values">
                                <span class="service-param-default">Def: ${choiceParams[p.id][Math.round(p.defaultValue)]}</span>
                                <span class="service-param-value" id="val-${p.id}">${choiceParams[p.id][Math.round(p.currentValue)]}</span>
                            </div>
                        </div>
                        <div class="service-slider-row">
                            <select class="service-select" onchange="ServiceMode.updateParam('${p.id}', this.value)">
                                ${options}
                            </select>
                            <button class="service-reset-btn param-reset" onclick="ServiceMode.resetParam('${p.id}')" title="Reset to default">↺</button>
                        </div>
                    `;
                } else {
                    row.innerHTML = `
                        <div class="service-param-info">
                            <span class="service-param-label">${p.label}</span>
                            <div class="service-param-values">
                                <span class="service-param-default">Def: ${fmtDef}${unitStr}</span>
                                <span class="service-param-value" id="val-${p.id}">${fmtVal}${unitStr}</span>
                            </div>
                        </div>
                        <div class="service-slider-row">
                            <input type="range" class="service-slider" 
                                min="${p.minValue}" max="${p.maxValue}" step="${p.stepSize}" 
                                value="${p.currentValue}" 
                                oninput="ServiceMode.updateParam('${p.id}', this.value)">
                            <button class="service-reset-btn param-reset" onclick="ServiceMode.resetParam('${p.id}')" title="Reset to ${fmtDef}${unitStr}">↺</button>
                        </div>
                    `;
                }
                grid.appendChild(row);
            });
            console.log(`ServiceMode: Rendered category ${cat} with ${catParams.length} params`);
            container.appendChild(grid);
        }
    },

    updateParam(id, value) {
        const val = parseFloat(value);
        const p = this.params.find(x => x.id === id);
        const display = document.getElementById(`val-${id}`);
        if (p && display) {
            display.innerText = val.toFixed(2) + (p.unit || '');
        }
        juce.setCalibrationParam(id, val);
    },

    renderVoices() {
        const container = document.getElementById('voice-test-grid');
        if (!container) return;

        container.innerHTML = '';
        for (let i = 0; i < 16; i++) {
            const btn = document.createElement('button');
            btn.className = 'voice-test-btn';
            btn.innerText = `V${i + 1}`;
            btn.id = `btn-voice-${i}`;
            btn.onclick = () => this.toggleVoiceTest(i);
            if (this.activeVoice === i) btn.classList.add('active');
            container.appendChild(btn);
        }
    },

    activeVoice: -1,
    toggleVoiceTest(index) {
        const btns = document.querySelectorAll('.voice-test-btn');
        if (this.activeVoice === index) {
            this.activeVoice = -1;
            juce.serviceAction({ action: 'stopVoiceTest' });
            btns.forEach(b => b.classList.remove('active'));
        } else {
            this.activeVoice = index;
            juce.serviceAction({ action: 'testVoice', voice: index });
            btns.forEach(b => b.classList.remove('active'));
            const activeBtn = document.getElementById(`btn-voice-${index}`);
            if (activeBtn) activeBtn.classList.add('active');
        }
    },

    startSweep() {
        juce.serviceAction({ action: 'sweepVCF' });
    },

    startAutoTune() {
        if (confirm("START AUTOMATED VCF TUNING?\nThis requires Voice 1 solo and a reference tone. The process will take ~3 seconds.")) {
            juce.serviceAction({ action: 'autoTuneVCF' });
        }
    },

    testScalePlaying: false,
    playTestScale() {
        this.testScalePlaying = !this.testScalePlaying;
        const btn = document.getElementById('btn-test-scale');
        if (btn) {
            btn.innerText = this.testScalePlaying ? 'STOP TEST SCALE' : 'PLAY TEST SCALE';
            btn.style.background = this.testScalePlaying ? '#060' : '';
        }
        juce.serviceAction({ action: 'playTestScale' });
    },

    exportCalibration() {
        juce.serviceAction({ action: 'exportCalibration' });
    },

    importCalibration() {
        juce.serviceAction({ action: 'importCalibration' });
    },

    async resetCalibration() {
        if (confirm("Restore ALL calibration parameters to factory defaults?")) {
            await juce.serviceAction({ action: 'resetToFactory' });
            await this.refreshParams();
        }
    },

    async resetParam(id) {
        await juce.serviceAction({ action: 'resetParam', id: id });
        await this.refreshParams();
    },

    async resetCategory(cat) {
        if (confirm(`Reset all parameters in ${cat} to defaults?`)) {
            await juce.serviceAction({ action: 'resetCategory', category: cat });
            await this.refreshParams();
        }
    },

    onHostEvent(msg) {
        if (msg.id === 'onCalibrationImported') {
            this.refreshParams();
        }
    }
};

window.ServiceMode = ServiceMode;

// Listen for calibration import event using unified mechanism
if (typeof listenEvent === 'function') {
    listenEvent('onCalibrationImported', () => ServiceMode.refreshParams());
} else if (window.juce) {
    window.addEventListener('message', (event) => {
        if (event.data && event.data.type === 'juce-event') {
            ServiceMode.onHostEvent(event.data.payload);
        }
    });
}
