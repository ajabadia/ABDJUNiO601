// service.js - Service Mode Logic for JUNiO 601

/**
 * Returns the compiled JUNO_TARGET_MODEL value from initData.
 * 0 = Super Six, 1 = J-106, 2 = J-60, 3 = J-6
 */
function getCompiledTargetModel() {
    const backend = getBackend();
    const initData = window.__JUCE__ ? window.__JUCE__.initialisationData :
                    (backend ? (backend.initialisationData || (typeof backend.getInitialisationData === 'function' ? backend.getInitialisationData() : null)) : null);
    if (initData && initData.targetModel !== undefined) {
        return parseInt(initData.targetModel);
    }
    return 0; // Default to Super Six
}

function applySkinTheme(skinVal) {
    console.log("Applying UI Skin Theme:", skinVal);
        const themes = {
        0: 'classic',
        1: 'juno-60',
        2: 'juno-6',
        3: 'juno-106',
        4: 'dark-106s',
        5: 'tr-808',
        6: 'deepmind',
        7: 'space-echo',
        8: 'arp-2600',
        9: 'jp-80x0'
    };
    const themeName = themes[Math.round(skinVal)] || 'classic';
    document.body.setAttribute('data-theme', themeName);
}
window.applySkinTheme = applySkinTheme;

