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
        this.params.forEach(p => {
            const row = document.createElement('div');
            row.className = 'service-param-row';
            row.title = p.tooltip; // Native tooltip for now
            row.innerHTML = `
                <div class="service-param-info">
                    <span class="service-param-label">${p.label}</span>
                    <div class="service-param-values">
                        <span class="service-param-default" title="Factory Default">Def: ${p.defaultValue.toFixed(2)}${p.unit}</span>
                        <span class="service-param-value" id="val-${p.id}">${p.currentValue.toFixed(2)}${p.unit}</span>
                    </div>
                </div>
                <input type="range" class="service-slider" 
                    min="${p.minValue}" max="${p.maxValue}" step="${p.stepSize}" 
                    value="${p.currentValue}" 
                    oninput="ServiceMode.updateParam('${p.id}', this.value)">
            `;
            container.appendChild(row);
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
