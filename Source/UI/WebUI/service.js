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
            row.innerHTML = `
                <div class="service-param-info">
                    <span class="service-param-label">${p.label}</span>
                    <span class="service-param-value" id="val-${p.id}">${p.currentValue.toFixed(2)}${p.unit}</span>
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
        document.getElementById(`val-${id}`).innerText = val.toFixed(2) + (this.params.find(p => p.id === id).unit || '');
        juce.setCalibrationParam(id, val);
    },

    renderVoices() {
        const container = document.getElementById('voice-test-grid');
        if (!container) return;

        container.innerHTML = '';
        for (let i = 0; i < 6; i++) {
            const btn = document.createElement('button');
            btn.className = 'voice-test-btn';
            btn.innerText = `VOICE ${i + 1}`;
            btn.id = `btn-voice-${i}`;
            btn.onclick = () => this.toggleVoiceTest(i);
            container.appendChild(btn);
        }
    },

    activeVoice: -1,
    toggleVoiceTest(index) {
        if (this.activeVoice === index) {
            this.activeVoice = -1;
            juce.serviceAction({ action: 'stopVoiceTest' });
            document.querySelectorAll('.voice-test-btn').forEach(b => b.classList.remove('active'));
        } else {
            this.activeVoice = index;
            juce.serviceAction({ action: 'testVoice', voice: index });
            document.querySelectorAll('.voice-test-btn').forEach(b => b.classList.remove('active'));
            document.getElementById(`btn-voice-${index}`).classList.add('active');
        }
    },

    startSweep() {
        juce.serviceAction({ action: 'sweepVCF' });
    },

    exportCalibration() {
        juce.serviceAction({ action: 'exportCalibration' });
    },

    importCalibration() {
        juce.serviceAction({ action: 'importCalibration' });
    },

    resetCalibration() {
        juce.serviceAction({ action: 'resetToFactory' });
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
