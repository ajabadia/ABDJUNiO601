/**
 * ABD JUNiO 601 - UI Keyboard
 * QWERTY piano keyboard playback and global keyboard shortcuts.
 */

let qwertyBaseNote = 48; // C3 (note 48)
const activeQwertyNotes = {};

const QWERTY_KEYS = {
    // Lower octave naturals
    'z': 0, 'x': 2, 'c': 4, 'v': 5, 'b': 7, 'n': 9, 'm': 11,
    ',': 12, '.': 14, '/': 16,
    // Lower octave sharps
    's': 1, 'd': 3, 'g': 6, 'h': 8, 'j': 10, 'l': 13, ';': 15, "'": 17,

    // Upper octave naturals
    'q': 12, 'w': 14, 'e': 16, 'r': 17, 't': 19, 'y': 21, 'u': 23,
    'i': 24, 'o': 26, 'p': 28, '[': 29, ']': 31,
    // Upper octave sharps
    '2': 13, '3': 15, '5': 18, '6': 20, '7': 22, '9': 25, '0': 27, '-': 30, '=': 32
};

function allQwertyNotesOff() {
    Object.keys(activeQwertyNotes).forEach(n => {
        const note = parseInt(n);
        callNative("pianoNoteOff", note);
        const keyOffset = note - (octaveShift * 12);
        const keyEl = document.querySelector(`.key[data-note="${keyOffset}"]`);
        if (keyEl) keyEl.classList.remove('pushed');
    });
    for (let key in activeQwertyNotes) {
        delete activeQwertyNotes[key];
    }
}

window.addEventListener('keydown', (e) => {
    // Skip if user is typing in inputs or textareas
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') {
        return;
    }

    // Handle Ctrl/Cmd Shortcuts (Undo/Redo)
    if ((e.ctrlKey || e.metaKey) && !e.altKey) {
        if (e.key === 'z' || e.key === 'Z') {
            e.preventDefault();
            if (e.shiftKey) {
                callNative("menuAction", "redo");
            } else {
                callNative("menuAction", "undo");
            }
        } else if (e.key === 'y' || e.key === 'Y') {
            e.preventDefault();
            callNative("menuAction", "redo");
        }
        return;
    }

    // Skip other modifier key presses
    if (e.ctrlKey || e.metaKey || e.altKey) {
        return;
    }

    // Handle Octave Shifting
    if (e.key === '`') {
        e.preventDefault();
        qwertyBaseNote = Math.max(12, qwertyBaseNote - 12);
        allQwertyNotesOff();
        return;
    }
    if (e.key === '1') {
        e.preventDefault();
        qwertyBaseNote = Math.min(96, qwertyBaseNote + 12);
        allQwertyNotesOff();
        return;
    }

    // Handle Note On
    const keyChar = e.key.toLowerCase();
    if (QWERTY_KEYS[keyChar] !== undefined) {
        const note = qwertyBaseNote + QWERTY_KEYS[keyChar];
        if (note >= 0 && note <= 127 && !activeQwertyNotes[note]) {
            e.preventDefault();
            activeQwertyNotes[note] = true;
            callNative("pianoNoteOn", note, 0.8);

            // Visual feedback on the onscreen keyboard
            const keyOffset = note - (octaveShift * 12);
            const keyEl = document.querySelector(`.key[data-note="${keyOffset}"]`);
            if (keyEl) keyEl.classList.add('pushed');
        }
    }
});

window.addEventListener('keyup', (e) => {
    // Skip if user is typing in inputs or textareas
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') {
        return;
    }

    const keyChar = e.key.toLowerCase();
    if (QWERTY_KEYS[keyChar] !== undefined) {
        const note = qwertyBaseNote + QWERTY_KEYS[keyChar];
        if (activeQwertyNotes[note]) {
            e.preventDefault();
            delete activeQwertyNotes[note];
            callNative("pianoNoteOff", note);

            // Remove visual highlight
            const keyOffset = note - (octaveShift * 12);
            const keyEl = document.querySelector(`.key[data-note="${keyOffset}"]`);
            if (keyEl) keyEl.classList.remove('pushed');
        }
    }
});
