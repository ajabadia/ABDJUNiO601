/**
 * ABD JUNiO 601 - Theme & Skin Manager
 * Dynamic performance tabs, skin/calibration theme switching, painter tape labels, and tape click filtering.
 */

// Delay state (now controlled by ECHO CANCEL toggle, not by the GEN button)
// ECHO CANCEL OFF = effect bypassed (SVG dim, controls hidden)
// ECHO CANCEL ON  = effect active (SVG animated, sync visible, buttons enabled)
let delayEffectActive = true; // true = delay effect is processing (ECHO CANCEL switch OFF = default)
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

    // Stop peak LED animation when leaving delay tab
    if (tabName !== 'delay') {
        startPeakLedAnimation(false);
    }

    // 3. When switching to delay tab: show SVG + nav row only.
    //    Controls only appear when user clicks a subpage button.
    if (tabName === 'delay') {
        const navRow = document.getElementById('delay-nav-row');
        if (navRow) navRow.classList.remove('hidden');

        // Always hide controls on tab switch (user must click subpage)
        const below = document.getElementById('delay-controls-below');
        if (below) below.classList.add('hidden');

        // Show SVG (it may have been hidden by a subpage selection)
        const svg = document.getElementById('tape-echo-visual');
        if (svg) svg.style.display = '';

        // Mark as main view so CSS shows SYNC/ECHO CANCEL toggles
        const panel = document.getElementById('panel-delay');
        if (panel) {
            panel.classList.add('delay-main-view');
            panel.classList.toggle('delay-stopped', !delayEffectActive);
            panel.classList.toggle('delay-running', delayEffectActive);
        }

        // Start/stop peak LED simulation based on effect state
        startPeakLedAnimation(delayEffectActive);
    }
}

// Sync division beats per measure (matches JunoTapeEcho::kDivBeats)
const kSyncDivBeats = [4.0, 2.0, 1.0, 2.0/3.0, 0.5, 1.0/3.0, 0.25, 1.0/6.0, 0.125];

// ARP sync LED blink at tempo
let arpSyncTimer = null;
function startArpSyncBlink() {
    const led = document.getElementById('led-arpSync');
    if (!led) return;
    if (arpSyncTimer) {
        clearInterval(arpSyncTimer);
        arpSyncTimer = null;
    }
    const arpSync = paramValuesCache['arpSync'] !== undefined ?
        paramValuesCache['arpSync'] > 0.5 : false;
    if (!arpSync || typeof delaySyncBPM === 'undefined' || delaySyncBPM <= 0) {
        // LED state is managed by syncUI via arpSync parameter — no timer needed
        return;
    }
    // Quarter-note tempo: 60000 / BPM ms
    const intervalMs = 60000 / delaySyncBPM;
    const clampedInterval = Math.max(125, Math.min(2000, intervalMs));

    let flip = false;
    arpSyncTimer = setInterval(() => {
        flip = !flip;
        led.classList.toggle('active', flip);
    }, clampedInterval);
}

