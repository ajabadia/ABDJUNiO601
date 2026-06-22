/**
 * ABD JUNiO 601 - UI Modals & Settings
 * Modal dialogs, settings tabs, save modal, notifications, library path management.
 */

// --- GLOBAL SETTINGS & TABS ---
function showGlobalSettings(tabName) {
    const el = document.getElementById('modal-globalSettings');
    if (el) {
        el.style.display = 'flex';
        if (tabName) switchTab(tabName);
    }
}

function hideGlobalSettings() {
    const el = document.getElementById('modal-globalSettings');
    if (el) el.style.display = 'none';
}

function showAbout() {
    const el = document.getElementById('modal-about');
    if (el) el.style.display = 'flex';
}

function hideAbout() {
    const el = document.getElementById('modal-about');
    if (el) el.style.display = 'none';
}

function showBrowser() {
    const el = document.getElementById('modal-browser');
    if (el && window.PresetBrowser) {
        el.style.display = 'flex';
        window.PresetBrowser.init();
    }
}

function hideBrowser() {
    const el = document.getElementById('modal-browser');
    if (el) el.style.display = 'none';
}

function switchTab(tabName) {
    // Reset hidden sections if going to calibration
    if (tabName === 'calibration') {
        document.querySelectorAll('.service-section').forEach(sec => {
            sec.style.display = 'block';
        });
    }

    // 1. Update Buttons
    document.querySelectorAll('.tab-btn').forEach(btn => {
        const isActive = btn.innerText.toLowerCase() === tabName.toLowerCase();
        btn.classList.toggle('active', isActive);
    });

    // 2. Update Content Sections
    document.querySelectorAll('.settings-content').forEach(section => {
        const sectionId = section.id;
        const isActive = sectionId === 'tab-' + tabName;
        section.classList.toggle('active', isActive);

        // Trigger ServiceMode refresh if entering general, calibration or diagnostics
        if (isActive && (tabName === 'general' || tabName === 'calibration' || tabName === 'diagnostics')) {
            if (window.ServiceMode && typeof window.ServiceMode.init === 'function') {
                window.ServiceMode.init();
            }
        }
    });

    updateLCD("MODE: " + tabName.toUpperCase(), true);
}

function updatePref(el) {
    const id = el.getAttribute('data-param');
    let val = parseFloat(el.value);

    // Route through ServiceMode if it's a calibration/pref parameter
    if (window.ServiceMode && window.ServiceMode.params && window.ServiceMode.params.some(p => p.id === id)) {
        window.ServiceMode.updateParam(id, val);
        return;
    }

    let normalized = val;

    if (window.juce && typeof window.juce.setParameter === 'function') {
        window.juce.setParameter(id, parseFloat(normalized));
    } else {
        callNative("setParameter", id, parseFloat(normalized));
    }
    syncUI(id, parseFloat(normalized));
}

// --- TUNING TABLE CLICK HANDLERS ---
function handleTuningClick(type) {
    console.log("handleTuningClick:", type);
    const info = document.getElementById('tuning-info');
    if (type === 'load') {
        if (info) info.innerText = "OPENING FILE SELECTOR...";
        juce.menuAction('handleLoadTuning');
    } else {
        juce.menuAction('handleResetTuning');
        if (info) info.innerText = "RESETTING...";
    }
}

function handleDacTableClick(type) {
    const info = document.getElementById('dac-table-info');
    if (type === 'import') {
        if (info) info.innerText = "OPENING FILE SELECTOR...";
        juce.serviceAction({ action: 'importDacTable' });
    } else {
        if (info) info.innerText = "SAVING...";
        juce.serviceAction({ action: 'exportDacTable' });
    }
}

function handleVcaTableClick(type) {
    const info = document.getElementById('vca-table-info');
    if (type === 'import') {
        if (info) info.innerText = "OPENING FILE SELECTOR...";
        juce.serviceAction({ action: 'importVcaTable' });
    } else {
        if (info) info.innerText = "SAVING...";
        juce.serviceAction({ action: 'exportVcaTable' });
    }
}

function handleLfoSpeedTableClick(type) {
    const info = document.getElementById('lfo-speed-table-info');
    if (type === 'import') {
        if (info) info.innerText = "OPENING FILE SELECTOR...";
        juce.serviceAction({ action: 'importLfoSpeedTable' });
    } else {
        if (info) info.innerText = "SAVING...";
        juce.serviceAction({ action: 'exportLfoSpeedTable' });
    }
}

