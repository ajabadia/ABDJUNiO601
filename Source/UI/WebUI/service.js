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
        
        // [Build 24/25] Order modules as requested: LFO, DCO, HPF, VCF, VCA, ENV, CHORUS
        const categories = ["LFO", "DCO", "HPF", "VCF", "VCA", "CHORUS", "ADSR", "AGING"];
        
        categories.forEach(cat => {
            const catParams = this.params.filter(p => p.category === cat);
            
            // Always create a header for the category as requested
            const header = document.createElement('div');
            header.className = 'service-cat-header';
            header.innerText = (cat === 'AGING') ? 'ANALOG AGING & FIDELITY' : cat;
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
                    row.title = p.tooltip;
                    row.innerHTML = `
                        <div class="service-param-info">
                            <span class="service-param-label">${p.label}</span>
                            <div class="service-param-values">
                                <span class="service-param-default">Def: ${p.defaultValue.toFixed(2)}${p.unit}</span>
                                <span class="service-param-value" id="val-${p.id}">${p.currentValue.toFixed(2)}${p.unit}</span>
                            </div>
                        </div>
                        <input type="range" class="service-slider" 
                            min="${p.minValue}" max="${p.maxValue}" step="${p.stepSize}" 
                            value="${p.currentValue}" 
                            oninput="ServiceMode.updateParam('${p.id}', this.value)">
                    `;
                    grid.appendChild(row);
                });
                container.appendChild(grid);

                // Inject Category Action Buttons
                if (cat === 'VCF' || cat === 'HPF' || cat === 'CHORUS') {
                    const actionContainer = document.createElement('div');
                    actionContainer.className = 'service-cat-actions';
                    
                    let btnText = '';
                    let actionFn = null;
                    let btnId = '';

                    if (cat === 'VCF') { btnText = 'START VCF SWEEP'; actionFn = () => this.startSweep(); btnId = 'btn-vcf-sweep'; }
                    else if (cat === 'HPF') { btnText = 'CYCLE HPF POSITIONS'; actionFn = () => this.startHpfCycle(); btnId = 'btn-hpf-cycle'; }
                    else if (cat === 'CHORUS') { btnText = 'CYCLE CHORUS MODES'; actionFn = () => this.startChorusCycle(); btnId = 'btn-chorus-cycle'; }

                    const btn = document.createElement('button');
                    btn.className = 'tuning-btn service-action-btn';
                    btn.id = btnId;
                    btn.innerText = btnText;
                    btn.onclick = actionFn;
                    
                    actionContainer.appendChild(btn);
                    container.appendChild(actionContainer);
                }
            }
        });
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

    startHpfCycle() {
        juce.serviceAction({ action: 'hpfCycle' });
    },

    startChorusCycle() {
        juce.serviceAction({ action: 'chorusCycle' });
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
        await juce.serviceAction({ action: 'resetToFactory' });
        await this.refreshParams();
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
