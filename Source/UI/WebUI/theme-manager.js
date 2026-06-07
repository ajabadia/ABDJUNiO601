/**
 * ABD JUNiO 601 - Theme & Skin Manager
 * Dynamic performance tabs, skin/calibration theme switching, painter tape labels, and tape click filtering.
 */

// Delay state
let delayPowerOn = false;
let delayActiveSubpage = 'settings';

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

    // 3. When switching to delay tab, show nav row always (ON button + subpage nav).
    //    Subpage controls hidden until ON is pressed.
    if (tabName === 'delay') {
        const navRow = document.getElementById('delay-nav-row');
        if (navRow) navRow.classList.remove('hidden');

        const below = document.getElementById('delay-controls-below');
        if (below) {
            below.classList.toggle('hidden', !delayPowerOn);
        }
        const panel = document.getElementById('panel-delay');
        if (panel) {
            panel.classList.toggle('delay-stopped', !delayPowerOn);
            panel.classList.toggle('delay-running', delayPowerOn);
        }
    }
}

function tabNavLeft() {
    const scroll = document.querySelector('.perf-tabs-scroll');
    if (!scroll) return;
    const tabWidth = scroll.querySelector('.perf-tab-btn')?.offsetWidth || 80;
    scroll.scrollBy({ left: -tabWidth, behavior: 'smooth' });
}

function tabNavRight() {
    const scroll = document.querySelector('.perf-tabs-scroll');
    if (!scroll) return;
    const tabWidth = scroll.querySelector('.perf-tab-btn')?.offsetWidth || 80;
    scroll.scrollBy({ left: tabWidth, behavior: 'smooth' });
}

function deactivatePerformanceTabs() {
    document.querySelectorAll('.perf-tab-btn').forEach(btn => {
        btn.classList.remove('active');
    });
    document.querySelectorAll('.perf-tab-content').forEach(panel => {
        panel.classList.add('hidden');
    });
}

function toggleDelayPower() {
    delayPowerOn = !delayPowerOn;
    const navBtn = document.getElementById('delay-nav-on');
    const below = document.getElementById('delay-controls-below');
    const panel = document.getElementById('panel-delay');
    const led = document.getElementById('led-delay-on');

    if (navBtn) navBtn.classList.toggle('active', delayPowerOn);
    if (led) led.classList.toggle('active', delayPowerOn);
    if (below) below.classList.toggle('hidden', !delayPowerOn);
    if (panel) {
        panel.classList.toggle('delay-stopped', !delayPowerOn);
        panel.classList.toggle('delay-running', delayPowerOn);
    }

    // Enable/disable subpage nav buttons
    document.querySelectorAll('.delay-subpage-btn').forEach(btn => {
        btn.style.opacity = delayPowerOn ? '1' : '0.4';
        btn.style.pointerEvents = delayPowerOn ? 'auto' : 'none';
    });

    // Highlight default subpage button when turning on
    if (delayPowerOn) {
        switchDelaySubpage(delayActiveSubpage);
    }

    callNative('setParameter', 'delayEnabled', delayPowerOn ? 1.0 : 0.0);
}

function initDelayControls() {
    // Wire delay setting buttons (1-11)
    document.querySelectorAll('.delay-setting-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            // Remove active from all siblings
            this.parentElement.querySelectorAll('.delay-setting-btn').forEach(b => b.classList.remove('active'));
            this.classList.add('active');
            const idx = parseInt(this.getAttribute('data-idx'));
            callNative('setParameter', 'delaySetting', idx / 10.0);
        });
    });

    // Wire horizontal sliders
    document.querySelectorAll('.h-slider-input').forEach(input => {
        const unit = input.closest('.h-slider-unit');
        const paramId = unit ? unit.getAttribute('data-param') : null;
        if (!paramId) return;

        const handle = unit.querySelector('.h-slider-handle');

        function updateSlider(val) {
            const pct = parseFloat(val) * 100;
            if (handle) handle.style.left = Math.max(0, Math.min(pct, 100 - 12)) + 'px';
            callNative('setParameter', paramId, parseFloat(val));
        }

        input.addEventListener('input', function() {
            updateSlider(this.value);
        });

        // Initialize position from current value
        updateSlider(input.value);
    });

    // Set initial disabled state for subpage buttons
    document.querySelectorAll('.delay-subpage-btn').forEach(btn => {
        btn.style.opacity = '0.4';
        btn.style.pointerEvents = 'none';
    });
}

// Wire delay controls after DOM ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initDelayControls);
} else {
    initDelayControls();
}

function switchDelaySubpage(page) {
    if (!delayPowerOn) return;
    delayActiveSubpage = page;

    // Update nav button active states (no content hiding — all controls visible simultaneously)
    document.querySelectorAll('.delay-subpage-btn').forEach(btn => {
        const btnPage = btn.id.replace('delay-nav-', '');
        btn.classList.toggle('active', btnPage === page);
    });
}

function updateThemeAndSkins() {
    let profileVal = paramValuesCache['calibrationProfile'] !== undefined ? paramValuesCache['calibrationProfile'] : 2.0;
    console.log(`[DEBUG] updateThemeAndSkins profileVal=${profileVal} cacheProfile=${paramValuesCache['calibrationProfile']}`);

    let targetModel = 0;
    const backend = getBackend();
    const initData = window.__JUCE__ ? window.__JUCE__.initialisationData :
                    (backend ? (backend.initialisationData || (typeof backend.getInitialisationData === 'function' ? backend.getInitialisationData() : null)) : null);

    if (initData && initData.targetModel !== undefined) {
        targetModel = parseInt(initData.targetModel);
    }

    // Sync UI sections to the selected profile by calling bridge-core's
    // updateModelVisibility with the profile mapped to a model number.
    // Mapping: profile 3→0 (SuperSix), 2→1 (J106), 1→2 (J60), 0→3 (J6)
    if (typeof window.__updateModelVisibility === 'function') {
        const profileModelMap = { 0: 3, 1: 2, 2: 1, 3: 0 };
        let mappedModel = profileModelMap[Math.round(profileVal)];
        if (mappedModel !== undefined) {
            window.__updateModelVisibility(mappedModel);
        }
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
        // Tapes are always clickable on Super Six hardware, regardless of profile
        // (profiles only affect which modules default to which circuit model)
        if (targetModel === 0) {
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

    // DELAY tab: only visible when Super Six hardware AND Super Six profile (3)
    const isSuperSixActive = (targetModel === 0 && profileVal === 3.0);
    const delayTab = document.getElementById('tab-btn-delay');
    if (delayTab) delayTab.classList.toggle('hidden', !isSuperSixActive);

    // Tab nav buttons (< >): only visible when Super Six hardware AND Super Six profile (3)
    const tabNavLeft = document.getElementById('tab-nav-left');
    const tabNavRight = document.getElementById('tab-nav-right');
    if (tabNavLeft) tabNavLeft.classList.toggle('hidden', !isSuperSixActive);
    if (tabNavRight) tabNavRight.classList.toggle('hidden', !isSuperSixActive);

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