function handleLfoRampTableClick(type) {
    const info = document.getElementById('lfo-ramp-table-info');
    if (type === 'import') {
        if (info) info.innerText = "OPENING FILE SELECTOR...";
        juce.serviceAction({ action: 'importLfoRampTable' });
    } else {
        if (info) info.innerText = "SAVING...";
        juce.serviceAction({ action: 'exportLfoRampTable' });
    }
}

function handleSubLevelTableClick(type) {
    const info = document.getElementById('sub-level-table-info');
    if (type === 'import') {
        if (info) info.innerText = "OPENING FILE SELECTOR...";
        juce.serviceAction({ action: 'importSubLevelTable' });
    } else {
        if (info) info.innerText = "SAVING...";
        juce.serviceAction({ action: 'exportSubLevelTable' });
    }
}

// --- TABLE IMPORT LISTENERS ---
listenEvent('onTuningLoaded', (data) => {
    const info = document.getElementById('tuning-info');
    if (!info) return;
    if (data === 'reset' || (typeof data === 'string' && data === 'reset')) {
        info.innerText = 'Standard Tuning';
    } else if (data && typeof data === 'object') {
        info.innerText = data.ok ? ('Loaded: ' + data.name) : 'ERROR loading .scl';
    }
});

listenEvent('onDacTableImport', (ok) => {
    const info = document.getElementById('dac-table-info');
    if (info) info.innerText = ok ? 'Custom Table Loaded (4096 values)' : 'IMPORT FAILED - Check CSV format';
});

listenEvent('onVcaTableImport', (ok) => {
    const info = document.getElementById('vca-table-info');
    if (info) info.innerText = ok ? 'Custom Table Loaded (256 values)' : 'IMPORT FAILED - Check CSV format';
});

listenEvent('onLfoSpeedTableImport', (ok) => {
    const info = document.getElementById('lfo-speed-table-info');
    if (info) info.innerText = ok ? 'Custom Table Loaded (128 values)' : 'IMPORT FAILED - Check CSV format';
});

listenEvent('onLfoRampTableImport', (ok) => {
    const info = document.getElementById('lfo-ramp-table-info');
    if (info) info.innerText = ok ? 'Custom Table Loaded (8 values)' : 'IMPORT FAILED - Check CSV format';
});

listenEvent('onSubLevelTableImport', (ok) => {
    const info = document.getElementById('sub-level-table-info');
    if (info) info.innerText = ok ? 'Custom Table Loaded (11 values)' : 'IMPORT FAILED - Check CSV format';
});

// --- SAVE MODAL ---
let pendingSaveSlot = -1;

function showSaveModal(bank, patch) {
    const slot = (bank - 1) * 8 + (patch - 1);
    pendingSaveSlot = slot;

    document.getElementById('save-target-display').innerText = `BANK ${bank} - PATCH ${patch}`;

    // Suggest name: "NEW " + lastPresetName
    const nameInput = document.getElementById('save-patch-name');
    nameInput.value = "NEW " + lastPresetName.substring(0, 12);

    // Set author from global settings
    document.getElementById('save-patch-author').value = globalUserName;

    document.getElementById('modal-savePatch').classList.add('active');
    nameInput.focus();
    nameInput.select();
}

function hideSaveModal() {
    document.getElementById('modal-savePatch').classList.remove('active');
    isWriteArmed = false;
    updateLCD(lastPresetName.toUpperCase().substring(0, 16));
}

function commitInternalSave() {
    const name = document.getElementById('save-patch-name').value || "NEW PATCH";
    const author = document.getElementById('save-patch-author').value || globalUserName;

    if (pendingSaveSlot >= 0) {
        callNative("menuAction", "writeToInternalSlot", pendingSaveSlot, name, author);

        // Visual feedback
        updateLCD("WRITTEN TO RAM");
        setTimeout(() => {
            updateLCD(name.toUpperCase().substring(0, 16));
            isWriteArmed = false;
        }, 1500);
    }

    document.getElementById('modal-savePatch').classList.remove('active');
}

