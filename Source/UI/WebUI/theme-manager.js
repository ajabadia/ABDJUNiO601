/**
 * ABD JUNiO 601 - Theme & Skin Manager
 * Dynamic performance tabs, skin/calibration theme switching, painter tape labels, and tape click filtering.
 */

function switchPerformanceTab(tabName) {
    // 1. Update buttons active state
    document.querySelectorAll('.perf-tab-btn').forEach(btn => {
        const isTarget = btn.id === 'tab-btn-' + tabName;
        btn.classList.toggle('active', isTarget);
    });

    // 2. Update panel contents visibility
    document.querySelectorAll('.perf-tab-content').forEach(panel => {
        const isTarget = panel.id === 'panel-' + tabName;
        panel.classList.toggle('hidden', !isTarget);
    });
}

function deactivatePerformanceTabs() {
    document.querySelectorAll('.perf-tab-btn').forEach(btn => {
        btn.classList.remove('active');
    });
    document.querySelectorAll('.perf-tab-content').forEach(panel => {
        panel.classList.add('hidden');
    });
}

function updateThemeAndSkins() {
    const profileVal = paramValuesCache['calibrationProfile'] !== undefined ? paramValuesCache['calibrationProfile'] : 2.0;

    let targetModel = 0;
    const backend = getBackend();
    const initData = window.__JUCE__ ? window.__JUCE__.initialisationData :
                    (backend ? (backend.initialisationData || (typeof backend.getInitialisationData === 'function' ? backend.getInitialisationData() : null)) : null);

    if (initData && initData.targetModel !== undefined) {
        targetModel = parseInt(initData.targetModel);
    }

    const appSpec = document.getElementById('active-model-spec');

    document.body.removeAttribute('data-theme');

    let skinTheme = "juno-106";
    let labelText = "SUPER SIX HYBRID";
    let modelSubtitle = "601";

    if (targetModel === 1) {
        const skinVal = paramValuesCache['skinType'] !== undefined ? paramValuesCache['skinType'] : 3.0;
        const themesMap = { 3: 'juno-106', 4: 'dark-106s', 5: 'tr-808', 6: 'deepmind', 7: 'space-echo', 8: 'arp-2600', 9: 'jp-80x0' };
        skinTheme = themesMap[Math.round(skinVal)] || 'juno-106';
        labelText = "JUNO-106 MODE";
        modelSubtitle = "601";
    } else if (targetModel === 2) {
        const skinVal = paramValuesCache['skinType'] !== undefined ? paramValuesCache['skinType'] : 1.0;
        const themesMap = { 1: 'juno-60', 5: 'tr-808', 6: 'deepmind', 7: 'space-echo', 8: 'arp-2600', 9: 'jp-80x0' };
        skinTheme = themesMap[Math.round(skinVal)] || 'juno-60';
        labelText = "JUNO-60 MODE";
        modelSubtitle = "06";
    } else if (targetModel === 3) {
        const skinVal = paramValuesCache['skinType'] !== undefined ? paramValuesCache['skinType'] : 2.0;
        const themesMap = { 2: 'juno-6', 5: 'tr-808', 6: 'deepmind', 7: 'space-echo', 8: 'arp-2600', 9: 'jp-80x0' };
        skinTheme = themesMap[Math.round(skinVal)] || 'juno-6';
        labelText = "JUNO-6 MODE";
        modelSubtitle = "SIX";
    } else {
        labelText = "SUPER SIX HYBRID";
        modelSubtitle = "SUPER SIX";
        const skinVal = paramValuesCache['skinType'] !== undefined ? paramValuesCache['skinType'] : 0.0;
        const themesMap = {
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
        skinTheme = themesMap[Math.round(skinVal)] || 'classic';

        if (profileVal === 0.0) {
            labelText = "SUPER SIX (JUNO-6 PROFILE)";
        } else if (profileVal === 1.0) {
            labelText = "SUPER SIX (JUNO-60 PROFILE)";
        } else if (profileVal === 2.0) {
            labelText = "SUPER SIX (JUNO-106 PROFILE)";
        } else {
            labelText = "SUPER SIX (HYBRID MODE)";
        }
    }

    // Set model name subtitle on the performance panel watermark
    const perfPanel = document.querySelector('.sunken-performance-panel');
    if (perfPanel) perfPanel.setAttribute('data-model-name', modelSubtitle);

    document.body.setAttribute('data-theme', skinTheme);
    if (appSpec) appSpec.innerText = labelText;

    document.querySelectorAll('.painter-tape').forEach(tape => {
        tape.classList.toggle('hidden', targetModel !== 0);
        if (profileVal === 3.0) {
            tape.style.pointerEvents = 'auto';
            tape.style.cursor = 'pointer';
        } else {
            tape.style.pointerEvents = 'none';
            tape.style.cursor = 'default';
        }
    });

    const btnArp = document.getElementById('tab-btn-arp');
    const btnPort = document.getElementById('tab-btn-port');
    const btnPoly = document.getElementById('tab-btn-poly');

    if (targetModel === 1) {
        if (btnArp) btnArp.classList.add('hidden');
        if (btnPort) btnPort.classList.remove('hidden');
        if (btnPoly) btnPoly.classList.remove('hidden');
    } else if (targetModel === 2 || targetModel === 3) {
        if (btnArp) btnArp.classList.remove('hidden');
        if (btnPort) btnPort.classList.add('hidden');
        if (btnPoly) btnPoly.classList.add('hidden');
    } else {
        if (profileVal === 0.0 || profileVal === 1.0) {
            if (btnArp) btnArp.classList.remove('hidden');
            if (btnPort) btnPort.classList.add('hidden');
            if (btnPoly) btnPoly.classList.add('hidden');
        } else if (profileVal === 2.0) {
            if (btnArp) btnArp.classList.add('hidden');
            if (btnPort) btnPort.classList.remove('hidden');
            if (btnPoly) btnPoly.classList.remove('hidden');
        } else {
            if (btnArp) btnArp.classList.remove('hidden');
            if (btnPort) btnPort.classList.remove('hidden');
            if (btnPoly) btnPoly.classList.remove('hidden');
        }
    }

    // Hide/show the ROUTING tab button in settings
    const tabRoutingBtn = document.querySelector('button[onclick="switchTab(\'routing\')"]');
    if (tabRoutingBtn) {
        if (profileVal === 3.0) {
            tabRoutingBtn.style.display = '';
        } else {
            tabRoutingBtn.style.display = 'none';
        }
    }

    const activeTabBtn = document.querySelector('.perf-tab-btn.active');
    if (activeTabBtn && activeTabBtn.classList.contains('hidden')) {
        deactivatePerformanceTabs();
    }
}

function updatePainterTapes() {
    const getVal = (paramId) => {
        const el = document.querySelector(`[data-param="${paramId}"]`);
        return el ? parseFloat(el.value) : 1.0;
    };

    const formatTape = (module, val) => {
        const tape = document.getElementById('tape-' + module);
        if (!tape) return;

        const moduleEl = document.getElementById(module);
        if (moduleEl) {
            moduleEl.setAttribute('data-model', val.toString());
        }

        tape.classList.remove('tape-j6', 'tape-j60', 'tape-j106');
        if (val === 0) {
            tape.classList.add('tape-j6');
        } else if (val === 0.5) {
            tape.classList.add('tape-j60');
        } else {
            tape.classList.add('tape-j106');
        }

        let txt = '';
        if (module === 'lfo') {
            txt = val === 0 ? "J6 (Analog)" : (val === 0.5 ? "J60 (LFO)" : "J106 (uPD7811)");
        } else if (module === 'dco') {
            txt = val === 0 ? "J6 (DCO)" : (val === 0.5 ? "J60 (DCO)" : "J106 (8253)");
        } else if (module === 'hpf') {
            txt = val === 0 ? "J6 (Cont)" : (val === 0.5 ? "J60 (122Hz)" : "J106 (Boost)");
        } else if (module === 'vcf') {
            txt = val === 0 ? "J6 (IR3109)" : (val === 0.5 ? "J60 (IR3109)" : "J106 (80017A)");
        } else if (module === 'vca') {
            txt = val === 0 ? "J6 (Shockley)" : (val === 0.5 ? "J60 (Shockley)" : "J106 (Boaris)");
        } else if (module === 'env') {
            txt = val === 0 ? "J6 (Analog RC)" : (val === 0.5 ? "J60 (RC)" : "J106 (Linear)");
        } else if (module === 'chorus') {
            txt = val === 0 ? "J6 (BBD)" : (val === 0.5 ? "J60 (MN3009)" : "J106 (Hiss)");
        }
        tape.innerText = txt;
    };

    formatTape('lfo', getVal('modelPoly'));
    formatTape('dco', getVal('modelDCO'));
    formatTape('hpf', getVal('modelHPF'));
    formatTape('vcf', getVal('modelVCF'));
    formatTape('vca', getVal('modelPoly'));
    formatTape('env', getVal('modelADSR'));
    formatTape('chorus', getVal('modelChorus'));
}

// Click listener to filter calibration modal by module
document.querySelectorAll('.painter-tape').forEach(tape => {
    tape.addEventListener('click', (e) => {
        const module = tape.getAttribute('data-module').toLowerCase();
        showGlobalSettings('calibration');

        setTimeout(() => {
            document.querySelectorAll('.service-section').forEach(sec => {
                const title = sec.querySelector('.service-cat-header span');
                if (title) {
                    const catName = title.innerText.toLowerCase();
                    let match = catName.includes(module);
                    if (module === 'env' && catName.includes('adsr')) {
                        match = true;
                    }
                    sec.style.display = match ? 'block' : 'none';
                    if (match) {
                        const selectEl = sec.querySelector('.model-selector-row select, select');
                        if (selectEl) selectEl.focus();
                    }
                }
            });
            const listContainer = document.getElementById('service-params-list');
            if (listContainer) listContainer.scrollTop = 0;
        }, 100);
    });
});