const ServiceMode = {
    params: [],
    showAllParams: false,

    toggleShowAllParams(checked) {
        this.showAllParams = checked;
        this.renderParams();
    },

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
            
            // Popule paramValuesCache
            this.params.forEach(p => {
                paramValuesCache[p.id] = p.currentValue;
            });
            
            this.renderParams();
            
            // Apply skin theme on load
            const skinParam = this.params.find(p => p.id === 'skinType');
            if (skinParam) {
                applySkinTheme(skinParam.currentValue);
            }
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
        const categories = ["LFO", "DCO", "HPF", "VCF", "VCA", "ADSR", "CHORUS", "THERMAL", "AGING", "SYSTEM"].concat(
            getCompiledTargetModel() === 0 ? ["SPACE ECHO"] : []
        );
        
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
        const catModelParams = {
            "LFO": "modelPoly",
            "DCO": "modelDCO",
            "HPF": "modelHPF",
            "VCF": "modelVCF",
            "VCA": "modelPoly",
            "ADSR": "modelADSR",
            "CHORUS": "modelChorus"
        };

        const modelParamId = catModelParams[cat];
        let activeSuffix = '';
        if (modelParamId) {
            const modelVal = paramValuesCache[modelParamId] !== undefined ? paramValuesCache[modelParamId] : 1.0;
            if (modelVal === 0.0) activeSuffix = '_J6';
            else if (modelVal === 0.5) activeSuffix = '_J60';
            else activeSuffix = '_J106';
        }

        const profile = paramValuesCache['calibrationProfile'] !== undefined ? Math.round(paramValuesCache['calibrationProfile']) : 2;

        // Filter parameters based on selected profile, active model suffix, or show all toggle
        const catParams = this.params.filter(p => {
            if (p.category !== cat) return false;
            
            if (this.showAllParams) {
                return true; // Show everything in Super Six Mode
            }
            
            // If category has a specific model (e.g. modelHPF), we filter J6/J60/J106 variants to match the active module model
            if (activeSuffix) {
                if (p.id.endsWith('_J6') || p.id.endsWith('_J60') || p.id.endsWith('_J106')) {
                    return p.id.endsWith(activeSuffix);
                }
                if (this.params.some(x => x.id === p.id + activeSuffix)) return false;
            } else {
                // Otherwise, filter based on global profile (0=J6, 1=J60, 2=J106)
                if (profile === 0) { // Juno-6
                    if (p.id.endsWith("_J60") || p.id.endsWith("_J106")) return false;
                    if (!p.id.endsWith("_J6") && !p.id.endsWith("_J60") && !p.id.endsWith("_J106")) {
                        if (this.params.some(x => x.id === p.id + '_J6')) return false;
                    }
                } else if (profile === 1) { // Juno-60
                    if (p.id.endsWith("_J6") || p.id.endsWith("_J106")) return false;
                    if (!p.id.endsWith("_J6") && !p.id.endsWith("_J60") && !p.id.endsWith("_J106")) {
                        if (this.params.some(x => x.id === p.id + '_J60')) return false;
                    }
                } else if (profile === 2) { // Juno-106
                    if (p.id.endsWith("_J6") || p.id.endsWith("_J60")) return false;
                    if (!p.id.endsWith("_J6") && !p.id.endsWith("_J60") && !p.id.endsWith("_J106")) {
                        if (this.params.some(x => x.id === p.id + '_J106')) return false;
                    }
                }
            }

            // Global profile visibility rules
            if (profile === 0 || profile === 1) { // J6/60 lacks Portamento and Unison
                if (p.id.includes("Porta") || p.id.includes("Unison") || p.id.includes("porta")) return false;
            } else if (profile === 2) { // J106 lacks Arpeggiator
                if (p.id.includes("Arp") || p.id.includes("arp")) return false;
            }
            
            // calibrationProfile: only show in Super Six (compiled model 0) — single-model builds don't need profile switching
            if (p.id === "calibrationProfile" && getCompiledTargetModel() !== 0) return false;
            
            // Hide anachronistic options for fixed/vintage models (not Super Six)
            if (profile !== 3) {
                if (p.id === "aftertouchToVCF" || p.id === "unisonWidth" || p.id === "unisonDetune" || p.id === "sustainMode") return false;
            }
            
            return true;
        });

        if (catParams.length === 0 && cat !== "GENERAL" && !modelParamId) return;

        const section = document.createElement('div');
        section.className = 'service-section';
        section.setAttribute('data-category', cat);

        const header = document.createElement('div');
        header.className = 'service-cat-header';
        header.innerHTML = `
            <span>${cat}</span>
            <button class="service-reset-btn cat-reset" onclick="ServiceMode.resetCategory('${cat}')" title="Reset this section to factory defaults">RESET SECTION</button>
        `;
        section.appendChild(header);

        const grid = document.createElement('div');
        grid.className = 'service-param-grid';

        if (modelParamId) {
            const modelVal = paramValuesCache[modelParamId] !== undefined ? paramValuesCache[modelParamId] : 1.0;
            const modelRow = document.createElement('div');
            modelRow.className = 'service-param-row model-selector-row';
            modelRow.style.borderBottom = '1px dashed rgba(255,255,255,0.15)';
            modelRow.style.paddingBottom = '12px';
            modelRow.style.marginBottom = '12px';

            const modelOptions = {
                "modelPoly": { 0: "Juno-6 (Analog / Shockley)", 0.5: "Juno-60 (LFO / Shockley)", 1: "Juno-106 (uPD7811 / Boaris)" },
                "modelDCO": { 0: "Juno-6 (Continuous DCO)", 0.5: "Juno-60 (Standard DCO)", 1: "Juno-106 (Quartz Clock)" },
                "modelHPF": { 0: "Juno-6 (Continuous HPF)", 0.5: "Juno-60 (Discrete 4-Pos HPF)", 1: "Juno-106 (Active Bass Boost)" },
                "modelVCF": { 0: "Juno-6 (IR3109 SoftClip)", 0.5: "Juno-60 (IR3109 Classic)", 1: "Juno-106 (80017A Emulation)" },
                "modelADSR": { 0: "Juno-6 (Analog RC)", 0.5: "Juno-60 (RC Envelope)", 1: "Juno-106 (Digital Ticks)" },
                "modelChorus": { 0: "Juno-6 (Classic BBD)", 0.5: "Juno-60 (MN3009 Wide)", 1: "Juno-106 (Hiss Emulation)" }
            };

            const options = Object.entries(modelOptions[modelParamId]).map(([val, lbl]) =>
                `<option value="${val}" ${Math.abs(modelVal - parseFloat(val)) < 0.1 ? 'selected' : ''}>${lbl}</option>`
            ).join('');

            modelRow.innerHTML = `
                <div class="service-param-info">
                    <span class="service-param-label" style="color: var(--juno-orange); font-weight: bold;">ACTIVE CIRCUIT MODEL</span>
                    <div class="service-param-values">
                        <span class="service-param-value" id="val-${modelParamId}" style="color: var(--juno-orange); font-weight: bold;">${modelOptions[modelParamId][modelVal] || ''}</span>
                    </div>
                </div>
                <div class="service-slider-row">
                    <select class="service-select" style="border: 1px solid var(--juno-orange);" onchange="ServiceMode.changeCircuitModel('${modelParamId}', this.value)">
                        ${options}
                    </select>
                </div>
            `;
            grid.appendChild(modelRow);
        }

        if (catParams.length === 0 && !modelParamId) {
            const empty = document.createElement('div');
            empty.className = 'service-param-empty';
            empty.innerText = 'No adjustable parameters';
            section.appendChild(empty);
        } else {
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
                    "calibrationProfile": { 0: "JUNO-6 (1982)", 1: "JUNO-60 (1982)", 2: "JUNO-106 (1984)", 3: "SUPER SIX (HYBRID)" },
                    "midiChannel": { 0: "OMNI", 1: "1", 2: "2", 3: "3", 4: "4", 5: "5", 6: "6", 7: "7", 8: "8", 9: "9", 10: "10", 11: "11", 12: "12", 13: "13", 14: "14", 15: "15", 16: "16" },
                    "numVoices": { 1: "1 (MONO)", 2: "2", 4: "4", 6: "6 (CLASSIC)", 8: "8", 12: "12", 16: "16 (MAX)" },
                    "benderRange": { 1: "1 SEMI", 2: "2 SEMIS", 3: "3 SEMIS", 4: "4 SEMIS", 7: "5th", 12: "OCTAVE" },
                    "midiFunction": { 0: "I (NOTES)", 1: "II (+PATCH)", 2: "III (+SYSEX)" },
                    "sustainPedalInvert": { 0: "NORMAL", 1: "INVERTED" },
                    "enableLogging": { 0: "OFF", 1: "ON" },
                    "skinType": { 0: "CLASSIC BLUE", 1: "JUNO-60 CLASSIC", 2: "JUNO-6 ANALOG", 3: "JUNO-106 CLASSIC", 4: "JUNO-106S DARK", 5: "TR-808 SEQUENCER", 6: "DEEPMIND AMBER", 7: "SPACE ECHO RE-201", 8: "ARP 2600 RETRO", 9: "JP-80X0 SUPERSÄW" },
                    "delayReverbType": { 0: "CONVOLUTION (FFT)", 1: "WAVEGUIDE", 2: "HYBRID" }
                };

                if (choiceParams[p.id]) {
                    let currentChoices = choiceParams[p.id];
                    if (p.id === 'skinType') {
                        const targetModel = getCompiledTargetModel();
                        if (targetModel === 0) {
                            // Super Six: all 9 skins available regardless of profile
                            currentChoices = choiceParams[p.id];
                        } else if (targetModel === 1) {
                            // J-106: 106 themes + universal skins
                            currentChoices = { 3: "JUNO-106 CLASSIC", 4: "JUNO-106S DARK", 5: "TR-808 SEQUENCER", 6: "DEEPMIND AMBER", 7: "SPACE ECHO RE-201", 8: "ARP 2600 RETRO", 9: "JP-80X0 SUPERSÄW" };
                        } else if (targetModel === 2) {
                            // J-60: 60 theme + universal skins
                            currentChoices = { 1: "JUNO-60 CLASSIC", 5: "TR-808 SEQUENCER", 6: "DEEPMIND AMBER", 7: "SPACE ECHO RE-201", 8: "ARP 2600 RETRO", 9: "JP-80X0 SUPERSÄW" };
                        } else if (targetModel === 3) {
                            // J-6: 6 theme + universal skins
                            currentChoices = { 2: "JUNO-6 ANALOG", 5: "TR-808 SEQUENCER", 6: "DEEPMIND AMBER", 7: "SPACE ECHO RE-201", 8: "ARP 2600 RETRO", 9: "JP-80X0 SUPERSÄW" };
                        }
                    }

                    const options = Object.entries(currentChoices).map(([val, lbl]) => 
                        `<option value="${val}" ${Math.round(p.currentValue) == val ? 'selected' : ''}>${lbl}</option>`
                    ).join('');

                    const isProfile = p.id === 'calibrationProfile';
                    const resetClick = isProfile ? `ServiceMode.fullResetToProfile()` : `ServiceMode.resetParam('${p.id}')`;
                    const resetTitle = isProfile ? `Reset ALL parameters, skin, and models to this profile` : `Reset to default`;

                    const isSkinDisabled = p.id === 'skinType' && Object.keys(currentChoices).length <= 1;

                    row.innerHTML = `
                        <div class="service-param-info">
                            <span class="service-param-label">${p.label}</span>
                            <div class="service-param-values">
                                <span class="service-param-default">Def: ${currentChoices[Math.round(p.defaultValue)] || currentChoices[Object.keys(currentChoices)[0]]}</span>
                                <span class="service-param-value" id="val-${p.id}">${currentChoices[Math.round(p.currentValue)] || p.currentValue.toFixed(0)}</span>
                            </div>
                        </div>
                        <div class="service-slider-row">
                            <select class="service-select" onchange="ServiceMode.updateParam('${p.id}', this.value)" ${isSkinDisabled ? 'disabled' : ''}>
                                ${options}
                            </select>
                            <button class="service-reset-btn param-reset" id="reset-btn-${p.id}" onclick="${resetClick}" title="${resetTitle}" ${isProfile && Math.round(p.currentValue) !== 3 ? 'style="display:none;"' : ''}>↺</button>
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
            section.appendChild(grid);
        }
        container.appendChild(section);
    },

    async updateParam(id, value) {
        const val = parseFloat(value);
        const p = this.params.find(x => x.id === id);
        const display = document.getElementById(`val-${id}`);
        
        const choiceParams = {
            "calibrationProfile": { 0: "JUNO-6 (1982)", 1: "JUNO-60 (1982)", 2: "JUNO-106 (1984)", 3: "SUPER SIX (HYBRID)" },
            "midiChannel": { 0: "OMNI", 1: "1", 2: "2", 3: "3", 4: "4", 5: "5", 6: "6", 7: "7", 8: "8", 9: "9", 10: "10", 11: "11", 12: "12", 13: "13", 14: "14", 15: "15", 16: "16" },
            "numVoices": { 1: "1 (MONO)", 2: "2", 4: "4", 6: "6 (CLASSIC)", 8: "8", 12: "12", 16: "16 (MAX)" },
            "benderRange": { 1: "1 SEMI", 2: "2 SEMIS", 3: "3 SEMIS", 4: "4 SEMIS", 7: "5th", 12: "OCTAVE" },
            "midiFunction": { 0: "I (NOTES)", 1: "II (+PATCH)", 2: "III (+SYSEX)" },
            "sustainPedalInvert": { 0: "NORMAL", 1: "INVERTED" },
            "enableLogging": { 0: "OFF", 1: "ON" },
            "skinType": { 0: "CLASSIC BLUE", 1: "JUNO-60 CLASSIC", 2: "JUNO-6 ANALOG", 3: "JUNO-106 CLASSIC", 4: "JUNO-106S DARK", 5: "TR-808 SEQUENCER", 6: "DEEPMIND AMBER", 7: "SPACE ECHO RE-201", 8: "ARP 2600 RETRO", 9: "JP-80X0 SUPERSÄW" },
            "delayReverbType": { 0: "CONVOLUTION (FFT)", 1: "WAVEGUIDE", 2: "HYBRID" }
        };

        if (p && display) {
            if (choiceParams[id]) {
                display.innerText = choiceParams[id][Math.round(val)] || val.toString();
            } else {
                display.innerText = val.toFixed(2) + (p.unit || '');
            }
        }
        
        const prevProfile = (id === 'calibrationProfile') ? (paramValuesCache['calibrationProfile'] !== undefined ? Math.round(paramValuesCache['calibrationProfile']) : 2) : null;
        
        if (p) p.currentValue = val;
        
        // Sync values to paramValuesCache
        paramValuesCache[id] = val;
        
        if (id === 'skinType') {
            applySkinTheme(val);
            if (typeof updateThemeAndSkins === 'function') updateThemeAndSkins();
        }
        
        if (id === 'calibrationProfile') {
            const newProfile = Math.round(val);
            if (newProfile === 3) {
                this.softUpdateModelsAndSkin(prevProfile, newProfile);
            } else {
                await this.hardResetToProfileDirect(newProfile);
            }
            const resetBtn = document.getElementById('reset-btn-calibrationProfile');
            if (resetBtn) {
                resetBtn.style.display = (newProfile === 3) ? '' : 'none';
            }
        }
        
        juce.setCalibrationParam(id, val);
    },

    softUpdateModelsAndSkin(prevProfile, newProfile) {
        // Map of model parameter defaults: J6=0, J60=0.5, J106=1
        const profileModelDefaults = {
            0: { "modelDCO": 0.0, "modelHPF": 0.0, "modelVCF": 0.0, "modelADSR": 0.0, "modelChorus": 0.0, "modelArp": 0.0, "modelPoly": 0.0, "modelPorta": 0.0, "modelUnison": 0.0 },
            1: { "modelDCO": 0.5, "modelHPF": 0.5, "modelVCF": 0.5, "modelADSR": 0.5, "modelChorus": 0.5, "modelArp": 0.5, "modelPoly": 0.5, "modelPorta": 0.5, "modelUnison": 0.5 },
            2: { "modelDCO": 1.0, "modelHPF": 1.0, "modelVCF": 1.0, "modelADSR": 1.0, "modelChorus": 1.0, "modelArp": 1.0, "modelPoly": 1.0, "modelPorta": 1.0, "modelUnison": 1.0 },
            3: { "modelDCO": 1.0, "modelHPF": 1.0, "modelVCF": 1.0, "modelADSR": 1.0, "modelChorus": 1.0, "modelArp": 0.5, "modelPoly": 1.0, "modelPorta": 1.0, "modelUnison": 1.0 }
        };

        const prevDefaults = profileModelDefaults[prevProfile] || profileModelDefaults[2];
        const newDefaults = profileModelDefaults[newProfile] || profileModelDefaults[2];

        // 1. Soft update of Routing Model parameters
        for (const [paramId, newVal] of Object.entries(newDefaults)) {
            const currentVal = paramValuesCache[paramId] !== undefined ? parseFloat(paramValuesCache[paramId]) : prevDefaults[paramId];
            const prevDefaultVal = prevDefaults[paramId];

            // If the parameter was at the previous default (not modified by hand), update it to new default
            if (Math.abs(currentVal - prevDefaultVal) < 0.05) {
                console.log(`Soft-updating ${paramId} to ${newVal}`);
                paramValuesCache[paramId] = newVal;
                juce.setParameter(paramId, newVal);
                
                // Update select element in UI if open
                const selectEl = document.querySelector(`select[data-param="${paramId}"]`);
                if (selectEl) selectEl.value = newVal;
            }
        }

        // 2. Soft update of UI Skin Theme (J6=2, J60=1, J106=3, SuperSix=0)
        let newSkinDefault = 0;
        if (newProfile === 0) newSkinDefault = 2;
        else if (newProfile === 1) newSkinDefault = 1;
        else if (newProfile === 2) {
            const currSkin = paramValuesCache['skinType'] !== undefined ? Math.round(paramValuesCache['skinType']) : 3;
            newSkinDefault = (currSkin === 3 || currSkin === 4) ? currSkin : 3;
        } else {
            newSkinDefault = 0;
        }
        
        const prevSkinDefault = prevProfile === 0 ? 2 : (prevProfile === 1 ? 1 : (prevProfile === 2 ? 3 : 0));
        const currentSkin = paramValuesCache['skinType'] !== undefined ? Math.round(paramValuesCache['skinType']) : prevSkinDefault;

        if (currentSkin === prevSkinDefault) {
            console.log(`Soft-updating skinType to ${newSkinDefault}`);
            this.updateParam('skinType', newSkinDefault);
        }

        if (typeof updatePainterTapes === 'function') updatePainterTapes();
        if (typeof updateThemeAndSkins === 'function') updateThemeAndSkins();
        deactivatePerformanceTabs();
        
        // Re-render GENERAL category list to update dynamic choice list/dropdowns
        this.renderCategory("GENERAL", "general-params-list");
    },

    async hardResetToProfileDirect(val) {
        await juce.serviceAction({ action: 'hardResetToProfile', profile: val });
        
        // Re-sync all GUI parameters
        await this.refreshParams();
        
        // Hard update all routing parameters in GUI
        const profileModelDefaults = {
            0: { "modelDCO": 0.0, "modelHPF": 0.0, "modelVCF": 0.0, "modelADSR": 0.0, "modelChorus": 0.0, "modelArp": 0.0, "modelPoly": 0.0, "modelPorta": 0.0, "modelUnison": 0.0 },
            1: { "modelDCO": 0.5, "modelHPF": 0.5, "modelVCF": 0.5, "modelADSR": 0.5, "modelChorus": 0.5, "modelArp": 0.5, "modelPoly": 0.5, "modelPorta": 0.5, "modelUnison": 0.5 },
            2: { "modelDCO": 1.0, "modelHPF": 1.0, "modelVCF": 1.0, "modelADSR": 1.0, "modelChorus": 1.0, "modelArp": 1.0, "modelPoly": 1.0, "modelPorta": 1.0, "modelUnison": 1.0 },
            3: { "modelDCO": 1.0, "modelHPF": 1.0, "modelVCF": 1.0, "modelADSR": 1.0, "modelChorus": 1.0, "modelArp": 0.5, "modelPoly": 1.0, "modelPorta": 1.0, "modelUnison": 1.0 }
        };
        const newDefaults = profileModelDefaults[val];
        for (const [paramId, newVal] of Object.entries(newDefaults)) {
            paramValuesCache[paramId] = newVal;
            juce.setParameter(paramId, newVal);
            const selectEl = document.querySelector(`select[data-param="${paramId}"]`);
            if (selectEl) selectEl.value = newVal;
        }
        
        // Hard update skin
        let targetSkin = 0;
        if (val === 0) targetSkin = 2;      // Juno-6
        else if (val === 1) targetSkin = 1; // Juno-60
        else if (val === 2) {
            const currSkin = paramValuesCache['skinType'] !== undefined ? Math.round(paramValuesCache['skinType']) : 3;
            targetSkin = (currSkin === 3 || currSkin === 4) ? currSkin : 3; // Keep J106 Classic or Dark
        } else {
            targetSkin = 0; // Super Six defaults to Classic Blue
        }
        
        // Set skin parameter (this will call updateParam again, which is fine since it's just skinType)
        this.params.forEach(p => {
            if (p.id === 'skinType') p.currentValue = targetSkin;
        });
        paramValuesCache['skinType'] = targetSkin;
        applySkinTheme(targetSkin);
        
        if (typeof updatePainterTapes === 'function') updatePainterTapes();
        if (typeof updateThemeAndSkins === 'function') updateThemeAndSkins();
        deactivatePerformanceTabs();
        
        // Re-render GENERAL category list to update dynamic choice list/dropdowns
        this.renderCategory("GENERAL", "general-params-list");
        
        // If user is currently in the routing tab and it gets hidden, switch back to general tab
        if (val !== 3) {
            const activeTab = document.querySelector('.tab-btn.active');
            if (activeTab && activeTab.innerText.toLowerCase() === 'routing') {
                if (typeof switchTab === 'function') switchTab('general');
            }
        }
    },

    async fullResetToProfile() {
        const val = paramValuesCache['calibrationProfile'] !== undefined ? Math.round(paramValuesCache['calibrationProfile']) : 2;
        const profileName = val === 0 ? 'JUNO-6' : (val === 1 ? 'JUNO-60' : (val === 2 ? 'JUNO-106' : 'SUPER SIX'));
        if (confirm(`FORCE FULL RESET TO ${profileName} MODEL?\nThis will overwrite all manual settings, skin, routing, and calibrations to their exact factory defaults.`)) {
            await this.hardResetToProfileDirect(val);
            alert("FULL PROFILE RESET COMPLETED");
        }
    },

    changeCircuitModel(paramId, value) {
        const val = parseFloat(value);
        paramValuesCache[paramId] = val;
        
        // Update DSP parameter on C++ host
        juce.setParameter(paramId, val);
        
        // Update the display label if it's currently rendered
        const displayVal = document.getElementById(`val-${paramId}`);
        if (displayVal) {
            const modelOptions = {
                "modelPoly": { 0: "Juno-6 (Analog / Shockley)", 0.5: "Juno-60 (LFO / Shockley)", 1: "Juno-106 (uPD7811 / Boaris)" },
                "modelDCO": { 0: "Juno-6 (Continuous DCO)", 0.5: "Juno-60 (Standard DCO)", 1: "Juno-106 (Quartz Clock)" },
                "modelHPF": { 0: "Juno-6 (Continuous HPF)", 0.5: "Juno-60 (Discrete 4-Pos HPF)", 1: "Juno-106 (Active Bass Boost)" },
                "modelVCF": { 0: "Juno-6 (IR3109 SoftClip)", 0.5: "Juno-60 (IR3109 Classic)", 1: "Juno-106 (80017A Emulation)" },
                "modelADSR": { 0: "Juno-6 (Analog RC)", 0.5: "Juno-60 (RC Envelope)", 1: "Juno-106 (Digital Ticks)" },
                "modelChorus": { 0: "Juno-6 (Classic BBD)", 0.5: "Juno-60 (MN3009 Wide)", 1: "Juno-106 (Hiss Emulation)" }
            };
            displayVal.innerText = modelOptions[paramId][val] || val.toString();
        }

        // Keep dropdown select element under Routing tab in sync
        const selectEl = document.querySelector(`select[data-param="${paramId}"]`);
        if (selectEl) {
            selectEl.value = val;
        }

        // Dynamic visual update for painter tapes and module themes
        if (typeof updatePainterTapes === 'function') {
            updatePainterTapes();
        }

        // Re-render parameters in calibration screen to update variants
        this.renderParams();
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

    isRecordingAudio: false,
    toggleRecord() {
        this.isRecordingAudio = !this.isRecordingAudio;
        const btn = document.getElementById('btn-record-audio');
        const txt = document.getElementById('record-text');
        
        if (btn && txt) {
            if (this.isRecordingAudio) {
                btn.classList.add('recording');
                btn.style.background = '#600';
                txt.innerText = 'STOP RECORDING';
            } else {
                btn.classList.remove('recording');
                btn.style.background = '';
                txt.innerText = 'RECORD AUDIO';
            }
        }
        juce.serviceAction({ action: 'toggleRecord' });
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