// Simulate peak LED blinking when delay is running
let peakLedTimer = null;
function startPeakLedAnimation(running) {
    const led = document.getElementById('svg-peak-led');
    if (!led) return;
    if (peakLedTimer) {
        clearInterval(peakLedTimer);
        peakLedTimer = null;
    }
    if (!running) {
        led.classList.remove('peak-led-active', 'peak-led-on');
        led.classList.add('peak-led-off');
        return;
    }

    // Check if sync is enabled
    const syncEnabled = paramValuesCache['delaySyncEnabled'] !== undefined ?
        paramValuesCache['delaySyncEnabled'] > 0.5 : false;

    if (syncEnabled && typeof delaySyncBPM !== 'undefined' && delaySyncBPM > 0) {
        // Rhythmic blink at division tempo
        const divIdx = paramValuesCache['delaySyncDivision'] !== undefined ?
            Math.round(paramValuesCache['delaySyncDivision'] * 8) : 2;
        const divBeats = kSyncDivBeats[divIdx] || 1.0;
        const intervalMs = 60000 / (delaySyncBPM * divBeats);
        // Clamp to sensible range (60ms – 2s)
        const clampedInterval = Math.max(62, Math.min(2000, intervalMs));

        led.classList.remove('peak-led-off');
        led.classList.add('peak-led-on');

        // Flash duration: 15% of interval, min 20ms, max 100ms
        const flashDuration = Math.max(20, Math.min(clampedInterval * 0.15, 100));

        peakLedTimer = setInterval(() => {
            led.classList.add('peak-led-active');
            setTimeout(() => led.classList.remove('peak-led-active'), flashDuration);
        }, clampedInterval);
    } else {
        // Random flash pattern simulates audio peaks (FREE mode / no BPM available)
        led.classList.remove('peak-led-off');
        led.classList.add('peak-led-on');
        peakLedTimer = setInterval(() => {
            if (Math.random() > 0.6) {
                led.classList.add('peak-led-active');
                setTimeout(() => led.classList.remove('peak-led-active'), 100);
            }
        }, 300);
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

// [DEPRECATED] toggleDelayPower removed - delay on/off is now controlled by ECHO CANCEL switch
// ECHO CANCEL OFF = effect bypassed (dry signal passes through)
// ECHO CANCEL ON  = effect active (processing)

function initDelayControls() {
    // RE-201 large preset knob: click the knob body to cycle positions
    const knobBody = document.getElementById('preset-knob-pointer') || document.getElementById('delay-preset-knob') || document.querySelector('.preset-knob-body');
    if (knobBody) {
        knobBody.addEventListener('click', function(e) {
            const pointer = document.getElementById('preset-knob-pointer');
            if (!pointer) return;
            // Get current idx, defaulting to 11 (preset 12 = REV ONLY)
            const currentIdx = parseInt(pointer.getAttribute('data-idx') || '11');
            // Cycle forward (shift-click = backward)
            const dir = e.shiftKey ? -1 : 1;
            const nextIdx = ((currentIdx + dir) + 12) % 12;
            setPresetKnobPosition(nextIdx);
            callNative('setParameter', 'delaySetting', nextIdx / 11.0);
        });
        // Right-click cycles backward
        knobBody.addEventListener('contextmenu', function(e) {
            e.preventDefault();
            const pointer = document.getElementById('preset-knob-pointer');
            if (!pointer) return;
            const currentIdx = parseInt(pointer.getAttribute('data-idx') || '11');
            const nextIdx = ((currentIdx - 1) + 12) % 12;
            setPresetKnobPosition(nextIdx);
            callNative('setParameter', 'delaySetting', nextIdx / 11.0);
        });
    }

    // Initial default: preset 12 (REV ONLY) at 180° (6 o'clock)
    setPresetKnobPosition(11);
    // Sync head highlights to initial preset
    updateHeadHighlights(11);

    // Sync initial delayEnabled state to C++ (delay runs when Echo Cancel is OFF = default)
    callNative('setParameter', 'delayEnabled', 1.0);

    // Division selector click handlers
    document.querySelectorAll('.div-lbl').forEach(lbl => {
        lbl.addEventListener('click', function(e) {
            const divIdx = parseInt(this.getAttribute('data-div'));
            if (isNaN(divIdx)) return;
            // Update active state
            document.querySelectorAll('.div-lbl').forEach(l => l.classList.remove('active'));
            this.classList.add('active');
            // Send to native (normalized 0.0-1.0)
            callNative('setParameter', 'delaySyncDivision', divIdx / 8.0);
            // Show feedback on LCD
            if (typeof updateLCD === 'function') {
                updateLCD('SYNC DIV: ' + K_SYNC_DIV_NAMES[divIdx], true);
            }
            // Update SVG sync indicator division label
            const syncEl = document.getElementById('svg-sync-indicator');
            if (syncEl) {
                syncEl.textContent = K_SYNC_DIV_NAMES[divIdx];
            }
        });
    });
}

// Update the SVG sync indicator visibility and division label
function updateSyncIndicator(enabled, divisionIdx) {
    const syncEl = document.getElementById('svg-sync-indicator');
    if (!syncEl) return;
    
    if (enabled && divisionIdx >= 0 && divisionIdx < K_SYNC_DIV_NAMES.length) {
        syncEl.textContent = K_SYNC_DIV_NAMES[divisionIdx];
        syncEl.classList.remove('svg-sync-hidden');
        syncEl.classList.add('svg-sync-visible');
    } else {
        syncEl.textContent = '';
        syncEl.classList.remove('svg-sync-visible');
        syncEl.classList.add('svg-sync-hidden');
    }
    
    // Update SVG BPM indicator
    const bpmEl = document.getElementById('svg-bpm-indicator');
    if (bpmEl) {
        if (enabled) {
            const bpm = typeof delaySyncBPM !== 'undefined' ? delaySyncBPM : 120;
            bpmEl.textContent = bpm + ' BPM';
            bpmEl.classList.remove('svg-bpm-hidden');
            bpmEl.classList.add('svg-bpm-visible');
        } else {
            bpmEl.textContent = '';
            bpmEl.classList.remove('svg-bpm-visible');
            bpmEl.classList.add('svg-bpm-hidden');
        }
    }
    
    // Restart peak LED animation with potentially new sync state
    if (typeof startPeakLedAnimation === 'function') {
        startPeakLedAnimation(delayEffectActive);
    }
}

// Head activation map: 12 presets → [H1, H2, H3] active booleans
const kPresetHeads = [
    [1,0,0],  // 0:  Echo 1 (H1)
    [0,1,0],  // 1:  Echo 2 (H2)
    [0,0,1],  // 2:  Echo 3 (H3)
    [1,1,0],  // 3:  Echo 1+2
    [1,0,0],  // 4:  Echo 1 + Rev
    [0,1,0],  // 5:  Echo 2 + Rev
    [0,0,1],  // 6:  Echo 3 + Rev
    [1,1,0],  // 7:  Echo 1+2 + Rev
    [1,0,1],  // 8:  Echo 1+3 + Rev
    [0,1,1],  // 9:  Echo 2+3 + Rev
    [1,1,1],  // 10: Echo 1+2+3 + Rev
    [0,0,0],  // 11: REV ONLY
];

// Default Sync Enabled per preset (false = free mode, true = sync to DAW BPM)
const kPresetSyncEnabled = [
    false,  // 0:  Echo 1 — free (single head)
    false,  // 1:  Echo 2 — free (single head)
    false,  // 2:  Echo 3 — free (single head)
    true,   // 3:  Echo 1+2 — SYNC (1/8)
    true,   // 4:  Echo 1+3 — SYNC (1/8)
    true,   // 5:  Echo 2+3 — SYNC (1/8T)
    true,   // 6:  Echo 1+2+3 — SYNC (1/16)
    false,  // 7:  Echo+Rev H1 — free (reverb focus)
    false,  // 8:  Echo+Rev H2 — free (reverb focus)
    false,  // 9:  Echo+Rev H3 — free (reverb focus)
    true,   // 10: Echo+Rev 1+2 — SYNC (1/8)
    false,  // 11: REV ONLY — free (no echo)
];

// Default Sync Division per preset (0-8, see JunoArpeggiator::kDivBeats)
// 0=1/1, 1=1/2, 2=1/4, 3=1/4T, 4=1/8, 5=1/8T, 6=1/16, 7=1/16T, 8=1/32
const kPresetSyncDivision = [
    2,  // 0:  Echo 1 — 1/4 note
    2,  // 1:  Echo 2 — 1/4 note
    2,  // 2:  Echo 3 — 1/4 note
    4,  // 3:  Echo 1+2 — 1/8 note (tighter dual)
    4,  // 4:  Echo 1+3 — 1/8 note
    5,  // 5:  Echo 2+3 — 1/8T (triplet feel for two heads)
    6,  // 6:  Echo 1+2+3 — 1/16 note (dense pattern)
    2,  // 7:  Echo+Rev H1 — 1/4 note
    2,  // 8:  Echo+Rev H2 — 1/4 note
    2,  // 9:  Echo+Rev H3 — 1/4 note
    4,  // 10: Echo+Rev 1+2 — 1/8 note
    2,  // 11: REV ONLY — 1/4 note
];

// Default Repeat Rate per preset (0.0=slow/long, 1.0=fast/short)
const kPresetRepeatRate = [
    0.30,  // 0:  Echo 1 — single head, slow decay
    0.40,  // 1:  Echo 2 — medium head
    0.50,  // 2:  Echo 3 — faster repeats for longer head
    0.35,  // 3:  Echo 1+2 — moderate for dual rhythm
    0.40,  // 4:  Echo 1+3 — moderate for wide spacing
    0.45,  // 5:  Echo 2+3 — slightly tighter
    0.50,  // 6:  Echo 1+2+3 — tighter for dense triple pattern
    0.30,  // 7:  Echo+Rev H1 — slow to let reverb breathe
    0.40,  // 8:  Echo+Rev H2 — moderate
    0.50,  // 9:  Echo+Rev H3 — faster
    0.35,  // 10: Echo+Rev 1+2 — moderate
    0.50,  // 11: REV ONLY — neutral (no echo feedback)
];

// Default Intensity per preset (0.0=single repeat, 1.0=max feedback)
const kPresetIntensity = [
    0.40,  // 0:  Echo 1 — moderate feedback, clear repeats
    0.45,  // 1:  Echo 2 — slightly more
    0.45,  // 2:  Echo 3 — same
    0.35,  // 3:  Echo 1+2 — less to avoid muddiness
    0.35,  // 4:  Echo 1+3 — less for wide spacing clarity
    0.40,  // 5:  Echo 2+3 — moderate
    0.30,  // 6:  Echo 1+2+3 — low to prevent overload
    0.40,  // 7:  Echo+Rev H1 — moderate
    0.45,  // 8:  Echo+Rev H2 — slightly more
    0.45,  // 9:  Echo+Rev H3 — same
    0.35,  // 10: Echo+Rev 1+2 — less for clarity
    0.50,  // 11: REV ONLY — neutral
];

// Update the SVG head highlights based on preset index (0-11)
function updateHeadHighlights(idx) {
    idx = ((idx % 12) + 12) % 12;
    const heads = kPresetHeads[idx];
    if (!heads) return;
    for (let h = 0; h < 3; ++h) {
        const headEl = document.getElementById('head-h' + (h + 1));
        if (!headEl) continue;
        headEl.classList.toggle('head-active', heads[h] === 1);
    }
}

// Set the large preset knob to a specific position (0-11) — continuous 360° rotation
function setPresetKnobPosition(idx) {
    // idx 0-11, wraps at 12 (continuous)
    idx = ((idx % 12) + 12) % 12;
    const pointer = document.getElementById('preset-knob-pointer');
    if (!pointer) return;

    // Shift the filmstrip instead of rotating
    const size = pointer.getBoundingClientRect().width || 100;
    const yOffset = -(idx * size);
    pointer.style.backgroundPositionY = yOffset + 'px';
    pointer.style.transform = 'none';
    pointer.setAttribute('data-idx', idx);

    // Update LCD with preset name
    const names = [
        'ECHO 1 (H1)',       // 0 - preset 1
        'ECHO 2 (H2)',       // 1 - preset 2
        'ECHO 3 (H3)',       // 2 - preset 3
        'ECHO 1+2',          // 3 - preset 4
        'ECHO 1+3',          // 4 - preset 5
        'ECHO 2+3',          // 5 - preset 6
        'ECHO 1+2+3',        // 6 - preset 7
        'ECHO+REV (H1)',     // 7 - preset 8
        'ECHO+REV (H2)',     // 8 - preset 9
        'ECHO+REV (H3)',     // 9 - preset 10
        'ECHO+REV (H1+H2)',  // 10 - preset 11
        'REV ONLY'           // 11 - preset 12
    ];
    if (typeof updateLCD === 'function') {
        updateLCD('DELAY: ' + names[idx], true);
    }

    // Sync head highlights for the SVG tape display
    updateHeadHighlights(idx);

    // Apply preset-specific defaults for Repeat Rate, Intensity, and Sync
    // This gives each RE-201 mode its authentic character
    if (kPresetRepeatRate[idx] !== undefined) {
        callNative('setParameter', 'delayRepeatRate', kPresetRepeatRate[idx]);
    }
    if (kPresetIntensity[idx] !== undefined) {
        callNative('setParameter', 'delayIntensity', kPresetIntensity[idx]);
    }
    // Sync settings: default to FREE for all presets, but set division per preset
    if (kPresetSyncEnabled[idx] !== undefined) {
        callNative('setParameter', 'delaySyncEnabled', kPresetSyncEnabled[idx] ? 1.0 : 0.0);
    }
    if (kPresetSyncDivision[idx] !== undefined) {
        callNative('setParameter', 'delaySyncDivision', kPresetSyncDivision[idx] / 8.0);
    }
    // Update SVG sync indicator
    if (typeof updateSyncIndicator === 'function') {
        const isSync = kPresetSyncEnabled[idx] !== undefined ? kPresetSyncEnabled[idx] : false;
        const divIdx = kPresetSyncDivision[idx] !== undefined ? kPresetSyncDivision[idx] : 2;
        updateSyncIndicator(isSync, divIdx);
    }

    // Show sync notification on LCD for multi-head presets with sync enabled
    if (kPresetSyncEnabled[idx]) {
        const divIdx = kPresetSyncDivision[idx] !== undefined ? kPresetSyncDivision[idx] : 2;
        const bpm = typeof delaySyncBPM !== 'undefined' ? delaySyncBPM : 120;
        if (typeof updateLCD === 'function') {
            updateLCD('SYNC: ' + K_SYNC_DIV_NAMES[divIdx] + ' @ ' + bpm + ' BPM', true);
        }
    }
}

// Wire delay controls after DOM ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initDelayControls);
} else {
    initDelayControls();
}

function showDelayMainView() {
    // Return to main delay view: show SVG, hide subpages + controls
    document.querySelectorAll('.delay-subpage').forEach(sp => {
        sp.classList.add('hidden');
    });
    document.querySelectorAll('.delay-subpage-btn').forEach(btn => {
        btn.classList.remove('active');
        const led = btn.querySelector('.nav-led');
        if (led) led.classList.remove('active');
    });
    const svg = document.getElementById('tape-echo-visual');
    if (svg) svg.style.display = '';
    const below = document.getElementById('delay-controls-below');
    if (below) below.classList.add('hidden');
    // Mark panel as main view so CSS shows SYNC/ECHO CANCEL toggles
    const panel = document.getElementById('panel-delay');
    if (panel) panel.classList.add('delay-main-view');
    // Update GEN button active state
    const genBtn = document.getElementById('delay-nav-gen');
    if (genBtn) {
        genBtn.classList.add('active');
        const led = genBtn.querySelector('.nav-led');
        if (led) led.classList.add('active');
    }
}

function switchDelaySubpage(page) {
    // Only allow subpage navigation when delay effect is active
    if (!delayEffectActive) return;
    delayActiveSubpage = page;

    // Remove main-view marker (hides SYNC/ECHO CANCEL via CSS)
    const panel = document.getElementById('panel-delay');
    if (panel) panel.classList.remove('delay-main-view');

    // Update nav button active states
    document.querySelectorAll('.delay-subpage-btn').forEach(btn => {
        const btnPage = btn.id.replace('delay-nav-', '');
        const isActive = btnPage === page;
        btn.classList.toggle('active', isActive);
        const led = btn.querySelector('.nav-led');
        if (led) led.classList.toggle('active', isActive);
    });
    // Deactivate GEN button when a subpage is active
    const genBtn = document.getElementById('delay-nav-gen');
    if (genBtn) {
        genBtn.classList.remove('active');
        const led = genBtn.querySelector('.nav-led');
        if (led) led.classList.remove('active');
    }

    // Show selected subpage content, hide others
    document.querySelectorAll('.delay-subpage').forEach(sp => {
        sp.classList.add('hidden');
    });
    const selected = document.getElementById('delay-sub-' + page);
    if (selected) selected.classList.remove('hidden');

    // Hide SVG when a subpage is active (controls replace visual)
    const svg = document.getElementById('tape-echo-visual');
    if (svg) svg.style.display = 'none';

    // Show controls container
    const below = document.getElementById('delay-controls-below');
    if (below) below.classList.remove('hidden');
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