// --- SETTINGS SYNC ---
function initUserSettingsSync() {
    // Fetch initial user name
    callNative("menuAction", "getUserName").then(name => {
        if (name) {
            globalUserName = name;
            const input = document.getElementById('setting-userName');
            if (input) input.value = name;
        }
    });

    const userNameInput = document.getElementById('setting-userName');
    if (userNameInput) {
        userNameInput.addEventListener('change', (e) => {
            globalUserName = e.target.value;
            callNative("menuAction", "setUserName", globalUserName);
        });
    }
}

// --- GLOBAL NOTIFICATIONS ---
function showNotification(message, type = 'success') {
    const toast = document.getElementById('notification-toast');
    const msg = document.getElementById('notification-message');
    if (!toast || !msg) return;

    msg.innerText = message;
    toast.className = `notification-toast active ${type}`;

    setTimeout(() => {
        toast.className = 'notification-toast';
    }, 4000);
}

// --- LIBRARY PATH ---
function browseLibraryPath() {
    const currentPath = document.getElementById('setting-libraryPath')?.value || '';
    juce.chooseDirectory(currentPath || undefined).then(path => {
        if (path) {
            document.getElementById('setting-libraryPath').value = path;
            juce.setLibraryPath(path);
            updateLCD('LIBRARY PATH SET', true);
        }
    }).catch(() => {});
}

function initLibraryPath() {
    juce.getLibraryPath().then(path => {
        const input = document.getElementById('setting-libraryPath');
        if (input && path) input.value = path;
    }).catch(() => {});
}

// Initialize settings sync and library path on DOM ready
document.addEventListener('DOMContentLoaded', () => {
    initUserSettingsSync();
    initLibraryPath();
});

// --- REPAIR EASTER EGG SEQUENCE ---
let repairSequenceRunning = false;
function startRepairSequence() {
    if (repairSequenceRunning) return;
    repairSequenceRunning = true;

    // 1. Close About modal
    hideAbout();

    const app = document.getElementById('synth-app');
    const overlay = document.getElementById('repair-bg-overlay');
    const slide1 = document.getElementById('repair-bg-1');
    const slide2 = document.getElementById('repair-bg-2');
    const slide3 = document.getElementById('repair-bg-3');

    if (!app || !overlay || !slide1 || !slide2 || !slide3) {
        console.error("Repair overlay or app elements not found!");
        repairSequenceRunning = false;
        return;
    }

    // Disable all pointer events on the app to prevent control interaction
    app.style.pointerEvents = 'none';

    // Reset classes and slides
    app.classList.remove('repair-stage-0', 'repair-stage-1', 'repair-stage-2');
    slide1.classList.remove('visible');
    slide2.classList.remove('visible');
    slide3.classList.remove('visible');

    // T = 0s: Fade out chassis panels, show interior.jpg (Slide 1)
    app.classList.add('repair-stage-0');
    overlay.classList.add('active');
    slide1.classList.add('visible');

    // T = 2.5s: Fade out handles, LEDs, screens, keyboard, right buttons, show interior_2.jpg (Slide 2)
    setTimeout(() => {
        app.classList.add('repair-stage-1');
        slide1.classList.remove('visible');
        slide2.classList.add('visible');
    }, 2500);

    // T = 5.0s: Fade out sidebar buttons, show interior_3.jpg (Slide 3)
    setTimeout(() => {
        app.classList.add('repair-stage-2');
        slide2.classList.remove('visible');
        slide3.classList.add('visible');
    }, 5000);

    // T = 8.5s (Wait 3.5 seconds): Re-assemble sidebar buttons, show interior_2.jpg (Slide 2)
    setTimeout(() => {
        app.classList.remove('repair-stage-2');
        slide3.classList.remove('visible');
        slide2.classList.add('visible');
    }, 8500);

    // T = 11.0s: Re-assemble handles, LEDs, screens, keyboard, right buttons, show interior.jpg (Slide 1)
    setTimeout(() => {
        app.classList.remove('repair-stage-1');
        slide2.classList.remove('visible');
        slide1.classList.add('visible');
    }, 11000);

    // T = 13.5s: Re-assemble chassis panels, restore theme background, fade out overlay
    setTimeout(() => {
        app.classList.remove('repair-stage-0');
        slide1.classList.remove('visible');
        overlay.classList.remove('active');

        // T = 14.7s: Restore pointer events after everything has faded back in
        setTimeout(() => {
            app.style.pointerEvents = 'auto';
            repairSequenceRunning = false;
        }, 1200);

    }, 13500);
}
